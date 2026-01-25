<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Getting Started

This tutorial guides you through creating your first Canopy application, from defining interfaces to making remote procedure calls.

## Prerequisites

- completed [Building Canopy](05-building.md)
- Understanding of [Core Concepts](02-core-concepts.md)
- Basic knowledge of C++17

## 1. Create the Project Structure

```
my_rpc_app/
├── CMakeLists.txt
├── idl/
│   └── calculator.idl
├── include/
│   └── calculator_impl.h
└── src/
    ├── calculator_impl.cpp
    └── main.cpp
```

## 2. Define the IDL Interface

Create `idl/calculator.idl`:

```idl
namespace calculator
{
    [status=production, description="Simple calculator service"]
    interface i_calculator
    {
        [description="Adds two integers"]
        error_code add(int a, int b, [out] int& result);

        [description="Subtracts two integers"]
        error_code subtract(int a, int b, [out] int& result);

        [description="Multiplies two integers"]
        error_code multiply(int a, int b, [out] int& result);

        [description="Divides two integers"]
        error_code divide(int a, int b, [out] int& result);
    };
}
```

## 3. CMake Configuration

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_rpc_app VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find or build Canopy
# For simplicity, we assume Canopy is in the parent directory
add_subdirectory(../rpc build/rpc)

# Generate code from IDL
CanopyGenerate(
    calculator
    ${CMAKE_CURRENT_SOURCE_DIR}/idl/calculator.idl
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
    ""
    yas_binary
    yas_json

    dependencies rpc_types_idl
    include_paths ${CMAKE_CURRENT_SOURCE_DIR}/idl
)

# Create executable
add_executable(my_rpc_app
    src/main.cpp
    src/calculator_impl.cpp
)

target_link_libraries(my_rpc_app PRIVATE
    rpc_host
    calculator_idl
)

target_include_directories(my_rpc_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated/include
)
```

## 4. Implement the Interface

Create `include/calculator_impl.h`:

```cpp
#pragma once

#include <calculator/calculator.h>
#include <rpc/rpc.h>

namespace calculator
{

class calculator_impl : public v1::i_calculator
{
public:
    calculator_impl() = default;

    // Required overrides
    void* get_address() const override
    {
        return const_cast<calculator_impl*>(this);
    }

    const rpc::casting_interface* query_interface(
        rpc::interface_ordinal interface_id) const override
    {
        if (rpc::match<v1::i_calculator>(interface_id))
        {
            return static_cast<const v1::i_calculator*>(this);
        }
        return nullptr;
    }

    // Interface methods
    CORO_TASK(error_code) add(int a, int b, int& result) override
    {
        result = a + b;
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(error_code) subtract(int a, int b, int& result) override
    {
        result = a - b;
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(error_code) multiply(int a, int b, int& result) override
    {
        result = a * b;
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(error_code) divide(int a, int b, int& result) override
    {
        if (b == 0)
        {
            CO_RETURN rpc::error::INVALID_DATA();
        }
        result = a / b;
        CO_RETURN rpc::error::OK();
    }
};

// Factory function
inline rpc::shared_ptr<v1::i_calculator> create_calculator_instance()
{
    return rpc::make_shared<calculator_impl>();
}

} // namespace calculator
```

## 5. Implement Main Program

Create `src/main.cpp`:

```cpp
#include "calculator_impl.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#ifdef CANOPY_BUILD_COROUTINE
#include <coro/io_scheduler.hpp>
#endif

using namespace calculator;

int main()
{
    std::cout << "Starting Canopy Calculator Demo\n";

    // Create root service
    std::atomic<uint64_t> zone_gen = 0;
    auto root_service = std::make_shared<rpc::service>(
        "root",
        rpc::zone{++zone_gen});

    // Create calculator instance
    auto calculator = create_calculator_instance();

    std::cout << "Service created with zone ID: " << root_service->get_zone_id().get_val() << "\n";

    // Make local calls (same zone)
    int result;

    auto error = calculator->add(10, 5, result);
    if (error == rpc::error::OK())
    {
        std::cout << "10 + 5 = " << result << "\n";
    }

    error = calculator->multiply(3, 4, result);
    if (error == rpc::error::OK())
    {
        std::cout << "3 * 4 = " << result << "\n";
    }

    // Test divide by zero error handling
    error = calculator->divide(10, 0, result);
    if (error != rpc::error::OK())
    {
        std::cout << "Division by zero correctly returned error: " << static_cast<int>(error) << "\n";
    }

    std::cout << "Demo completed successfully!\n";
    return 0;
}
```

## 6. Build and Run

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake --preset Debug -DCMAKE_BUILD_TYPE=Debug ../my_rpc_app

# Build
cmake --build . --parallel $(nproc)

# Run
./my_rpc_app
```

**Expected Output**:
```
Starting Canopy Calculator Demo
Service created with zone ID: 1
10 + 5 = 15
3 * 4 = 12
Division by zero correctly returned error: 4
Demo completed successfully!
```

## 7. Tutorial: Cross-Zone Communication

This tutorial extends the calculator to support two zones communicating via the local transport.

### Client-Server Architecture

```
┌─────────────────────┐         ┌─────────────────────┐
│      Client Zone    │         │     Server Zone     │
│                     │  local  │                     │
│  client_service     │◄───────►│  server_service     │
│         │           │ transport│         │          │
│         ▼           │         │         ▼          │
│  client_proxy       │         │  calculator_impl   │
└─────────────────────┘         └─────────────────────┘
```

### Updated Main Program

```cpp
#include "calculator_impl.h"
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>

#ifdef CANOPY_BUILD_COROUTINE
#include <coro/io_scheduler.hpp>
#endif

using namespace calculator;

int main()
{
    std::cout << "Starting Cross-Zone Calculator Demo\n";

    std::atomic<uint64_t> zone_gen = 0;

    // === SERVER SIDE ===

    // Create server service
    auto server_service = std::make_shared<rpc::service>(
        "server",
        rpc::zone{++zone_gen});

    // Register calculator implementation
    auto calculator = create_calculator_instance();

    std::cout << "Server zone ID: " << server_service->get_zone_id().get_val() << "\n";

    // === CLIENT SIDE ===

    // Create client service
    auto client_service = std::make_shared<rpc::service>(
        "client",
        rpc::zone{++zone_gen});

    std::cout << "Client zone ID: " << client_service->get_zone_id().get_val() << "\n";

    // Create child transport connecting client to server
    auto child_transport = std::make_shared<rpc::local::child_transport>(
        "server",
        server_service,
        client_service->get_zone_id(),
        rpc::local::parent_transport::bind<v1::i_calculator>(
            client_service->get_zone_id(),
            [&](const rpc::shared_ptr<v1::i_calculator>& /* host */,
                rpc::shared_ptr<v1::i_calculator>& calculator_out,
                const std::shared_ptr<rpc::child_service>& child_service) -> CORO_TASK(int)
            {
                calculator_out = calculator;
                CO_RETURN rpc::error::OK();
            }));

    // Connect client to server
    rpc::shared_ptr<v1::i_calculator> remote_calculator;
    auto error = CO_AWAIT client_service->connect_to_zone(
        "server", child_transport, remote_calculator);

    if (error != rpc::error::OK())
    {
        std::cerr << "Failed to connect: " << static_cast<int>(error) << "\n";
        return 1;
    }

    std::cout << "Connected to server!\n";

    // === MAKE REMOTE CALLS ===

    int result;

    // Call add on server
    error = CO_AWAIT remote_calculator->add(100, 200, result);
    if (error == rpc::error::OK())
    {
        std::cout << "100 + 200 = " << result << " (remote call)\n";
    }

    // Call multiply on server
    error = CO_AWAIT remote_calculator->multiply(7, 8, result);
    if (error == rpc::error::OK())
    {
        std::cout << "7 * 8 = " << result << " (remote call)\n";
    }

    std::cout << "Cross-zone demo completed!\n";
    return 0;
}
```

## 8. Tutorial: Coroutine Version

Enable coroutines for async/await patterns:

### CMake Configuration

```cmake
cmake --preset Coroutine_Debug -DCMAKE_BUILD_TYPE=Debug
```

### Coroutine Main Program

```cpp
#include "calculator_impl.h"
#include <iostream>
#include <memory>
#include <coro/io_scheduler.hpp>

using namespace calculator;

int main()
{
    std::cout << "Coroutine Calculator Demo\n";

    // Create IO scheduler for coroutines
    auto scheduler = coro::io_scheduler::make_shared(
        coro::io_scheduler::options{
            .thread_strategy = coro::io_scheduler::thread_strategy_t::spawn,
            .pool = coro::thread_pool::options{
                .thread_count = std::thread::hardware_concurrency(),
            },
            .execution_strategy = coro::io_scheduler::execution_strategy_t::process_tasks_on_thread_pool
        });

    // Create service with scheduler
    std::atomic<uint64_t> zone_gen = 0;
    auto service = std::make_shared<rpc::service>(
        "coro_service",
        rpc::zone{++zone_gen},
        scheduler);

    // Create calculator
    auto calculator = create_calculator_instance();

    // Define coroutine function
    auto calculate_task = [&]() -> CORO_TASK(void)
    {
        int result;

        // Async calls with co_await
        auto error = CO_AWAIT calculator->add(5, 3, result);
        if (error == rpc::error::OK())
        {
            std::cout << "5 + 3 = " << result << "\n";
        }

        error = CO_AWAIT calculator->multiply(4, 6, result);
        if (error == rpc::error::OK())
        {
            std::cout << "4 * 6 = " << result << "\n";
        }

        CO_RETURN;
    };

    // Spawn coroutine
    bool completed = false;
    scheduler->spawn([&]() -> CORO_TASK(void)
    {
        CO_AWAIT calculate_task();
        completed = true;
        CO_RETURN;
    }());

    // Process events until coroutine completes
    while (!completed)
    {
        scheduler->process_events(std::chrono::milliseconds(1));
    }

    std::cout << "Coroutine demo completed!\n";
    return 0;
}
```

## 9. Common Patterns

### Error Handling

```cpp
auto error = CO_AWAIT calculator->add(a, b, result);

switch (error)
{
    case rpc::error::OK():
        // Success
        break;
    case rpc::error::INVALID_DATA():
        // Handle invalid input
        break;
    case rpc::error::OBJECT_GONE():
        // Object was destroyed
        break;
    default:
        // Other error
        break;
}
```

### Interface Casting

```cpp
// Dynamic cast to different interface
auto bar_ptr = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(foo_ptr);
if (bar_ptr)
{
    // Successfully cast
}
```

### Optimistic Pointers

For objects with independent lifetimes (like database connections or services managed externally):

```cpp
// Service with its own lifetime - not managed by shared_ptr
auto db_service = database_manager->get_connection();

// Create optimistic (non-RAII) reference
rpc::optimistic_ptr<xxx::i_database> opt_db;
auto error = CO_AWAIT rpc::make_optimistic(db_service, opt_db);

// If database is shut down, returns OBJECT_GONE (expected for independent lifetime)
// If using shared_ptr, would return OBJECT_NOT_FOUND (serious error)
auto result = CO_AWAIT opt_db->query("SELECT * FROM users");
```

**Key distinction**: `OBJECT_GONE` vs `OBJECT_NOT_FOUND`

## 10. Next Steps

- [Bi-Modal Execution](05-bi-modal-execution.md) - Deep dive into execution modes
- [Transports and Passthroughs](architecture/06-transports-and-passthroughs.md) - Explore TCP, SPSC, and SGX transports
- [Examples](10-examples.md) - More code examples
- [API Reference](09-api-reference.md) - Complete API documentation
