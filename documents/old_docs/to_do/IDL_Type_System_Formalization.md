<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# IDL Type System Formalization

**Status**: 🟡 PLANNED - HIGH IMPACT FOUNDATIONAL WORK
**Priority**: HIGH
**Estimated Effort**:

## Objective

Formalize the Canopy IDL type system with explicit type definitions in the parser and AST, eliminating platform ambiguity and enabling better code generation.

## Problem Identified

The current IDL parser and AST have no formal concept of types beyond structural elements (structs, interfaces, namespaces, enums). Types like `int`, `long`, `string`, `vector<T>` are treated as opaque strings passed through to serialization generators, which must guess their meaning or rely on C++ compiler knowledge. This creates several issues:

1. **Platform Ambiguity**: C++ types like `long`, `int`, `size_t` have different sizes on different platforms
   - `long` is 32-bit on Windows x64, 64-bit on Linux x64
   - `int` could be 16-bit on embedded systems, 32-bit on most platforms
   - `size_t` varies based on pointer width

2. **Poor Type Safety**: Generators must parse type strings and guess semantics
   - Is `vector<int>` a `std::vector<int32_t>` or `std::vector<int64_t>`?
   - No validation that types are used correctly
   - No ability to enforce serialization constraints

3. **Limited Metaprogramming**: Cannot query type properties in IDL
   - Cannot ask "is this type fixed-size?"
   - Cannot determine wire format size at code generation time
   - Cannot validate that types are serializable

4. **Unclear RPC Semantics**: No distinction between value types and handle types
   - `shared_ptr<T>` passed by value vs. RPC shared handle with reference counting
   - Raw pointers vs. optimistic handles
   - No way to express ownership semantics in IDL

5. **Code Generator Complexity**: Type conversion requires multiple passes with string manipulation
   - Protobuf generator: `cpp_type_to_proto_type()` converts `std::vector<rpc::back_channel_entry>` → `"repeated rpc::back_channel_entry"` (string with keyword)
   - Then `sanitize_type_name()` must preserve `"repeated "` keyword while converting `::` → `.` for cross-package references
   - Two-phase conversion creates fragile code with special cases for `"repeated "`, `"map<"`, etc.
   - Example: Lines 399-421 in `protobuf_generator.cpp` need complex logic to handle `"repeated TypeName"` strings
   - **Root cause**: Types are strings, not AST nodes with proper structure (container type + element type + namespace)

## Inspiration Sources

- **Rust Type System**: Clear distinction between primitive types (`i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`), with no platform ambiguity
- **WebAssembly Component Model**: Formal type system with explicit integer widths, strings, records, variants, lists, and handles
- **Protocol Buffers**: Explicit types (`int32`, `int64`, `uint32`, `uint64`, `float`, `double`, `string`, `bytes`)

---

## Proposed Type System

### 1. Primitive Integer Types (explicit bit widths)

```idl
// Signed integers
i8      // 8-bit signed integer (-128 to 127)
i16     // 16-bit signed integer
i32     // 32-bit signed integer
i64     // 64-bit signed integer

// Unsigned integers
u8      // 8-bit unsigned integer (0 to 255)
u16     // 16-bit unsigned integer
u32     // 32-bit unsigned integer
u64     // 64-bit unsigned integer

// Floating point
f32     // 32-bit IEEE 754 float
f64     // 64-bit IEEE 754 double
```

### 2. String and Binary Types

```idl
string          // UTF-8 encoded string (variable length)
bytes           // Binary data (equivalent to vector<u8>)
```

### 3. Collection Types

```idl
vector<T>       // Dynamic array of elements (std::vector equivalent)
array<T, N>     // Fixed-size array of N elements
map<K, V>       // Key-value associative container (std::map/unordered_map)
optional<T>     // Optional value (std::optional equivalent)
```

### 4. RPC Handle Types (explicit ownership semantics)

```idl
shared_handle<T>      // Shared reference-counted handle (like rpc::shared_ptr)
optimistic_handle<T>  // Optimistic handle without reference counting
```

### 5. Composite Types

```idl
struct SomeName {
    field1: i32;
    field2: string;
    field3: vector<u8>;
}

enum SomeEnum : u32 {  // Explicit underlying type
    Variant1 = 0,
    Variant2 = 1
}

variant SomeVariant {  // Tagged union (like Rust enum or C++ std::variant)
    case1: i32;
    case2: string;
    case3: struct { x: f64; y: f64; };
}
```

### 6. C++ Type Compatibility (typedef mapping)

```idl
// In a standard library IDL file (cpp_compat.idl):
typedef long = i64;        // On Linux x64
typedef long = i32;        // On Windows x64 (platform-conditional)
typedef int = i32;         // Most platforms
typedef size_t = u64;      // 64-bit platforms
typedef size_t = u32;      // 32-bit platforms

// Users can still use C++ type names, but they map to formal types
typedef std::string = string;
typedef std::vector<T> = vector<T>;
typedef std::optional<T> = optional<T>;
```

---

## Implementation Strategy

### Phase 1: AST Enhancement (2-3 weeks)

1. **Update `/submodules/idlparser/` AST classes**:
   ```cpp
   // New type representation in AST
   class type_entity {
   public:
       enum class type_kind {
           // Primitives
           I8, I16, I32, I64,
           U8, U16, U32, U64,
           F32, F64,
           STRING, BYTES,

           // Collections
           VECTOR, ARRAY, MAP, OPTIONAL,

           // RPC handles
           SHARED_HANDLE, OPTIMISTIC_HANDLE, WEAK_HANDLE,

           // Composite
           STRUCT, ENUM, VARIANT,

           // Typedef (resolved to underlying type)
           TYPEDEF
       };

       type_kind get_kind() const;
       std::vector<type_entity> get_type_parameters() const;  // For vector<T>, map<K,V>
       size_t get_fixed_size() const;  // For array<T, N>

       // Queries
       bool is_primitive() const;
       bool is_fixed_size() const;
       bool is_handle_type() const;
       size_t get_wire_size() const;  // Returns 0 for variable-size types
   };
   ```

2. **Parser updates** to recognize new type syntax:
   - Add keywords: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`
   - Add keywords: `string`, `bytes`, `vector`, `array`, `map`, `optional`
   - Add keywords: `shared_handle`, `optimistic_handle`, `weak_handle`
   - Add `variant` composite type support

3. **Type resolution system**:
   - Resolve typedefs to underlying types
   - Validate type parameters (e.g., `map<K, V>` requires K to be comparable)
   - Build type symbol table during parsing

### Phase 2: Code Generator Updates (3-4 weeks)

1. **Update `/generator/src/synchronous_generator.cpp`**:
   - Replace string-based type handling with AST type queries
   - Generate correct C++ types from formal IDL types:
     - `i32` → `int32_t`
     - `u64` → `uint64_t`
     - `f32` → `float`
     - `shared_handle<T>` → `rpc::shared_ptr<T>`

2. **Update `/generator/src/protobuf_generator.cpp`**:
   - Map IDL types to protobuf types precisely:
     - `i32` → `int32`
     - `u32` → `uint32`
     - `f64` → `double`
     - `bytes` → `bytes`
     - `vector<T>` → `repeated T`

3. **Serialization improvements**:
   - Fixed-size types can use optimized serialization
   - Variable-size types require length prefixes
   - Handle types get special marshalling logic

### Phase 3: Migration and Compatibility (2-3 weeks)

1. **Backward compatibility layer**:
   ```idl
   // Legacy IDL (still supported via implicit typedef):
   struct OldStyle {
       int value;           // Implicitly treated as i32
       long timestamp;      // Platform-dependent typedef to i32/i64
   }

   // New IDL (explicit types):
   struct NewStyle {
       value: i32;
       timestamp: i64;      // Explicit, no platform ambiguity
   }
   ```

2. **Migration tool**:
   - Analyze existing `.idl` files
   - Suggest explicit type replacements
   - Warn about platform-dependent types

3. **Standard library IDL**:
   - Create `rpc/idl/std_types.idl` with common typedefs
   - Platform-conditional type mappings
   - Import in user IDL files for C++ compatibility

### Phase 4: Validation and Tooling (2 weeks)

1. **Type validation rules**:
   - Ensure all struct fields have explicit types
   - Validate generic type parameters (e.g., `vector<T>` where T is serializable)
   - Detect non-portable type usage

2. **Enhanced error messages**:
   ```
   error: type 'long' is platform-dependent
     --> example.idl:15:5
      |
   15 |     long timestamp;
      |     ^^^^ platform-dependent size (32-bit on Windows, 64-bit on Linux)
      |
      = help: use explicit 'i32' or 'i64' instead
   ```

3. **Documentation generation**:
   - Generate docs showing exact wire format
   - Show type sizes and alignment
   - Document handle semantics

---

## Benefits

1. **Platform Independence**: Eliminate "works on my machine" bugs due to type size differences
2. **Better Tooling**: IDL parsers can validate types, suggest fixes, generate precise documentation
3. **Optimized Serialization**: Fixed-size types enable zero-copy and optimized encoding
4. **Clear Semantics**: Explicit handle types document ownership and lifetime
5. **WebAssembly Compatibility**: Formal type system aligns with WASM Component Model for future interop
6. **Protocol Evolution**: Easier to add new wire formats (JSON, MessagePack, etc.) with clear type mappings

---

## Example IDL Comparison

### Before (Current)

```idl
namespace example;

interface i_user {
    // Ambiguous types - generator must guess
    int get_user_id();
    long get_timestamp();
    void update_balance(double amount);
    std::vector<std::string> get_messages();
}

struct user_data {
    int id;                    // int32? int64? Platform-dependent?
    long created_at;           // 32-bit on Windows, 64-bit on Linux!
    std::vector<char> avatar;  // Binary data, but looks like a string
}
```

### After (With Formal Types)

```idl
namespace example;

interface i_user {
    // Explicit, unambiguous types
    get_user_id() -> i32;
    get_timestamp() -> i64;
    update_balance(amount: f64) -> ();
    get_messages() -> vector<string>;
}

struct user_data {
    id: i32;                   // Exactly 32-bit signed
    created_at: i64;           // Exactly 64-bit, all platforms
    avatar: bytes;             // Clearly binary data
}
```

---

## Dependencies

- Requires updates to `/submodules/idlparser/` AST and parser
- Affects all code generators (synchronous, protobuf, future generators)
- Benefits all serialization work (WebSocket, REST, QUIC transports)
