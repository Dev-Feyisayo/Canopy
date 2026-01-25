<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Memory Management

Canopy implements a sophisticated memory management system based on reference counting across zone boundaries. Understanding this system is crucial for building reliable applications.

## 1. Reference Counting Model

### Triple-Count System

Each object maintains three reference counts:

```
Shared Count (shared_count_)
├── Owned by: rpc::shared_ptr
├── Keeps: Remote object AND transport chain alive
└── Returns: OBJECT_NOT_FOUND when count exhausted

Optimistic Count (optimistic_count_)
├── Owned by: rpc::optimistic_ptr
├── Does NOT keep object alive
└── Returns: OBJECT_GONE when count exhausted

Weak Count (weak_count_)
├── Owned by: rpc::weak_ptr
├── Does NOT keep object alive
└── Used for: Breaking circular references
```

### Count Flow

```
Creation:
  rpc::make_shared<impl>() → shared_count_ = 1

Add Reference:
  remote_call_add_ref() → shared_count_++

Release Reference:
  remote_call_release() → shared_count_--

 Destruction:
  shared_count_ == 0 → object deleted
```

## 2. rpc::shared_ptr Deep Dive

### Custom Control Block

Unlike `std::shared_ptr`, `rpc::shared_ptr` has a specialized control block:

```cpp
template<typename T>
class shared_ptr
{
    casting_interface* ptr_;          // Raw pointer
    control_block* block_;            // Custom control block
};
```

### Control Block Structure

```cpp
struct control_block
{
    std::atomic<int32_t> shared_count_;     // Strong references
    std::atomic<int32_t> weak_count_;       // Weak references
    std::atomic<int32_t> optimistic_count_; // Optimistic references
    std::atomic<bool> destroyed_;           // Destruction flag
};
```

### Reference Acquisition

```cpp
// Via add_ref transport call
CORO_TASK(int) transport::add_ref(uint64_t transaction_id,
                                   rpc::add_ref_options options)
{
    auto result = remote_add_ref(object_id, options);
    CO_RETURN result;
}

// On server side
void object_stub::add_ref()
{
    shared_count_.fetch_add(1, std::memory_order_acq_rel);
}
```

## 3. rpc::optimistic_ptr

### Purpose

Non-RAII references to objects with independent lifetimes. Unlike `rpc::shared_ptr` which follows RAII semantics (object dies when reference count reaches zero), `rpc::optimistic_ptr` assumes the object has its own lifetime managed externally.

**Use Cases**:
1. References to long-lived services (databases, message queues)
2. Callback patterns - Object A creates Object B and needs callbacks without circular dependency
3. Preventing circular dependencies in distributed systems
4. Objects managed by external lifetime managers

```cpp
// Use Case: Cross-zone callback without circular dependency
//
// Zone 1: Parent (creates child)
//   └── shared_ptr<Child> (ownership - keeps child alive)
//
// Zone 2: Child (created by parent)
//   └── optimistic_ptr<Parent> (callbacks only - no ownership)

class parent_service : public i_parent
{
    rpc::shared_ptr<child_service> child_;  // Ownership

public:
    CORO_TASK(error_code) create_child()
    {
        // Create child in another zone
        rpc::shared_ptr<child_service> new_child;
        auto error = CO_AWAIT factory_->create_child(new_child);

        // Give child an optimistic pointer to us for callbacks
        // This allows child to call us without creating circular reference
        CO_AWAIT rpc::make_optimistic(
            rpc::shared_ptr<parent_service>(this),
            new_child->get_callback_target());

        child_ = new_child;  // Ownership reference
        CO_RETURN error::OK();
    }

    // Callback from child
    CORO_TASK(error_code) on_child_event(int data) override
    {
        // Handle event
        CO_RETURN error::OK();
    }
};

class child_service : public i_child
{
    rpc::optimistic_ptr<parent_service> parent_;  // For callbacks only
public:
    void set_parent_callback(rpc::optimistic_ptr<parent_service> parent)
    {
        parent_ = parent;  // No refcount - no cycle!
    }

    CORO_TASK(error_code) process()
    {
        // Notify parent (if parent is gone, returns OBJECT_GONE - expected)
        CO_AWAIT parent_->on_child_event(progress_);
        CO_RETURN error::OK();
    }
};
```

### Error Semantics

| Pointer Type | Object Gone Error | Meaning |
|--------------|-------------------|---------|
| `rpc::shared_ptr` | `OBJECT_NOT_FOUND` | Serious - reference held but object destroyed unexpectedly |
| `rpc::optimistic_ptr` | `OBJECT_GONE` | Expected - object with independent lifetime is gone |

### Behavior Comparison

| Behavior | shared_ptr | optimistic_ptr |
|----------|------------|----------------|
| Keeps object alive | Yes | No |
| Error on destruction | OBJECT_NOT_FOUND | OBJECT_GONE |
| Use case | Managed references | Independent lifetime |
| Prevents cycles | Via weak_ptr | Yes (directly) |

### Circular Dependency Prevention

Like `weak_ptr` breaks cycles between `shared_ptr`s, `optimistic_ptr` breaks cycles in distributed systems:

```cpp
class node : public i_node
{
    rpc::shared_ptr<node> parent_;      // Strong reference to parent
    rpc::shared_ptr<node> child_;       // Strong reference to child
    // This creates a cycle if child also references parent

    // Solution: Use optimistic_ptr for parent reference
    rpc::optimistic_ptr<node> parent_opt_;
};
```

## 4. Zone Lifecycle (Amnesia)

### Death Sequence

```
1. Triple-count reaches zero:
   - Inbound stubs count = 0
   - Outbound proxies count = 0
   - Pass-through objects count = 0

2. Service signals transport:
   service_->on_amnesia(transport_);

3. Transport cleanup:
   transport_->cleanup();

4. Parent release:
   child_transport_->release_parent();

5. Optimistic notification:
   Optimistic pointers receive OBJECT_GONE
```

### State Diagram

```
        ┌─────────────────┐
        │   ALIVE         │◄──────────────────────┐
        │   (refs > 0)    │                        │
        └────────┬────────┘                        │
                 │ all refs released               │
                 ▼                                 │
        ┌─────────────────┐                        │
        │  AMNESIA        │──► transport_down() ───┘
        │  (refs = 0)     │
        └────────┬────────┘
                 │ transport cleanup
                 ▼
        ┌─────────────────┐
        │  DELETED        │
        │  (gone forever) │
        └─────────────────┘
```

## 5. Transport Lifetime Management

### Transport References

```cpp
// Services hold weak references
std::weak_ptr<transport> service::transports_[zone_id];

// Pass-throughs hold strong references
class pass_through
{
    stdex::member_ptr<transport> forward_transport_;
    stdex::member_ptr<transport> reverse_transport_;
};
```

### Reference Acquisition Pattern

```
Zone A ──► Transport ──► Zone B
  │                     │
  │ weak_ptr            │ member_ptr (strong)
  ▼                     ▼
Service             PassThrough
```

## 6. member_ptr

Canopy uses `stdex::member_ptr` for thread-safe transport references:

```cpp
// member_ptr is a thread-safe wrapper around shared_ptr
class member_ptr<T>
{
    std::shared_ptr<T> ptr_;
    mutable std::shared_mutex mutex_;

public:
    T* get() const
    {
        std::shared_lock lock(mutex_);
        return ptr_.get();
    }

    void reset(std::shared_ptr<T> ptr)
    {
        std::unique_lock lock(mutex_);
        ptr_ = std::move(ptr);
    }
};
```

### Why member_ptr?

- Safe access during concurrent RPC calls
- Safe access during object destruction
- No raw pointer lifetime issues

## 7. No Bridging Policy

### Critical Rule

**Never cast between `rpc::shared_ptr` and `std::shared_ptr`**

### Wrong (Breaks Reference Counting)

```cpp
auto rpc_ptr = rpc::make_shared<impl>();

// WRONG: Mixing pointer types
std::shared_ptr<base> std_ptr = std::dynamic_pointer_cast<base>(rpc_ptr);

// WRONG: Converting to raw and back
base* raw = rpc_ptr.get();
rpc_ptr.reset();
auto copy = rpc::shared_ptr<base>(raw);  // Lost refcount!
```

### Correct (Use RPC Types Throughout)

```cpp
// All RPC code uses rpc::shared_ptr
rpc::shared_ptr<interface> rpc_ptr = rpc::make_shared<impl>();

// Non-RPC code uses std::shared_ptr
std::shared_ptr<non_rpc_impl> std_ptr = std::make_shared<non_rpc_impl>();

// Bridge via factory function
auto create_bridge() -> rpc::shared_ptr<bridge_interface>
{
    auto impl = std::make_shared<non_rpc_impl>();
    return wrap_in_rpc(impl);  // Custom wrapper that creates rpc::shared_ptr
}
```

## 8. Memory Best Practices

### Do

- Use `rpc::make_shared<T>()` for creation
- Use `rpc::shared_ptr` for all RPC references
- Use `rpc::weak_ptr` for breaking cycles
- Use `rpc::optimistic_ptr` for independent lifetimes
- Let reference counting handle cleanup

### Don't

- Don't store raw pointers to RPC objects
- Don't mix with `std::shared_ptr`
- Don't manually delete RPC objects
- Don't assume synchronous destruction
- Don't hold references longer than needed

### Object Lifetime Patterns

```cpp
// Pattern 1: Short-lived operation
{
    auto svc = get_service();  // Acquire
    auto error = CO_AWAIT svc->do_work();  // Use
    // svc goes out of scope, reference released
}

// Pattern 2: Long-lived cache
class service_cache
{
    std::map<std::string, rpc::shared_ptr<i_service>> cache_;
public:
    void add(const std::string& name, rpc::shared_ptr<i_service> svc)
    {
        cache_[name] = svc;  // Keeps alive
    }

    void remove(const std::string& name)
    {
        cache_.erase(name);  // Release
    }
};

// Pattern 3: Break cycles with weak_ptr
class node
{
    rpc::weak_ptr<node> parent_;
    std::vector<rpc::shared_ptr<node>> children_;
};
```

## 9. Debugging Memory Issues

### Enable Telemetry

```cpp
#ifdef CANOPY_USE_TELEMETRY
rpc::console_telemetry_service::create(telemetry, "test", "memory_test", "/tmp");
#endif
```

### Watch For

```
Warning: shared_count_ was 0 before decrement
  → Double-release or use-after-free

Warning: stub zone_id X has been released but not deregistered
  → Orphaned stub reference

Error: OBJECT_NOT_FOUND
  → Reference released before call completed

Error: OBJECT_GONE
  → Object destroyed during async operation
```

## 10. Next Steps

- [Zone Hierarchies](architecture/07-zone-hierarchies.md) - Complex topologies
- [Error Handling](06-error-handling.md) - Lifecycle errors
- [API Reference](09-api-reference.md) - Smart pointer API
