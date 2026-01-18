<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# post() Fire-and-Forget Messaging - Implementation and Testing

**Status**: PARTIALLY IMPLEMENTED - Needs comprehensive testing and non-coroutine considerations
**Priority**: HIGH
**Estimated Effort**:

## Current Status

- ✅ `post()` method signature defined in `i_marshaller` interface
- ✅ Basic implementation exists in transport classes (SPSC, TCP, Local)
- ✅ Some integration tests exist in `post_functionality_test_suite.cpp`
- ⚠️ Documentation may be incorrect - needs review
- ❌ Non-coroutine behavior not fully tested/documented
- ❌ Comprehensive unit testing incomplete

## Interface Definition

From `rpc/include/rpc/internal/marshaller.h`:

```cpp
// post a function call to a different zone and not expect a reply (unidirectional)
// in synchronous builds this may still block
virtual CORO_TASK(void) post(uint64_t protocol_version,
    encoding encoding,
    uint64_t tag,
    caller_zone caller_zone_id,
    destination_zone destination_zone_id,
    object object_id,
    interface_ordinal interface_id,
    method method_id,
    
    const rpc::span& in_data,
    const std::vector<rpc::back_channel_entry>& in_back_channel)
    = 0;
```

## Important Behavioral Notes

### Synchronous vs. Asynchronous Behavior

**Key Documentation Point** (from interface comment):
> "in synchronous builds this may still block"

This is critical - the `post()` function is:
- **Asynchronous mode** (`CANOPY_BUILD_COROUTINE=ON`): Truly non-blocking, returns immediately
- **Synchronous mode** (`CANOPY_BUILD_COROUTINE=OFF`): **MAY STILL BLOCK** despite being "fire-and-forget"

This needs to be clearly documented and tested.

### Bi-Modal Implementation Requirements

1. **Async mode**:
   - Should return immediately without waiting for remote execution
   - Message queued for async delivery
   - No response expected or handled

2. **Sync mode**:
   - May block while message is delivered
   - Still no response expected or handled
   - Simpler implementation but not truly "non-blocking"

## Existing Tests

Located in `/tests/test_host/post_functionality_test_suite.cpp`:

1. ✅ `basic_post_normal` - Basic post with normal option
2. ✅ `post_with_zone_terminating` - Post with zone termination option
3. ✅ `post_with_release_optimistic` - Post with optimistic release option
4. ✅ `concurrent_post_operations` - Multiple concurrent posts
5. ✅ `post_with_different_data_sizes` - Various data sizes
6. ✅ `post_does_not_interfere_with_regular_calls` - Interaction with send()
7. ✅ `post_with_optimistic_ptr` - Post with optimistic pointers

These tests appear to be integration tests. Additional unit tests may be needed.

## Missing Tests

### Test: post() timing in async mode
```cpp
#ifdef CANOPY_BUILD_COROUTINE
CORO_TYPED_TEST(post_timing_test, "post returns immediately in async mode") {
    auto& lib = this->get_lib();
    auto example = lib.get_example();

    auto start = std::chrono::steady_clock::now();
    CO_AWAIT example->some_post_method(...);
    auto duration = std::chrono::steady_clock::now() - start;

    // Should return in microseconds, not wait for remote execution
    REQUIRE(duration < std::chrono::milliseconds(10));
}
#endif
```

### Test: post() blocking behavior in sync mode
```cpp
#ifndef CANOPY_BUILD_COROUTINE
TEST(post_blocking_test, "post may block in sync mode") {
    auto& lib = this->get_lib();
    auto example = lib.get_example();

    // Document that post() can block in sync mode
    // This test just verifies the behavior is documented
    auto start = std::chrono::steady_clock::now();
    example->some_post_method(...);
    auto duration = std::chrono::steady_clock::now() - start;

    // In sync mode, this MAY take longer
    // Just document the behavior
    std::cout << "Sync mode post() duration: " << duration.count() << "ms\n";
}
#endif
```

### Test: No response handling
```cpp
CORO_TYPED_TEST(post_no_response_test, "post does not expect response") {
    // Verify that post() never waits for or processes a response
    // Even if remote stub sends data, it should be ignored
}
```

## Documentation Issues to Fix

### Current Documentation Problems

1. **Interface comment** says "may still block" but doesn't explain:
   - Under what conditions it blocks
   - Whether this is only in sync mode
   - Performance implications

2. **Missing guidance** on when to use `post()` vs `send()`:
   - What are the trade-offs?
   - When is blocking in sync mode acceptable?
   - How to handle errors (no return value)?

### Recommended Documentation Updates

Update `marshaller.h` comment to:

```cpp
/**
 * Post a function call to a different zone without expecting a reply (fire-and-forget).
 *
 * Behavioral differences:
 * - Asynchronous mode (CANOPY_BUILD_COROUTINE=ON):
 *   Returns immediately. Message queued for async delivery.
 *   Remote execution happens asynchronously.
 *
 * - Synchronous mode (CANOPY_BUILD_COROUTINE=OFF):
 *   May block while message is delivered to remote zone.
 *   No response is expected or processed.
 *   Simpler than send() but not truly non-blocking.
 *
 * Use cases:
 * - Notifications where response is not needed
 * - Logging and telemetry
 * - Zone termination broadcasts 
 * - Optimistic reference cleanup 
 *
 * Error handling:
 * - Returns void - no error code available
 * - Delivery failures are silent (by design)
 * - Use send() if you need error confirmation
 */
virtual CORO_TASK(void) post(...) = 0;
```

These options enable special handling in the receiving zone.

## Implementation Tasks

### Week 1: Testing and Documentation

1. **Day 1-2**: Review and fix documentation
   - Update `marshaller.h` interface comments
   - Document sync vs async behavior clearly
    - Add usage guidelines to Canopy User Guide

2. **Day 3-4**: Add missing unit tests
   - Timing tests for async mode
   - Blocking behavior tests for sync mode
   - No-response verification tests
   - Error case handling

3. **Day 5**: Integration testing
   - Test with all transport types (Local, SPSC, TCP, WebSocket, REST)
   - Verify bi-modal behavior
   - Performance benchmarking

### Week 2: Verification and Refinement (if needed)

1. **Code review** of existing implementations
2. **Performance optimization** if needed
3. **Additional edge case testing**
4. **Documentation review and updates**

## Acceptance Criteria

- ✅ post() implemented for all transport types
- ⏳ Documentation accurate and comprehensive
- ⏳ Bi-modal behavior clearly documented
- ⏳ Timing tests verify async behavior
- ⏳ Sync mode blocking behavior documented
- ⏳ No-response handling verified

## Related Documents

- `object_released_Method.md` - Unidirectional object cleanup notification
- `transport_down_Method.md` - Unidirectional transport failure notification
- `Zone_Termination_Broadcast.md` - Uses post() with zone_terminating option
- `Optimistic_Pointer_Support.md` - Uses post() with release_optimistic option

## References

- Interface: `/rpc/include/rpc/internal/marshaller.h`
- Tests: `/tests/test_host/post_functionality_test_suite.cpp`
- User Guide: `/docs/Canopy_User_Guide.md`
