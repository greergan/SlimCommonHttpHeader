#include <catch2/catch_test_macros.hpp>
#include <slim/common/http/header.h>

// ─── Construction ─────────────────────────────────────────────────────────────

TEST_CASE("Header: valid construction sets name and value") {
    slim::common::http::Header h("Content-Type", "text/html");
    CHECK(h.serialize() == "Content-Type: text/html\r\n");
}

TEST_CASE("Header: constructor trims leading/trailing whitespace from name and value") {
    slim::common::http::Header h("  Content-Type  ", "  text/html  ");
    CHECK(h.serialize() == "Content-Type: text/html\r\n");
}

TEST_CASE("Header: constructor with explicit delimiter stores it") {
    slim::common::http::Header h("X-Custom", "a", ",");
    h.set_value("b");
    CHECK(h.serialize() == "X-Custom: a,b\r\n");
}

TEST_CASE("Header: constructor throws on empty name") {
    CHECK_THROWS_AS(slim::common::http::Header("", "value"), slim::common::http::HeaderException);
}

TEST_CASE("Header: constructor throws on empty value") {
    CHECK_THROWS_AS(slim::common::http::Header("X-Foo", ""), slim::common::http::HeaderException);
}

TEST_CASE("Header: constructor throws on invalid name character") {
    CHECK_THROWS_AS(slim::common::http::Header("Bad Name", "value"), slim::common::http::HeaderException);
}

TEST_CASE("Header: constructor throws on invalid value character") {
    // DEL (0x7F) is not a valid field-value char
    CHECK_THROWS_AS(slim::common::http::Header("X-Foo", std::string(1, '\x7F')), slim::common::http::HeaderException);
}

TEST_CASE("Header: constructor throws on invalid delimiter") {
    CHECK_THROWS_AS(slim::common::http::Header("X-Foo", "value", "??"), slim::common::http::HeaderException);
}

// ─── set_name ─────────────────────────────────────────────────────────────────

TEST_CASE("set_name: accepts valid token characters") {
    slim::common::http::Header h("X-Foo", "bar");
    CHECK(h.set_name("X-Bar") == HEADER::STATUS::OK);
    CHECK(h.serialize() == "X-Bar: bar\r\n");
}

TEST_CASE("set_name: rejects empty name") {
    slim::common::http::Header h("X-Foo", "bar");
    CHECK(h.set_name("") == HEADER::STATUS::NAME_EMPTY);
}

TEST_CASE("set_name: rejects name with space") {
    slim::common::http::Header h("X-Foo", "bar");
    CHECK(h.set_name("Bad Name") == HEADER::STATUS::NAME_INVALID_CHAR);
}

TEST_CASE("set_name: rejects name with colon") {
    slim::common::http::Header h("X-Foo", "bar");
    CHECK(h.set_name("Bad:Name") == HEADER::STATUS::NAME_INVALID_CHAR);
}

TEST_CASE("set_name: updates delimiter for known header") {
    slim::common::http::Header h("X-Custom", "a");
    h.set_value("b");
    h.set_name("Content-Type");
    h.set_value("charset=utf-8");
    // Content-Type uses "; " delimiter
    CHECK(h.serialize() == "Content-Type: a; b; charset=utf-8\r\n");
}

// ─── set_value / replace_value ────────────────────────────────────────────────

TEST_CASE("set_value: appends multiple values") {
    slim::common::http::Header h("Accept", "text/html");
    h.set_value("application/json");
    CHECK(h.serialize() == "Accept: text/html, application/json\r\n");
}

TEST_CASE("set_value: rejects empty value") {
    slim::common::http::Header h("X-Foo", "bar");
    CHECK(h.set_value("") == HEADER::STATUS::VALUE_EMPTY);
}

TEST_CASE("set_value: rejects value with control character") {
    slim::common::http::Header h("X-Foo", "bar");
    CHECK(h.set_value(std::string(1, '\x01')) == HEADER::STATUS::VALUE_INVALID_CHAR);
}

TEST_CASE("set_value: accepts HTAB (0x09) in value") {
    slim::common::http::Header h("X-Foo", "bar");
    CHECK(h.set_value(std::string("a\tb")) == HEADER::STATUS::OK);
}

TEST_CASE("replace_value: clears existing values and sets new one") {
    slim::common::http::Header h("Accept", "text/html");
    h.set_value("application/json");
    h.replace_value("text/plain");
    CHECK(h.serialize() == "Accept: text/plain\r\n");
}

// ─── set_delimiter ────────────────────────────────────────────────────────────

TEST_CASE("set_delimiter: accepts semicolon") {
    slim::common::http::Header h("X-Foo", "a");
    h.set_value("b");
    CHECK(h.set_delimiter(";") == HEADER::STATUS::OK);
    CHECK(h.serialize() == "X-Foo: a;b\r\n");
}

TEST_CASE("set_delimiter: accepts comma") {
    slim::common::http::Header h("X-Foo", "a");
    h.set_value("b");
    CHECK(h.set_delimiter(",") == HEADER::STATUS::OK);
    CHECK(h.serialize() == "X-Foo: a,b\r\n");
}

TEST_CASE("set_delimiter: accepts space") {
    slim::common::http::Header h("X-Foo", "a");
    h.set_value("b");
    CHECK(h.set_delimiter(" ") == HEADER::STATUS::OK);
    CHECK(h.serialize() == "X-Foo: a b\r\n");
}

TEST_CASE("set_delimiter: rejects unknown delimiter") {
    slim::common::http::Header h("X-Foo", "a");
    CHECK(h.set_delimiter("|") == HEADER::STATUS::DELIMITER_INVALID);
}

TEST_CASE("set_delimiter: empty string is accepted (no delimiter)") {
    slim::common::http::Header h("X-Foo", "a");
    CHECK(h.set_delimiter("") == HEADER::STATUS::OK);
}

// ─── Known-header delimiters ──────────────────────────────────────────────────

TEST_CASE("Header: Content-Type uses semicolon-space delimiter") {
    slim::common::http::Header h("Content-Type", "text/html");
    h.set_value("charset=utf-8");
    CHECK(h.serialize() == "Content-Type: text/html; charset=utf-8\r\n");
}

TEST_CASE("Header: Accept uses comma-space delimiter") {
    slim::common::http::Header h("Accept", "text/html");
    h.set_value("application/json");
    CHECK(h.serialize() == "Accept: text/html, application/json\r\n");
}

TEST_CASE("Header: Authorization uses space delimiter") {
    slim::common::http::Header h("Authorization", "Bearer");
    h.set_value("token123");
    CHECK(h.serialize() == "Authorization: Bearer token123\r\n");
}

TEST_CASE("Header: User-Agent uses space delimiter") {
    slim::common::http::Header h("User-Agent", "Mozilla/5.0");
    h.set_value("(compatible)");
    CHECK(h.serialize() == "User-Agent: Mozilla/5.0 (compatible)\r\n");
}

TEST_CASE("Header: Content-Disposition uses semicolon-space delimiter") {
    slim::common::http::Header h("Content-Disposition", "attachment");
    h.set_value("filename=report.pdf");
    CHECK(h.serialize() == "Content-Disposition: attachment; filename=report.pdf\r\n");
}

TEST_CASE("Header: unknown header name gets no default delimiter") {
    slim::common::http::Header h("X-Custom", "a");
    h.set_value("b");
    // delimiter should be empty for unknown headers
    CHECK(h.serialize() == "X-Custom: ab\r\n");
}

// ─── Case-insensitive name matching for delimiter lookup ──────────────────────

TEST_CASE("Header: delimiter lookup is case-insensitive for header name") {
    slim::common::http::Header h("CONTENT-TYPE", "text/html");
    h.set_value("charset=utf-8");
    CHECK(h.serialize() == "CONTENT-TYPE: text/html; charset=utf-8\r\n");
}

// ─── serialize ────────────────────────────────────────────────────────────────

TEST_CASE("serialize: single value produces correct format") {
    slim::common::http::Header h("X-Foo", "bar");
    CHECK(h.serialize() == "X-Foo: bar\r\n");
}

TEST_CASE("serialize: three values joined by correct delimiter") {
    slim::common::http::Header h("Cache-Control", "no-cache");
    h.set_value("no-store");
    h.set_value("must-revalidate");
    CHECK(h.serialize() == "Cache-Control: no-cache, no-store, must-revalidate\r\n");
}

TEST_CASE("serialize: value containing all printable ASCII is accepted") {
    // 0x20–0x7E should all be valid
    std::string printable;
    for (int i = 0x20; i <= 0x7E; ++i) printable += static_cast<char>(i);
    slim::common::http::Header h("X-Foo", printable);
    CHECK(h.serialize().starts_with("X-Foo: "));
    CHECK(h.serialize().ends_with("\r\n"));
}

TEST_CASE("operator==: identical headers are equal") {
    slim::common::http::Header a("X-Foo", "bar");
    slim::common::http::Header b("X-Foo", "bar");
    CHECK(a == b);
}

TEST_CASE("operator==: name comparison is case-insensitive") {
    slim::common::http::Header a("x-foo", "bar");
    slim::common::http::Header b("X-FOO", "bar");
    CHECK(a == b);
}

TEST_CASE("operator==: different names are not equal") {
    slim::common::http::Header a("X-Foo", "bar");
    slim::common::http::Header b("X-Baz", "bar");
    CHECK_FALSE(a == b);
}

TEST_CASE("operator==: different values are not equal") {
    slim::common::http::Header a("X-Foo", "bar");
    slim::common::http::Header b("X-Foo", "baz");
    CHECK_FALSE(a == b);
}

TEST_CASE("operator==: different value counts are not equal") {
    slim::common::http::Header a("Accept", "text/html");
    slim::common::http::Header b("Accept", "text/html");
    b.set_value("application/json");
    CHECK_FALSE(a == b);
}

TEST_CASE("operator==: multiple values in same order are equal") {
    slim::common::http::Header a("Accept", "text/html");
    a.set_value("application/json");
    slim::common::http::Header b("Accept", "text/html");
    b.set_value("application/json");
    CHECK(a == b);
}

TEST_CASE("operator==: same delimiter are equal") {
    slim::common::http::Header a("X-Foo", "bar", ",");
    slim::common::http::Header b("X-Foo", "bar", ",");
    CHECK(a == b);
}

TEST_CASE("operator==: different delimiters are not equal") {
    slim::common::http::Header a("X-Foo", "bar", ",");
    slim::common::http::Header b("X-Foo", "bar", ";");
    CHECK_FALSE(a == b);
}

TEST_CASE("Content-Type: value with charset parameter") {
    slim::common::http::Header h("Content-Type", "text/html");
    h.set_value("charset=utf-8");
    CHECK(h.serialize() == "Content-Type: text/html; charset=utf-8\r\n");
}

TEST_CASE("Content-Type: multipart with boundary parameter") {
    slim::common::http::Header h("Content-Type", "multipart/form-data");
    h.set_value("boundary=----WebKitFormBoundary");
    CHECK(h.serialize() == "Content-Type: multipart/form-data; boundary=----WebKitFormBoundary\r\n");
}

TEST_CASE("Content-Type: application/json with charset") {
    slim::common::http::Header h("Content-Type", "application/json");
    h.set_value("charset=utf-8");
    CHECK(h.serialize() == "Content-Type: application/json; charset=utf-8\r\n");
}

TEST_CASE("Content-Disposition: attachment with filename") {
    slim::common::http::Header h("Content-Disposition", "attachment");
    h.set_value("filename=report.pdf");
    CHECK(h.serialize() == "Content-Disposition: attachment; filename=report.pdf\r\n");
}

TEST_CASE("Content-Disposition: form-data with name and filename") {
    slim::common::http::Header h("Content-Disposition", "form-data");
    h.set_value("name=file");
    h.set_value("filename=upload.png");
    CHECK(h.serialize() == "Content-Disposition: form-data; name=file; filename=upload.png\r\n");
}

TEST_CASE("Authorization: Bearer token with space delimiter") {
    slim::common::http::Header h("Authorization", "Bearer");
    h.set_value("eyJhbGciOiJIUzI1NiJ9");
    CHECK(h.serialize() == "Authorization: Bearer eyJhbGciOiJIUzI1NiJ9\r\n");
}

TEST_CASE("User-Agent: product with comment tokens") {
    slim::common::http::Header h("User-Agent", "Mozilla/5.0");
    h.set_value("AppleWebKit/537.36");
    h.set_value("Chrome/124.0");
    CHECK(h.serialize() == "User-Agent: Mozilla/5.0 AppleWebKit/537.36 Chrome/124.0\r\n");
}

TEST_CASE("Content-Type: single value containing inline parameters") {
    slim::common::http::Header h("Content-Type", "application/json; charset=utf-8");
    CHECK(h.serialize() == "Content-Type: application/json; charset=utf-8\r\n");
}
