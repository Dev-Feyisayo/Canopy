<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Local Transport (rpc::local)

In-process communication between parent and child zones. No network overhead.

## When to Use

- Unit testing
- Microservices in a single process
- High-performance local communication

## Structure

```cpp
namespace rpc::local {
    class parent_transport : public rpc::transport;
    class child_transport : public rpc::transport;
}
```

## Setup

**Parent Zone (server)**:
```cpp
auto root_service = std::make_shared<rpc::service>("root", rpc::zone{1});

// Child will connect to this zone
```

**Child Zone (client)**:
```cpp
uint64_t new_zone_id = 2;

auto child_transport = std::make_shared<rpc::local::child_transport>(
    "child_zone",
    root_service_,
    rpc::zone{new_zone_id},
    rpc::local::parent_transport::bind<yyy::i_host, yyy::i_example>(
        rpc::zone{new_zone_id},
        [&](const rpc::shared_ptr<yyy::i_host>& host,
            rpc::shared_ptr<yyy::i_example>& new_example,
            const std::shared_ptr<rpc::child_service>& child_service_ptr) -> CORO_TASK(int)
        {
            // Initialize child zone
            example_idl_register_stubs(child_service_ptr);
            new_example = rpc::make_shared<example_impl>(child_service_ptr, host);
            CO_RETURN rpc::error::OK();
        }));

rpc::shared_ptr<yyy::i_host> host_ptr;
rpc::shared_ptr<yyy::i_example> example_ptr;

auto ret = CO_AWAIT root_service_->connect_to_zone(
    "child_zone", child_transport, host_ptr, example_ptr);
```

## Key Characteristics

- **No handshake**: Status is immediately `CONNECTED`
- **Direct function calls**: Messages pass through inbound handlers
- **Hierarchical zones**: Supports `child_service` creation
- **Bidirectional**: parent_transport and child_transport reference each other

## Local Transport Include Order

**Important**: When using local transport, include `<transports/local/transport.h>` before any headers that use `rpc::local::`:

```cpp
// Correct order
#include <transports/local/transport.h>
#include <demo_impl.h>
#include <rpc/rpc.h>

// Incorrect - rpc::local:: may not be visible
#include <demo_impl.h>
#include <rpc/rpc.h>
#include <transports/local/transport.h>
```
