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
