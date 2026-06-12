#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <slim/common/http/header.h>

namespace {

    static constexpr std::array<std::string_view, 3> allowed_delimiters = {
        ";",
        ",",
        " ",
    };

    enum struct HEADER_TYPE : uint8_t {
        ACCEPT,
        ACCEPT_CHARSET,
        ACCEPT_ENCODING,
        ACCEPT_LANGUAGE,
        ALLOW,
        AUTHORIZATION,
        CACHE_CONTROL,
        CONNECTION,
        CONTENT_DISPOSITION,
        CONTENT_ENCODING,
        CONTENT_TYPE,
        FORWARDED,
        IF_MATCH,
        IF_NONE_MATCH,
        LINK,
        TRANSFER_ENCODING,
        USER_AGENT,
        VARY,
        VIA,
        END
    };

    static constexpr std::array<std::string_view, static_cast<std::size_t>(HEADER_TYPE::END)> delimiter_strings = {
        ", ",  // ACCEPT
        ", ",  // ACCEPT_CHARSET
        ", ",  // ACCEPT_ENCODING
        ", ",  // ACCEPT_LANGUAGE
        ", ",  // ALLOW
        " ",   // AUTHORIZATION
        ", ",  // CACHE_CONTROL
        ", ",  // CONNECTION
        "; ",  // CONTENT_DISPOSITION
        ", ",  // CONTENT_ENCODING
        "; ",  // CONTENT_TYPE
        ", ",  // FORWARDED
        ", ",  // IF_MATCH
        ", ",  // IF_NONE_MATCH
        ", ",  // LINK
        ", ",  // TRANSFER_ENCODING
        " ",   // USER_AGENT
        ", ",  // VARY
        ", ",  // VIA
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

    HEADER::STATUS validate_delimiter(std::string_view s) noexcept {
        trim(s);
        if(!s.empty()) {
            for(const auto& d : allowed_delimiters)
                if(s == d) return HEADER::STATUS::OK;
        }
        else {
            return HEADER::STATUS::OK;
        }
        return HEADER::STATUS::DELIMITER_INVALID;
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
        // hot path: highest frequency headers first
        if (iequals(s, "content-type"))      return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::CONTENT_TYPE)];
        if (iequals(s, "accept"))            return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::ACCEPT)];
        if (iequals(s, "cache-control"))     return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::CACHE_CONTROL)];
        if (iequals(s, "connection"))        return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::CONNECTION)];
        if (iequals(s, "transfer-encoding")) return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::TRANSFER_ENCODING)];
        if (iequals(s, "accept-encoding"))   return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::ACCEPT_ENCODING)];
        if (iequals(s, "accept-language"))   return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::ACCEPT_LANGUAGE)];
        if (iequals(s, "user-agent"))        return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::USER_AGENT)];
        if (iequals(s, "authorization"))     return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::AUTHORIZATION)];
        // warm path
        if (iequals(s, "vary"))              return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::VARY)];
        if (iequals(s, "allow"))             return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::ALLOW)];
        if (iequals(s, "accept-charset"))    return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::ACCEPT_CHARSET)];
        if (iequals(s, "content-encoding"))  return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::CONTENT_ENCODING)];
        if (iequals(s, "content-disposition")) return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::CONTENT_DISPOSITION)];
        if (iequals(s, "link"))              return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::LINK)];
        // cold path
        if (iequals(s, "forwarded"))         return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::FORWARDED)];
        if (iequals(s, "if-match"))          return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::IF_MATCH)];
        if (iequals(s, "if-none-match"))     return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::IF_NONE_MATCH)];
        if (iequals(s, "via"))               return delimiter_strings[static_cast<std::size_t>(HEADER_TYPE::VIA)];
        return {};
    }
} // namespace

slim::common::http::Header::Header(std::string_view n, std::string_view v, std::string d) {
    auto e = set_name(n);
    if(e != HEADER::STATUS::OK) throw(HeaderException(e));

    e = set_value(v);
    if(e != HEADER::STATUS::OK) throw(HeaderException(e));

    if(!d.empty()) {
        e = validate_delimiter(d);
        if(e != HEADER::STATUS::OK) throw(HeaderException(e));
    }
    else {
        delimiter = std::string(get_delimiter(name));
    }
}

HEADER::STATUS slim::common::http::Header::set_delimiter(std::string s) noexcept {
    auto e = validate_delimiter(s);
    if(e == HEADER::STATUS::OK) delimiter = s;
    return e;
}

HEADER::STATUS slim::common::http::Header::set_name(std::string_view s) noexcept {
    auto e = validate_name(s);
    if(e == HEADER::STATUS::OK) name = std::string(s);
    delimiter = get_delimiter(name);
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

    std::size_t total_size = name.size() + 2;                            // "HeaderName: "
    if(values.size() > 1) total_size += values.size() * delimiter.size();// delimiter
    for(const auto& v : values) total_size += v.size();                  // value length
    total_size += 2;                                                     // "\r\n"

    std::string result;
    result.reserve(total_size);
    result.append(name);
    result.append(": ");

    std::size_t values_appended = 0;
    for(const auto& v : values) {
        result.append(v);
        values_appended++;
        if(values_appended < values.size()) result.append(delimiter);
    }

    result.append("\r\n");
    return result;
}
