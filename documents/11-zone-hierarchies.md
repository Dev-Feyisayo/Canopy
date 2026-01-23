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

## 2. Creating Child Zones

### Simple Child Zone

```cpp
// In parent service
auto child_transport = std::make_shared<rpc::local::child_transport>(
    "child_zone",
    parent_service_,
    rpc::zone{child_zone_id},
    rpc::local::parent_transport::bind<yyy::i_host, yyy::i_example>(
        rpc::zone{child_zone_id},
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

auto error = CO_AWAIT parent_service_->connect_to_zone(
    "child_zone", child_transport, host_ptr, example_ptr);
```

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
    stdex::member_ptr<transport> forward_transport_;   // To destination
    stdex::member_ptr<transport> reverse_transport_;   // To caller
    std::atomic<int32_t> function_count_;              // Active calls
    std::atomic<int32_t> shared_count_;                // References
    std::atomic<int32_t> optimistic_count_;            // Optimistic refs
};
```

**For comprehensive passthrough documentation**, including relay operations (options=3), reference counting, and routing logic, see `documents/architecture/passthroughs.md`.

## 4. Autonomous Zones

### Creating Autonomous Children

Zones that can operate independently:

```cpp
CORO_TASK(rpc::shared_ptr<i_autonomous_node>) fork_child_node()
{
    rpc::shared_ptr<i_autonomous_node> child_node;
    auto child_zone_id = ++g_zone_id_counter;

    auto child_transport = std::make_shared<rpc::local::child_transport>(
        child_zone_name.c_str(),
        current_service_->shared_from_this(),
        rpc::zone{child_zone_id},
        rpc::local::parent_transport::bind<i_autonomous_node, i_autonomous_node>(
            rpc::zone{child_zone_id},
            [child_type, child_zone_id](
                const rpc::shared_ptr<i_autonomous_node>& parent,
                rpc::shared_ptr<i_autonomous_node>& new_child,
                const std::shared_ptr<rpc::child_service>& child_service_ptr) -> CORO_TASK(int)
            {
                fuzz_test_idl_register_stubs(child_service_ptr);
                new_child = rpc::make_shared<autonomous_node_impl>(
                    child_type, child_zone_id);
                CO_RETURN CO_AWAIT new_child->initialize_node(
                    child_type, child_zone_id);
            }));

    auto result = CO_AWAIT current_service_->connect_to_zone(
        child_zone_name.c_str(), child_transport,
        parent_controller, child_node);

    CO_RETURN child_node;
}
```

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

## 7. Template-Based Test Setup

Canopy tests use templates for flexible zone hierarchies:

```cpp
template<bool UseHostInChild, bool RunStandardTests, bool CreateSubordinateZone>
class inproc_setup
{
    std::shared_ptr<rpc::service> root_service_;
    std::shared_ptr<rpc::child_service> child_service_;
    rpc::shared_ptr<yyy::i_host> i_host_ptr_;
    rpc::shared_ptr<yyy::i_example> i_example_ptr_;

    CORO_TASK(bool) CoroSetUp()
    {
        // Create root service
        root_service_ = std::make_shared<rpc::service>(
            "host", rpc::zone{++zone_gen_});

        // Register stubs
        example_idl_register_stubs(root_service_);

        // Create child transport
        auto new_zone_id = ++zone_gen_;
        auto child_transport = std::make_shared<rpc::local::child_transport>(
            "main child",
            root_service_,
            rpc::zone{new_zone_id},
            rpc::local::parent_transport::bind<yyy::i_host, yyy::i_example>(
                rpc::zone{new_zone_id},
                [&](const rpc::shared_ptr<yyy::i_host>& host,
                    rpc::shared_ptr<yyy::i_example>& new_example,
                    const std::shared_ptr<rpc::child_service>& child_service_ptr) -> CORO_TASK(int)
                {
                    example_idl_register_stubs(child_service_ptr);
                    new_example = rpc::make_shared<marshalled_tests::example>(
                        child_service_ptr, host);
                    CO_RETURN rpc::error::OK();
                }));

        auto ret = CO_AWAIT root_service_->connect_to_zone(
            "main child", child_transport, i_host_ptr_, i_example_ptr_);

        CO_RETURN ret == rpc::error::OK();
    }

public:
    void set_up()
    {
        bool result = SYNC_WAIT(CoroSetUp());
        RPC_ASSERT(result);
    }
};
```

## 8. Zone Routing

### Finding Routes

```cpp
// Direct route (same zone)
if (destination_zone == local_zone)
{
    return service_;  // Local service
}

// Route through pass-throughs
auto pass_through = transport_->find_any_passthrough_for_destination(
    destination_zone_id,
    caller_zone_id);

if (pass_through)
{
    return pass_through->get_transport_to_destination();
}

// No route found
return nullptr;
```

### Deadlock Prevention

Pass-throughs acquire transports in zone-ID order to prevent deadlock:

```cpp
// Lock lower zone ID first
auto lock1 = (zone_a < zone_b) ?
    acquire(transport_a) : acquire(transport_b);
auto lock2 = (zone_a < zone_b) ?
    acquire(transport_b) : acquire(transport_a);
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
3. **Keep hierarchies shallow** for better performance
4. **Use pass-throughs sparingly** - they add overhead
5. **Consider autonomous zones** for independent subsystems
6. **Use templates for test hierarchies** - enables parameterized testing

## 11. Next Steps

- [Examples](14-examples.md) - Working examples
- [API Reference](12-api-reference.md) - Service and transport APIs
- [Memory Management](10-memory-management.md) - Reference counting across zones
