<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# YAS Serializer

Canopy uses YAS (Yet Another Serialization) as its primary serialization framework, supporting binary, compressed binary, and JSON formats.

## 1. Supported YAS Formats

| Format | Type | Use Case |
|--------|------|----------|
| `yas_binary` | Binary | High-performance, small payloads |
| `yas_compressed_binary` | Binary + compression | Large payloads, network transfer |
| `yas_json` | JSON text | Debugging, interoperability |

### Default Format Selection

```idl
// In IDL or build configuration
encoding = {
    yas_binary,
    yas_compressed_binary,
    yas_json
}
```

## 2. Using YAS Serialization

### Basic Serialization

```cpp
#include <rpc/internal/serialiser.h>

// Serialize an object
my_struct obj{42, "hello"};
auto serialized = rpc::serialise(obj, rpc::encoding::yas_binary);

// Deserialize
rpc::span data(serialized);
my_struct deserialized;
auto error = rpc::deserialise(rpc::encoding::yas_binary, data, deserialized);
```

### Getting Serialized Size

```cpp
auto size = rpc::get_saved_size(obj, rpc::encoding::yas_binary);
```

## 3. Format Negotiation

### Automatic Fallback

If an encoding is not supported, Canopy falls back to `yas_json`:

```cpp
// Client requests binary
auto error = CO_AWAIT proxy_->send(
    protocol_version,
    rpc::encoding::yas_binary,  // Requested
    tag,
    interface_id,
    method_id,
    input_buffer,
    output_buffer);

// Server supports only JSON
// Response uses yas_json automatically
```

### Manual Selection

```cpp
// Force specific encoding
auto error = CO_AWAIT proxy_->send(
    protocol_version,
    rpc::encoding::yas_json,  // Explicit choice
    tag,
    interface_id,
    method_id,
    input_buffer,
    output_buffer);
```

## 4. IDL Type Mapping

### Basic Types

| IDL Type | Serialized As |
|----------|---------------|
| `int`, `int32_t` | 32-bit integer |
| `int64_t` | 64-bit integer |
| `uint32_t`, `uint64_t` | Unsigned integer |
| `float`, `double` | IEEE 754 float |
| `bool` | 1 byte (0/1) |
| `std::string` | Length + UTF-8 bytes |

### Container Types

| IDL Type | Serialized As |
|----------|---------------|
| `std::vector<T>` | Length + elements |
| `std::list<T>` | Length + elements |
| `std::map<K,V>` | Length + (key, value) pairs |
| `std::array<T,N>` | Fixed number of elements |
| `std::optional<T>` | Present flag + value |

### Custom Structs

```idl
struct person
{
    std::string name;
    int age;
    std::vector<std::string> hobbies;
};
```

**YAS Binary**: Optimized binary format with type information
**YAS JSON**: JSON object with field names

## 5. JSON Schema Generation

Canopy automatically generates JSON schemas for all interfaces:

### Generated Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "calculator",
  "definitions": {
    "i_calculator_add_send": {
      "type": "object",
      "description": "Parameters for add from interface i_calculator",
      "properties": {
        "a": { "type": "integer" },
        "b": { "type": "integer" }
      },
      "required": ["a", "b"],
      "additionalProperties": false
    },
    "i_calculator_add_receive": {
      "type": "object",
      "description": "Result for add from interface i_calculator",
      "properties": {
        "result": { "type": "integer" },
        "return_value": { "type": "integer" }
      },
      "required": ["result", "return_value"],
      "additionalProperties": false
    }
  }
}
```

### Accessing Schemas

```cpp
// Get function info including schemas
auto functions = xxx::i_calculator::get_function_info();

for (const auto& func : functions)
{
    std::cout << "Function: " << func.name << "\n";
    std::cout << "Input schema: " << func.in_json_schema << "\n";
    std::cout << "Output schema: " << func.out_json_schema << "\n";
}
```

## 6. Custom Serializers

### Implementing a Custom Serializer

```cpp
namespace rpc
{

template<>
struct serialiser<my_custom_type, custom_encoding>
{
    static std::vector<uint8_t> serialise(const my_custom_type& obj)
    {
        // Custom serialization logic
        std::vector<uint8_t> result;
        // ... serialize to result
        return result;
    }

    static error_code deserialise(const std::vector<uint8_t>& data,
                                  my_custom_type& obj)
    {
        // Custom deserialization logic
        // ... deserialize from data
        return error::OK();
    }
};

} // namespace rpc
```

### Registering Custom Serializers

```cpp
// In your serialization initialization
rpc::register_custom_serializer<my_custom_type, custom_encoding>(
    &serialiser<my_custom_type, custom_encoding>::serialise,
    &serialiser<my_custom_type, custom_encoding>::deserialise);
```

## 7. Performance Considerations

### Choosing the Right Format

| Scenario | Recommended Format |
|----------|-------------------|
| High-performance local | `yas_binary` |
| Network transfer | `yas_compressed_binary` |
| Debugging/inspection | `yas_json` |

### Size Comparison (Typical)

```
yas_binary:           100 bytes (baseline)
yas_compressed_binary: 60 bytes (compressed)
yas_json:            200 bytes (text)
```

### Speed Comparison (Typical)

```
yas_binary:           0.1 ms (fastest)
yas_compressed_binary: 0.5 ms (compression overhead)
yas_json:             1.0 ms (slowest)
```

## 8. Error Handling

### Serialization Errors

```cpp
auto error = rpc::deserialise(enc, data, obj);

switch (error)
{
    case rpc::error::OK():
        // Success
        break;
    case rpc::error::PROXY_DESERIALISATION_ERROR():
        // Failed to deserialize proxy
        break;
    case rpc::error::STUB_DESERIALISATION_ERROR():
        // Failed to deserialize stub
        break;
    case rpc::error::INCOMPATIBLE_SERIALISATION():
        // Unsupported encoding format
        break;
}
```

## 9. Best Practices

1. **Use binary formats** for production
2. **Use JSON for debugging** when inspecting traffic
3. **Enable compression** for large payloads over network
4. **Test all formats** during development
5. **Document format requirements** for API consumers

## 10. Next Steps

- [Protocol Buffers](protocol-buffers.md) - Cross-language serialization
- [Error Handling](../08-error-handling.md) - Error code reference
- [API Reference](../12-api-reference.md) - Complete API
