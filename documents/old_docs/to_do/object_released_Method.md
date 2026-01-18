<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# object_released() Unidirectional Notification

**Status**: IMPLEMENTED AND FUNCTIONAL
**Priority**: MEDIUM (Testing and documentation improvements)
**Estimated Effort**: 2-3 days for comprehensive testing and documentation

## Current Status

- ✅ Method signature defined in `i_marshaller` interface
- ✅ Implementation complete in transport layer
- ✅ Zone tracking fully implemented in object_stub
- ✅ Notifications sent on object destruction
- ✅ Receiving zone handler implemented via service_event callback
- ⚠️ Comprehensive tests may be incomplete
- ⚠️ Documentation needs enhancement

## Interface Definition

From `rpc/include/rpc/internal/marshaller.h` (lines 168-174):

```cpp
// notify callers that an object has been released (for callers with optimistic ref counts only) unidirectional call
virtual CORO_TASK(void) object_released(uint64_t protocol_version,
    destination_zone destination_zone_id,
    object object_id,
    caller_zone caller_zone_id,
    const std::vector<rpc::back_channel_entry>& in_back_channel)
    = 0;
```

## Purpose

The `object_released()` method is a **unidirectional notification** (like `post()`) used to inform zones with optimistic references that an object has been destroyed in its home zone.

### Why It's Needed

**Optimistic pointers** (`rpc::optimistic_ptr<T>`) don't maintain reference counts on the remote object. This is more efficient but means:
- Remote zone doesn't know if optimistic pointers still exist
- When object is destroyed, optimistic pointers become dangling
- Need to notify holders of optimistic pointers so they can clean up

**Solution**: When an object with optimistic references is destroyed:
1. Home zone calls `object_released()` to all zones with optimistic pointers
2. Those zones mark their `optimistic_ptr<T>` instances as invalid
3. Subsequent access attempts fail gracefully

## Call Flow

```
Zone A (Home)                         Zone B (Optimistic holder)
─────────────────────────────────────────────────────────────────
rpc::shared_ptr<T> obj                rpc::optimistic_ptr<T> opt_ptr
                                                │
                                                │ (no ref count increment)
                                                │
obj.reset()  ───────────────────────────────────┤
  │                                             │
  └─> object_stub destructor                    │
        │                                       │
        └─> FOR EACH zone with optimistic refs  │
              │                                 │
              └─> object_released() ────────────┤
                    ║                           │
                    ║ (unidirectional)          │
                    ╚═══════════════════════════▶ Handle notification
                                                  │
                                                  └─> Mark opt_ptr invalid
```

## Actual Implementation

### 1. Object Stub Tracking (✅ IMPLEMENTED)

**Object stubs track zones with optimistic AND shared references** - from `rpc/include/rpc/internal/stub.h` (lines 42-44):

```cpp
class object_stub {
    std::atomic<uint64_t> shared_count_ = 0;
    std::atomic<uint64_t> optimistic_count_ = 0;

    // Track optimistic and shared references per zone for transport_down cleanup
    std::unordered_map<caller_zone, std::atomic<uint64_t>> optimistic_references_;
    std::unordered_map<caller_zone, std::atomic<uint64_t>> shared_references_;
    mutable std::mutex references_mutex_; // Protects both maps

    // Implementation in rpc/src/stub.cpp (lines 98-138)
    uint64_t add_ref(bool is_optimistic, caller_zone caller_zone_id);
    uint64_t release(bool is_optimistic, caller_zone caller_zone_id);
};
```

**Key points:**
- Uses atomic counters per zone for thread-safe tracking
- Tracks BOTH shared and optimistic references (important for `transport_down`)
- Mutex protects map modifications while atomics protect counts

### 2. Destruction Notification (✅ IMPLEMENTED)

**Notifications are sent during release, not in destructor** - from `rpc/src/service.cpp` (lines 750-809):

```cpp
// In service::release() when shared_count drops to zero:
if (!count && !(release_options::optimistic & options)) {
    // Collect all zones with optimistic references
    std::vector<caller_zone> optimistic_refs;
    {
        std::lock_guard<std::mutex> lock(stub->references_mutex_);
        optimistic_refs.reserve(stub->optimistic_references_.size());
        for (const auto& [zone, count_atomic] : stub->optimistic_references_) {
            uint64_t count_val = count_atomic.load(std::memory_order_acquire);
            if (count_val > 0) {
                optimistic_refs.push_back(zone);
            }
        }
    }

    // Remove stub from service maps
    {
        std::lock_guard l(stub_control_);
        stubs_.erase(object_id);
        wrapped_object_to_stub_.erase(pointer);
    }

    stub->reset();

    // IMPORTANT: Notify AFTER releasing mutex to avoid deadlock
    for (const auto& caller_zone_id : optimistic_refs) {
        auto transport = get_transport(caller_zone_id.as_destination());
        if (transport) {
            CO_AWAIT transport->object_released(protocol_version,
                zone_id_.as_destination(),
                object_id,
                caller_zone_id,
                {});
        }
    }
}
```

**Key design decisions:**
- Notifications sent during `release()`, not in `~object_stub()`
- Mutex released BEFORE calling `object_released()` to avoid deadlock
- Only notifies zones with non-zero optimistic reference counts
- Fire-and-forget coroutine (unidirectional)

### 3. Receiving Zone Handling (✅ IMPLEMENTED via Callback Pattern)

**Transport layer routes to service** - from `rpc/src/transport.cpp` (lines 767-796):

```cpp
CORO_TASK(void) transport::inbound_object_released(uint64_t protocol_version,
    destination_zone destination_zone_id,
    object object_id,
    caller_zone caller_zone_id,
    const std::vector<back_channel_entry>& in_back_channel)
{
    if (caller_zone_id == get_zone_id().as_caller()) {
        // Direct delivery to local service
        CO_AWAIT get_service()->object_released(protocol_version, destination_zone_id,
                                                 object_id, caller_zone_id, in_back_channel);
        CO_RETURN;
    }

    // Route through pass-through
    auto dest = get_destination_handler(caller_zone_id.as_destination(),
                                        destination_zone_id.as_caller());
    if (dest) {
        CO_AWAIT dest->object_released(protocol_version, destination_zone_id,
                                        object_id, caller_zone_id, in_back_channel);
    }
}
```

**Service layer uses callback pattern** - from `rpc/src/service.cpp` (lines 817-845):

```cpp
CORO_TASK(void) service::object_released(uint64_t protocol_version,
    destination_zone destination_zone_id,
    object object_id,
    caller_zone caller_zone_id,
    const std::vector<rpc::back_channel_entry>& in_back_channel)
{
    // Validate version
    if (protocol_version < LOWEST_SUPPORTED_VERSION ||
        protocol_version > HIGHEST_SUPPORTED_VERSION) {
        RPC_ERROR("Unsupported service version {} in object_released", protocol_version);
        CO_RETURN;
    }

    // Notify registered handlers via callback pattern
    CO_AWAIT notify_object_gone_event(object_id, destination_zone_id);
}

// From rpc/src/service.cpp (lines 1161-1174)
CORO_TASK(void) service::notify_object_gone_event(object object_id,
                                                    destination_zone destination) {
    if (!service_events_.empty()) {
        auto service_events_copy = service_events_;
        for (auto se : service_events_copy) {
            auto se_handler = se.lock();
            if (se_handler)
                CO_AWAIT se_handler->on_object_released(object_id, destination);
        }
    }
    CO_RETURN;
}
```

**Service event handler interface** - from `rpc/include/rpc/internal/service.h` (lines 45-50):

```cpp
class service_event {
public:
    virtual ~service_event() = default;
    virtual CORO_TASK(void) on_object_released(object object_id,
                                                 destination_zone destination) = 0;
};
```

**Key architecture:**
- Uses **observer pattern** with `service_event` handlers
- Handlers register with service via `service_events_` collection
- Each handler implements `on_object_released()` callback
- Allows multiple handlers per service (telemetry, proxy invalidation, etc.)

### 4. optimistic_ptr Implementation (✅ EXISTS - Complex Control Block Architecture)

**The `rpc::optimistic_ptr<T>` class exists** - from `rpc/include/rpc/internal/remote_pointer.h` (lines 1878-2066):

```cpp
template<typename T> class optimistic_ptr {
    using element_type_impl = std::remove_extent_t<T>;

    // For local proxies: local_proxy_holder_ holds the proxy, ptr_/cb_ are nullptr
    // For remote proxies: ptr_ points to interface_proxy, cb_ for refcounting
    element_type_impl* ptr_{nullptr};
    __rpc_internal::__shared_ptr_control_block::control_block_base* cb_{nullptr};
    std::shared_ptr<local_proxy<T>> local_proxy_holder_;

    void acquire_this() noexcept {
        if (cb_) {
            cb_->increment_optimistic_no_lock();
        }
    }

    void release_this() noexcept {
        if (cb_) {
            cb_->decrement_optimistic_and_dispose_if_zero();
        }
    }

public:
    // Copy/move constructors, operators, etc.
    // Uses control block for reference counting
    // No explicit "invalidation" - relies on weak semantics
};
```

**Key architecture differences from document:**
- `optimistic_ptr` uses **control block** reference counting, not validity flags
- Invalidation happens through **weak pointer semantics**, not explicit marking
- Control block manages both shared and optimistic counts
- When object is destroyed, control block remains valid for cleanup
- No explicit `mark_released()` or `is_valid()` methods - uses weak pointer pattern instead

**Actual invalidation mechanism:**
- When `object_released()` notification arrives at a zone
- Registered `service_event` handlers can implement cleanup logic
- `optimistic_ptr` instances become "expired" through control block semantics
- Attempting to access expired pointer returns error through normal weak pointer behavior

## Implementation Status Summary

### ✅ Fully Implemented
1. **Zone Tracking** - `object_stub` tracks per-zone optimistic and shared references
2. **Notification Sending** - `service::release()` sends `object_released()` to all zones with optimistic refs
3. **Transport Routing** - All transport types (local, TCP, SPSC) route `object_released()` correctly
4. **Pass-through Support** - `pass_through` forwards notifications to caller zones
5. **Receiving Handler** - `service::object_released()` triggers `service_event` callbacks
6. **Control Block Architecture** - `optimistic_ptr` uses sophisticated control block refcounting

### ⚠️ Needs Verification
1. **Service Event Handler Usage** - Need to verify if any code registers `service_event` handlers
2. **Optimistic Proxy Cleanup** - Need to verify proxy invalidation on `on_object_released()` callback
3. **Testing Coverage** - Need comprehensive tests for the notification flow
4. **Multi-zone Scenarios** - Need tests with optimistic pointers across multiple zone hierarchies

## Recommended Next Steps

Since the core implementation is complete, focus should be on verification, testing, and documentation.

### Phase 1: Verification (1-2 days)

**Task 1: Identify Service Event Handlers**
- Search codebase for classes implementing `service_event` interface
- Verify if any handlers implement `on_object_released()`
- Document expected behavior when object_released notification arrives

**Task 2: Verify Optimistic Pointer Lifecycle**
- Trace how `optimistic_ptr` instances are created and managed
- Verify control block cleanup when remote object is destroyed
- Document interaction between `object_released()` and control block expiration

### Phase 2: Comprehensive Testing (2-3 days)

**Test 1: Basic Notification Flow**
```cpp
CORO_TYPED_TEST(object_released_test, "basic_notification") {
    // Zone A creates object
    auto zone_a = create_zone(1);
    rpc::shared_ptr<i_test> obj = zone_a->create_object<test_impl>();

    // Zone B creates optimistic reference
    auto zone_b = create_zone(2);
    rpc::optimistic_ptr<i_test> opt_ptr;
    CO_AWAIT make_optimistic(obj, opt_ptr);

    // Verify zone A stub tracks zone B
    // Verify zone B has valid optimistic pointer

    // Zone A destroys object
    obj.reset();

    // Verify object_released() was sent to zone B
    // Verify service_event handlers were called
}
```

**Test 2: Multi-Zone Scenario**
```cpp
CORO_TYPED_TEST(object_released_test, "multiple_zones") {
    // Create object in zone A
    // Create optimistic pointers in zones B, C, D
    // Destroy object
    // Verify all zones received notification
}
```

**Test 3: Pass-through Routing**
```cpp
CORO_TYPED_TEST(object_released_test, "passthrough_routing") {
    // Zone A creates object
    // Zone C has optimistic pointer (via pass-through in zone B)
    // Destroy object in zone A
    // Verify notification routes through pass-through to zone C
}
```

**Test 4: Transport Failure Handling**
```cpp
CORO_TYPED_TEST(object_released_test, "transport_down_before_notification") {
    // Create object with optimistic refs
    // Disconnect transport
    // Destroy object
    // Verify graceful handling of unavailable transport
}
```

### Phase 3: Documentation (1 day)

**Update Canopy User Guide**
- Add section on `optimistic_ptr` lifecycle
- Document `object_released()` notification flow
- Explain `service_event` handler registration
- Add diagrams showing notification routing through pass-throughs

**Update API Documentation**
- Document `service_event::on_object_released()` callback
- Document control block behavior with optimistic pointers
- Add examples of safe optimistic pointer usage

**Create Architecture Document**
- Explain interaction between stub tracking, notifications, and control blocks
- Document deadlock avoidance (mutex release before notification)
- Explain pass-through routing logic for unidirectional calls

## Acceptance Criteria

### Core Implementation (✅ Complete)
- ✅ Object stubs track zones with optimistic references (per-zone atomic counters)
- ✅ Object destruction sends `object_released()` to all optimistic holders
- ✅ Transport layer routes notifications correctly (local, TCP, SPSC, pass-through)
- ✅ Service layer implements receiver handler with callback pattern
- ✅ Works with all transport types (Local, SPSC, TCP, pass-through)
- ✅ Bi-modal support (sync and async builds via coroutines)
- ✅ Telemetry integration (sends/receives logged)
- ✅ Deadlock avoidance (mutex released before sending notification)

### Testing & Verification (⚠️ Needs Work)
- ⏳ Comprehensive tests for notification flow
- ⏳ Integration tests with multiple zones holding optimistic pointers
- ⏳ Tests for pass-through routing of notifications
- ⏳ Tests for transport failure during notification
- ⏳ Verify `service_event` handler usage in codebase
- ⏳ Verify `optimistic_ptr` control block cleanup

### Documentation (⚠️ Needs Work)
- ⏳ Document `optimistic_ptr` lifecycle in User Guide
- ⏳ Document `service_event` handler registration pattern
- ⏳ Document `object_released()` notification architecture
- ⏳ Add diagrams showing notification flow through transports
- ⏳ Document control block interaction with notifications

## Related Documents

- `Post_Function_Implementation_And_Testing.md` - Similar unidirectional call pattern
- `transport_down_Method.md` - Another unidirectional notification with zone cleanup
- `Optimistic_Pointer_Support.md` - Overall optimistic pointer implementation details
- `Zone_Termination_Broadcast.md` - Zone-level cleanup vs object-level cleanup

## Key File References

### Interface & Core
- **Interface Definition**: `/rpc/include/rpc/internal/marshaller.h` (lines 168-174)
- **Object Stub Implementation**: `/rpc/include/rpc/internal/stub.h` (lines 32-86)
- **Stub Implementation**: `/rpc/src/stub.cpp` (lines 98-297)
- **Service Handler**: `/rpc/include/rpc/internal/service.h` (lines 45-50)

### Transport Layer
- **Base Transport**: `/rpc/src/transport.cpp` (lines 767-796 - `inbound_object_released`)
- **Local Transport**: `/transports/local/transport.cpp` (lines 219-240)
- **Pass-through**: `/rpc/src/pass_through.cpp` (lines 516-561)

### Service Layer
- **Service Implementation**: `/rpc/src/service.cpp`
  - Release with notification: lines 750-815
  - Handler: lines 817-845
  - Event notification: lines 1161-1174

### Optimistic Pointer
- **Optimistic Pointer**: `/rpc/include/rpc/internal/remote_pointer.h` (lines 1878-2066)
- **Control Block**: Same file (control block implementation)

## Resolved Implementation Questions

**Q: Is `object_released()` sent synchronously in destructor or queued?**
**A:** Sent during `service::release()` when shared count drops to zero, NOT in destructor. Uses `CO_AWAIT` so it's processed by coroutine scheduler in async builds, synchronous in sync builds.

**Q: What if transport is down when sending notification?**
**A:** Transport lookup returns nullptr, notification is silently skipped. No retry mechanism. This is acceptable as optimistic pointers don't guarantee object lifetime.

**Q: Race conditions with optimistic_ptr during invalidation?**
**A:** Control block architecture handles this. Multiple threads can safely access control block atomics during destruction. Weak pointer semantics prevent use-after-free.

**Q: Who owns the optimistic proxy tracking structures?**
**A:** `object_stub` owns `optimistic_references_` map. Protected by `references_mutex_`. Cleaned up when stub is destroyed. No external ownership needed.

**Q: Error handling - silent failure or log errors?**
**A:** Silent for missing transports (expected behavior). Errors logged for unexpected conditions (version mismatch, etc.). Unidirectional calls don't propagate errors back to caller.

## Remaining Open Questions

1. **Service Event Usage**: Are there any existing `service_event` implementations that handle `on_object_released()`? Need to search codebase.

2. **Optimistic Pointer Cleanup**: What's the expected behavior when an `optimistic_ptr` holder receives `object_released()` notification? Should there be a default handler that marks control blocks as invalid?

3. **Testing Coverage**: What existing tests (if any) exercise the `object_released()` notification flow? Need test audit.

4. **Performance Impact**: What's the overhead of per-zone reference tracking? Has this been profiled with many zones and objects?
