<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Transport Failure Detection and Zone Shutdown

**Status**: DESIGNED BUT NOT PROPERLY HANDLED
**Priority**: HIGH
**Estimated Effort**:

## Overview

This document covers two fundamentally different scenarios:
1. **Ungraceful Failure**: transport_down() notification when transport fails (requires zone_terminating)
2. **Graceful Shutdown**: Natural zone termination through reference counting (no notifications needed)

## Current Status

- ✅ `transport_down()` method signature defined in `i_marshaller` interface
- ✅ Reference counting mechanism exists for zone lifecycle
- ⚠️ Basic implementation may exist in transport classes
- ❌ Not properly integrated with failure detection
- ❌ Cascading cleanup not fully implemented
- ❌ Transport connection cleanup on zero ref count not documented
- ❌ Hierarchical transport parent-stays-alive logic not documented
- ❌ Testing incomplete or missing

---

## Part 1: Graceful Shutdown - Reference Counting

### How Graceful Shutdown Works

**Important**: Graceful shutdown requires **NO special notifications or broadcasts**.

When all client reference counts to a zone reach 0:
- It indicates no one is interested in that zone anymore
- The zone quietly goes away
- No `post()`, no `object_released()`, no broadcasts
- Completely natural and automatic

### Reference Counting Mechanism

```
Zone A                                      Zone B
──────────────────────────────────────────────────────
[Has objects]                               [Has proxies to Zone A objects]

                                            rpc::shared_ptr<obj1>  (ref count: 1)
                                            rpc::shared_ptr<obj2>  (ref count: 1)

[Reference count from B: 2]

                                            // Application releases references
                                            obj1.reset();  (ref count: 0)
                                            obj2.reset();  (ref count: 0)

[Reference count from B: 0]
    │
    └─> No more interest from Zone B
        No notifications sent
        Zone A can shut down if no other zones interested
```

### Transport-Level Connection Cleanup

**Key Insight**: Transport connection cleanup is a **transport-internal concern**, not a stack-wide event.

When a transport detects reference counts in **both directions** reach 0:
- Transport destroys the connection between the two zones
- This is handled internally by the transport
- Does NOT impact the entire RPC stack
- Does NOT require notifications to other zones
- Completely transparent cleanup

```cpp
// Example: Transport internal logic
class transport {
    uint64_t forward_ref_count_ = 0;  // Refs going A→B
    uint64_t reverse_ref_count_ = 0;  // Refs going B→A

    void check_connection_needed() {
        if (forward_ref_count_ == 0 && reverse_ref_count_ == 0) {
            // No references in either direction
            // This connection is no longer needed
            close_connection();
            // No notifications needed - this is internal to transport
        }
    }

    void on_release() {
        // Update reference counts
        update_ref_counts();

        // Check if connection still needed
        check_connection_needed();
    }
};
```

### Hierarchical Transport Special Case

**Important Exception**: Parent zones stay alive for child zones.

If a child zone is still wanted but its parent is not:
- Parent zone will stay alive for the benefit of the child
- Even if reference count from other zones is 0
- Parent serves as infrastructure for child zone
- Parent only terminates when child is also unwanted

```
Zone Hierarchy Example:

    Zone 1 (Root)
       │
       └─> Zone 2 (Parent)
              │
              └─> Zone 3 (Child)

Scenario: No one references Zone 2 directly, but Zone 3 is still in use

Result:
- Zone 2 stays alive (parent-stays-alive-for-child logic)
- Zone 2 reference count might be 0 from external zones
- But Zone 2 can't shut down while Zone 3 exists
- Zone 2 serves as transport infrastructure for Zone 3
```

**Implementation consideration**:
```cpp
class service {
    bool can_shutdown() const {
        // Can't shutdown if we have child zones
        if (has_active_child_zones()) {
            return false;  // Stay alive for children
        }

        // Can shutdown if no external references
        return (total_external_ref_count_ == 0);
    }
};
```

### No Special Notifications for Graceful Shutdown

**What happens during graceful shutdown**:
1. Application releases all `rpc::shared_ptr<T>` references
2. Reference counts go to 0
3. Zone detects no more interest
4. Zone shuts down quietly
5. Transports detect 0 ref counts and close connections
6. **No notifications sent**
7. **No broadcasts required**
8. **No coordination needed**

**What does NOT happen**:
- ❌ No `post(zone_terminating)` - this is ONLY for failures
- ❌ No `object_released()` broadcasts - this is only for optimistic pointers
- ❌ No special shutdown API calls
- ❌ No zone status updates to other zones
- ❌ No cleanup coordination

### object_released() Role

`object_released()` is **only** for optimistic pointers:
- Optimistic pointers do NOT contribute to reference counts
- When object deleted, `object_released()` notifies zones with optimistic pointers
- This is **separate** from graceful shutdown
- This happens **regardless** of whether zone is shutting down

**Example**:
```cpp
// Zone A has an object
auto obj = create_object();

// Zone B has optimistic pointer (no ref count increment)
rpc::optimistic_ptr<T> opt_ptr = get_optimistic_ptr(obj);

// Zone A deletes object
obj.reset();
  └─> object_released() sent to Zone B
  └─> Zone B invalidates opt_ptr

// This is NOT zone shutdown - just object lifecycle
// Zone A continues running with other objects
```

---

## Part 2: Ungraceful Failure - transport_down()

### When transport_down() is Used

**ONLY for ungraceful failures**:
- Process crash (SEGFAULT, kill -9)
- Network disconnect
- Remote machine power loss
- Container/VM termination
- SGX enclave crash
- Any unexpected transport failure

**NOT used for**:
- Graceful application shutdown
- Normal zone termination
- Reference count reaching 0
- Orderly object cleanup

### Purpose of transport_down()

When a transport fails **unexpectedly**, connected zones need immediate notification because:
- Can't rely on reference counting (broken connection)
- Can't send release() messages (transport failed)
- Need fast failure detection (not timeout-based)
- Need to invalidate proxies immediately
- Need cascading cleanup through pass-throughs


**When zone_terminating is used**:
- Transport fails and can't be recovered
- Zone crashes and can't send normal release() messages
- Need to notify connected zones immediately
- Sent via `zone_terminating` through alternative paths

**NOT used for graceful shutdown**.

### Interface Definition

From `rpc/include/rpc/internal/marshaller.h`:

```cpp
// notify callers that a transport is down
// unidirectional call
virtual CORO_TASK(void) transport_down(uint64_t protocol_version,
    destination_zone destination_zone_id,
    caller_zone caller_zone_id,
    const std::vector<rpc::back_channel_entry>& in_back_channel)
    = 0;
```

### Call Flow - Simple Two-Zone Failure

```
Zone A                                Zone B
────────────────────────────────────────────────
[Transport A→B connected]            [Transport B→A connected]
        │                                    │
        │  send()/receive() working          │
        │◄──────────────────────────────────►│
        │                                    │
    [CRASH - kill -9]                        │
        │                                    │
    [Can't send release()]                   │
    [Can't update ref counts normally]       │
        │                                    │
        └─> transport_down() ────────────────┤
              (or detect via heartbeat)      │
              ║                              │
              ║ (unidirectional)             │
              ╚══════════════════════════════▶ Handle notification
                                              │
                                              ├─> Mark all proxies to Zone A non-operational
                                              ├─> Cancel pending operations
                                              ├─> Release resources (forced cleanup)
                                              └─> Update zone status = DISCONNECTED
```

### Call Flow - Multi-Level Hierarchy Cascade

```
Zone 1                    Zone 2 (Pass-through)              Zone 3
────────────────────────────────────────────────────────────────────
[Connected via PT]              [Pass-through active]        [Connected]
        │                              │                          │
        │◄─────────────────────────────┼─────────────────────────►│
        │                              │                          │
                                   [CRASH]                        │
                                       │                          │
                                       ├─> transport_down() ──────┤
                                       │         ║                │
                                       │         ╚════════════════▶ Handle
                                       │                          │
                                       │                          ├─> Mark Zone 2 down
                                       │                          └─> Cleanup proxies
                                       │
                                       └─> transport_down() ───────────────┐
                                                 ║                         │
                                                 ╚═════════════════════════▶ Zone 1
                                                                            │
                                                                            ├─> Mark Zone 2 down
                                                                            ├─> Mark Zone 3 unreachable
                                                                            └─> Cleanup all proxies
```

### Implementation Requirements

#### 1. Transport Failure Detection

Transports must detect failures:

```cpp
class transport {
protected:
    std::atomic<transport_status> status_{transport_status::CONNECTING};
    std::weak_ptr<service> service_;

    // Heartbeat mechanism for early detection
    std::chrono::steady_clock::time_point last_heartbeat_;
    std::chrono::milliseconds heartbeat_interval_{5000};  // 5 seconds

    void handle_failure(const std::string& reason) {
        // Update status
        status_ = transport_status::DISCONNECTED;

        // Get service
        auto svc = service_.lock();
        if (!svc) return;

        // Try to notify remote zone (best effort)
        notify_remote_zone_of_failure();

        // Notify local service of transport failure
        svc->handle_transport_down(adjacent_zone_id_);
    }

    virtual void notify_remote_zone_of_failure() {
        // Try to send transport_down() or post(zone_terminating)
        // May not be possible if transport completely failed
        // This is best-effort notification
    }

    void check_heartbeat() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - last_heartbeat_;

        if (elapsed > heartbeat_interval_ * 2) {
            // No heartbeat for 2 intervals - consider failed
            handle_failure("heartbeat timeout");
        }
    }
};
```

#### 2. Service-Level Handling

When service receives `transport_down()` or detects failure:

```cpp
// In rpc::service class
CORO_TASK(void) handle_transport_down(zone failed_zone_id) {
    // 1. Mark all proxies to failed zone as non-operational
    mark_proxies_non_operational(failed_zone_id);

    // 2. Cancel pending operations to that zone
    cancel_pending_operations(failed_zone_id);

    // 3. Update zone status
    zone_statuses_[failed_zone_id] = zone_status::disconnected;

    // 4. Force cleanup resources (can't rely on normal release())
    force_cleanup_zone_resources(failed_zone_id);

    // 5. Check if this affects pass-throughs
    handle_passthrough_cascade(failed_zone_id);

    // 6. Telemetry logging
    log_zone_down(failed_zone_id);

    CO_RETURN;
}
```

#### 3. Pass-Through Cascade

Pass-throughs must propagate `transport_down()`:

```cpp
class pass_through : public i_marshaller {
    CORO_TASK(void) transport_down(
        uint64_t protocol_version,
        destination_zone destination_zone_id,
        caller_zone caller_zone_id,
        const std::vector<rpc::back_channel_entry>& in_back_channel) override
    {
        // Determine which direction failed
        bool forward_failed = (caller_zone_id == forward_destination_.zone);
        bool reverse_failed = (caller_zone_id == reverse_destination_.zone);

        // Cascade to opposite direction
        if (forward_failed) {
            CO_AWAIT reverse_transport_->transport_down(
                protocol_version,
                reverse_destination_,
                caller_zone_id,
                in_back_channel
            );
        } else if (reverse_failed) {
            CO_AWAIT forward_transport_->transport_down(
                protocol_version,
                forward_destination_,
                caller_zone_id,
                in_back_channel
            );
        }

        // Both-or-neither guarantee: delete self when one direction fails
        delete_self();

        CO_RETURN;
    }
};
```

#### 4. Proxy Invalidation

All proxies to the failed zone must be marked non-operational:

```cpp
class service_proxy {
    std::atomic<bool> is_operational_{true};

    void mark_non_operational() {
        is_operational_ = false;
    }

    bool check_operational() const {
        if (!is_operational_) {
            throw std::runtime_error("service_proxy: zone not operational");
        }
        return true;
    }
};

// All RPC calls should check operational status
CORO_TASK(int) service_proxy::send(...) {
    check_operational(); // Throws if zone is down
    // ... rest of send implementation
}
```

---

## Comparison: Graceful vs Ungraceful

| Aspect | Graceful Shutdown | Ungraceful Failure |
|--------|------------------|-------------------|
| **Trigger** | Reference count reaches 0 | Crash, disconnect, kill -9 |
| **Detection** | Automatic (ref counting) | Heartbeat timeout, connection failure |
| **Notification** | None (silent) | transport_down() or post(zone_terminating) |
| **Cleanup** | Normal release() messages | Forced cleanup (can't send release()) |
| **Transport** | Closes when ref count 0 both ways | Immediate failure, forced close |
| **Proxies** | Naturally become invalid | Force invalidate immediately |
| **Cascading** | Natural (ref count propagation) | Explicit (transport_down cascade) |
| **Speed** | Orderly, controlled | Immediate, emergency |
| **Zone status** | Just disappears | Marked DISCONNECTED |

---

## Implementation Tasks

### Week 1: Failure Detection

**Day 1-2: Heartbeat Mechanism**
- Add heartbeat to transport base class
- Implement periodic heartbeat sending
- Implement timeout detection
- Test with all transport types

**Day 3: Connection Monitoring**
- Add connection state monitoring
- Detect various failure scenarios
- Platform-specific failure detection (TCP, SPSC, Local)

**Day 4-5: Reference Count Based Cleanup**
- Document transport cleanup on zero ref count
- Implement both-directions zero detection
- Test connection cleanup
- Handle hierarchical transport parent-stays-alive logic

### Week 2: Cascading and Integration

**Day 1-2: Service Handling**
- Implement `handle_transport_down()` in service
- Add proxy invalidation logic (forced)
- Implement operation cancellation
- Add forced resource cleanup

**Day 2-3: Pass-Through Cascade**
- Implement cascade logic in pass_through
- Test both-or-neither cleanup
- Verify multi-level propagation

**Day 3-4: Parent-Stays-Alive Logic**
- Implement hierarchical transport special case
- Prevent parent shutdown while child active
- Test parent/child lifecycle
- Document edge cases

### Week 3: Testing and Hardening

**Day 1-2: Ungraceful Failure Tests**
```cpp
CORO_TYPED_TEST(transport_failure_test, "zone_crash") {
    auto zone_a = create_zone("A");
    auto zone_b = create_zone("B");
    connect_zones(zone_a, zone_b);

    // Simulate crash/kill -9
    simulate_zone_crash(zone_a);

    // Verify notification received
    REQUIRE_EVENTUALLY(zone_b->get_zone_status(zone_a) == zone_status::disconnected);

    // Verify proxies non-operational
    auto proxy = zone_b->get_proxy_to(zone_a);
    REQUIRE(!proxy->is_operational());

    // Verify fast failure (not timeout)
    auto start = std::chrono::steady_clock::now();
    REQUIRE_THROWS(proxy->some_method());
    auto duration = std::chrono::steady_clock::now() - start;
    REQUIRE(duration < std::chrono::seconds(1));
}
```

**Day 2-3: Graceful Shutdown Tests**
```cpp
CORO_TYPED_TEST(graceful_shutdown_test, "reference_count_zero") {
    auto zone_a = create_zone("A");
    auto zone_b = create_zone("B");

    // Zone B has shared pointers to Zone A objects
    auto obj1 = create_object_in_zone_a();
    auto obj2 = create_object_in_zone_a();

    rpc::shared_ptr<T> ref1 = obj1;  // Ref count: 1
    rpc::shared_ptr<T> ref2 = obj2;  // Ref count: 1

    // Zone B releases all references
    ref1.reset();  // Ref count: 0
    ref2.reset();  // Ref count: 0

    // Zone A can now shut down (no notifications sent)
    // Transport should detect zero ref count both ways
    // Connection should close automatically

    // Verify transport closed
    REQUIRE_EVENTUALLY(!zone_b->has_transport_to(zone_a));
}
```

**Day 3-4: Parent-Stays-Alive Tests**
```cpp
CORO_TYPED_TEST(hierarchical_test, "parent_stays_for_child") {
    auto zone_1 = create_zone("1");  // Root
    auto zone_2 = create_zone("2");  // Parent
    auto zone_3 = create_zone("3");  // Child of zone_2

    setup_hierarchy(zone_1, zone_2, zone_3);

    // Release all references to zone_2 from zone_1
    release_all_refs_to(zone_2);

    // Zone 2 should NOT shut down (child zone 3 still active)
    REQUIRE(zone_2->is_running());

    // Zone 2 should only shut down when zone 3 is gone
    shutdown_zone(zone_3);
    REQUIRE_EVENTUALLY(!zone_2->is_running());
}
```

**Day 5: Transport-Specific Tests**
- TCP disconnect scenarios
- SPSC channel failures
- Local transport shutdown
- WebSocket connection loss
- Heartbeat timeout scenarios

---

## Zone Status Tracking

Zones should track the status of other zones:

```cpp
enum class zone_status {
    unknown,        // Never connected
    connecting,     // Connection in progress
    connected,      // Active connection
    disconnected,   // Transport failed (ungraceful)
    // Note: No "gracefully_terminated" status - zone just disappears
};

class service {
    std::unordered_map<zone, zone_status> zone_statuses_;

    zone_status get_zone_status(zone zone_id) const;
    void update_zone_status(zone zone_id, zone_status status);
};
```

**Important**: There's no "terminated" or "gracefully_shutdown" status because:
- Graceful shutdown is silent (no notification)
- Zone just becomes unreachable
- Eventually detected when operations fail or timeout
- No need to track "gracefully terminated" vs "crashed"

---

## Special Considerations

### 1. Reference Counting is Per-Zone

Each transport tracks references in both directions:
```cpp
// Zone A ←→ Zone B
forward_ref_count_;   // A's references to B's objects
reverse_ref_count_;   // B's references to A's objects

// Connection closes when BOTH reach 0
if (forward_ref_count_ == 0 && reverse_ref_count_ == 0) {
    close_connection();
}
```

### 2. Hierarchical Transport Special Case

Parent zones serve as infrastructure:
```cpp
bool service::can_shutdown() const {
    // Can't shutdown if we have child zones
    if (has_active_child_zones()) {
        return false;  // Stay alive for children
    }

    // Can shutdown if no external references
    return (total_external_ref_count_ == 0);
}
```

### 3. Optimistic Pointers Don't Prevent Shutdown

Optimistic pointers don't contribute to reference counts:
- Zone can shut down even if optimistic pointers exist
- object_released() notifications sent when objects deleted
- But this is separate from zone shutdown

### 4. Failure Detection vs Reference Counting

**Graceful**: Reference counting determines lifecycle
**Ungraceful**: Failure detection triggers emergency cleanup

---

## Acceptance Criteria

### Graceful Shutdown
- ⏳ Reference counting determines zone lifecycle
- ⏳ Transport closes when ref count 0 in both directions
- ⏳ No notifications sent for graceful shutdown
- ⏳ Parent zones stay alive for child zones
- ⏳ Silent, automatic cleanup
- ⏳ Process documented and tested

### Ungraceful Failure Handling
- ⏳ Transport failure detection implemented (heartbeat, timeout)
- ⏳ transport_down() or post(zone_terminating) sent on failures
- ⏳ Service handles failures with forced cleanup
- ⏳ Proxies marked non-operational immediately
- ⏳ Pass-through cascades notifications correctly
- ⏳ Multi-level hierarchies cleanup properly
- ⏳ Both-or-neither guarantee triggers on failure

### Testing
- ⏳ Comprehensive tests for graceful shutdown
- ⏳ Comprehensive tests for ungraceful failures
- ⏳ Parent-stays-alive tests
- ⏳ Multi-level cascade tests
- ⏳ Works with all transport types
- ⏳ Bi-modal support (sync and async builds)
- ⏳ Performance tests (fast failure detection)

### Documentation
- ⏳ Documentation updated
- ⏳ Telemetry logging complete
- ⏳ Best practices documented
- ⏳ Reference counting behavior documented

---

## Related Documents

- `object_released_Method.md` - For optimistic pointer cleanup (separate from zone shutdown)
- `Post_Function_Implementation_And_Testing.md` - Unidirectional call mechanism

## References

- Interface: `/rpc/include/rpc/internal/marshaller.h` (line 177-181)
- Transport: `/rpc/include/rpc/internal/transport.h`
- Service: `/rpc/include/rpc/internal/service.h`
- Pass-through: `/rpc/include/rpc/internal/pass_through.h`

---

## Summary

### Graceful Shutdown
- **Mechanism**: Reference counting
- **When ref count reaches 0**: Zone quietly disappears
- **Notifications**: None
- **Transport cleanup**: Internal, automatic
- **Parent zones**: Stay alive for children

### Ungraceful Failure
- **Mechanism**: transport_down() or post(zone_terminating)
- **When transport fails**: Immediate notification
- **Notifications**: Explicit, cascading
- **Cleanup**: Forced, emergency
- **Fast failure detection**: Heartbeat, timeout

**Key Insight**: Graceful shutdown is reference-count driven and silent. Ungraceful failure requires explicit notifications because normal communication is impossible.
