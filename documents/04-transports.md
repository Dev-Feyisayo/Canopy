<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Transports

Transports provide the communication channels between zones in Canopy. Each transport implements a specific communication mechanism while adhering to a common interface that enables the Canopy framework to route messages, manage connections, and handle lifecycle events uniformly across different transport types.

## Transport Architecture

All transports inherit from `rpc::transport`, which defines the interface for:

- **Connection establishment** via the `connect()` virtual method
- **Message sending** with `send()` for request-response and `post()` for fire-and-forget operations
- **Reference counting** through `add_ref()` and `release()` for distributed object lifecycle management
- **Interface queries** using `try_cast()` to support dynamic interface resolution

### Core Transport Responsibilities

The base `transport` class manages:

1. **Zone identity** - Each transport connects two zones and knows its local zone ID and the adjacent zone ID
2. **Destination routing** - Maintains handlers for zone pairs to route incoming messages to the correct service
3. **Pass-through routing** - For multi-hop zone hierarchies, tracks which transports can reach which destinations
4. **Reference counting** - Tracks outbound proxies and inbound stubs to manage object lifetimes across transport boundaries
5. **Connection status** - Enum values: `CONNECTING`, `CONNECTED`, `RECONNECTING`, `DISCONNECTED`

### Transport Status

```cpp
enum class transport_status
{
    CONNECTING,   // Initial state, establishing connection
    CONNECTED,    // Fully operational
    RECONNECTING, // Attempting to recover connection
    DISCONNECTED  // Terminal state, no further traffic allowed
};
```

### Inbound Message Processing

Transports implement the `i_marshaller` interface for outbound communication—sending requests to the adjacent zone. 

These base class methods are called by the specfic transport implementation when invoked by messages from the adjacent zone or client to process the request. These methods call the `i_marshaller` interface of either a pass-through object to another transport (for multi-hop routing) or the local service (for direct delivery).

- `inbound_send()` - Request-response RPC call; returns a response to the caller
- `inbound_post()` - Fire-and-forget notification; no response expected
- `inbound_try_cast()` - Interface query to obtain a different interface on an object
- `inbound_add_ref()` - Increments reference count on a remote object
- `inbound_release()` - Decrements reference count; may trigger object destruction
- `inbound_object_released()` - Notifies the transport that an object has been released
- `inbound_transport_down()` - Notifies the transport that the adjacent transport has failed

Each `inbound_*` method handles the deserialization, routing, and any necessary response generation for the corresponding outbound operation.

## Transport Types

Canopy provides several transport implementations, each optimized for different use cases:

| Transport | Purpose | Requirements |
|-----------|---------|--------------|
| [Local](transports/local.md) | In-process parent-child zone communication | None |
| [TCP](transports/tcp.md) | Network communication between machines | Coroutines |
| [SPSC](transports/spsc.md) | Lock-free inter-process communication | Coroutines |
| [SGX](transports/sgx.md) | Secure enclave communication | SGX SDK |
| [Custom](transports/custom.md) | User-defined transport implementations | Depends on implementation |

Choose the transport that matches your use case: Local for in-process testing, TCP for network communication, SPSC for high-performance IPC, or SGX for secure computation.

## Connection Handshake

Most transports use a two-phase handshake:

**Client sends** `init_client_channel_send`:
```idl
struct init_channel_send
{
    uint64_t caller_zone_id;
    uint64_t caller_object_id;
    uint64_t destination_zone_id;
    // TCP includes: adjacent_zone_id
};
```

**Server responds** `init_client_channel_response`:
```idl
struct init_channel_response
{
    int32_t err_code;
    uint64_t destination_zone_id;
    uint64_t destination_object_id;
    uint64_t caller_zone_id;
};
```

## Common Patterns

### Zone Hierarchy

Canopy zones form hierarchical structures. Each zone can only create zones directly adjacent to itself:

```
Zone 1 (Root)
├── Zone 2 (created by Zone 1)
│   └── Zone 4 (created by Zone 2)
└── Zone 3 (created by Zone 1)
    └── Zone 5 (created by Zone 3)
```

Rules:
- Zone 1 can directly create Zone 2 and Zone 3
- Zone 2 can directly create Zone 4 (its child)
- Zone 2 cannot directly create Zone 3 (sibling) or Zone 5 (grandchild)

### Multi-Hop Routing

For complex zone hierarchies, messages may route through intermediate zones. This is an emergent behavior controlled at a strategic level—the library handles routing automatically.

### Transport Attachment

```cpp
// Attach transport to service
service_->add_transport(destination_zone{2}, transport_);

// Get transport for zone
auto transport = service_->get_transport(destination_zone{2});
```

### Transport Lifecycle

```cpp
// Services hold weak references
std::weak_ptr<transport> transport_;

// Pass-throughs hold strong references
stdex::member_ptr<transport> forward_transport_;
stdex::member_ptr<transport> reverse_transport_;
```

#### Lifetime Management

In peer-to-peer arrangements (e.g., TCP transport between two standalone services), the service object typically manages the lifetimes of all objects within its zone. The service holds weak references to transports, and when all references to a transport are released, the transport can be destroyed.

In hierarchical arrangements using `child_service` (e.g., local transport connecting parent and child zones), the transport adjacent to the zone's parent is the last to survive. The child service holds strong references to pass-through transports that connect upward through the hierarchy. When the child zone shuts down, these transports ensure the parent-side references remain valid until all child objects have been properly released, maintaining the integrity of the zone hierarchy during teardown.

### connect_to_zone Signature

The `connect_to_zone` function creates a connection between zones:

```cpp
template<class in_param_type, class out_param_type>
CORO_TASK(int)
connect_to_zone(const char* name,
    std::shared_ptr<transport> child_transport,
    const rpc::shared_ptr<in_param_type>& input_interface,
    rpc::shared_ptr<out_param_type>& output_interface);
```

Parameters:
- `name` - Unique name for the zone connection
- `child_transport` - Transport connecting to the child zone
- `input_interface` - Interface the child can call back to parent (owned by parent)
- `output_interface` - Interface returned from child (owned by child)

### Child Transport with parent_transport::bind

When creating a child zone, use `parent_transport::bind` to provide the entry point for initializing the child zone.

## Next Steps

- [Building Canopy](05-building.md) - Configure builds with different transports
- [Getting Started](06-getting-started.md) - Tutorial with transport setup
- [Bi-Modal Execution](07-bi-modal-execution.md) - Blocking and coroutine modes
- [YAS Serializer](serializers/yas-serializer.md) - Serialization formats
