<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Introduction to Canopy

Canopy is a modern C++ Remote Procedure Call library that enables type-safe communication across different execution contexts. It provides a unified programming model for in-process calls, inter-process communication, remote machines, embedded devices, and secure Intel SGX enclaves.

## What is Canopy?

Canopy bridges the gap between local C++ objects and distributed systems by providing:

- **Type Safety**: Full C++ type system integration with compile-time verification through IDL-generated code
- **Transport Independence**: A single API works across multiple transport mechanisms
- **Format Flexibility**: Support for JSON, binary (YAS), and Protocol Buffers serialization
- **Bi-Modal Execution**: Same code runs in both blocking and coroutine modes
- **Secure Enclaves**: Native support for Intel SGX secure computation

## Key Features

### Type-Safe Interface Definitions

Define interfaces using a C++-like Interface Definition Language (IDL):

```idl
namespace calculator
{
    interface i_calculator
    {
        error_code add(int a, int b, [out] int& result);
        error_code multiply(int a, int b, [out] int& result);
    };
}
```

The IDL compiler generates:
- Pure virtual interface classes
- Proxy implementations for clients
- Stub implementations for servers
- Serialization code for all parameters
- JSON schema metadata for introspection

### Transport Agnostic Design

Canopy abstracts transport details behind a consistent interface:

| Transport | Use Case | Requirements |
|-----------|----------|--------------|
| Local | In-process communication | None |
| TCP | Network communication | CANOPY_BUILD_COROUTINE=ON |
| SPSC | High-performance IPC | CANOPY_BUILD_COROUTINE=ON |
| SGX Enclave | Secure computation | CANOPY_BUILD_ENCLAVE=ON |

### Bi-Modal Execution

Write code once, deploy in blocking or coroutine mode:

```cpp
CORO_TASK(error_code) calculate(int a, int b, int& result)
{
    auto error = CO_AWAIT calculator_->add(a, b, result);
    CO_RETURN error;
}
```

- **Blocking Mode**: `CORO_TASK(int)` resolves to `int`, `CO_AWAIT` is no-op
- **Coroutine Mode**: `CORO_TASK(int)` becomes `coro::task<int>`, `CO_AWAIT` suspends execution

## Architecture Overview

Canopy is built around three core concepts:

### Zones

A zone represents an execution context with its own:
- Zone ID for identification
- Service instance
- Transport connections

Zones can form hierarchies (parent/child relationships) for complex distributed systems.

### Services

A service manages the lifecycle of objects within a zone:
- Object registration and lookup
- Reference counting
- Transport management
- Zone identity

### Transports

Transports provide the communication channel between zones:
- Local transport for in-process communication
- TCP transport for network communication
- SPSC transport for lock-free IPC
- SGX transport for secure enclaves

## When to Use Canopy

Canopy is ideal for:

1. **Microservices in C++**: Build distributed systems with type safety
2. **Embedded Systems**: Efficient IPC with minimal overhead
3. **Secure Computation**: Intel SGX enclave support for sensitive operations
4. **Cross-Process Communication**: Seamless object passing between processes
5. **Plugin Architectures**: Load/unload plugins with automatic cleanup

## Design Philosophy

Canopy follows several key design principles:

1. **Type Safety First**: Compile-time verification of all RPC calls
2. **No Bridging Policy**: Never mix `rpc::shared_ptr` with `std::shared_ptr`
3. **RAII Throughout**: Resource management via constructors/destructors
4. **Coroutines First**: API designed for async/await patterns
5. **Transparent Marshalling**: Parameters pass like local objects

## Project Structure

```
rpc/
├── include/rpc/              # Public headers
│   └── internal/             # Internal implementation
├── src/                      # Core library implementation
├── interfaces/               # Base IDL interfaces (rpc_types.idl)
└── ...

transports/                   # Transport implementations
├── local/                    # In-process transport
├── tcp/                      # Network transport
├── spsc/                     # SPSC queue transport
├── sgx/                      # SGX enclave transport
└── ...

generator/                    # IDL code generator
tests/                        # Test suite
demos/                        # Demo applications
telemetry/                    # Telemetry services
```

## Version and Requirements

- **Version**: 2.2.0
- **C++ Standard**: C++17 (C++20 when CANOPY_BUILD_COROUTINE=ON)
- **CMake**: 3.24+
- **Compilers**: Clang 10+, GCC 9.4+, MSVC 2019+

## Next Steps

1. [Architecture Overview](architecture/01-overview.md) - Understand the fundamental building blocks
2. [IDL Guide](03-idl-guide.md) - Learn to define interfaces
3. [Getting Started](02-getting-started.md) - Follow a tutorial
4. [Building Canopy](04-building.md) - Set up your build environment
