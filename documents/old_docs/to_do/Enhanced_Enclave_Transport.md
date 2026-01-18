<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Enhanced Enclave Transport (SGX/TrustZone)

**Status**: 📋 PLANNED, ENHANCEMENT TO EXISTING SGX TRANSPORT
**Priority**: MEDIUM - After DLL transport
**Estimated Effort**:

## Objective

Implement bi-modal hierarchical transport for secure enclave zones (SGX, TrustZone, etc.)

## Requirements

### 1. Bi-Modal Support

**Synchronous Mode** (`CANOPY_BUILD_COROUTINE=OFF`):
- Behaves like local transport with enclave boundary crossing
- Synchronous ECALL/OCALL semantics
- Blocking enclave transitions
- Minimal scheduling overhead

**Asynchronous Mode** (`CANOPY_BUILD_COROUTINE=ON`):
- Async ECALL/OCALL with coroutine suspension
- Non-blocking enclave transitions
- Scheduler options (shared or dedicated)
- Efficient for I/O-heavy enclave workloads

### 2. Hierarchical Architecture

- Parent (untrusted) ↔ Child (enclave/trusted) relationship
- Similar to local transport pattern
- Enclave lifetime managed by parent zone

### 3. Transport Classes

```cpp
namespace enclave {
    class parent_transport : public rpc::transport {
        // Untrusted → Enclave (ECALL)
        sgx_enclave_id_t enclave_id_;

        // Sync mode: blocking ECALL
        // Async mode: async ECALL with scheduler
    };

    class child_transport : public rpc::transport {
        // Enclave → Untrusted (OCALL)

        // Sync mode: blocking OCALL
        // Async mode: async OCALL
    };
}
```

### 4. Scheduler Options for Async Mode

**Shared Scheduler**: Enclave shares untrusted zone's scheduler
- Requires scheduler state in untrusted memory (security consideration)
- Lower overhead

**Dedicated Scheduler**: Enclave has own scheduler
- Scheduler state can be in enclave memory (more secure)
- Better isolation
- Recommended for security-critical workloads

### 5. Security Considerations

- All data crossing enclave boundary must be serialized (attestation, integrity)
- No raw pointer passing across boundary
- Careful scheduler state management (trust boundary)
- Memory allocation strategies (sealed vs. unsealed)

### 6. Platform Support

- Intel SGX (current implementation exists, needs bi-modal enhancement)
- ARM TrustZone (future)
- Other TEE implementations

## Differences from Current SGX Transport

- **Current**: Likely synchronous-only
- **Enhanced**: Full bi-modal support with scheduler options
- **Enhanced**: Better integration with transport base class
- **Enhanced**: Pass-through support for multi-level enclave hierarchies

## Use Cases

- Secure computation in enclaves with RPC communication
- Trusted execution environments with async I/O
- Multi-level enclave hierarchies (enclave calling another enclave via untrusted zone)

## Implementation Tasks

1. **Week 1-2**: Analyze current SGX transport implementation
   - Document existing architecture
   - Identify bi-modal migration points
   - Design scheduler integration strategy

2. **Week 3-4**: Implement bi-modal ECALL/OCALL
   - Sync mode with blocking transitions
   - Async mode with coroutine suspension
   - Test both modes with existing SGX tests

3. **Week 4-5**: Scheduler integration
   - Shared scheduler option
   - Dedicated scheduler option
   - Security analysis and hardening

4. **Week 5-6**: Multi-level hierarchy support
   - Pass-through integration
   - Nested enclave scenarios
   - Complex topology testing

5. **Week 7**: ARM TrustZone support (optional)
   - Platform abstraction layer
   - TrustZone-specific implementation
   - Cross-platform testing

6. **Week 8**: Documentation and security review
   - Security guidelines
   - Performance benchmarking
   - Threat model documentation

## Dependencies

- Existing SGX transport implementation
- DLL transport (for similar pattern reference)
- Intel SGX SDK or ARM TrustZone SDK
- Security review and audit tools
