// An owned, deterministic target for REA/Ghidra and Frida qualification.
// It performs no network, file, UI, or game operations.
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" __declspec(dllexport) __declspec(noinline)
unsigned midnafx_rea_probe(unsigned input) {
    return ((input * 3u) ^ 0x5a17u) + 7u;
}

int main(int argc, char** argv) {
    if (argc == 1) {
        const auto result = midnafx_rea_probe(7);
        std::printf("input=7 result=%u expected=23049\n", result);
        return result == 23049 ? 0 : 2;
    }
    if (argc != 3 || std::strcmp(argv[1], "--serve-ms")) return 3;
    char* end = nullptr;
    const auto duration = std::strtoul(argv[2], &end, 10);
    if (!end || *end || !duration || duration > 15000) return 3;
    const auto start = GetTickCount64();
    unsigned calls = 0;
    while (GetTickCount64() - start < duration) {
        volatile unsigned observed = midnafx_rea_probe(calls & 255u);
        (void)observed;
        ++calls;
        Sleep(10);
    }
    std::printf("fixture complete calls=%u\n", calls);
    return 0;
}
