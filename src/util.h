#ifndef VNES_UTIL_H
#define VNES_UTIL_H

#include <string>
#include <charconv>
#include <algorithm>
#include <type_traits>
#include <optional>
#include <string_view>

namespace vnes::util {

// Convert an unsigned integer to an uppercase hexadecimal string with zero padding.
template<typename T>
inline std::string toHex(T val, int width) {
    static_assert(std::is_integral_v<T>, "toHex requires an integral type");
    char buf[32] = {0};
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), static_cast<unsigned long long>(static_cast<std::make_unsigned_t<T>>(val)), 16);
    size_t len = ptr - buf;
    std::string s;
    if (len == 0) s = std::string(width, '0');
    else if ((int)len < width) s = std::string(width - (int)len, '0') + std::string(buf, len);
    else s = std::string(buf, len);
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

inline std::string hexByte(uint8_t v) {
    return toHex<uint8_t>(v, 2);
}

inline std::string hexWord(uint16_t v) {
    return toHex<uint16_t>(v, 4);
}

// Parse integer from string_view. Supports decimal, $FFFF (hex), and 0xFFFF prefixes.
inline std::optional<uint64_t> parseInteger(std::string_view sv) {
    if (sv.empty()) return std::nullopt;

    std::string_view s = sv;
    int base = 10;
    if (s.size() > 0 && s[0] == '$') {
        s.remove_prefix(1);
        base = 16;
    } else if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s.remove_prefix(2);
        base = 16;
    }

    uint64_t val = 0;
    auto res = std::from_chars(s.data(), s.data() + s.size(), val, base);
    if (res.ec != std::errc() || res.ptr != s.data() + s.size()) return std::nullopt;
    return val;
}

// Format CPU flags as string (e.g., "NvUBdIZc")
inline std::string formatFlags(uint8_t status) {
    std::string flags;
    flags += (status & 0x80) ? 'N' : 'n';
    flags += (status & 0x40) ? 'V' : 'v';
    flags += (status & 0x20) ? 'U' : 'u';
    flags += (status & 0x10) ? 'B' : 'b';
    flags += (status & 0x08) ? 'D' : 'd';
    flags += (status & 0x04) ? 'I' : 'i';
    flags += (status & 0x02) ? 'Z' : 'z';
    flags += (status & 0x01) ? 'C' : 'c';
    return flags;
}

} // namespace vnes::util

#endif // VNES_UTIL_H
