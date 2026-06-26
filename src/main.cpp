#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <slim/common/http/header.h>
#include <slim/common/utilities.h>

namespace {

    struct AsciiTables {
        // HTTP Header specific tables (RFC 9110) — not provided by SlimCommonUtilities
        std::array<bool, 256> is_token_char{};
        std::array<bool, 256> is_value_char{};

        constexpr AsciiTables() noexcept {
            for (size_t i = 0; i < 256; ++i) {
                unsigned char uc = static_cast<unsigned char>(i);
                bool alnum = (uc >= 'a' && uc <= 'z') || (uc >= 'A' && uc <= 'Z') || (uc >= '0' && uc <= '9');

                // RFC 9110 tchar
                is_token_char[i] = alnum ||
                              uc == '!' || uc == '#' || uc == '$' || uc == '%' || uc == '&' ||
                              uc == '\''|| uc == '*' || uc == '+' || uc == '-' || uc == '.' ||
                              uc == '^' || uc == '_' || uc == '`' || uc == '|' || uc == '~';

                // RFC 9110 field-value: printable ASCII (0x20-0x7E) + HTAB (0x09)
                is_value_char[i] = (uc == 0x09) || (uc >= 0x20 && uc <= 0x7E);
            }
        }
    };

    constexpr AsciiTables ascii{};

    using slim::common::http::ErrorStatus;
    using slim::common::http::HeaderType;
    using slim::common::utilities::iequals;
    using slim::common::utilities::iiequals;
    using slim::common::utilities::trim;

    ErrorStatus validate_delimiter(std::string_view s) noexcept {
        if (s.empty()) return ErrorStatus::OK;
        for (const auto& c : s)
            if (c != ';' && c != ',' && c != ' ') return ErrorStatus::HeaderDelimiterInvalid;
        return ErrorStatus::OK;
    }

    ErrorStatus validate_name(std::string_view& s) noexcept {
        trim(s);
        if (s.empty()) return ErrorStatus::HeaderNameEmpty;

        for (char c : s)
            if (!ascii.is_token_char[static_cast<unsigned char>(c)]) return ErrorStatus::HeaderNameInvalidChar;

        return ErrorStatus::OK;
    }

    ErrorStatus validate_value(std::string_view& s) noexcept {
        trim(s);
        if (s.empty()) return ErrorStatus::HeaderValueEmpty;

        for (char c : s)
            if (!ascii.is_value_char[static_cast<unsigned char>(c)]) return ErrorStatus::HeaderValueInvalidChar;

        return ErrorStatus::OK;
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

using slim::common::http::ErrorStatus;
using slim::common::http::HttpHeaderException;

slim::common::http::Header::Header(std::string_view n, std::string_view v, std::string d) {
    auto e = set_name(n);
    if (e != ErrorStatus::OK) throw HttpHeaderException(e);

    e = set_value(v);
    if (e != ErrorStatus::OK) throw HttpHeaderException(e);

    if (!d.empty()) {
        e = validate_delimiter(d);
        if (e != ErrorStatus::OK) throw HttpHeaderException(e);
        delimiter = d;
    }
    else delimiter = std::string(get_delimiter(name));
}

bool slim::common::http::Header::operator==(const Header& other) const noexcept {
    if (values.size() != other.values.size()) return false;
    if (delimiter != other.delimiter) return false;
    if (!slim::common::utilities::iiequals(name, other.name)) return false;
    for (size_t i = 0; i < values.size(); ++i) if (values[i] != other.values[i]) return false;
    return true;
}

ErrorStatus slim::common::http::Header::set_delimiter(std::string s) noexcept {
    auto e = validate_delimiter(s);
    if (e == ErrorStatus::OK) delimiter = s;
    return e;
}

ErrorStatus slim::common::http::Header::set_name(std::string_view s) noexcept {
    auto e = validate_name(s);
    if (e == ErrorStatus::OK) name = std::string(s);
    delimiter = get_delimiter(name);
    return e;
}

ErrorStatus slim::common::http::Header::replace_value(std::string_view s) noexcept {
    values.clear();
    return set_value(s);
}

ErrorStatus slim::common::http::Header::set_value(std::string_view s) noexcept {
    std::string_view d = get_delimiter(name);
    if (d.empty()) d = delimiter;

    if (d.empty() || d.size() != 1) {
        const ErrorStatus status = validate_value(s);
        if (status == ErrorStatus::OK) values.emplace_back(s);
        return status;
    }

    const std::size_t original_size = values.size();
    slim::common::utilities::split(s, d.front(), values);

    if (values.size() == original_size) {
        const ErrorStatus status = validate_value(s);
        if (status == ErrorStatus::OK) values.emplace_back(s);
        return status;
    }

    for (std::size_t i = original_size; i < values.size(); ++i) {
        std::string_view view = values[i];
        const ErrorStatus status = validate_value(view);
        if (status != ErrorStatus::OK) {
            values.resize(original_size);
            return status;
        }
    }

    return ErrorStatus::OK;
}

std::string slim::common::http::Header::serialize() const {
    if (name.empty()) throw HttpHeaderException(ErrorStatus::HeaderNameEmpty);
    if (values.size() == 0) throw HttpHeaderException(ErrorStatus::HeaderValueEmpty);

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
