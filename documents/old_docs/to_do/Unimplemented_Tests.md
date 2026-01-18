<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Unimplemented Tests from Master Implementation Plan

This document lists test specifications from the Master Implementation Plan that may not yet be fully implemented.

## Milestone 1: Back-channel Support Tests

Most back-channel tests appear to be implemented implicitly through integration tests, but specific unit tests may be missing:

### Test 1.1: back_channel_entry structure
**Status**: Likely implemented as part of integration tests
**File**: Should be in unit test suite
```cpp
TEST(back_channel_test, "entry has correct fields") {
    back_channel_entry entry;
    entry.rpc_type = rpc_type::INTERFACE_PROXY;
    entry.zone_id = zone{1};
    entry.object_id = object{100};
    entry.interface_id = interface_ordinal{5};

    REQUIRE(entry.rpc_type == rpc_type::INTERFACE_PROXY);
    REQUIRE(entry.zone_id.get_val() == 1);
}
```

### Test 1.2: i_marshaller send() with back-channel
**Status**: Integration tests exist, specific unit test unclear

### Test 1.3: post() with back-channel
**Status**: Integration tests exist

---

## Milestone 2: post() Fire-and-Forget Tests

### Test 2.1: post() completes immediately
**Status**: ❓ UNKNOWN - May need explicit timing test
**Location**: Unclear if timing test exists

```cpp
CORO_TYPED_TEST(post_messaging_test, "post completes without waiting") {
    // GIVEN
    auto service_a = create_service("zone_a");
    auto service_b = create_service("zone_b");
    auto proxy_a_to_b = connect_zones(service_a, service_b);

    // WHEN
    auto start = std::chrono::steady_clock::now();
    CO_AWAIT proxy_a_to_b->post(
        VERSION_3, encoding::yas_binary, tag++,
        caller_zone{1}, destination_zone{2},
        object{100}, interface_ordinal{1}, method{20},
        in_size, in_buf, {});
    auto duration = std::chrono::steady_clock::now() - start;

    // THEN - should complete in microseconds (not wait for processing)
    REQUIRE(duration < std::chrono::milliseconds(10));
}
```

### Test 2.2: zone_terminating notification
**Status**: ⏳ PENDING (Task 2.4 not implemented)
**Location**: Not yet implemented

```cpp
CORO_TYPED_TEST(post_messaging_test, "zone_terminating broadcast") {
    // GIVEN
    auto service_a = create_service("zone_a");
    auto service_b = create_service("zone_b");
    auto service_c = create_service("zone_c");
    auto proxy_a_to_b = connect_zones(service_a, service_b);
    auto proxy_b_to_c = connect_zones(service_b, service_c);

    // WHEN - zone B terminates
    CO_AWAIT service_b->shutdown_and_broadcast_termination();

    // THEN - zones A and C receive termination notification
    REQUIRE(!proxy_a_to_b->is_operational());
    REQUIRE_EVENTUALLY(service_a->get_zone_status(zone{2}) == zone_status::terminated);
}
```

---

## Milestone 3: Transport Base Class Tests

### Test 3.1: Transport base class instantiation
**Status**: ✅ Likely implemented (local, TCP, SPSC transports exist)

### Test 3.2: Add and remove destinations
**Status**: ❓ UNKNOWN - May need explicit unit test

```cpp
TEST(transport_test, "add and remove destinations") {
    // Test add_destination and remove_destination
}
```

### Test 3.3: Message routing by destination
**Status**: ✅ Likely covered by integration tests

### Test 3.4: Transport status management
**Status**: ❓ UNKNOWN - May need explicit unit test

```cpp
TEST(transport_test, "status transitions") {
    // Test CONNECTING → CONNECTED → DISCONNECTED transitions
}
```

### Test 3.5: service_proxy refuses traffic when DISCONNECTED
**Status**: ❓ UNKNOWN - May need explicit test

---

## Milestone 4: Transport Status Monitoring Tests

### Test 4.1: Status is CONNECTED when active
**Status**: ❓ UNKNOWN

### Test 4.2: Status becomes DISCONNECTED after peer cancel
**Status**: ❓ UNKNOWN

### Test 4.3: clone_for_zone() refuses when DISCONNECTED
**Status**: ❓ UNKNOWN

### Test 4.4: RECONNECTING state handling
**Status**: ❓ UNKNOWN

---

## Milestone 5: Pass-Through Core Tests

### Test 5.1: Forward routing via transports
**Status**: ✅ Likely implemented (pass-through tests exist)

### Test 5.2: Reverse routing via transports
**Status**: ✅ Likely implemented

### Test 5.3: Reference counting management
**Status**: ✅ Likely implemented

### Test 5.4: Auto-delete on zero counts
**Status**: ❓ UNKNOWN - May need explicit test

### Test 5.5: Detect, send zone_terminating, and delete
**Status**: ⏳ PENDING (depends on Task 2.4)

---

## Milestone 6: Both-or-Neither Guarantee Tests

### Test 6.1: Both operational
**Status**: ✅ Likely implemented

### Test 6.2: Enforce symmetry on failure
**Status**: ❓ UNKNOWN - May need explicit test

```cpp
TEST(pass_through_test, "enforce symmetry on failure") {
    // Create pass-through with two transports
    // Fail one transport
    // Verify pass-through deletes itself
}
```

### Test 6.3: Refuse clone on non-operational
**Status**: ❓ UNKNOWN

---

## Milestone 7: Zone Termination Broadcast Tests

### Test 7.1: Graceful shutdown broadcast
**Status**: ⏳ PENDING (Task 2.4)

### Test 7.2: Forced failure detection
**Status**: ⏳ PENDING (Task 2.4)

### Test 7.3: Cascading termination
**Status**: ⏳ PENDING (Task 2.4)

---

## Milestone 8: Y-Topology Routing Tests

### Test 8.1: Bidirectional creation on connect
**Status**: ✅ Likely implemented

### Test 8.2: Y-topology with known_direction_zone
**Status**: ❓ UNKNOWN - May need explicit test

```cpp
TEST(y_topology_test, "route with known_direction_zone") {
    // Test Y-topology routing using known_direction_zone_id parameter
}
```

### Test 8.3: No reactive creation in send()
**Status**: ❓ UNKNOWN

---

## Milestone 9: SPSC Integration Tests

### Test 9.1: Destination registration
**Status**: ✅ SPSC tests exist

### Test 9.2: Operational state check
**Status**: ✅ SPSC tests exist

### Test 9.3: Integrated transport lifecycle
**Status**: ✅ SPSC tests exist

---

## Milestone 10: Full Integration Tests

### Test 10.1: End-to-end integration
**Status**: ✅ Many integration tests exist

### Test 10.2: Cascading failure
**Status**: ⏳ PENDING (depends on zone termination)

### Test 10.3: Bi-modal test suite
**Status**: ✅ Tests run in both modes

---

## Summary

### Confirmed Missing Tests
1. ⏳ Test 2.2: zone_terminating notification (depends on Task 2.4)
2. ⏳ Test 7.1-7.3: Zone termination broadcast tests (depends on Task 2.4)
3. ⏳ Test 10.2: Cascading failure (depends on zone termination)

### Tests Needing Verification
1. ❓ Test 2.1: post() timing test
2. ❓ Test 3.2: Add/remove destinations unit test
3. ❓ Test 3.4: Transport status management unit test
4. ❓ Test 3.5: service_proxy refuses DISCONNECTED traffic
5. ❓ Test 4.1-4.4: All transport status monitoring tests
6. ❓ Test 5.4: Auto-delete on zero counts
7. ❓ Test 6.2: Enforce symmetry on failure
8. ❓ Test 6.3: Refuse clone on non-operational
9. ❓ Test 8.2: Y-topology explicit test
10. ❓ Test 8.3: No reactive creation verification

### Recommendation

Create a systematic test audit by:
1. Running test coverage analysis
2. Mapping existing tests to plan specifications
3. Implementing missing unit tests
4. Verifying integration tests cover all scenarios
