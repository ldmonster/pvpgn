# Migration Guide: xalloc/xstr/scoped_ptr → STL

This document explains how to migrate legacy PvPGN code that uses custom allocators (`xalloc`, `xfree`, `xrealloc`), string functions (`xstrdup`, `xstrcat`), and custom smart pointers (`scoped_ptr`) to modern C++ STL equivalents.

## Overview

The legacy codebase uses custom memory management and string utilities:
- **xalloc/xfree/xrealloc**: Custom memory allocation wrappers
- **xstrdup/xstrcat**: Custom string manipulation functions
- **scoped_ptr**: Custom smart pointer for RAII

The v3 codebase uses standard C++ STL:
- **std::vector, std::string**: For dynamic memory and strings
- **std::unique_ptr, std::shared_ptr**: For smart pointers
- **std::string_view**: For non-owning string references

## Migration Patterns

### Pattern 1: xalloc(n) → std::vector<uint8_t>(n)

**Legacy code:**
```cpp
void* buffer = xalloc(1024);
// ... use buffer ...
xfree(buffer);
```

**Modern v3 code:**
```cpp
std::vector<uint8_t> buffer(1024);
// ... use buffer.data() or buffer directly ...
// Automatically freed when buffer goes out of scope
```

**Advantages:**
- Automatic memory management (RAII)
- Bounds checking with `.at()`
- No manual free() calls
- Exception-safe (if exceptions were used)

### Pattern 2: xalloc(n) → std::make_unique<T[]>(n)

For typed allocations:

**Legacy code:**
```cpp
struct packet_t* packets = (struct packet_t*)xalloc(sizeof(struct packet_t) * count);
// ... use packets ...
xfree(packets);
```

**Modern v3 code:**
```cpp
auto packets = std::make_unique<packet_t[]>(count);
// ... use packets[i] or packets.get() ...
// Automatically freed when packets goes out of scope
```

### Pattern 3: xstrdup(s) → std::string(s)

**Legacy code:**
```cpp
char* name = xstrdup(user_input);
// ... use name ...
xfree(name);
```

**Modern v3 code:**
```cpp
std::string name(user_input);
// ... use name.c_str() or name directly ...
// Automatically freed when name goes out of scope
```

**Handling nullptr safely:**
```cpp
// Legacy code doesn't handle nullptr well
char* name = xstrdup(maybe_null_ptr);  // May crash if nullptr

// Modern v3 code handles nullptr gracefully
std::string name = pvpgn::core::compat::to_string(maybe_null_ptr);
// Returns empty string if maybe_null_ptr is nullptr
```

### Pattern 4: xstrcat(dest, src) → std::string concatenation

**Legacy code:**
```cpp
char buffer[256];
strcpy(buffer, "Hello ");
xstrcat(buffer, "World");
```

**Modern v3 code:**
```cpp
std::string buffer = "Hello ";
buffer += "World";
// or
std::string buffer = std::string("Hello ") + "World";
```

### Pattern 5: scoped_ptr<T> → std::unique_ptr<T>

**Legacy code:**
```cpp
scoped_ptr<Connection> conn(new Connection());
conn->send_data();
// Automatically deleted when conn goes out of scope
```

**Modern v3 code:**
```cpp
auto conn = std::make_unique<Connection>();
conn->send_data();
// Automatically deleted when conn goes out of scope
```

**For arrays:**
```cpp
// Legacy
scoped_array<uint8_t> buffer(new uint8_t[1024]);

// Modern
auto buffer = std::make_unique<uint8_t[]>(1024);
```

### Pattern 6: xrealloc(ptr, size) → std::vector::resize()

**Legacy code:**
```cpp
void* buffer = xalloc(100);
// ... use buffer ...
buffer = xrealloc(buffer, 200);  // Resize to 200 bytes
```

**Modern v3 code:**
```cpp
std::vector<uint8_t> buffer(100);
// ... use buffer ...
buffer.resize(200);  // Resize to 200 bytes
```

## Migration Checklist

When migrating a function or module:

- [ ] Replace all `xalloc()` calls with `std::vector<uint8_t>()` or `std::make_unique<T[]>()`
- [ ] Replace all `xfree()` calls with scope exit (RAII handles it)
- [ ] Replace all `xrealloc()` calls with `std::vector::resize()`
- [ ] Replace all `xstrdup()` calls with `std::string()` or `pvpgn::core::compat::to_string()`
- [ ] Replace all `scoped_ptr<T>` with `std::unique_ptr<T>`
- [ ] Replace all `scoped_array<T>` with `std::unique_ptr<T[]>`
- [ ] Update function signatures to use `std::string_view` for read-only string parameters
- [ ] Add unit tests to verify behavior
- [ ] Run existing tests to ensure no regressions

## Compatibility Shim

For gradual migration, `src/v3/core/include/core/legacy_compat.hpp` provides compatibility functions:

```cpp
#include "core/legacy_compat.hpp"

using namespace pvpgn::core::compat;

// These work like the legacy functions but are safer
void* ptr = xalloc(100);
char* str = xstrdup("hello");
auto smart_ptr = std::make_unique<int>(42);
```

**Note:** The compatibility shim is a temporary bridge. New code should use STL directly.

## Common Pitfalls

### Pitfall 1: Forgetting to use .data() or .c_str()

**Wrong:**
```cpp
std::vector<uint8_t> buffer(100);
send_to_network(buffer);  // Compiler error: can't pass vector to function expecting uint8_t*
```

**Correct:**
```cpp
std::vector<uint8_t> buffer(100);
send_to_network(buffer.data());  // Pass pointer to underlying data
```

### Pitfall 2: Mixing ownership models

**Wrong:**
```cpp
std::unique_ptr<Connection> conn = new Connection();  // Compiler error
```

**Correct:**
```cpp
auto conn = std::make_unique<Connection>();
// or
std::unique_ptr<Connection> conn(new Connection());
```

### Pitfall 3: Returning local unique_ptr

**Wrong:**
```cpp
std::unique_ptr<Buffer> create_buffer() {
    std::unique_ptr<Buffer> buf = std::make_unique<Buffer>();
    return buf;  // Compiler error: can't copy unique_ptr
}
```

**Correct:**
```cpp
std::unique_ptr<Buffer> create_buffer() {
    auto buf = std::make_unique<Buffer>();
    return buf;  // OK: move semantics
}
```

### Pitfall 4: Dangling string_view

**Wrong:**
```cpp
std::string_view get_name() {
    std::string name = "Alice";
    return name;  // Dangling reference! name is destroyed
}
```

**Correct:**
```cpp
std::string get_name() {
    std::string name = "Alice";
    return name;  // OK: returns by value
}

// Or if you need a view:
std::string_view get_name(const std::string& name) {
    return name;  // OK: caller owns the string
}
```

## Performance Considerations

### std::vector vs raw pointers

- **std::vector**: Slightly more overhead (bounds checking, capacity tracking)
- **Raw pointers**: Faster but unsafe
- **Recommendation**: Use std::vector for safety; optimize only if profiling shows it's a bottleneck

### std::string vs char*

- **std::string**: Small string optimization (SSO) makes short strings very fast
- **char***: Requires manual management
- **Recommendation**: Use std::string; it's often faster than manual char* management

### std::unique_ptr vs raw pointers

- **std::unique_ptr**: Zero-cost abstraction (no runtime overhead)
- **Raw pointers**: Requires manual delete
- **Recommendation**: Always use std::unique_ptr for ownership

## Testing

After migration, ensure:

1. **Unit tests pass**: Run existing tests to catch regressions
2. **Memory tests pass**: Use AddressSanitizer to catch memory errors
3. **Performance tests pass**: Ensure no unexpected slowdowns
4. **Integration tests pass**: Test with real workloads

Example test:
```cpp
TEST(MigrationTest, StringAllocation) {
    std::string name = pvpgn::core::compat::to_string("Alice");
    EXPECT_EQ(name, "Alice");
    
    std::string empty = pvpgn::core::compat::to_string(nullptr);
    EXPECT_TRUE(empty.empty());
}
```

## References

- [cppreference: std::vector](https://en.cppreference.com/w/cpp/container/vector)
- [cppreference: std::string](https://en.cppreference.com/w/cpp/string/basic_string)
- [cppreference: std::unique_ptr](https://en.cppreference.com/w/cpp/memory/unique_ptr)
- [cppreference: std::string_view](https://en.cppreference.com/w/cpp/string/basic_string_view)
- [C++ Core Guidelines: Resource Management](https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#S-resource)
