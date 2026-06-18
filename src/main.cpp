#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <slim/common/http/header.h>

namespace {

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
                to_lower[i] = (i >= 'A' && i <= 'Z') ? static_cast<char>(i + 32) : static_cast<char>(i);
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

    using slim::common::http::HeaderStatus;
    using slim::common::http::HeaderType;

    constexpr bool iequals(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (ascii.to_lower[static_cast<unsigned char>(a[i])] != static_cast<unsigned char>(b[i])) return false;

        return true;
    }

    constexpr bool iiequals(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (ascii.to_lower[static_cast<unsigned char>(a[i])] != ascii.to_lower[static_cast<unsigned char>(b[i])])
                return false;

        return true;
    }

    constexpr void trim(std::string_view& s) noexcept {
        while (!s.empty() && ascii.is_space[static_cast<unsigned char>(s.front())]) s.remove_prefix(1);
        while (!s.empty() && ascii.is_space[static_cast<unsigned char>(s.back())]) s.remove_suffix(1);
    }

    HeaderStatus validate_delimiter(std::string_view s) noexcept {
        if (s.empty()) return HeaderStatus::OK;
        for (const auto& c : s)
            if (c != ';' && c != ',' && c != ' ') return HeaderStatus::DelimiterInvalid;
        return HeaderStatus::OK;
    }

    HeaderStatus validate_name(std::string_view& s) noexcept {
        trim(s);
        if (s.empty()) return HeaderStatus::NameEmpty;

        for (char c : s)
            if (!ascii.is_token_char[static_cast<unsigned char>(c)]) return HeaderStatus::NameInvalidChar;

        return HeaderStatus::OK;
    }

    HeaderStatus validate_value(std::string_view& s) noexcept {
        trim(s);
        if (s.empty()) return HeaderStatus::ValueEmpty;

        for (char c : s)
            if (!ascii.is_value_char[static_cast<unsigned char>(c)]) return HeaderStatus::ValueInvalidChar;

        return HeaderStatus::OK;
    }

    std::string_view get_delimiter(std::string_view s) noexcept {
        using slim::common::http::header::type::delimiter;
        // hot path: highest frequency headers first
        if (iequals(s, "content-type"))        return delimiter(HeaderType::ContentType);
        if (iequals(s, "accept"))              return delimiter(HeaderType::Accept);
        if (iequals(s, "cache-control"))       return delimiter(HeaderType::CacheControl);
        if (iequals(s, "connection"))          return delimiter(HeaderType::Connection);
        if (iequals(s, "transfer-encoding"))   return delimiter(HeaderType::TransferEncoding);
        if (iequals(s, "accept-encoding"))     return delimiter(HeaderType::AcceptEncoding);
        if (iequals(s, "accept-language"))     return delimiter(HeaderType::AcceptLanguage);
        if (iequals(s, "user-agent"))          return delimiter(HeaderType::UserAgent);
        if (iequals(s, "authorization"))       return delimiter(HeaderType::Authorization);
        // warm path
        if (iequals(s, "vary"))                return delimiter(HeaderType::Vary);
        if (iequals(s, "allow"))               return delimiter(HeaderType::Allow);
        if (iequals(s, "accept-charset"))      return delimiter(HeaderType::AcceptCharset);
        if (iequals(s, "content-encoding"))    return delimiter(HeaderType::ContentEncoding);
        if (iequals(s, "content-disposition")) return delimiter(HeaderType::ContentDisposition);
        if (iequals(s, "link"))                return delimiter(HeaderType::Link);
        // cold path
        if (iequals(s, "forwarded"))           return delimiter(HeaderType::Forwarded);
        if (iequals(s, "if-match"))            return delimiter(HeaderType::IfMatch);
        if (iequals(s, "if-none-match"))       return delimiter(HeaderType::IfNoneMatch);
        if (iequals(s, "via"))                 return delimiter(HeaderType::Via);
        return {};
    }
} // namespace

using slim::common::http::HeaderException;
using slim::common::http::HeaderStatus;

slim::common::http::Header::Header(std::string_view n, std::string_view v, std::string d) {
    auto e = set_name(n);
    if (e != HeaderStatus::OK) throw HeaderException(e);

    e = set_value(v);
    if (e != HeaderStatus::OK) throw HeaderException(e);

    if (!d.empty()) {
        e = validate_delimiter(d);
        if (e != HeaderStatus::OK) throw HeaderException(e);
        delimiter = d;
    } else {
        delimiter = std::string(get_delimiter(name));
    }
}

bool slim::common::http::Header::operator==(const Header& other) const noexcept {
    if (values.size() != other.values.size()) return false;
    if (delimiter != other.delimiter) return false;
    if (!iiequals(name, other.name)) return false;
    for (size_t i = 0; i < values.size(); ++i)
        if (values[i] != other.values[i]) return false;

    return true;
}

HeaderStatus slim::common::http::Header::set_delimiter(std::string s) noexcept {
    auto e = validate_delimiter(s);
    if (e == HeaderStatus::OK) delimiter = s;
    return e;
}

HeaderStatus slim::common::http::Header::set_name(std::string_view s) noexcept {
    auto e = validate_name(s);
    if (e == HeaderStatus::OK) name = std::string(s);
    delimiter = get_delimiter(name);
    return e;
}

HeaderStatus slim::common::http::Header::replace_value(std::string_view s) noexcept {
    values.clear();
    return set_value(s);
}

HeaderStatus slim::common::http::Header::set_value(std::string_view s) noexcept {
    auto e = validate_value(s);
    if (e == HeaderStatus::OK) values.push_back(std::string(s));
    return e;
}

std::string slim::common::http::Header::serialize() const {
    if (name.empty()) throw HeaderException(HeaderStatus::NameEmpty);
    if (values.size() == 0) throw HeaderException(HeaderStatus::ValueEmpty);

    std::size_t total_size = name.size() + 2;                             // "HeaderName: "
    if (values.size() > 1) total_size += values.size() * delimiter.size(); // delimiter
    for (const auto& v : values) total_size += v.size();                  // value length
    total_size += 2;                                                      // "\r\n"

    std::string result;
    result.reserve(total_size);
    result.append(name);
    result.append(": ");

    std::size_t values_appended = 0;
    for (const auto& v : values) {
        result.append(v);
        values_appended++;
        if (values_appended < values.size()) result.append(delimiter);
    }

    result.append("\r\n");
    return result;
}
