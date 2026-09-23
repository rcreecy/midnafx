#include "bmd_rebuild.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace midnafx::bmd_rebuild {
namespace {
constexpr std::size_t HeaderSize = 32;
constexpr std::size_t MaxFileSize = 512 * 1024 * 1024;
constexpr std::size_t MaxBlocks = 256;
constexpr std::size_t MaxShapes = 4096;
constexpr std::size_t MaxGroups = 65536;
constexpr std::size_t MaxTriangles = 2'000'000;
constexpr unsigned Pos = 9, Nrm = 10, Clr0 = 11, Clr1 = 12, Tex0 = 13, Tex7 = 20;

struct Failure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Block {
    std::uint32_t tag = 0;
    std::size_t offset = 0;
    std::size_t size = 0;
};

struct Format {
    std::uint32_t components = 0;
    std::uint32_t type = 0;
    std::uint8_t fraction = 0;
    bool present = false;
};

struct Slot {
    std::size_t offset = 0;
    std::uint32_t kind = 0;
};

struct Layout {
    std::array<Slot, 21> slots{};
    std::size_t stride = 0;
};

struct Vec3 {
    float x, y, z;
};

struct GroupRewrite {
    std::size_t shape = 0;
    std::size_t group = 0;
    std::size_t draw_index = 0;
    std::vector<std::byte> bytes;
    std::uint32_t draws = 0;
};

struct Parsed {
    std::vector<Block> blocks;
    Block vtx, shp;
    std::array<Format, 21> formats{};
    std::vector<Vec3> positions;
    std::size_t normal_at = 0;
    std::size_t normal_end = 0;
    std::size_t normal_stride = 0;
    std::vector<std::byte> normals;
    std::vector<GroupRewrite> groups;
    Evidence evidence;
};

using Bytes = std::span<const std::byte>;

bool fits(std::size_t at, std::size_t count, std::size_t size) {
    return at <= size && count <= size - at;
}

std::uint8_t u8(Bytes data, std::size_t at) {
    if (!fits(at, 1, data.size()))
        throw Failure("byte read outside resource");
    return std::to_integer<std::uint8_t>(data[at]);
}

std::uint16_t be16(Bytes data, std::size_t at) {
    if (!fits(at, 2, data.size()))
        throw Failure("u16 read outside resource");
    return static_cast<std::uint16_t>((u8(data, at) << 8) | u8(data, at + 1));
}

std::uint32_t be32(Bytes data, std::size_t at) {
    if (!fits(at, 4, data.size()))
        throw Failure("u32 read outside resource");
    return (std::uint32_t(u8(data, at)) << 24) | (std::uint32_t(u8(data, at + 1)) << 16) |
           (std::uint32_t(u8(data, at + 2)) << 8) | u8(data, at + 3);
}

void put16(std::vector<std::byte>& data, std::size_t at, std::uint16_t value) {
    if (!fits(at, 2, data.size()))
        throw Failure("u16 write outside resource");
    data[at] = std::byte(value >> 8);
    data[at + 1] = std::byte(value & 0xff);
}

void put32(std::vector<std::byte>& data, std::size_t at, std::uint32_t value) {
    if (!fits(at, 4, data.size()))
        throw Failure("u32 write outside resource");
    data[at] = std::byte(value >> 24);
    data[at + 1] = std::byte((value >> 16) & 0xff);
    data[at + 2] = std::byte((value >> 8) & 0xff);
    data[at + 3] = std::byte(value & 0xff);
}

std::uint32_t tag(const char value[5]) {
    return (std::uint32_t(value[0]) << 24) | (std::uint32_t(value[1]) << 16) |
           (std::uint32_t(value[2]) << 8) | std::uint8_t(value[3]);
}

std::size_t aligned(std::size_t value, std::size_t boundary = 32) {
    if (value > std::numeric_limits<std::size_t>::max() - (boundary - 1))
        throw Failure("alignment overflow");
    return (value + boundary - 1) / boundary * boundary;
}

std::size_t checked_add(std::size_t a, std::size_t b, const char* error) {
    if (a > std::numeric_limits<std::size_t>::max() - b)
        throw Failure(error);
    return a + b;
}

std::size_t checked_mul(std::size_t a, std::size_t b, const char* error) {
    if (a && b > std::numeric_limits<std::size_t>::max() / a)
        throw Failure(error);
    return a * b;
}

void append(std::vector<std::byte>& to, Bytes from) {
    if (checked_add(to.size(), from.size(), "output size overflow") > MaxFileSize)
        throw Failure("output file budget exceeded");
    to.insert(to.end(), from.begin(), from.end());
}

void add16(std::uint64_t& hash, std::uint16_t value) {
    for (unsigned shift : {0u, 8u}) {
        hash ^= (value >> shift) & 0xff;
        hash *= 1099511628211ULL;
    }
}

std::vector<Block> parse_blocks(Bytes data) {
    if (data.size() < HeaderSize || data.size() > MaxFileSize ||
        !(be32(data, 0) == tag("J3D1") || be32(data, 0) == tag("J3D2")) ||
        be32(data, 8) != data.size())
        throw Failure("invalid J3D header");
    const std::size_t count = be32(data, 12);
    if (!count || count > MaxBlocks)
        throw Failure("invalid J3D block count");
    std::vector<Block> blocks;
    blocks.reserve(count);
    std::size_t at = HeaderSize;
    for (std::size_t i = 0; i < count; ++i) {
        if (!fits(at, 8, data.size()))
            throw Failure("truncated J3D block");
        const std::size_t size = be32(data, at + 4);
        if (size < 8 || !fits(at, size, data.size()))
            throw Failure("invalid J3D block length");
        blocks.push_back({be32(data, at), at, size});
        at += size;
    }
    if (at != data.size())
        throw Failure("trailing bytes after J3D blocks");
    return blocks;
}

Block one_block(const std::vector<Block>& blocks, std::uint32_t wanted) {
    Block found{};
    unsigned count = 0;
    for (const auto& block : blocks)
        if (block.tag == wanted) {
            found = block;
            ++count;
        }
    if (count != 1)
        throw Failure("required J3D block missing or duplicated");
    return found;
}

unsigned direct_size(unsigned attr, const Format& fmt) {
    if (attr <= 8)
        return 1;
    if (!fmt.present)
        return 0;
    if (attr == Clr0 || attr == Clr1) {
        static constexpr unsigned sizes[] = {2, 3, 4, 2, 3, 4};
        return fmt.type < std::size(sizes) ? sizes[fmt.type] : 0;
    }
    const unsigned component = fmt.type <= 1 ? 1 : fmt.type <= 3 ? 2 : fmt.type == 4 ? 4 : 0;
    unsigned count = 0;
    if (attr == Pos)
        count = fmt.components <= 1 ? 2 + fmt.components : 0;
    else if (attr == Nrm)
        count = fmt.components == 0 ? 3 : fmt.components <= 2 ? 9 : 0;
    else if (attr >= Tex0 && attr <= Tex7)
        count = fmt.components <= 1 ? 1 + fmt.components : 0;
    return component * count;
}

Layout parse_layout(Bytes data, const Block& shp, std::size_t begin,
                    const std::array<Format, 21>& formats) {
    Layout out;
    std::array<bool, 21> seen{};
    std::size_t at = begin;
    for (unsigned i = 0; i < 32; ++i, at += 8) {
        if (!fits(at, 8, shp.offset + shp.size))
            throw Failure("shape descriptor overrun");
        const std::uint32_t attr = be32(data, at), kind = be32(data, at + 4);
        if (attr == 255) {
            if (!seen[Pos] || !seen[Nrm] || out.slots[Pos].kind < 2 ||
                out.slots[Nrm].kind != 3 || !out.stride)
                throw Failure("split requires indexed position and GX_INDEX16 normal");
            return out;
        }
        if (attr > Tex7 || kind < 1 || kind > 3 || seen[attr])
            throw Failure("invalid shape descriptor");
        seen[attr] = true;
        unsigned size = kind == 1 ? direct_size(attr, formats[attr]) : kind - 1;
        if (attr == Nrm && kind >= 2 && formats[attr].present && formats[attr].components == 2)
            size *= 3;
        if (!size || out.stride + size > 255)
            throw Failure("unsupported shape descriptor");
        out.slots[attr] = {out.stride, kind};
        out.stride += size;
    }
    throw Failure("unterminated shape descriptor");
}

float be_float(Bytes data, std::size_t at) {
    return std::bit_cast<float>(be32(data, at));
}

float length(Vec3 value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

bool nondegenerate(const std::array<std::vector<std::byte>, 3>& triangle, const Layout& layout,
                   const std::vector<Vec3>& positions) {
    std::array<std::uint16_t, 3> indices{};
    const auto slot = layout.slots[Pos];
    for (unsigned i = 0; i < 3; ++i) {
        Bytes record{triangle[i].data(), triangle[i].size()};
        indices[i] = slot.kind == 2 ? u8(record, slot.offset) : be16(record, slot.offset);
        if (indices[i] >= positions.size())
            throw Failure("position index out of bounds");
    }
    if (indices[0] == indices[1] || indices[1] == indices[2] || indices[2] == indices[0])
        return false;
    const Vec3 p = positions[indices[0]], q = positions[indices[1]], r = positions[indices[2]];
    const Vec3 a{q.x - p.x, q.y - p.y, q.z - p.z};
    const Vec3 b{r.x - p.x, r.y - p.y, r.z - p.z};
    const Vec3 cross{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                     a.x * b.y - a.y * b.x};
    const float magnitude = length(cross);
    return std::isfinite(magnitude) && magnitude > 1e-8f;
}

Parsed parse(Bytes data, bool make_rewrite) {
    Parsed out;
    out.blocks = parse_blocks(data);
    out.vtx = one_block(out.blocks, tag("VTX1"));
    out.shp = one_block(out.blocks, tag("SHP1"));
    if (out.vtx.size < 64 || out.shp.size < 44)
        throw Failure("truncated VTX1 or SHP1");

    const std::size_t fmt_at = checked_add(out.vtx.offset, be32(data, out.vtx.offset + 8),
                                           "format offset overflow");
    bool terminated = false;
    for (unsigned i = 0; i < 32; ++i) {
        const std::size_t at = fmt_at + i * 16;
        if (!fits(at, 16, out.vtx.offset + out.vtx.size))
            throw Failure("VTX1 format list overrun");
        const std::uint32_t attr = be32(data, at);
        if (attr == 255) {
            terminated = true;
            break;
        }
        if (attr > Tex7 || out.formats[attr].present)
            throw Failure("invalid VTX1 format list");
        out.formats[attr] = {be32(data, at + 4), be32(data, at + 8), u8(data, at + 12), true};
    }
    if (!terminated || !out.formats[Pos].present || !out.formats[Nrm].present)
        throw Failure("missing position or normal format");

    const std::size_t pos_relative = be32(data, out.vtx.offset + 12);
    const std::size_t normal_relative = be32(data, out.vtx.offset + 16);
    if (!pos_relative || !normal_relative || pos_relative >= out.vtx.size ||
        normal_relative >= out.vtx.size)
        throw Failure("invalid VTX1 array offset");
    std::size_t pos_end = out.vtx.size, normal_end_relative = out.vtx.size;
    for (std::size_t field = 16; field < 64; field += 4) {
        const std::size_t value = be32(data, out.vtx.offset + field);
        if (value > pos_relative && value < pos_end)
            pos_end = value;
    }
    for (std::size_t field = 20; field < 64; field += 4) {
        const std::size_t value = be32(data, out.vtx.offset + field);
        if (value > normal_relative && value < normal_end_relative)
            normal_end_relative = value;
    }
    if (normal_end_relative == out.vtx.size)
        throw Failure("normal array must have a following array");
    const auto& pf = out.formats[Pos];
    if (pf.components != 1 || (pf.type != 3 && pf.type != 4))
        throw Failure("unsupported position encoding");
    const std::size_t pos_stride = pf.type == 3 ? 6 : 12;
    if (pos_end <= pos_relative || (pos_end - pos_relative) / pos_stride > 65536)
        throw Failure("invalid position array");
    const std::size_t position_count = (pos_end - pos_relative) / pos_stride;
    out.positions.reserve(position_count);
    for (std::size_t i = 0; i < position_count; ++i) {
        const std::size_t at = out.vtx.offset + pos_relative + i * pos_stride;
        Vec3 value{};
        if (pf.type == 3) {
            const float scale = std::ldexp(1.0f, -static_cast<int>(pf.fraction));
            const auto signed16 = [&](std::size_t p) { return static_cast<std::int16_t>(be16(data, p)); };
            value = {signed16(at) * scale, signed16(at + 2) * scale, signed16(at + 4) * scale};
        } else {
            value = {be_float(data, at), be_float(data, at + 4), be_float(data, at + 8)};
        }
        if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
            throw Failure("non-finite position");
        out.positions.push_back(value);
    }
    const auto& nf = out.formats[Nrm];
    if (nf.components != 0 || (nf.type != 3 && nf.type != 4))
        throw Failure("unsupported normal encoding");
    out.normal_stride = nf.type == 3 ? 6 : 12;
    if (normal_end_relative <= normal_relative ||
        (normal_end_relative - normal_relative) < out.normal_stride)
        throw Failure("invalid normal array");
    const std::size_t normal_count = (normal_end_relative - normal_relative) / out.normal_stride;
    if (!normal_count || normal_count > 65536)
        throw Failure("invalid normal count");
    out.normal_at = normal_relative;
    out.normal_end = normal_end_relative;
    out.normals.assign(data.begin() + out.vtx.offset + normal_relative,
                       data.begin() + out.vtx.offset + normal_relative + normal_count * out.normal_stride);
    out.evidence.original_normals = static_cast<std::uint32_t>(normal_count);

    const std::size_t shape_count = be16(data, out.shp.offset + 8);
    if (!shape_count || shape_count > MaxShapes)
        throw Failure("invalid shape count");
    out.evidence.shapes = static_cast<std::uint32_t>(shape_count);
    const std::size_t init = checked_add(out.shp.offset, be32(data, out.shp.offset + 12),
                                         "shape init offset overflow");
    const std::size_t indices = checked_add(out.shp.offset, be32(data, out.shp.offset + 16),
                                            "shape index offset overflow");
    const std::size_t descriptors = checked_add(out.shp.offset, be32(data, out.shp.offset + 24),
                                                "descriptor offset overflow");
    const std::size_t display_lists = checked_add(out.shp.offset, be32(data, out.shp.offset + 32),
                                                  "display-list offset overflow");
    const std::size_t display_end = checked_add(out.shp.offset, be32(data, out.shp.offset + 36),
                                                "display-list end overflow");
    const std::size_t draw_table = checked_add(out.shp.offset, be32(data, out.shp.offset + 40),
                                               "draw-table offset overflow");
    if (!(display_lists < display_end && display_end <= draw_table &&
          draw_table < out.shp.offset + out.shp.size))
        throw Failure("unsupported SHP1 section order");

    std::unordered_set<std::size_t> used_draw_entries;
    std::uint64_t original_hash = 14695981039346656037ULL;
    std::uint64_t rebuilt_hash = 14695981039346656037ULL;
    std::size_t triangle_count = 0, group_count_total = 0;
    for (std::size_t shape = 0; shape < shape_count; ++shape) {
        const std::size_t index_at = checked_add(indices, shape * 2, "shape index overflow");
        const std::size_t shape_init = checked_add(init, std::size_t(be16(data, index_at)) * 40,
                                                   "shape init overflow");
        if (!fits(shape_init, 40, out.shp.offset + out.shp.size))
            throw Failure("shape init outside SHP1");
        const std::size_t groups = be16(data, shape_init + 2);
        if (!groups || group_count_total + groups > MaxGroups)
            throw Failure("invalid matrix group count");
        group_count_total += groups;
        const std::size_t descriptor = checked_add(descriptors, be16(data, shape_init + 4),
                                                   "descriptor address overflow");
        const Layout layout = parse_layout(data, out.shp, descriptor, out.formats);
        const std::size_t first_draw = be16(data, shape_init + 8);
        for (std::size_t group = 0; group < groups; ++group) {
            const std::size_t draw_index = first_draw + group;
            if (!used_draw_entries.insert(draw_index).second)
                throw Failure("shared draw-table entry");
            const std::size_t draw = checked_add(draw_table, checked_mul(draw_index, 8, "draw index overflow"),
                                                 "draw address overflow");
            if (!fits(draw, 8, out.shp.offset + out.shp.size))
                throw Failure("draw-table entry outside SHP1");
            const std::size_t at_begin = checked_add(display_lists, be32(data, draw + 4),
                                                     "display-list address overflow");
            const std::size_t at_end = checked_add(at_begin, be32(data, draw),
                                                   "display-list size overflow");
            if (at_begin < display_lists || at_end > display_end)
                throw Failure("display list outside SHP1 display section");
            GroupRewrite rewrite{shape, group, draw_index};
            std::size_t at = at_begin;
            while (at < at_end) {
                const std::uint8_t command = u8(data, at);
                if (!command) {
                    ++at;
                    continue;
                }
                const unsigned primitive = command & 0xf8;
                if (primitive != 0x80 && primitive != 0x90 && primitive != 0x98 && primitive != 0xa0)
                    throw Failure("unsupported GX command");
                if (!fits(at, 3, at_end))
                    throw Failure("draw header overrun");
                const std::size_t count = be16(data, at + 1);
                if ((primitive == 0x90 && count % 3) || (primitive == 0x80 && count % 4) ||
                    ((primitive == 0x98 || primitive == 0xa0) && count < 3))
                    throw Failure("invalid primitive vertex count");
                ++out.evidence.source_draws;
                at += 3;
                const std::size_t records_size = checked_mul(count, layout.stride, "draw size overflow");
                if (!fits(at, records_size, at_end))
                    throw Failure("draw vertex overrun");
                std::vector<std::vector<std::byte>> vertices;
                vertices.reserve(count);
                for (std::size_t i = 0; i < count; ++i)
                    vertices.emplace_back(data.begin() + at + i * layout.stride,
                                          data.begin() + at + (i + 1) * layout.stride);
                at += records_size;
                const auto emit = [&](std::size_t a, std::size_t b, std::size_t c) {
                    if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size())
                        throw Failure("triangle vertex outside draw");
                    std::array<std::vector<std::byte>, 3> triangle{vertices[a], vertices[b], vertices[c]};
                    if (!nondegenerate(triangle, layout, out.positions)) {
                        ++out.evidence.degenerate_triangles;
                        return;
                    }
                    if (++triangle_count > MaxTriangles)
                        throw Failure("triangle budget exceeded");
                    if (!make_rewrite)
                        rewrite.draws++;
                    else {
                        rewrite.bytes.push_back(std::byte{0x98});
                        rewrite.bytes.push_back(std::byte{0});
                        rewrite.bytes.push_back(std::byte{3});
                        ++rewrite.draws;
                    }
                    add16(original_hash, static_cast<std::uint16_t>(shape));
                    add16(original_hash, static_cast<std::uint16_t>(group));
                    add16(rebuilt_hash, static_cast<std::uint16_t>(shape));
                    add16(rebuilt_hash, static_cast<std::uint16_t>(group));
                    for (auto& record : triangle) {
                        Bytes view{record.data(), record.size()};
                        const Slot ps = layout.slots[Pos], ns = layout.slots[Nrm];
                        const std::uint16_t position =
                            ps.kind == 2 ? u8(view, ps.offset) : be16(view, ps.offset);
                        const std::uint16_t old_normal = be16(view, ns.offset);
                        if (old_normal >= normal_count)
                            throw Failure("normal index out of bounds");
                        add16(original_hash, position);
                        add16(original_hash, old_normal);
                        if (make_rewrite) {
                            const std::size_t new_normal = out.normals.size() / out.normal_stride;
                            if (new_normal >= 65536)
                                throw Failure("normal index exceeds GX_INDEX16");
                            out.normals.insert(out.normals.end(),
                                               data.begin() + out.vtx.offset + normal_relative +
                                                   old_normal * out.normal_stride,
                                               data.begin() + out.vtx.offset + normal_relative +
                                                   (old_normal + 1) * out.normal_stride);
                            put16(record, ns.offset, static_cast<std::uint16_t>(new_normal));
                            append(rewrite.bytes, {record.data(), record.size()});
                            add16(rebuilt_hash, position);
                            add16(rebuilt_hash, static_cast<std::uint16_t>(new_normal));
                        } else {
                            add16(rebuilt_hash, position);
                            add16(rebuilt_hash, old_normal);
                        }
                    }
                };
                if (primitive == 0x90)
                    for (std::size_t i = 0; i < count; i += 3)
                        emit(i, i + 1, i + 2);
                else if (primitive == 0x98)
                    for (std::size_t i = 2; i < count; ++i)
                        i & 1 ? emit(i - 1, i - 2, i) : emit(i - 2, i - 1, i);
                else if (primitive == 0xa0)
                    for (std::size_t i = 2; i < count; ++i)
                        emit(0, i - 1, i);
                else
                    for (std::size_t i = 0; i < count; i += 4) {
                        emit(i, i + 1, i + 2);
                        emit(i + 2, i + 3, i);
                    }
            }
            out.evidence.rebuilt_draws += rewrite.draws;
            out.groups.push_back(std::move(rewrite));
        }
    }
    out.evidence.matrix_groups = static_cast<std::uint32_t>(group_count_total);
    out.evidence.triangles = static_cast<std::uint32_t>(triangle_count);
    out.evidence.original_corner_hash = original_hash;
    out.evidence.rebuilt_corner_hash = rebuilt_hash;
    out.evidence.written_normals = static_cast<std::uint32_t>(out.normals.size() / out.normal_stride);
    out.evidence.rebuilt_normal_capacity = out.evidence.written_normals;
    return out;
}

std::vector<std::byte> rebuild(Bytes input, const Parsed& parsed) {
    const Bytes old_vtx = input.subspan(parsed.vtx.offset, parsed.vtx.size);
    const std::size_t new_normal_end = aligned(parsed.normal_at + parsed.normals.size());
    if (new_normal_end < parsed.normal_end)
        throw Failure("expanded normal array shrank");
    const std::size_t vtx_shift = new_normal_end - parsed.normal_end;
    std::vector<std::byte> new_vtx;
    new_vtx.reserve(parsed.vtx.size + vtx_shift);
    append(new_vtx, old_vtx.first(parsed.normal_at));
    append(new_vtx, {parsed.normals.data(), parsed.normals.size()});
    new_vtx.resize(new_normal_end, std::byte{0});
    append(new_vtx, old_vtx.subspan(parsed.normal_end));
    for (std::size_t field = 20; field < 64; field += 4) {
        const std::uint32_t value = be32({new_vtx.data(), new_vtx.size()}, field);
        if (value >= parsed.normal_end)
            put32(new_vtx, field, value + static_cast<std::uint32_t>(vtx_shift));
    }
    put32(new_vtx, 4, static_cast<std::uint32_t>(new_vtx.size()));

    const Bytes old_shp = input.subspan(parsed.shp.offset, parsed.shp.size);
    const std::size_t dl_at = be32(old_shp, 32), dl_end = be32(old_shp, 36);
    const std::size_t old_draw_table = be32(old_shp, 40);
    std::vector<std::byte> display_lists;
    struct DrawValue { std::size_t index, size, offset; };
    std::vector<DrawValue> draw_values;
    draw_values.reserve(parsed.groups.size());
    for (const auto& group : parsed.groups) {
        const std::size_t start = display_lists.size();
        append(display_lists, {group.bytes.data(), group.bytes.size()});
        display_lists.resize(aligned(display_lists.size()), std::byte{0});
        draw_values.push_back({group.draw_index, display_lists.size() - start, start});
    }
    if (display_lists.size() < dl_end - dl_at)
        throw Failure("rewritten display lists unexpectedly shrank");
    const std::size_t shp_shift = display_lists.size() - (dl_end - dl_at);
    std::vector<std::byte> new_shp;
    new_shp.reserve(parsed.shp.size + shp_shift);
    append(new_shp, old_shp.first(dl_at));
    append(new_shp, {display_lists.data(), display_lists.size()});
    append(new_shp, old_shp.subspan(dl_end));
    put32(new_shp, 36, static_cast<std::uint32_t>(dl_end + shp_shift));
    put32(new_shp, 40, static_cast<std::uint32_t>(old_draw_table + shp_shift));
    for (const auto& value : draw_values) {
        const std::size_t entry = old_draw_table + shp_shift + value.index * 8;
        put32(new_shp, entry, static_cast<std::uint32_t>(value.size));
        put32(new_shp, entry + 4, static_cast<std::uint32_t>(value.offset));
    }
    put32(new_shp, 4, static_cast<std::uint32_t>(new_shp.size()));

    std::vector<std::byte> output;
    output.reserve(input.size() + vtx_shift + shp_shift);
    append(output, input.first(HeaderSize));
    for (const auto& block : parsed.blocks) {
        if (block.tag == tag("VTX1"))
            append(output, {new_vtx.data(), new_vtx.size()});
        else if (block.tag == tag("SHP1"))
            append(output, {new_shp.data(), new_shp.size()});
        else
            append(output, input.subspan(block.offset, block.size));
    }
    put32(output, 8, static_cast<std::uint32_t>(output.size()));
    return output;
}
} // namespace

Result split_normals(std::span<const std::byte> input) {
    Result result;
    try {
        Parsed source = parse(input, true);
        result.bytes = rebuild(input, source);
        Parsed check = parse({result.bytes.data(), result.bytes.size()}, false);
        if (check.evidence.shapes != source.evidence.shapes ||
            check.evidence.matrix_groups != source.evidence.matrix_groups ||
            check.evidence.triangles != source.evidence.triangles ||
            check.evidence.original_normals < source.evidence.written_normals ||
            check.evidence.original_corner_hash != source.evidence.rebuilt_corner_hash)
            throw Failure("rebuilt topology validation failed");
        source.evidence.rebuilt_normal_capacity = check.evidence.original_normals;
        result.evidence = source.evidence;
    } catch (const std::exception& error) {
        result.bytes.clear();
        result.error = error.what();
    }
    return result;
}

} // namespace midnafx::bmd_rebuild
