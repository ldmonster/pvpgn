# R255 — Fix ip_address.hpp cstdio include

## Checklist

- [x] Read `domain/shared/ip_address.hpp`
- [x] Replaced `#include <cstdio>` with `#include <format>`
- [x] Replaced `std::snprintf` with `std::format`
- [x] No `<iostream>`, `<fstream>`, `<cstdio>`, `<ctime>` remain in the header
- [x] No `infra/` includes in the header
- [x] `IpAddress::to_string()` still produces correct output

## Exit Criterion

`grep '<cstdio>' src/v3/domain/shared/include/domain/shared/ip_address.hpp` returns no output.
`IpAddress::parse("192.168.1.1")->to_string() == "192.168.1.1"`.

## Status: GREEN
