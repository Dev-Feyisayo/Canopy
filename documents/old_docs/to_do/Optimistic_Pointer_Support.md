<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Optimistic Pointer Support - Testing and IDL Integration

**Status**: PARTIALLY IMPLEMENTED - Needs comprehensive testing and IDL parser support
**Priority**: MEDIUM
**Estimated Effort**:

## Current Status

- ✅ `rpc::optimistic_ptr<T>` class exists in C++ runtime
- ✅ Some tests exist in `post_functionality_test_suite.cpp`
- ❌ **Not supported in IDL parser** - can't declare in .idl files
- ❌ Comprehensive testing incomplete
- ❌ object_released() integration not fully implemented
- ❌ Documentation gaps

## What Are Optimistic Pointers?

**Optimistic pointers** (`rpc::optimistic_ptr<T>`) are a lightweight alternative to shared pointers for RPC scenarios where:
- The remote object's lifetime is managed elsewhere
- You don't need strong guarantees that the object still exists
- You want to avoid the overhead of reference counting

### Comparison with shared_ptr

| Feature | rpc::shared_ptr<T> | rpc::optimistic_ptr<T> |
|---------|-------------------|------------------------|
| **Reference counting** | Yes (add_ref/release) | No |
| **Remote object lifetime** | Guaranteed while pointer exists | Not guaranteed |
| **Overhead** | Higher (ref count messages) | Lower (no ref count) |
| **Use case** | Long-lived references | Short-lived, cache-like usage |
| **Object destruction** | Waits for all refs to release | Object can be destroyed anytime |
| **Cleanup notification** | Automatic via ref counting | Via `object_released()` |

### Usage Example (C++ code)

```cpp
// Zone A creates and shares an object
rpc::shared_ptr<i_foo> foo = ...;  // Strong reference

// Zone B gets optimistic pointer
rpc::optimistic_ptr<i_foo> opt_foo = foo;  // Lightweight, no ref count

// Use optimistic pointer (may fail if object destroyed)
try {
    opt_foo->some_method();  // Works if object still exists
} catch (const std::exception& e) {
    // Object was destroyed, opt_foo is now invalid
}
```

## Existing Tests

Located in `/tests/test_host/post_functionality_test_suite.cpp`:

```cpp
TYPED_TEST(post_functionality_test, post_with_optimistic_ptr)
{
    // Get example object
    auto example = lib.get_example();

    // Create a foo object
    rpc::shared_ptr<xxx::i_foo> foo_obj;
    CORO_ASSERT_EQ(CO_AWAIT example->create_foo(foo_obj), 0);

    // Create an optimistic pointer
    rpc::optimistic_ptr<xxx::i_foo> opt_foo;
    // ... test implementation ...
}
```

This test exists but may not cover all scenarios.

## Missing Tests

### Test 1: Optimistic pointer basic usage
```cpp
CORO_TYPED_TEST(optimistic_ptr_test, "basic_creation_and_usage") {
    // Create shared pointer
    rpc::shared_ptr<i_foo> shared = ...;

    // Create optimistic pointer from shared
    rpc::optimistic_ptr<i_foo> opt = shared;

    // Verify can call methods
    REQUIRE(opt != nullptr);
    REQUIRE_EQ(opt->some_method(), expected_value);

    // Verify no reference count increase
    // (implementation-specific check)
}
```

### Test 2: Object destruction invalidates optimistic pointer
```cpp
CORO_TYPED_TEST(optimistic_ptr_test, "invalidation_on_destruction") {
    rpc::shared_ptr<i_foo> shared = ...;
    rpc::optimistic_ptr<i_foo> opt = shared;

    // Optimistic pointer works initially
    REQUIRE(opt.is_valid());
    REQUIRE_NOTHROW(opt->some_method());

    // Destroy shared pointer
    shared.reset();

    // Eventually, optimistic pointer becomes invalid
    REQUIRE_EVENTUALLY(!opt.is_valid());

    // Access should fail gracefully
    REQUIRE_THROWS(opt->some_method());
}
```

### Test 3: Multiple zones with optimistic pointers
```cpp
CORO_TYPED_TEST(optimistic_ptr_test, "multiple_zones") {
    // Zone A creates object
    auto zone_a = get_zone("A");
    rpc::shared_ptr<i_foo> shared = ...;

    // Zones B, C, D get optimistic pointers
    auto opt_b = get_optimistic_ptr_in_zone("B", shared);
    auto opt_c = get_optimistic_ptr_in_zone("C", shared);
    auto opt_d = get_optimistic_ptr_in_zone("D", shared);

    // All work initially
    REQUIRE(opt_b.is_valid());
    REQUIRE(opt_c.is_valid());
    REQUIRE(opt_d.is_valid());

    // Zone A destroys object
    shared.reset();

    // All zones receive object_released() and become invalid
    REQUIRE_EVENTUALLY(!opt_b.is_valid());
    REQUIRE_EVENTUALLY(!opt_c.is_valid());
    REQUIRE_EVENTUALLY(!opt_d.is_valid());
}
```

### Test 4: Optimistic pointer with pass-through
```cpp
CORO_TYPED_TEST(optimistic_ptr_test, "through_passthrough") {
    // Zone 1 creates object
    rpc::shared_ptr<i_foo> shared = create_in_zone1();

    // Zone 3 gets optimistic pointer via Zone 2 (pass-through)
    auto opt = get_optimistic_ptr_in_zone3(shared);

    REQUIRE(opt.is_valid());
    REQUIRE_NOTHROW(opt->some_method());

    // Destroy in Zone 1
    shared.reset();

    // object_released() should route through pass-through to Zone 3
    REQUIRE_EVENTUALLY(!opt.is_valid());
}
```

### Test 5: Post with release_optimistic
```cpp
CORO_TYPED_TEST(optimistic_ptr_test, "release_optimistic") {
    // when optimistic count reaches zero

    // Create object with optimistic references
    auto obj = ...;
    add_optimistic_reference(obj);

    // Release optimistic reference using post
    // (should trigger post with release_optimistic option)
    release_optimistic_reference(obj);

    // Verify cleanup
    REQUIRE_EVENTUALLY(optimistic_count == 0);
}
```

## IDL Parser Support

### Current Status

**NOT SUPPORTED** - Search results show no mention of `optimistic_ptr` in IDL parser:
```bash
$ grep -r "optimistic_ptr" submodules/idlparser/
# No results
```

### Implementation Required

The IDL parser must support `rpc::optimistic_ptr<T>` similar to how it supports `rpc::shared_ptr<T>`.

### Where to Add Support

Based on finding `shared_ptr` in these files:
- `submodules/idlparser/parsers/ast_parser/coreclasses.cpp`
- `submodules/idlparser/parsers/ast_parser/library_loader.cpp`
- `submodules/idlparser/parsers/ast_parser/coreclasses.h`
- `submodules/idlparser/parsers/ast_parser/cpp_parser.cpp`

### Example IDL Usage (Target)

```idl
namespace example;

interface i_cache {
    // Methods can accept optimistic pointers
    void add_to_cache([in] rpc::optimistic_ptr<i_foo> item);

    // Methods can return optimistic pointers
    [out] rpc::optimistic_ptr<i_foo> get_cached_item(int id);
};

struct cached_data {
    // Structs can contain optimistic pointers
    rpc::optimistic_ptr<i_foo> cached_foo;
    int cache_time;
};
```

### Code Generation Requirements

When `rpc::optimistic_ptr<T>` is in IDL, the generator must:

1. **Parse the type**: Recognize `rpc::optimistic_ptr<...>` syntax
2. **Generate C++ code**: Use `rpc::optimistic_ptr<T>` in generated headers
3. **Marshalling**: Handle serialization (object_id, zone_id, interface_id)
4. **Unmarshalling**: Create optimistic_ptr on receiving side
5. **Reference tracking**: Register optimistic reference on remote object

### Marshalling Differences

**shared_ptr marshalling**:
```cpp
// Increments reference count on remote object
// Sends add_ref() message
// Object kept alive while shared_ptr exists
```

**optimistic_ptr marshalling**:
```cpp
// Does NOT increment reference count
// Just registers zone as optimistic holder
// Object can be destroyed anytime
// object_released() sent when destroyed
```

## Implementation Tasks

### Week 1: IDL Parser Support

**Day 1-2: Parser Changes**
- Study how `rpc::shared_ptr` is parsed
- Add `optimistic_ptr` recognition to lexer/parser
- Update AST classes if needed
- Add to type resolution system

**Day 3-4: Code Generator Changes**
- Update `synchronous_generator.cpp` to handle optimistic_ptr
- Generate correct marshalling/unmarshalling code
- Ensure no ref counting for optimistic pointers

**Day 5: Testing Parser/Generator**
- Create test IDL files with optimistic_ptr
- Verify generated code compiles
- Basic marshalling/unmarshalling tests

### Week 2: Runtime Integration

**Day 1-2: Optimistic Pointer Class Enhancement**
- Add `is_valid()` method (if missing)
- Add `mark_released()` for invalidation
- Thread-safety review
- Error handling improvements

**Day 2-3: Object Stub Tracking**
- Add optimistic holder tracking (see `object_released_Method.md`)
- Integrate with add_ref/release
- Implement object_released() sending

**Day 3-4: Object Proxy Handling**
- Implement optimistic proxy tracking in service
- Handle object_released() reception
- Implement proxy invalidation

**Day 5: Integration Testing**
- Test IDL-generated code with optimistic_ptr
- Test marshalling through all transport types
- Verify object_released() flow

### Week 3: Comprehensive Testing and Documentation

**Day 1-2: Unit Tests**
- All missing tests listed above
- Edge cases (nullptr, already invalid, etc.)
- Error handling

**Day 2-3: Integration Tests**
- Multi-zone scenarios
- Pass-through routing
- Transport failure during optimistic_ptr usage
- Mixing shared_ptr and optimistic_ptr

**Day 3-4: Performance Testing**
- Compare overhead: shared_ptr vs optimistic_ptr
- Measure object_released() latency
- Large-scale scenarios (many optimistic pointers)

**Day 5: Documentation**
- Update Canopy User Guide
- Add optimistic pointer best practices
- Document when to use shared vs optimistic
- API reference documentation
- IDL syntax documentation

## Acceptance Criteria

### IDL Parser
- ⏳ IDL parser recognizes `rpc::optimistic_ptr<T>` syntax
- ⏳ Code generator produces correct C++ code
- ⏳ Marshalling works correctly (no ref counting)
- ⏳ Unmarshalling creates valid optimistic pointers
- ⏳ Test IDL files compile and run

### Runtime
- ⏳ optimistic_ptr has `is_valid()` and `mark_released()` methods
- ⏳ Object stubs track optimistic holders
- ⏳ object_released() sent on object destruction
- ⏳ Optimistic pointers invalidated on object_released()
- ⏳ Access to invalid optimistic_ptr fails gracefully

### Testing
- ⏳ All unit tests passing
- ⏳ Integration tests with multiple zones
- ⏳ Pass-through routing works
- ⏳ Works with all transport types
- ⏳ Bi-modal support (sync and async)
- ⏳ Performance benchmarks show lower overhead than shared_ptr

### Documentation
- ⏳ User guide updated
- ⏳ IDL syntax documented
- ⏳ Best practices documented
- ⏳ API reference complete
- ⏳ Examples provided

## Related Documents

- `object_released_Method.md` - Critical for optimistic pointer invalidation
- `Post_Function_Implementation_And_Testing.md` 
- `IDL_Type_System_Formalization.md` - Future type system might affect this

## References

- Runtime class: `/rpc/include/rpc/types/optimistic_ptr.h` (verify path)
- Existing tests: `/tests/test_host/post_functionality_test_suite.cpp`
- IDL parser: `/submodules/idlparser/parsers/ast_parser/`
- Code generator: `/generator/src/synchronous_generator.cpp`
- Object stub: `/rpc/include/rpc/internal/stub.h`

## Use Case Examples

### 1. Caching

```cpp
// Good use of optimistic_ptr: cache entries
class cache_service {
    std::map<int, rpc::optimistic_ptr<i_data>> cache_;

    void add_to_cache(int key, rpc::shared_ptr<i_data> data) {
        cache_[key] = data;  // Convert to optimistic
    }

    rpc::shared_ptr<i_data> get_from_cache(int key) {
        auto it = cache_.find(key);
        if (it != cache_.end() && it->second.is_valid()) {
            return it->second;  // Convert back to shared
        }
        return nullptr;  // Cache miss or invalidated
    }
};
```

### 2. Event Listeners

```cpp
// Listeners don't need to keep objects alive
class event_dispatcher {
    std::vector<rpc::optimistic_ptr<i_listener>> listeners_;

    void notify_all(const Event& evt) {
        for (auto& listener : listeners_) {
            if (listener.is_valid()) {
                listener->on_event(evt);
            }
        }
        // Remove invalid listeners
        listeners_.erase(
            std::remove_if(listeners_.begin(), listeners_.end(),
                [](auto& l) { return !l.is_valid(); }),
            listeners_.end()
        );
    }
};
```

### 3. Weak References in Data Structures

```cpp
// Parent-child relationships where parent owns children
struct TreeNode {
    rpc::shared_ptr<i_tree_node> self;  // Strong
    rpc::optimistic_ptr<i_tree_node> parent;  // Weak, doesn't own parent
    std::vector<rpc::shared_ptr<i_tree_node>> children;  // Strong ownership
};
```

## Anti-Patterns (What NOT to do)

### ❌ Don't use for long-lived ownership

```cpp
// BAD: Using optimistic_ptr for long-term ownership
class UserSession {
    rpc::optimistic_ptr<i_user> user_;  // Could be destroyed anytime!

    void critical_operation() {
        user_->update_account();  // May crash if user was destroyed
    }
};

// GOOD: Use shared_ptr for ownership
class UserSession {
    rpc::shared_ptr<i_user> user_;  // Keeps user alive during session
};
```

### ❌ Don't ignore is_valid() checks

```cpp
// BAD: Not checking validity
auto opt = get_optimistic_ptr();
opt->some_method();  // May crash

// GOOD: Check before use
auto opt = get_optimistic_ptr();
if (opt.is_valid()) {
    opt->some_method();
} else {
    // Handle invalid case
}
```

## Performance Considerations

**Benefits of optimistic_ptr**:
- No add_ref() message when created
- No release() message when destroyed
- Lower network overhead
- Faster creation/destruction

**Costs of optimistic_ptr**:
- object_released() notifications when object destroyed
- Extra tracking in object stubs
- No guarantee object exists

**When to use**:
- Use `optimistic_ptr` for: caches, event listeners, weak references
- Use `shared_ptr` for: ownership, long-lived references, critical operations
