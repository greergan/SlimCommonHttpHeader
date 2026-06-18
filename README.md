<a href="https://github.com/greergan/SlimTS">
  <img src="https://raw.githubusercontent.com/greergan/SlimTS/master/assets/slimts_logo.png" width="75" alt="SlimTS Logo">
</a>

# SlimCommonHTTPHeader

A lightweight, RFC 9110-oriented HTTP header implementation in modern C++.  
Acts as a validating, backing store for the [SlimTS](https://codeberg.org/greergan/SlimTS) Javascript Header object.  
Part of the [SlimCommon](https://codeberg.org/greergan/SlimCommon) library.  
Dependency of the [SlimCommonHttpHeaders](https://codeberg.org/greergan/SlimCommonHttpHeaders) micro-library.  
Built using [SlimLibraryPackager](https://codeberg.org/greergan/SlimLibraryPackager).  
CI/CD supplied by unified workflows provided by [SlimLibraryPackager](https://codeberg.org/greergan/SlimLibraryPackager).

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Core API](#core-api)
  - [HeaderStatus enum](#headerstatus-enum)
  - [HeaderType enum](#headertype-enum)
  - [HeaderException](#headerexception)
  - [Header class](#header-class)
  - [Constructors and object lifetime](#constructors-and-object-lifetime)
  - [Operators](#operators)
  - [Setters](#setters)
  - [Getters](#getters)
  - [Validation](#validation)
  - [Serialization](#serialization)
- [Known-header delimiter table](#known-header-delimiter-table)
- [Building](#building)
- [Dependencies](#dependencies)
- [Examples](#examples)

## Overview

This library provides a strict, validation-heavy HTTP header builder and serializer with:
- RFC 9110 compliant token and field-value validation
- Strong validation for all header components
- Zero dynamic allocation in validation paths (where possible)
- Explicit status reporting via `HeaderStatus`
- Strict parsing over permissive recovery
- Explicit validation at each setter
- Automatic delimiter selection for known headers
- Minimal runtime overhead in hot paths
- Heavy use of `noexcept`

[↑ Top](#table-of-contents)

## Features

| Feature | Description |
|--------|-------------|
| Name validation | RFC 9110 tchar-only enforcement |
| Value validation | Printable ASCII (0x20–0x7E) + HTAB (0x09) |
| Multi-value support | Append multiple values, joined on serialize |
| Delimiter selection | Automatic per-header delimiter from known table (defaults to space) |
| Custom delimiters | Override with `;` `,` or space |
| Serialize | Preallocated, zero-fragment string build |
| Serialize | Header validation with thrown exceptions |
| Error model | Strong enum-based status reporting via `HeaderStatus` |

[↑ Top](#table-of-contents)

## Core API

### HeaderStatus enum

All setters and `validate()` return a `HeaderStatus`. The full set of values is:

| Value | Meaning |
|-------|---------|
| `OK` | Success |
| `DelimiterInvalid` | Header delimiter is not valid |
| `NameEmpty` | Header name cannot be empty |
| `NameInvalidChar` | Header name contains an invalid character |
| `ValueEmpty` | Header value cannot be empty |
| `ValueInvalidChar` | Header value contains an invalid character |
| `ValueInvalidFolding` | Header value contains obsolete line folding (obs-fold) |

Status values can be converted to a human-readable string via:

```cpp
std::string_view msg = slim::common::http::header::status::to_string(status);
```

[↑ Top](#table-of-contents)

### HeaderType enum

Enumerates the set of headers with a known, automatically-selected delimiter. Used internally by the parameterised `Header` constructor when no explicit delimiter is supplied.

| Value | Delimiter |
|-------|-----------|
| `Accept` | `, ` |
| `AcceptCharset` | `, ` |
| `AcceptEncoding` | `, ` |
| `AcceptLanguage` | `, ` |
| `Allow` | `, ` |
| `Authorization` | ` ` |
| `CacheControl` | `, ` |
| `Connection` | `, ` |
| `ContentDisposition` | `; ` |
| `ContentEncoding` | `, ` |
| `ContentType` | `; ` |
| `Forwarded` | `, ` |
| `IfMatch` | `, ` |
| `IfNoneMatch` | `, ` |
| `Link` | `, ` |
| `TransferEncoding` | `, ` |
| `UserAgent` | ` ` |
| `Vary` | `, ` |
| `Via` | `, ` |

Headers outside this set fall back to a space delimiter unless overridden.

```cpp
std::string_view delim = slim::common::http::header::type::delimiter(HeaderType::ContentType);
// -> "; "
```

[↑ Top](#table-of-contents)

### HeaderException

Thrown by the parameterised constructor and `serialize()` on validation failure.

```cpp
class HeaderException : public std::logic_error {
public:
    explicit HeaderException(HeaderStatus e);   // formats "Invalid Header: <status string>"
    explicit HeaderException(std::string msg);  // arbitrary message
};
```

[↑ Top](#table-of-contents)

### Header class

```cpp
slim::common::http::Header h;
```

[↑ Top](#table-of-contents)

### Constructors and object lifetime

| Form | Description |
|------|-------------|
| `Header()` | Default constructor, produces an empty header |
| `Header(std::string_view name, std::string_view value, std::string delimiter = "")` | Construct with name and initial value, validated immediately. Delimiter is optional; if omitted, it is selected automatically from the known-header table. Throws `HeaderException` on failure |
| `Header(const Header&)` | Deleted — copies are not allowed |
| `Header& operator=(const Header&)` | Deleted — copies are not allowed |
| `Header(Header&&) noexcept` | Move construction is supported |
| `Header& operator=(Header&&) noexcept` | Move assignment is supported |

[↑ Top](#table-of-contents)

### Operators

| Operator | Description |
|----------|-------------|
| `bool operator==(const Header&) const noexcept` | Equality by name (case-insensitive), values, and delimiter |

[↑ Top](#table-of-contents)

### Setters

| Method | Description |
|--------|-------------|
| `HeaderStatus set_name(std::string_view) noexcept` | Set header name (validated) |
| `HeaderStatus set_value(std::string_view) noexcept` | Append a value (validated) |
| `HeaderStatus replace_value(std::string_view) noexcept` | Clear all values and set a new one |
| `HeaderStatus set_delimiter(std::string) noexcept` | Override the join delimiter |

[↑ Top](#table-of-contents)

### Getters

| Method | Returns |
|--------|---------|
| `std::string_view get_name() const noexcept` | Header name |
| `const std::vector<std::string>& get_value() const noexcept` | All values |

[↑ Top](#table-of-contents)

### Validation

```cpp
HeaderStatus Header::validate() const noexcept;
```

Checks:
- Name is non-empty and contains only valid RFC 9110 tchar characters
- At least one value has been set
- Values contain only printable ASCII or HTAB, with no obsolete line folding
- Delimiter, if overridden, is valid

[↑ Top](#table-of-contents)

### Serialization

```cpp
std::string Header::serialize() const;
// -> "Content-Type: text/html; charset=utf-8\r\n"
```

Outputs a fully formatted header line: `Name: value1<delim>value2\r\n`.  
Throws `HeaderException` if validation fails — for example, an empty name or no values set.

[↑ Top](#table-of-contents)

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

[↑ Top](#table-of-contents)

## Building

This library is built using [SlimLibraryPackager](https://codeberg.org/greergan/SlimLibraryPackager). See that repository for build instructions.

[↑ Top](#table-of-contents)

## Dependencies

External package dependencies for this library are declared in the `required_packages` file at the repository root. This file is read by [SlimLibraryPackager](https://codeberg.org/greergan/SlimLibraryPackager) during the build process to resolve dependencies and install them if not present.

```
none
```

[↑ Top](#table-of-contents)

## Examples

```cpp
// Constructor with automatic delimiter selection and status checking
slim::common::http::Header h("Content-Type", "text/html");

HeaderStatus e = h.set_value("charset=utf-8");
if (e != HeaderStatus::OK) return e;

auto line = h.serialize();
// -> "Content-Type: text/html; charset=utf-8\r\n"
```

```cpp
// Multi-value header with exception-based usage
slim::common::http::Header h("Accept", "text/html");
h.set_value("application/json");
h.set_value("*/*");

try {
    auto line = h.serialize();
    // -> "Accept: text/html, application/json, */*\r\n"
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
// -> "X-Custom: a;b\r\n"
```

[↑ Top](#table-of-contents)
