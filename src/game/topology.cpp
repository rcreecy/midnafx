#include "topology.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace midnafx::topology {
namespace {
// These are the GX command/attribute values consumed by Aurora's dl::Reader at
// the pinned Dusklight revision. Aurora's Reader symbols are not exported to
// mods, so this file mirrors its bounded command and layout rules locally.
constexpr unsigned Pos = 9, Nrm = 10, Clr0 = 11, Clr1 = 12, Tex0 = 13, Tex7 = 20;
constexpr std::size_t MaxDisplayList = 16 * 1024 * 1024;
constexpr std::size_t MaxTriangles = 2'000'000;

template <class T> std::uint64_t vector_bytes(const std::vector<T>& values) {
    return static_cast<std::uint64_t>(values.capacity()) * sizeof(T);
}
std::uint64_t vector_bytes(const std::vector<bool>& values) { return (values.capacity() + 7) / 8; }
template <class T> std::uint64_t nested_vector_bytes(const std::vector<std::vector<T>>& values) {
    std::uint64_t bytes = vector_bytes(values);
    for (const auto& inner : values)
        bytes += vector_bytes(inner);
    return bytes;
}

struct Slot {
    std::uint8_t kind = 0;
    std::uint8_t size = 0;
    std::uint8_t offset = 0;
};

struct Layout {
    std::array<Slot, 21> slots{};
    std::uint8_t stride = 0;
};

std::uint16_t be16(const std::uint8_t* p) { return static_cast<std::uint16_t>((p[0] << 8) | p[1]); }
std::uint32_t be32(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) |
           p[3];
}
bool fits(std::size_t at, std::size_t bytes, std::size_t size) {
    return at <= size && bytes <= size - at;
}

unsigned direct_size(unsigned attr, const Format* fmt) {
    if (attr <= 8)
        return 1; // PNMTXIDX / TEXMTXIDX
    if (!fmt)
        return 0;
    if (attr == Clr0 || attr == Clr1) {
        switch (fmt->type) {
        case 0:
        case 3:
            return 2;
        case 1:
        case 4:
            return 3;
        case 2:
        case 5:
            return 4;
        default:
            return 0;
        }
    }
    const unsigned component_bytes = fmt->type <= 1   ? 1
                                     : fmt->type <= 3 ? 2
                                     : fmt->type == 4 ? 4
                                                      : 0;
    unsigned components = 0;
    if (attr == Pos)
        components = fmt->components <= 1 ? 2 + fmt->components : 0;
    else if (attr == Nrm)
        components = fmt->components == 0 ? 3 : fmt->components <= 2 ? 9 : 0;
    else if (attr >= Tex0 && attr <= Tex7)
        components = fmt->components <= 1 ? 1 + fmt->components : 0;
    return component_bytes * components;
}

bool make_layout(std::span<const Attribute> attrs, std::span<const Format> fmts, Layout& out) {
    if (attrs.empty() || attrs.size() > 32)
        return false;
    std::array<std::uint8_t, 21> kinds{};
    for (const auto attr : attrs) {
        if (attr.id > Tex7 || attr.kind > 3 || (attr.kind && kinds[attr.id]))
            return false;
        kinds[attr.id] = attr.kind;
    }
    unsigned stride = 0;
    for (unsigned id = 0; id <= Tex7; ++id) {
        const unsigned kind = kinds[id];
        if (!kind)
            continue;
        const auto it =
            std::find_if(fmts.begin(), fmts.end(), [&](const Format& f) { return f.id == id; });
        const Format* fmt = it == fmts.end() ? nullptr : &*it;
        unsigned size = kind == 1 ? direct_size(id, fmt) : kind == 2 ? 1 : 2;
        if (id == Nrm && kind >= 2 && fmt && fmt->components == 2)
            size *= 3; // NBT3 carries three independent indices.
        if (!size || stride + size > 255)
            return false;
        out.slots[id] = {static_cast<std::uint8_t>(kind), static_cast<std::uint8_t>(size),
                         static_cast<std::uint8_t>(stride)};
        stride += size;
    }
    if (!stride || out.slots[Pos].kind < 2 || out.slots[Nrm].kind < 2 ||
        out.slots[Nrm].size > 2) // NBT3 is not a single normal index.
        return false;
    out.stride = static_cast<std::uint8_t>(stride);
    return true;
}

Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float length(Vec3 a) { return std::sqrt(dot(a, a)); }
float angle(Vec3 a, Vec3 b) {
    const float la = length(a), lb = length(b);
    if (!(la > 0 && lb > 0))
        return 0;
    return std::acos(std::clamp(dot(a, b) / (la * lb), -1.0f, 1.0f));
}

bool add_triangle(Result& out, std::span<const Vec3> positions, const std::vector<Corner>& vertices,
                  unsigned a, unsigned b, unsigned c, std::uint16_t shape, std::uint16_t material,
                  std::uint16_t group) {
    if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) {
        out.error = "draw index outside vertex range";
        return false;
    }
    const Corner ca = vertices[a], cb = vertices[b], cc = vertices[c];
    if (ca.position == cb.position || cb.position == cc.position || cc.position == ca.position) {
        ++out.degenerate_count;
        return true;
    }
    const Vec3 p = positions[ca.position], q = positions[cb.position], r = positions[cc.position];
    const Vec3 e1 = sub(q, p), e2 = sub(r, p);
    const Vec3 n = cross(e1, e2);
    const float magnitude = length(n);
    if (!std::isfinite(magnitude) || magnitude <= 1e-8f) {
        ++out.degenerate_count;
        return true;
    }
    if (out.triangles.size() >= MaxTriangles) {
        out.error = "triangle budget exceeded";
        return false;
    }
    out.triangles.push_back(
        {{{ca, cb, cc}},
         shape,
         material,
         group,
         {n.x / magnitude, n.y / magnitude, n.z / magnitude},
         {angle(e1, e2), angle(sub(r, q), sub(p, q)), angle(sub(p, r), sub(q, r))}});
    return true;
}

bool decode_group(Result& out, std::span<const Vec3> positions, std::uint32_t normal_count,
                  std::span<const std::uint8_t> dl, const Layout& layout, std::uint16_t shape,
                  std::uint16_t material, std::uint16_t group) {
    std::size_t at = 0;
    while (at < dl.size()) {
        const std::uint8_t cmd = dl[at];
        const unsigned op = cmd & 0xf8;
        if (cmd == 0 || cmd == 0x48) {
            ++at;
            continue;
        }
        if (cmd == 0x61 || op == 0x08 || op == 0x20 || op == 0x28 || op == 0x30 || op == 0x38) {
            const std::size_t bytes = cmd == 0x61 || op != 0x08 ? 5 : 6;
            if (!fits(at, bytes, dl.size())) {
                out.error = "state command overrun";
                return false;
            }
            at += bytes;
            continue;
        }
        if (op == 0x10) {
            if (!fits(at, 5, dl.size())) {
                out.error = "XF header overrun";
                return false;
            }
            const std::size_t bytes = 5 + (std::size_t(be16(dl.data() + at + 1)) + 1) * 4;
            if (!fits(at, bytes, dl.size())) {
                out.error = "XF payload overrun";
                return false;
            }
            at += bytes;
            continue;
        }
        if (op == 0x40) {
            out.error = "nested display list unsupported";
            return false;
        }

        bool indexed = op == 0x50;
        unsigned primitive = op;
        std::size_t vertex_count = 0, index_count = 0, vertices_at = 0, indices_at = 0;
        if (indexed) {
            if (!fits(at, 10, dl.size()) || be16(dl.data() + at + 1) != 0x41) {
                out.error = "unsupported Aurora command";
                return false;
            }
            primitive = dl[at + 3] & 0xf8;
            vertex_count = be16(dl.data() + at + 4);
            index_count = be32(dl.data() + at + 6);
            if (index_count > MaxTriangles * 3 || !fits(at + 10, index_count * 2, dl.size())) {
                out.error = "indexed payload overrun";
                return false;
            }
            indices_at = at + 10;
            vertices_at = indices_at + index_count * 2;
            ++out.indexed_count;
        } else {
            if (!fits(at, 3, dl.size())) {
                out.error = "draw header overrun";
                return false;
            }
            vertex_count = be16(dl.data() + at + 1);
            index_count = vertex_count;
            vertices_at = at + 3;
        }
        if (primitive != 0x80 && primitive != 0x90 && primitive != 0x98 && primitive != 0xa0 &&
            primitive != 0xa8 && primitive != 0xb0 && primitive != 0xb8) {
            out.error = "unsupported draw primitive";
            return false;
        }
        if (!fits(vertices_at, vertex_count * layout.stride, dl.size())) {
            out.error = "vertex payload overrun";
            return false;
        }
        const std::size_t end = vertices_at + vertex_count * layout.stride;
        ++out.primitive_count;
        if (primitive == 0x98)
            ++out.strip_count;
        if (primitive == 0xa0)
            ++out.fan_count;
        if (primitive >= 0xa8) {
            ++out.ignored_nontriangles;
            at = end;
            continue;
        }
        if ((primitive == 0x90 && index_count % 3) || (primitive == 0x80 && index_count % 4) ||
            ((primitive == 0x98 || primitive == 0xa0) && index_count < 3)) {
            out.error = "invalid primitive vertex count";
            return false;
        }

        std::vector<Corner> vertices;
        vertices.reserve(vertex_count);
        out.peak_temporary_vector_bytes =
            std::max(out.peak_temporary_vector_bytes, vector_bytes(vertices));
        for (std::size_t i = 0; i < vertex_count; ++i) {
            const auto* vertex = dl.data() + vertices_at + i * layout.stride;
            const Slot ps = layout.slots[Pos], ns = layout.slots[Nrm];
            const std::uint16_t p = ps.kind == 2 ? vertex[ps.offset] : be16(vertex + ps.offset);
            const std::uint16_t n = ns.kind == 2 ? vertex[ns.offset] : be16(vertex + ns.offset);
            if (p >= positions.size() || n >= normal_count) {
                out.error = "position or normal index out of bounds";
                return false;
            }
            vertices.push_back({p, n});
        }
        const auto v = [&](std::size_t i) -> unsigned {
            if (!indexed)
                return static_cast<unsigned>(i);
            std::uint16_t value;
            std::memcpy(&value, dl.data() + indices_at + i * 2, 2); // Aurora host-endian
            return value;
        };
        const auto tri = [&](std::size_t a, std::size_t b, std::size_t c) {
            return add_triangle(out, positions, vertices, v(a), v(b), v(c), shape, material, group);
        };
        if (primitive == 0x90) {
            for (std::size_t i = 0; i < index_count; i += 3)
                if (!tri(i, i + 1, i + 2))
                    return false;
        } else if (primitive == 0x98) {
            for (std::size_t i = 2; i < index_count; ++i)
                if (!(i & 1 ? tri(i - 1, i - 2, i) : tri(i - 2, i - 1, i)))
                    return false;
        } else if (primitive == 0xa0) {
            for (std::size_t i = 2; i < index_count; ++i)
                if (!tri(0, i - 1, i))
                    return false;
        } else {
            for (std::size_t i = 0; i < index_count; i += 4)
                if (!tri(i, i + 1, i + 2) || !tri(i + 2, i + 3, i))
                    return false;
        }
        at = end;
    }
    return true;
}
} // namespace

Result decode(std::span<const Shape> shapes, std::span<const Format> formats,
              std::span<const Vec3> positions, std::uint32_t normal_count) {
    Result result;
    if (shapes.empty() || shapes.size() > 4096 || formats.size() > 32 || positions.empty() ||
        positions.size() > 65536 || normal_count == 0 || normal_count > 65536) {
        result.error = "invalid model dimensions";
        return result;
    }
    for (std::size_t si = 0; si < shapes.size(); ++si) {
        const auto& shape = shapes[si];
        Layout layout;
        if (!make_layout(shape.attributes, formats, layout)) {
            result.error = "unsupported vertex layout";
            return result;
        }
        if (shape.groups.empty() || shape.groups.size() > 4096) {
            result.error = "invalid matrix group count";
            return result;
        }
        for (std::size_t gi = 0; gi < shape.groups.size(); ++gi) {
            const auto& group = shape.groups[gi];
            if (!group.display_list || !group.size || group.size > MaxDisplayList) {
                result.error = "invalid display list";
                return result;
            }
            if (!decode_group(result, positions, normal_count, {group.display_list, group.size},
                              layout, static_cast<std::uint16_t>(si), shape.material,
                              static_cast<std::uint16_t>(gi)))
                return result;
        }
    }
    std::vector<std::vector<std::uint16_t>> normal_by_position(positions.size());
    std::vector<bool> used_normal(normal_count);
    for (const auto& triangle : result.triangles)
        for (const auto& corner : triangle.corners) {
            auto& normals = normal_by_position[corner.position];
            if (std::find(normals.begin(), normals.end(), corner.normal) == normals.end())
                normals.push_back(corner.normal);
            used_normal[corner.normal] = true;
        }
    result.peak_temporary_vector_bytes =
        std::max(result.peak_temporary_vector_bytes,
                 nested_vector_bytes(normal_by_position) + vector_bytes(used_normal));
    for (const auto& normals : normal_by_position) {
        result.unique_positions += !normals.empty();
        result.position_normal_splits += normals.size() > 1;
    }
    result.unique_normals =
        static_cast<std::uint32_t>(std::count(used_normal.begin(), used_normal.end(), true));
    std::uint64_t hash = 14695981039346656037ull;
    const auto add16 = [&](std::uint16_t value) {
        for (unsigned shift : {0u, 8u}) {
            hash ^= (value >> shift) & 0xff;
            hash *= 1099511628211ull;
        }
    };
    for (const auto& triangle : result.triangles) {
        add16(triangle.shape);
        add16(triangle.group);
        for (const auto& corner : triangle.corners) {
            add16(corner.position);
            add16(corner.normal);
        }
    }
    result.corner_hash = hash;
    return result;
}
} // namespace midnafx::topology
