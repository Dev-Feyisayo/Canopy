<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# SGX Enclave Transport (rpc::sgx)

Secure communication between host application and Intel SGX enclaves.

## When to Use

- Secure computation with hardware guarantees
- Protecting sensitive data processing
- Confidential computing scenarios

## Requirements

- Intel SGX SDK
- `CANOPY_BUILD_ENCLAVE=ON`

## Architecture

```
┌─────────────────────────────────────────┐
│                 Host                     │
│  ┌───────────────────────────────────┐  │
│  │         Host Application          │  │
│  │  ┌─────────────────────────────┐  │  │
│  │  │    host_service_proxy       │  │  │
│  │  └──────────────┬──────────────┘  │  │
│  └─────────────────┼──────────────────┘  │
│                    │ OCALL               │
│ ┌──────────────────┴──────────────────┐ │
│ │           Enclave Boundary          │ │
│ │  ┌─────────────────────────────┐    │ │
│  │  │    enclave_service_proxy   │    │ │
│  │  └──────────────┬──────────────┘    │ │
│  │                 │ ECALL              │ │
│  │  ┌─────────────────────────────┐    │ │
│  │  │      Enclave Code           │    │ │
│  │  └─────────────────────────────┘    │ │
│  └────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

## Host Setup

```cpp
auto enclave_proxy = rpc::enclave_service_proxy::create(
    "enclave_service",
    rpc::destination_zone{enclave_zone_id},
    host_service,
    "enclave.signed.so");  // Enclave binary
```

## Enclave Setup

```cpp
// Inside enclave (marshal_test_enclave.cpp)
int marshal_test_init_enclave(
    uint64_t host_zone_id,
    uint64_t host_id,
    uint64_t child_zone_id,
    uint64_t* example_object_id)
{
    auto ret = rpc::child_service::create_child_zone<
        rpc::host_service_proxy,
        yyy::i_host,
        yyy::i_example>(
        "test_enclave",
        rpc::zone{child_zone_id},
        rpc::destination_zone{host_zone_id},
        input_descr,
        output_descr,
        [](const rpc::shared_ptr<yyy::i_host>& host,
            rpc::shared_ptr<yyy::i_example>& new_example,
            const std::shared_ptr<rpc::child_service>& child_service_ptr) -> int
        {
            new_example = rpc::make_shared<example_impl>(
                child_service_ptr, host);
            return rpc::error::OK();
        },
        rpc_server);

    return ret;
}
```

## Enclave Service Proxy

```cpp
class enclave_service_proxy : public rpc::service_proxy
{
public:
    static std::shared_ptr<enclave_service_proxy> create(
        const char* name,
        rpc::destination_zone destination_zone_id,
        std::weak_ptr<rpc::service> service,
        const std::string& enclave_path);
};
```

## Host Service Proxy (for enclave-to-host calls)

```cpp
class host_service_proxy : public rpc::service_proxy
{
public:
    static std::shared_ptr<host_service_proxy> create(
        const char* name,
        rpc::caller_zone caller_zone_id,
        std::weak_ptr<rpc::child_service> service);
};
```
