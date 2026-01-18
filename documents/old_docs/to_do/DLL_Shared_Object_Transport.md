<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# DLL/Shared Object Transport

**Status**: 📋 PLANNED, NOT YET STARTED
**Priority**: MEDIUM - After object destruction notification fix
**Estimated Effort**:

## Objective

Implement bi-modal hierarchical transport for loading zones from shared libraries (DLLs/.so files)

## Requirements

### 1. Bi-Modal Support

**Synchronous Mode** (`CANOPY_BUILD_COROUTINE=OFF`):
- Behaves like local transport
- Direct function calls across DLL boundary
- No serialization overhead for in-process communication
- Blocking calls, immediate returns

**Asynchronous Mode** (`CANOPY_BUILD_COROUTINE=ON`):
- Option A: Share application's coroutine scheduler
- Option B: Create dedicated scheduler for DLL zone
- Async message passing with coroutine yields
- Non-blocking operations

### 2. Hierarchical Architecture

- Parent-child relationship (not peer-to-peer)
- Parent process loads DLL and creates child zone
- Similar to `rpc::local::parent_transport` and `rpc::local::child_transport` pattern
- Child zone lifetime bound to DLL lifetime

### 3. Transport Classes

```cpp
namespace dll {
    class parent_transport : public rpc::transport {
        // Parent → Child (loaded DLL)
        void* dll_handle_;  // DLL/SO handle

        // Sync mode: direct function calls
        // Async mode: message queue + scheduler option
    };

    class child_transport : public rpc::transport {
        // Child (DLL) → Parent

        // Sync mode: direct callbacks
        // Async mode: message queue to parent
    };
}
```

### 4. Scheduler Options for Async Mode

**Shared Scheduler**: Child zone shares parent's `coro::thread_pool`
- Lower overhead, single scheduler
- Simpler resource management
- Child operations scheduled on parent's threads

**Dedicated Scheduler**: Child zone has own `coro::thread_pool`
- Isolation between parent and child
- Independent thread pools
- Better for CPU-intensive child zones

### 5. Serialization Strategy

- **Sync Mode**: Optional (can pass raw pointers if in same address space)
- **Async Mode**: Required (message boundaries, scheduler safety)

## Use Cases

- Plugin architectures with hot-reloadable DLLs
- Sandboxed extension zones in same process
- Modular applications with dynamically loaded components

## Implementation Tasks

1. **Week 1-2**: Design DLL transport interface and loading mechanism
   - Define parent/child transport classes
   - Implement DLL loading/unloading (dlopen/LoadLibrary)
   - Design scheduler option selection API

2. **Week 2-3**: Implement synchronous mode
   - Direct function call mechanism
   - Symbol resolution for RPC entry points
   - Error handling and validation

3. **Week 3-4**: Implement asynchronous mode
   - Message queue implementation
   - Shared scheduler integration
   - Dedicated scheduler option

4. **Week 5**: Integration and testing
   - Create test DLLs with RPC interfaces
   - Test bi-modal behavior
   - Test hot-reloading scenarios

5. **Week 6**: Documentation and examples
   - Create example plugin architecture
   - Document best practices
   - Performance benchmarking

## Dependencies

- Local transport implementation (existing)
- Coroutine scheduler (libcoro)
- Platform-specific DLL APIs (dlopen/LoadLibrary)
