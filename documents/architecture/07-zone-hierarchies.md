<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Zone Hierarchies

Canopy supports complex hierarchical zone topologies for organizing distributed systems. This section covers creating, managing, and routing through zone hierarchies.

## 1. Hierarchical Topology

### Tree Structure

Zones form a strict tree hierarchy:

```
Zone 1 (Root - Host)
│
├── Zone 2 (Child - Plugin A)
│   ├── Zone 4 (Grandchild)
│   └── Zone 5 (Grandchild)
│
├── Zone 3 (Child - Plugin B)
│   └── Zone 6 (Grandchild)
│
└── Zone 7 (Child - Enclave)
```

### Key Properties

- **Parent-Child Relationships**: Each zone (except root) has one parent
- **Strong References**: Children hold strong references to parents
- **Lifecycle Guarantee**: Parent outlives all children
- **Unique IDs**: Zone IDs are unique across the entire system


### Child Service Pattern

```cpp
class my_child_service : public rpc::child_service
{
    rpc::shared_ptr<yyy::i_host> host_;

public:
    my_child_service(const char* name, rpc::zone zone_id,
                     std::shared_ptr<rpc::transport> parent_transport,
                     rpc::shared_ptr<yyy::i_host> host)
        : rpc::child_service(name, zone_id, std::move(parent_transport))
        , host_(host)
    {
    }
};
```

## 3. Pass-Through Routing

### Multi-Hop Communication

When Zone 1 needs to communicate with Zone 4:

```
Zone 1 ──► Zone 2 ──► Zone 4
   │          │
   │          └── pass_through ──►
   └── find route through Zone 2
```

### Pass-Through Creation

```cpp
// Pass-throughs are created automatically during routing
auto pass_through = transport_->find_any_passthrough_for_destination(
    destination_zone_id,  // Zone 4
    caller_zone_id);      // Zone 1
```

### Pass-Through Lifecycle

```cpp
class pass_through
{
    std::shared_ptr<transport> forward_transport_;   // To destination
    std::shared_ptr<transport> reverse_transport_;   // To caller
    std::shared_ptr<service> service_;               // Keeps intermediary zone alive
    std::atomic<int32_t> function_count_;            // Active calls
    std::atomic<int32_t> shared_count_;              // References
    std::atomic<int32_t> optimistic_count_;          // Optimistic refs
};
```

Passthroughs hold `std::shared_ptr<service>` to keep the intermediary zone alive while routing between non-adjacent zones.

**For comprehensive passthrough documentation**, including relay operations (options=3), reference counting, and routing logic, see [Transports and Passthroughs](06-transports-and-passthroughs.md).

## 4. Autonomous Zones

### Creating Autonomous Children

Zones that can operate independently:

## 5. Fork Patterns

### Simple Fork

```
Zone N
    │
    └── fork() ──► Zone N+1 (copy of Zone N)
```

### Y-Topology Fork

```
         Zone 1 (Origin)
         /    \
        /      \
    Zone 2   Zone 3 (forked from Zone 1)
              \
               \
              Zone 4 (forked from Zone 3)
```

### Implementation

```idl
interface i_example
{
    error_code create_fork_and_return_object(
        [in] rpc::shared_ptr<i_example> zone_factory,
        [in] const std::vector<uint64_t>& fork_zone_ids,
        [out] rpc::shared_ptr<i_example>& object_from_forked_zone);

    error_code create_y_topology_fork(
        [in] rpc::shared_ptr<i_example> factory_zone,
        [in] const std::vector<uint64_t>& fork_zone_ids);
};
```

## 6. Caching Across Zones

### Object Caching Pattern

```cpp
// Cache object in autonomous zone for retrieval by other zones
error_code cache_object_from_autonomous_zone(
    [in] const std::vector<uint64_t>& zone_ids)
{
    // Create autonomous zone
    auto autonomous_zone = create_autonomous_zone();

    // Cache object
    cached_object_ = create_object();

    // Other zones can now retrieve
    CO_RETURN rpc::error::OK();
}

error_code retrieve_cached_autonomous_object(
    [out] rpc::shared_ptr<i_example>& cached_object)
{
    cached_object = cached_object_;
    CO_RETURN rpc::error::OK();
}
```

## 9. Common Patterns

### Service Discovery

```cpp
// Look up service by name in parent zone
error_code look_up_app(const std::string& name,
                       [out] rpc::shared_ptr<i_example>& app)
{
    auto it = cached_apps_.find(name);
    if (it != cached_apps_.end())
    {
        app = it->second;
        CO_RETURN rpc::error::OK();
    }
    CO_RETURN rpc::error::NOT_FOUND;
}

// Register service in parent zone
error_code set_app(const std::string& name,
                   [in] const rpc::shared_ptr<i_example>& app)
{
    cached_apps_[name] = app;
    CO_RETURN rpc::error::OK();
}
```

### Zone Factory

```cpp
class zone_factory : public i_zone_factory
{
    std::atomic<uint64_t> next_zone_id_{100};

    error_code create_zone([out] rpc::shared_ptr<i_zone>& new_zone,
                           uint64_t parent_zone_id)
    {
        auto zone_id = ++next_zone_id_;
        new_zone = create_zone_instance(zone_id, parent_zone_id);
        CO_RETURN rpc::error::OK();
    }
};
```

## 10. Best Practices

1. **Plan zone hierarchy** before implementation
2. **Use consistent ID generation** to avoid collisions
3. **Keep hierarchies shallow** for better performance, however there is no additional parameter serialisation, however certain transports may have additional enveloping overhead
4. **Use pass-throughs sparingly** - they add overhead
5. **Consider autonomous zones** for independent subsystems
6. **Use templates for test hierarchies** - enables parameterized testing

## 11. Code References

**Hierarchical Transport Pattern**:
- `documents/transports/hierarchical.md` - Circular dependency pattern details
- `documents/transports/local.md` - Local transport implementation
- `documents/transports/sgx.md` - SGX enclave hierarchies

**Implementation**:
- `rpc/include/rpc/child_service.h` - Child service class
- `transports/local/include/local/child_transport.h` - Child transport
- `transports/local/include/local/parent_transport.h` - Parent transport

## 12. Next Steps

- [Zones](02-zones.md) - Zone fundamentals
- [Services](03-services.md) - Service and child_service details
- [Memory Management](04-memory-management.md) - Reference counting across hierarchies
- [Transports and Passthroughs](06-transports-and-passthroughs.md) - Multi-hop routing
- [../10-examples.md](../10-examples.md) - Working examples
- [../09-api-reference.md](../09-api-reference.md) - Service and transport APIs
