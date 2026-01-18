<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Custom Transports

Implement your own transport by inheriting from `rpc::transport`.

## Required Overrides

```cpp
class my_transport : public rpc::transport
{
public:
    // Connection establishment
    CORO_TASK(int) connect() override
    {
        // Perform handshake
        status_ = transport_status::CONNECTED;
        CO_RETURN rpc::error::OK();
    }

    // Request-response RPC
    CORO_TASK(int) send(uint64_t protocol_version,
                        rpc::encoding enc,
                        uint64_t transaction_id,
                        const rpc::span& data,
                        rpc::span& response) override
    {
        // Serialize and send
        // Wait for response
        // Deserialize response
    }

    // Fire-and-forget
    CORO_TASK(int) post(uint64_t protocol_version,
                        rpc::encoding enc,
                        const rpc::span& data) override
    {
        // Send without waiting for response
    }

    // Interface query
    CORO_TASK(int) try_cast(uint64_t transaction_id,
                            rpc::interface_ordinal interface_id,
                            void** object) override;

    // Reference counting
    CORO_TASK(int) add_ref(uint64_t transaction_id,
                           rpc::add_ref_options options) override;

    CORO_TASK(int) release(uint64_t transaction_id,
                           rpc::release_options options) override;
};
```

## Lifecycle Notifications

```cpp
// Called when object is released
void object_released(rpc::object object_id);

// Called when transport fails
void transport_down();
```
