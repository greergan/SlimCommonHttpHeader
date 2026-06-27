#include <catch2/catch_test_macros.hpp>
#include <slim/common/http/header.h>

using slim::common::http::ErrorStatus;
using slim::common::http::Header;
using slim::common::http::HttpHeaderException;

// ─── Construction ─────────────────────────────────────────────────────────────

TEST_CASE("Header: construction") {
    SECTION("valid construction sets name and value") {
        Header h("Content-Type", "text/html");
        CHECK(h.serialize() == "Content-Type: text/html\r\n");
    }
    SECTION("trims leading/trailing whitespace from name and value") {
        Header h("  Content-Type  ", "  text/html  ");
        CHECK(h.serialize() == "Content-Type: text/html\r\n");
    }
    SECTION("explicit delimiter is stored") {
        Header h("X-Custom", "a", ",");
        h.set_value("b");
        CHECK(h.serialize() == "X-Custom: a,b\r\n");
    }
    SECTION("throws on empty name") {
        CHECK_THROWS_AS(Header("", "value"), HttpHeaderException);
    }
    SECTION("throws on empty value") {
        CHECK_THROWS_AS(Header("X-Foo", ""), HttpHeaderException);
    }
    SECTION("throws on invalid name character") {
        CHECK_THROWS_AS(Header("Bad Name", "value"), HttpHeaderException);
    }
    SECTION("throws on invalid value character") {
        CHECK_THROWS_AS(Header("X-Foo", std::string(1, '\x7F')), HttpHeaderException);
    }
    SECTION("throws on invalid delimiter") {
        CHECK_THROWS_AS(Header("X-Foo", "value", "??"), HttpHeaderException);
    }
}

// ─── set_name ─────────────────────────────────────────────────────────────────

TEST_CASE("Header: set_name") {
    Header h("X-Foo", "bar");

    SECTION("accepts valid token characters") {
        CHECK(h.set_name("X-Bar") == ErrorStatus::OK);
        CHECK(h.serialize() == "X-Bar: bar\r\n");
    }
    SECTION("rejects empty name") {
        CHECK(h.set_name("") == ErrorStatus::HeaderNameEmpty);
    }
    SECTION("rejects name with space") {
        CHECK(h.set_name("Bad Name") == ErrorStatus::HeaderNameInvalidChar);
    }
    SECTION("rejects name with colon") {
        CHECK(h.set_name("Bad:Name") == ErrorStatus::HeaderNameInvalidChar);
    }
    SECTION("updates delimiter for known header") {
        Header ct("X-Custom", "a");
        ct.set_value("b");
        ct.set_name("Content-Type");
        ct.set_value("charset=utf-8");
        CHECK(ct.serialize() == "Content-Type: a; b; charset=utf-8\r\n");
    }
}

// ─── set_value / replace_value ────────────────────────────────────────────────

TEST_CASE("Header: set_value / replace_value") {
    Header h("X-Foo", "bar");

    SECTION("appends multiple values") {
        Header accept("Accept", "text/html");
        accept.set_value("application/json");
        CHECK(accept.serialize() == "Accept: text/html, application/json\r\n");
    }
    SECTION("rejects empty value") {
        CHECK(h.set_value("") == ErrorStatus::HeaderValueEmpty);
    }
    SECTION("rejects value with control character") {
        CHECK(h.set_value(std::string(1, '\x01')) == ErrorStatus::HeaderValueInvalidChar);
    }
    SECTION("accepts HTAB in value") {
        CHECK(h.set_value(std::string("a\tb")) == ErrorStatus::OK);
    }
    SECTION("replace_value clears existing values and sets new one") {
        Header accept("Accept", "text/html");
        accept.set_value("application/json");
        accept.replace_value("text/plain");
        CHECK(accept.serialize() == "Accept: text/plain\r\n");
    }
}

// ─── set_delimiter ────────────────────────────────────────────────────────────

TEST_CASE("Header: set_delimiter") {
    Header h("X-Foo", "a");
    h.set_value("b");

    SECTION("accepts semicolon") {
        CHECK(h.set_delimiter(";") == ErrorStatus::OK);
        CHECK(h.serialize() == "X-Foo: a;b\r\n");
    }
    SECTION("accepts comma") {
        CHECK(h.set_delimiter(",") == ErrorStatus::OK);
        CHECK(h.serialize() == "X-Foo: a,b\r\n");
    }
    SECTION("accepts space") {
        CHECK(h.set_delimiter(" ") == ErrorStatus::OK);
        CHECK(h.serialize() == "X-Foo: a b\r\n");
    }
    SECTION("rejects unknown delimiter") {
        CHECK(h.set_delimiter("|") == ErrorStatus::HeaderDelimiterInvalid);
    }
    SECTION("empty string is accepted") {
        CHECK(h.set_delimiter("") == ErrorStatus::OK);
    }
}

// ─── Known-header delimiters ──────────────────────────────────────────────────

TEST_CASE("Header: known-header delimiter lookup") {
    SECTION("Content-Type uses semicolon-space delimiter") {
        Header h("Content-Type", "text/html");
        h.set_value("charset=utf-8");
        CHECK(h.serialize() == "Content-Type: text/html; charset=utf-8\r\n");
    }
    SECTION("Accept uses comma-space delimiter") {
        Header h("Accept", "text/html");
        h.set_value("application/json");
        CHECK(h.serialize() == "Accept: text/html, application/json\r\n");
    }
    SECTION("User-Agent uses space delimiter") {
        Header h("User-Agent", "Mozilla/5.0");
        h.set_value("(compatible)");
        CHECK(h.serialize() == "User-Agent: Mozilla/5.0 (compatible)\r\n");
    }
    SECTION("Content-Disposition uses semicolon-space delimiter") {
        Header h("Content-Disposition", "attachment");
        h.set_value("filename=report.pdf");
        CHECK(h.serialize() == "Content-Disposition: attachment; filename=report.pdf\r\n");
    }
    SECTION("unknown header gets no default delimiter") {
        Header h("X-Custom", "a");
        h.set_value("b");
        CHECK(h.serialize() == "X-Custom: ab\r\n");
    }
    SECTION("lookup is case-insensitive") {
        Header h("CONTENT-TYPE", "text/html");
        h.set_value("charset=utf-8");
        CHECK(h.serialize() == "CONTENT-TYPE: text/html; charset=utf-8\r\n");
    }
}

// ─── serialize ────────────────────────────────────────────────────────────────

TEST_CASE("Header: serialize") {
    SECTION("single value produces correct format") {
        Header h("X-Foo", "bar");
        CHECK(h.serialize() == "X-Foo: bar\r\n");
    }
    SECTION("three values joined by correct delimiter") {
        Header h("Cache-Control", "no-cache");
        h.set_value("no-store");
        h.set_value("must-revalidate");
        CHECK(h.serialize() == "Cache-Control: no-cache, no-store, must-revalidate\r\n");
    }
    SECTION("value containing all printable ASCII is accepted") {
        std::string printable;
        for (int i = 0x20; i <= 0x7E; ++i) printable += static_cast<char>(i);
        Header h("X-Foo", printable);
        CHECK(h.serialize().starts_with("X-Foo: "));
        CHECK(h.serialize().ends_with("\r\n"));
    }
}

// ─── operator== ───────────────────────────────────────────────────────────────

TEST_CASE("Header: operator==") {
    SECTION("identical headers are equal") {
        Header a("X-Foo", "bar");
        Header b("X-Foo", "bar");
        CHECK(a == b);
    }
    SECTION("name comparison is case-insensitive") {
        Header a("x-foo", "bar");
        Header b("X-FOO", "bar");
        CHECK(a == b);
    }
    SECTION("different names are not equal") {
        Header a("X-Foo", "bar");
        Header b("X-Baz", "bar");
        CHECK_FALSE(a == b);
    }
    SECTION("different values are not equal") {
        Header a("X-Foo", "bar");
        Header b("X-Foo", "baz");
        CHECK_FALSE(a == b);
    }
    SECTION("different value counts are not equal") {
        Header a("Accept", "text/html");
        Header b("Accept", "text/html");
        b.set_value("application/json");
        CHECK_FALSE(a == b);
    }
    SECTION("multiple values in same order are equal") {
        Header a("Accept", "text/html");
        a.set_value("application/json");
        Header b("Accept", "text/html");
        b.set_value("application/json");
        CHECK(a == b);
    }
    SECTION("same delimiter are equal") {
        Header a("X-Foo", "bar", ",");
        Header b("X-Foo", "bar", ",");
        CHECK(a == b);
    }
    SECTION("different delimiters are not equal") {
        Header a("X-Foo", "bar", ",");
        Header b("X-Foo", "bar", ";");
        CHECK_FALSE(a == b);
    }
}

// ─── Content-Type ─────────────────────────────────────────────────────────────

TEST_CASE("Header: Content-Type") {
    SECTION("value with charset parameter") {
        Header h("Content-Type", "text/html");
        h.set_value("charset=utf-8");
        CHECK(h.serialize() == "Content-Type: text/html; charset=utf-8\r\n");
    }
    SECTION("multipart with boundary parameter") {
        Header h("Content-Type", "multipart/form-data");
        h.set_value("boundary=----WebKitFormBoundary");
        CHECK(h.serialize() == "Content-Type: multipart/form-data; boundary=----WebKitFormBoundary\r\n");
    }
    SECTION("application/json with charset") {
        Header h("Content-Type", "application/json");
        h.set_value("charset=utf-8");
        CHECK(h.serialize() == "Content-Type: application/json; charset=utf-8\r\n");
    }
    SECTION("single value containing inline parameters") {
        Header h("Content-Type", "application/json; charset=utf-8");
        CHECK(h.serialize() == "Content-Type: application/json; charset=utf-8\r\n");
    }
}

// ─── Content-Disposition ──────────────────────────────────────────────────────

TEST_CASE("Header: Content-Disposition") {
    SECTION("attachment with filename") {
        Header h("Content-Disposition", "attachment");
        h.set_value("filename=report.pdf");
        CHECK(h.serialize() == "Content-Disposition: attachment; filename=report.pdf\r\n");
    }
    SECTION("form-data with name and filename") {
        Header h("Content-Disposition", "form-data");
        h.set_value("name=file");
        h.set_value("filename=upload.png");
        CHECK(h.serialize() == "Content-Disposition: form-data; name=file; filename=upload.png\r\n");
    }
}

// ─── Authorization ────────────────────────────────────────────────────────────

TEST_CASE("Header: Authorization") {
    SECTION("Bearer token is stored as a single value") {
        Header h("Authorization", "Bearer eyJhbGciOiJIUzI1NiJ9");
        CHECK(h.serialize() == "Authorization: Bearer eyJhbGciOiJIUzI1NiJ9\r\n");
    }
    SECTION("a second set_value call replaces rather than appends") {
        Header h("Authorization", "Bearer first-token");
        h.set_value("Bearer second-token");
        CHECK(h.serialize() == "Authorization: Bearer second-token\r\n");
    }
    SECTION("a failing second set_value call clears the existing value") {
        Header h("Authorization", "Bearer good-token");
        CHECK(h.set_value("") == ErrorStatus::HeaderValueEmpty);
        CHECK_THROWS_AS(h.serialize(), HttpHeaderException); // values is now empty
    }
}

// ─── Proxy-Authorization ──────────────────────────────────────────────────────

TEST_CASE("Header: Proxy-Authorization") {
    SECTION("Bearer token is stored as a single value") {
        Header h("Proxy-Authorization", "Bearer eyJhbGciOiJIUzI1NiJ9");
        CHECK(h.serialize() == "Proxy-Authorization: Bearer eyJhbGciOiJIUzI1NiJ9\r\n");
    }
    SECTION("Basic credential is stored as a single value") {
        Header h("Proxy-Authorization", "Basic dXNlcjpwYXNz");
        CHECK(h.serialize() == "Proxy-Authorization: Basic dXNlcjpwYXNz\r\n");
    }
    SECTION("a second set_value call replaces rather than appends") {
        Header h("Proxy-Authorization", "Bearer first-token");
        h.set_value("Bearer second-token");
        CHECK(h.serialize() == "Proxy-Authorization: Bearer second-token\r\n");
    }
    SECTION("a failing second set_value call clears the existing value") {
        Header h("Proxy-Authorization", "Bearer good-token");
        CHECK(h.set_value("") == ErrorStatus::HeaderValueEmpty);
        CHECK_THROWS_AS(h.serialize(), HttpHeaderException); // values is now empty
    }
    SECTION("lookup is case-insensitive") {
        Header h("PROXY-AUTHORIZATION", "Bearer token");
        CHECK(h.serialize() == "PROXY-AUTHORIZATION: Bearer token\r\n");
    }
}

// ─── User-Agent ───────────────────────────────────────────────────────────────

TEST_CASE("Header: User-Agent") {
    SECTION("product with comment tokens") {
        Header h("User-Agent", "Mozilla/5.0");
        h.set_value("AppleWebKit/537.36");
        h.set_value("Chrome/124.0");
        CHECK(h.serialize() == "User-Agent: Mozilla/5.0 AppleWebKit/537.36 Chrome/124.0\r\n");
    }
}

// ─── Transfer-Encoding ────────────────────────────────────────────────────────

TEST_CASE("Header: Transfer-Encoding") {
    SECTION("single chunked value") {
        Header h("Transfer-Encoding", "chunked");
        CHECK(h.serialize() == "Transfer-Encoding: chunked\r\n");
    }
    SECTION("single value with chunked as the last token") {
        Header h("Transfer-Encoding", "gzip, chunked");
        CHECK(h.serialize() == "Transfer-Encoding: gzip, chunked\r\n");
    }
}

// ─── Accept ───────────────────────────────────────────────────────────────────

TEST_CASE("Header: Accept") {
    SECTION("multiple types with quality factors") {
        Header h("Accept", "text/html; q=1.0");
        h.set_value("application/json; q=0.9");
        h.set_value("application/xml; q=0.8");
        h.set_value("*/*; q=0.1");
        CHECK(h.serialize() == "Accept: text/html; q=1.0, application/json; q=0.9, application/xml; q=0.8, */*; q=0.1\r\n");
    }
    SECTION("single type with quality factor") {
        Header h("Accept", "text/html; q=1.0");
        CHECK(h.serialize() == "Accept: text/html; q=1.0\r\n");
    }
    SECTION("replace_value clears and sets single type") {
        Header h("Accept", "text/html; q=1.0");
        h.set_value("application/json; q=0.9");
        h.replace_value("*/*; q=0.1");
        CHECK(h.serialize() == "Accept: */*; q=0.1\r\n");
    }
}
