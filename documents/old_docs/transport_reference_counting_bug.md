<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Transport Reference Counting Bug Fix

## Problem Description

The `transport::inbound_add_ref` method has a critical bug where it doesn't properly increment transport reference counts or notify passthrough, causing incorrect internal state tracking.

### Failing Tests
- `add_ref_optimistic`: expects `add_ref_count` to be 1, getting 0
- `release_happy_path`: expects `release_count` to be 1, getting 0  
- `reference_count_balance`: expects `send_count` to be 2, getting 0

## Root Cause Analysis

### Current Logic Issues:
1. **Same-zone operations**: When `forward_transport->inbound_add_ref()` is called with `forward_dest` (same zone), it routes through service but doesn't increment transport's own `add_ref_count_`

2. **Missing Passthrough Notification**: Passthrough's `add_ref` method isn't being called when it should be for cross-zone operations

3. **Incorrect Reference Counting Logic**: Missing proper handling of `add_ref_options` scenarios

### Reference Counting Rules (From Analysis)

**Increment When:**
- **Local service operations**: `caller_or_destination == service_zone_id`
- **add_ref_options::normal** OR **add_ref_options::optimistic** 
- **XOR logic**: `build_caller_route` XOR `build_destination_route` (exactly one true)

**Don't Increment When:**
- **Back-channel only**: `build_caller_route && build_destination_route` (both true)
- Reference counts impact zones beyond this zone, not through this zone

### Release Counting Rules (Simpler)
- All releases go through passthrough if not involving local service
- No transformation of routing context

## Implementation Plan

### Phase 1: Fix Same-Zone Reference Counting
Add proper reference counting logic to `transport::inbound_add_ref` for same-zone operations based on `add_ref_options`.

### Phase 2: Add Passthrough Notification with Synchronization
Ensure passthrough's `add_ref` method is called when appropriate, with race condition protection.

### Phase 3: Apply Same Logic to `inbound_release`
Mirror reference counting logic for release operations for symmetry.

### Phase 4: Add Race Condition Protection
Thread-safe passthrough creation for same source/destination pairs.

### Phase 5: Testing and Validation
Ensure all failing tests pass and existing working tests remain fixed.

## Expected Test Results After Fix

- ✅ `add_ref_optimistic`: `add_ref_count` increments from 0→1
- ✅ `release_happy_path`: `release_count` increments from 0→1  
- ✅ `reference_count_balance`: `send_count` reaches expected value 2
- ✅ All existing working tests remain fixed

## Files to Modify
- `/var/home/edward/projects/rpc/rpc/src/transport.cpp` - Main implementation
- Potential synchronization improvements in transport headers

## Issue Tracking
Created as issue `rpc-y7n` in bd for comprehensive tracking.