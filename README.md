# SlimCommonHTTPHeader

A lightweight, RFC 9110-oriented HTTP header implementation in modern C++.  
Acts as a validating, backing store for [SlimTS's](https://github.com/greergan/SlimTS) Javascript Header object.

## Overview

This library provides a strict, validation-heavy HTTP header builder and serializer with:
- RFC 9110 compliant token and field-value validation
- Strong validation for all header components
- Zero dynamic allocation in validation paths (where possible)
- Explicit status reporting via `HEADER::STATUS`
- Strict parsing over permissive recovery
- Explicit validation at each setter
- Automatic delimiter selection for known headers
- Minimal runtime overhead in hot paths
- Heavy use of `noexcept`

## Features

| Feature | Description |
|---------|-------------|
| Name validation | RFC 9110 tchar-only enforcement |
| Value validation | Printable ASCII (0x20–0x7E) + HTAB (0x09) |
| Multi-value support | Append multiple values, joined on serialize |
| Delimiter selection | Automatic per-header delimiter from known table (defaults to space) |
| Custom delimiters | Override with `;` `,` or space |
| Serialize | Preallocated, zero-fragment string build |
| Serialize | Header validation with thrown exceptions |
| Error model | Strong enum-based status reporting |

## Core API

### Header class

```cpp
slim::common::http::Header h("Content-Type", "text/html");
```

Constructor accepts an optional explicit delimiter as a third argument. If omitted, the delimiter is
selected automatically from the known-header table. Throws `HeaderException` on any validation failure.

### Setters

| Method | Description |
|--------|-------------|
| `HEADER::STATUS set_name(std::string_view) noexcept;` | Set header name (validated) |
| `HEADER::STATUS set_value(std::string_view) noexcept;` | Append a value (validated) |
| `HEADER::STATUS replace_value(std::string_view) noexcept;` | Clear all values and set a new one |
| `HEADER::STATUS set_delimiter(std::string) noexcept;` | Override the join delimiter |

### Getters

| Method | Returns |
|--------|---------|
| `std::string_view get_name() const noexcept;` | Header name |
| `const std::vector<std::string>& get_value() const noexcept;` | All values |

### Operators

| Operator | Description |
|----------|-------------|
| `bool operator==(const Header&) const noexcept;` | Equality by name (case-insensitive), values, and delimiter |

### Serialization

```cpp
std::string Header::serialize() const;
```

Outputs a fully formatted header line: `Name: value1<delim>value2\r\n`.  
Throws `HeaderException` if name is empty or no values have been set.

## Known-header delimiter table

| Header | Delimiter |
|--------|-----------|
| Accept | `, ` |
| Accept-Charset | `, ` |
| Accept-Encoding | `, ` |
| Accept-Language | `, ` |
| Allow | `, ` |
| Authorization | ` ` |
| Cache-Control | `, ` |
| Connection | `, ` |
| Content-Disposition | `; ` |
| Content-Encoding | `, ` |
| Content-Type | `; ` |
| Forwarded | `, ` |
| If-Match | `, ` |
| If-None-Match | `, ` |
| Link | `, ` |
| Transfer-Encoding | `, ` |
| User-Agent | ` ` |
| Vary | `, ` |
| Via | `, ` |

Unknown headers receive an empty delimiter; values are concatenated directly.

## Example

```cpp
slim::common::http::Header h("Content-Type", "text/html");
HEADER::STATUS e = h.set_value("charset=utf-8");
if (e != HEADER::STATUS::OK) return e;

auto line = h.serialize();
// → "Content-Type: text/html; charset=utf-8\r\n"
```

```cpp
slim::common::http::Header h("Accept", "text/html");
h.set_value("application/json");
h.set_value("*/*");

try {
    auto line = h.serialize();
    // → "Accept: text/html, application/json, */*\r\n"
}
catch (const slim::common::http::HeaderException& e) {
    std::cerr << "Header serialization failed: " << e.what() << '\n';
}
catch (const std::exception& e) {
    std::cerr << "Unexpected error: " << e.what() << '\n';
}
```

```cpp
// Custom delimiter
slim::common::http::Header h("X-Custom", "a", ";");
h.set_value("b");
auto line = h.serialize();
// → "X-Custom: a;b\r\n"
```
