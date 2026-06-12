#include <array>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <slim/common/http/header.h>

namespace {

    enum struct HEADER_TYPE : uint8_t {
        CONTENT_TYPE,
        USER_AGENT,
        END
    };
    constexpr std::array<std::string_view, static_cast<std::size_t>(HEADER_TYPE::END)> delimiter_strings = {
        "; ",   // CONTENT_TYPE
        " ",    // USER_AGENT
    };
    struct AsciiTables {
        std::array<char, 256> to_lower{};
        std::array<bool, 256> is_alnum{};
        std::array<bool, 256> is_space{};
        std::array<bool, 256> is_date_delimiter{};

        // HTTP Header specific tables
        std::array<bool, 256> is_token_char{};
        std::array<bool, 256> is_value_char{};

        constexpr AsciiTables() noexcept {
            for (size_t i = 0; i < 256; ++i) {
                is_alnum[i] = (i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z') || (i >= '0' && i <= '9');
                is_space[i] = (i == ' ' || i == '\t' || i == '\r' || i == '\n' || i == '\v' || i == '\f');

                unsigned char uc = static_cast<unsigned char>(i);

                // RFC 6265 §5.1.1: delimiter = %x09 / %x20-2F / %x3B-40 / %x5B-60 / %x7B-7E
                is_date_delimiter[i] = (i == 0x09)
                                || (i >= 0x20 && i <= 0x2F)
                                || (i >= 0x3B && i <= 0x40)
                                || (i >= 0x5B && i <= 0x60)
                                || (i >= 0x7B && i <= 0x7E);

                // RFC 9110 tchar
                is_token_char[i] = is_alnum[i] ||
                              uc == '!' || uc == '#' || uc == '$' || uc == '%' || uc == '&' ||
                              uc == '\''|| uc == '*' || uc == '+' || uc == '-' || uc == '.' ||
                              uc == '^' || uc == '_' || uc == '`' || uc == '|' || uc == '~';

                // RFC 9110 field-value: printable ASCII (0x20-0x7E) + HTAB (0x09)
                is_value_char[i] = (uc == 0x09) || (uc >= 0x20 && uc <= 0x7E);
            }
        }
    };

    constexpr AsciiTables ascii{};

    constexpr bool iequals(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (ascii.to_lower[static_cast<unsigned char>(a[i])] != static_cast<unsigned char>(b[i])) return false;

        return true;
    }

    constexpr void trim(std::string_view& s) noexcept {
        while (!s.empty() && ascii.is_space[static_cast<unsigned char>(s.front())]) s.remove_prefix(1);
        while (!s.empty() && ascii.is_space[static_cast<unsigned char>(s.back())]) s.remove_suffix(1);
    }

    HEADER::STATUS validate_name(std::string_view& s) noexcept {
        trim(s);
        if (s.empty()) return HEADER::STATUS::NAME_EMPTY;

        for (char c : s)
            if (!ascii.is_token_char[static_cast<unsigned char>(c)]) return HEADER::STATUS::NAME_INVALID_CHAR;

        return HEADER::STATUS::OK;
    }

    HEADER::STATUS validate_value(std::string_view& s) noexcept {
        trim(s);
        if (s.empty()) return HEADER::STATUS::VALUE_EMPTY;

        for (char c : s)
            if (!ascii.is_value_char[static_cast<unsigned char>(c)]) return HEADER::STATUS::VALUE_INVALID_CHAR;

        return HEADER::STATUS::OK;
    }

    std::string_view get_delimiter(std::string s) noexcept {
        if(iequals(s, "content-type")) return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::CONTENT_TYPE)];
        if(iequals(s, "user-agent")) return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::USER_AGENT)];
        return {};
    }
} // namespace

slim::common::http::Header::Header(std::string_view n, std::string_view v) {
    auto e = set_name(n);
    if(e != HEADER::STATUS::OK) throw(HeaderException(e));
    e = set_value(v);
    if(e != HEADER::STATUS::OK) throw(HeaderException(e));
}

HEADER::STATUS slim::common::http::Header::set_name(std::string_view s) noexcept {
    auto e = validate_name(s);
    if(e == HEADER::STATUS::OK) name = std::string(s);
    return e;
}

HEADER::STATUS slim::common::http::Header::replace_value(std::string_view s) noexcept {
    values.clear();
    return set_value(s);
}

HEADER::STATUS slim::common::http::Header::set_value(std::string_view s) noexcept {
    auto e = validate_value(s);
    if(e == HEADER::STATUS::OK) values.push_back(std::string(s));
    return e;
}

std::string slim::common::http::Header::serialize() const {
    if(name.empty()) throw(HeaderException(HEADER::STATUS::NAME_EMPTY));
    if(values.size() == 0) throw(HeaderException(HEADER::STATUS::VALUE_EMPTY));

    std::size_t total_size = name.size() + 2;                          // "HeaderName: "
    std::size_t values_count = 0;
    for(const auto& v : values) {
        values_count++;
        total_size += v.size();                                        // value length
    }

    std::string d;
    if(values_count > 1) {
        d = get_delimiter(name);
        for(std::size_t i = values_count; i > 0; --i) total_size += d.size(); // delimiter size
    }

    total_size += 2;                                                   // "\r\n"

    std::string result;
    result.reserve(total_size);
    result.append(name);
    result.append(": ");

    std::size_t values_appended = 0;
    for(const auto& v : values) {
        result.append(v);
        values_appended++;
        if(values_appended < values_count) result.append(d);
    }

    result.append("\r\n");
    return result;
}
