<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# TODO: Outstanding Work Items

This folder contains outstanding work items extracted from the Master Implementation Plan v2. Items here represent work that has NOT been completed.

## Organization

- **to_do/**: Work items that need to be done (this folder)
- **done/**: Work items that have been completed (move here when finished)

## Current Outstanding Items

### Major Features

1. **[IDL_Type_System_Formalization.md](IDL_Type_System_Formalization.md)**
   - **Status**: PLANNED - HIGH IMPACT FOUNDATIONAL WORK
   - **Priority**: HIGH
   - **Effort**: 10-12 weeks
    - **Description**: Formalize the Canopy IDL type system with explicit type definitions (i8, i16, i32, i64, u8, u16, u32, u64, f32, f64)

2. **[DLL_Shared_Object_Transport.md](DLL_Shared_Object_Transport.md)**
   - **Status**: PLANNED, NOT YET STARTED
   - **Priority**: MEDIUM
   - **Effort**: 4-6 weeks
   - **Description**: Implement bi-modal hierarchical transport for loading zones from shared libraries

3. **[Enhanced_Enclave_Transport.md](Enhanced_Enclave_Transport.md)**
   - **Status**: PLANNED, ENHANCEMENT TO EXISTING SGX TRANSPORT
   - **Priority**: MEDIUM
   - **Effort**: 6-8 weeks
   - **Description**: Implement bi-modal hierarchical transport for secure enclave zones

4. **[WebSocket_REST_QUIC_Transport.md](WebSocket_REST_QUIC_Transport.md)**
   - **Status**: IN PROGRESS (WebSocket and REST partial, QUIC not started)
   - **Priority**: HIGH
   - **Effort**: 13-18 weeks total
   - **Description**: Implement modern web-based communication protocols

### Core RPC Methods - Implementation and Testing

5. **[Post_Function_Implementation_And_Testing.md](Post_Function_Implementation_And_Testing.md)**
   - **Status**: PARTIALLY IMPLEMENTED - Needs comprehensive testing and non-coroutine considerations
   - **Priority**: HIGH
   - **Effort**: 1-2 weeks
   - **Description**: Complete testing and documentation for post() fire-and-forget messaging
   - **Issues**: Documentation may be incorrect, non-coroutine behavior not fully tested

6. **[object_released_Method.md](object_released_Method.md)**
   - **Status**: DESIGNED BUT NOT PROPERLY HANDLED
   - **Priority**: HIGH
   - **Effort**: 1-2 weeks
   - **Description**: Implement unidirectional notification for optimistic pointer cleanup
   - **Missing**: Zone tracking, notification sending, optimistic pointer invalidation

7. **[Transport_Failure_And_Zone_Shutdown.md](Transport_Failure_And_Zone_Shutdown.md)**
   - **Status**: DESIGNED BUT NOT PROPERLY HANDLED
   - **Priority**: HIGH
   - **Effort**: 2-3 weeks
   - **Description**: Implement transport_down() for failure notification + graceful shutdown via object cleanup
   - **Missing**: Robust failure detection, cascading cleanup, proxy invalidation
   - **Key Insight**: Graceful shutdown happens through object deletion (object_released), NOT special broadcast

### Feature Completions

8. **[Optimistic_Pointer_Support.md](Optimistic_Pointer_Support.md)**
   - **Status**: PARTIALLY IMPLEMENTED - Needs IDL parser support and comprehensive testing
   - **Priority**: MEDIUM
   - **Effort**: 2-3 weeks
   - **Description**: Add rpc::optimistic_ptr support to IDL parser and comprehensive testing
   - **Missing**: IDL parser integration, comprehensive tests, object_released() integration

### Testing Gaps

9. **[Unimplemented_Tests.md](Unimplemented_Tests.md)**
   - **Status**: NEEDS AUDIT
   - **Description**: Tests from Master Implementation Plan that may not be fully implemented

## Dependency Graph

```
┌─────────────────────────────────────────────────────┐
│  Foundation (Must Complete First)                   │
├─────────────────────────────────────────────────────┤
│  1. Post_Function_Implementation_And_Testing.md     │
│     └─> All other methods depend on post()          │
│                                                      │
│  2. Transport_Failure_And_Zone_Shutdown.md          │
│     └─> Failure detection foundation                │
└─────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────┐
│  Tier 2 (Depends on Foundation)                     │
├─────────────────────────────────────────────────────┤
│  3. object_released_Method.md                       │
│     └─> Depends on: post() working                  │
│     └─> Needed for: Graceful shutdown               │
│                                                      │
│  4. Optimistic_Pointer_Support.md                   │
│     └─> Depends on: object_released()               │
└─────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────┐
│  Tier 3 (Can be parallelized)                       │
├─────────────────────────────────────────────────────┤
│  5. WebSocket_REST_QUIC_Transport.md                │
│  6. IDL_Type_System_Formalization.md                │
│  7. DLL_Shared_Object_Transport.md                  │
│  8. Enhanced_Enclave_Transport.md                   │
└─────────────────────────────────────────────────────┘
```

## Key Architectural Clarifications

### Graceful Shutdown Process

**Important**: There is NO special "zone termination broadcast" for graceful shutdown.

Instead, graceful shutdown happens naturally:
```
1. Application deletes all objects in zone
2. Each object deletion triggers object_released() to zones with optimistic pointers
3. When all objects deleted, zone has no more work
4. Zone can safely shut down
5. Transport closure (if any) may trigger transport_down() on remote side
```

### transport_down() vs Graceful Shutdown

| Mechanism | Use Case | Notification |
|-----------|----------|--------------|
| **transport_down()** | Ungraceful failure (crash, disconnect) | Immediate failure notification |
| **object_released()** | Graceful shutdown (orderly cleanup) | Per-object cleanup notification |

Both result in proper cleanup, but through different mechanisms.

## Recommended Implementation Order

### Phase 1: Core Methods (3-4 weeks)

**Priority: CRITICAL - These block other work**

1. **Week 1**: `Post_Function_Implementation_And_Testing.md`
   - Fix documentation
   - Add non-coroutine tests
   - Verify bi-modal behavior

2. **Week 2-3**: `Transport_Failure_And_Zone_Shutdown.md`
   - Implement failure detection (heartbeat, timeout)
   - Add transport_down() cascading cleanup
   - Document graceful shutdown process
   - Comprehensive testing

3. **Week 4**: `object_released_Method.md`
   - Implement zone tracking
   - Add notification sending/receiving
   - Test optimistic pointer invalidation
   - Verify graceful shutdown integration

### Phase 2: Feature Completion (2-3 weeks)

**Priority: HIGH - Completes existing features**

4. **Week 5-6**: `Optimistic_Pointer_Support.md`
   - Add IDL parser support
   - Implement code generation
   - Comprehensive testing

5. **Week 7**: `Unimplemented_Tests.md`
   - Test audit
   - Implement missing tests
   - Verification

### Phase 3: Transport Expansion (4-6 weeks)

**Priority: HIGH - Web integration**

6. **Week 8-11**: `WebSocket_REST_QUIC_Transport.md`
   - Complete WebSocket (2 weeks)
   - Complete REST (2 weeks)
   - Plan QUIC (ongoing)

### Phase 4: Advanced Features (20+ weeks)

**Priority: MEDIUM-LOW - Future enhancements**

7. **10-12 weeks**: `IDL_Type_System_Formalization.md`
8. **4-6 weeks**: `DLL_Shared_Object_Transport.md`
9. **6-8 weeks**: `Enhanced_Enclave_Transport.md`

## Completed Milestones (Reference Only)

The following milestones are **COMPLETE** and documented in Master_Implementation_Plan v2.md:

- ✅ Milestone 1: Back-channel Support
- ✅ Milestone 2: post() Fire-and-Forget (implementation complete, testing/docs pending)
- ✅ Milestone 3: Transport Base Class
- ✅ Milestone 4: Transport Status Monitoring
- ✅ Milestone 5: Pass-Through Core
- ✅ Milestone 6: Both-or-Neither Guarantee
- ✅ Milestone 7: Zone Termination (graceful via object cleanup, ungraceful via transport_down)
- ✅ Milestone 8: Y-Topology Routing
- ✅ Milestone 9: SPSC Integration
- 🚀 Milestone 10: Full Integration (IN PROGRESS)

## Critical Path Summary

**Blocking all other work:**
1. Post function testing/docs (1-2 weeks)
2. Transport failure detection and graceful shutdown (2-3 weeks)

**Required for feature completion:**
3. object_released implementation (1-2 weeks) - Critical for graceful shutdown
4. Optimistic pointer support (2-3 weeks)

**Can proceed in parallel after above:**
- WebSocket/REST completion
- IDL type system formalization
- New transport types

## How to Use This Folder

1. **When starting work on an item**:
   - Read the corresponding markdown file
   - Check dependencies in the graph above
   - Create a feature branch
   - Update the status in the markdown file

2. **When completing an item**:
   - Move the markdown file from `to_do/` to `done/`
   - Update the Master Implementation Plan v2.md
   - Document the completion in CLAUDE.md
   - Update this README to remove the item

3. **When adding new work items**:
   - Create a new markdown file in this folder
   - Follow the template format from existing files
   - Update this README with the new item
   - Update the dependency graph if needed

## Template for New Work Items

```markdown
# [Feature Name]

**Status**: [PLANNED/IN PROGRESS/BLOCKED]
**Priority**: [HIGH/MEDIUM/LOW]
**Estimated Effort**: [X weeks]

## Objective
[Brief description of what needs to be done]

## Current Status
[What's implemented, what's not]

## Requirements
[Detailed requirements]

## Implementation Tasks
[Step-by-step tasks with timeline]

## Dependencies
[What needs to be complete first]

## Acceptance Criteria
[How we know it's done]

## Related Documents
[Links to related work items]

## References
[Code locations, documentation]
```

## Quick Reference

### By Priority

**HIGH**:
- Post_Function_Implementation_And_Testing.md
- Transport_Failure_And_Zone_Shutdown.md
- object_released_Method.md
- WebSocket_REST_QUIC_Transport.md
- IDL_Type_System_Formalization.md

**MEDIUM**:
- Optimistic_Pointer_Support.md
- DLL_Shared_Object_Transport.md
- Enhanced_Enclave_Transport.md

### By Effort

**Short (1-2 weeks)**:
- Post_Function_Implementation_And_Testing.md (1-2 weeks)
- object_released_Method.md (1-2 weeks)

**Medium (2-4 weeks)**:
- Transport_Failure_And_Zone_Shutdown.md (2-3 weeks)
- Optimistic_Pointer_Support.md (2-3 weeks)

**Large (4+ weeks)**:
- DLL_Shared_Object_Transport.md (4-6 weeks)
- Enhanced_Enclave_Transport.md (6-8 weeks)
- IDL_Type_System_Formalization.md (10-12 weeks)
- WebSocket_REST_QUIC_Transport.md (13-18 weeks total)

### By Status

**Partially Implemented**:
- Post_Function_Implementation_And_Testing.md
- Optimistic_Pointer_Support.md
- WebSocket_REST_QUIC_Transport.md

**Designed But Not Handled**:
- object_released_Method.md
- Transport_Failure_And_Zone_Shutdown.md

**Planned**:
- IDL_Type_System_Formalization.md
- DLL_Shared_Object_Transport.md
- Enhanced_Enclave_Transport.md

## Total Outstanding Items: 9 documents

**Core Methods**: 3 documents (Post, object_released, Transport_Failure_And_Zone_Shutdown)
**Features**: 1 document (Optimistic_Pointer_Support)
**Transports**: 3 documents (WebSocket/REST/QUIC, DLL, Enhanced_Enclave)
**Foundation**: 1 document (IDL_Type_System_Formalization)
**Testing**: 1 document (Unimplemented_Tests)
