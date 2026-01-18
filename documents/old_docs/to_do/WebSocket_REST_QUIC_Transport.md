<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# WebSocket/REST/QUIC Transport

**Status**: 🚀 WEBSOCKET AND REST IN PROGRESS - Basic implementations working, needs production hardening and full RPC integration
**Priority**: HIGH - WebSocket and REST provide immediate value for web-based integrations
**Estimated Effort**:

## Objective

Implement modern web-based communication protocols for Canopy, enabling browser-based clients, HTTP-based APIs, and high-performance UDP-based communication.

## Overview

This implementation adds support for three complementary web protocols:
- **WebSocket** - Full-duplex communication over HTTP for bidirectional RPC
- **REST** - HTTP-based request/response for stateless API endpoints
- **QUIC** - Modern UDP-based transport with built-in TLS and multiplexing

**Architecture**: These transports operate as **peer-to-peer** (symmetric) transports, similar to TCP and SPSC, rather than hierarchical parent-child relationships.

---

## 1. WebSocket Transport

### Objective
Enable full-duplex, bidirectional RPC communication over WebSocket protocol.

### Requirements

#### Protocol Support
- RFC 6455 WebSocket protocol implementation
- HTTP/1.1 upgrade handshake
- Frame-based message encoding (text and binary frames)
- Ping/pong heartbeat mechanism
- Graceful connection close

#### Bi-Modal Support

**Synchronous Mode** (`CANOPY_BUILD_COROUTINE=OFF`):
- Blocking send/receive operations
- Simple request-response pattern

**Asynchronous Mode** (`CANOPY_BUILD_COROUTINE=ON`):
- Coroutine-based async I/O with libcoro
- Non-blocking message handling
- Efficient for high-concurrency scenarios

#### Transport Implementation

```cpp
namespace websocket {
    class ws_transport : public rpc::transport {
        // WebSocket connection (built on TCP)
        coro::net::tcp::client socket_;
        wslay_event_context_ptr ctx_;  // wslay library context

        // Message queue for async mode
        std::queue<std::vector<char>> pending_messages_;

        // Bi-modal send/receive
        CORO_TASK(int) send(destination_zone dest_zone,
                            caller_zone caller_zone_id,
                            uint64_t destination_channel_id,
                            const std::vector<char>& buffer,
                            const std::vector<rpc::back_channel_entry>& in_back_channel,
                            std::vector<char>& out_buffer,
                            std::vector<rpc::back_channel_entry>& out_back_channel) override;
    };
}
```

#### Integration Features
- **Server Side**: HTTP server accepts WebSocket upgrade requests
- **Client Side**: Initiates WebSocket handshake with server
- **Message Framing**: RPC messages serialized into WebSocket binary frames
- **Reconnection**: Automatic reconnection with exponential backoff
- **Compression**: Optional per-message compression (permessage-deflate extension)

#### Use Cases
- Browser-based RPC clients (JavaScript/TypeScript)
- Real-time dashboards and monitoring tools
- Mobile apps requiring persistent connections
- Bidirectional event streaming

#### Dependencies
- `wslay` - WebSocket library (already in submodules)
- `llhttp` - HTTP parser for upgrade handshake (already in submodules)

#### Current Status
- ✅ Basic HTTP server with upgrade handling (2026-01-04)
- ✅ wslay integration for WebSocket frame processing
- ✅ Message routing to RPC service
- 🟡 **PARTIAL** - Needs bi-modal support, reconnection, compression

---

## 2. REST Transport

### Objective
Expose RPC interfaces as RESTful HTTP endpoints for stateless API access.

### Requirements

#### HTTP Methods Mapping
- `GET` - Read-only RPC methods (queries)
- `POST` - Create/invoke RPC methods
- `PUT` - Update RPC methods
- `DELETE` - Delete/cleanup RPC methods
- `PATCH` - Partial update methods

#### URL Routing

```
HTTP Pattern                    RPC Mapping
────────────────────────────────────────────────────────
GET    /api/v1/{interface}/{method}?params=...
                                → RPC call with query params

POST   /api/v1/{interface}/{method}
       Body: JSON parameters    → RPC call with JSON body

GET    /api/v1/objects/{object_id}/{interface}/{method}
                                → RPC call on specific object
```

#### Content Negotiation
- Request: `Content-Type: application/json` or `application/x-protobuf`
- Response: `Content-Type` based on `Accept` header
- Support for both JSON and Protocol Buffers serialization

#### Transport Implementation

```cpp
namespace rest {
    class http_rest_transport : public rpc::transport {
        // HTTP server for incoming requests
        coro::net::tcp::server server_;
        llhttp_settings_t parser_settings_;

        // Route table: maps URL patterns to RPC interfaces
        std::map<std::string, rpc::interface_descriptor> routes_;

        // Convert HTTP request to RPC call
        CORO_TASK(std::string) handle_http_request(
            const std::string& method,
            const std::string& path,
            const std::string& body);

        // Convert RPC response to HTTP response
        std::string build_http_response(
            int status_code,
            const std::vector<char>& rpc_buffer,
            const std::string& content_type);
    };
}
```

#### Features
- **Automatic Route Generation**: Generate REST routes from IDL interface definitions
- **OpenAPI/Swagger**: Auto-generate API documentation from IDL
- **Authentication**: Support for Bearer tokens, API keys
- **Rate Limiting**: Configurable per-endpoint rate limits
- **CORS Support**: Cross-origin resource sharing for browser clients
- **Error Handling**: HTTP status codes mapped to RPC error codes

#### Use Cases
- Public HTTP APIs
- Third-party integrations
- Legacy system compatibility
- Simple curl/wget based testing
- Microservices architecture

#### Dependencies
- `llhttp` - HTTP parser (already in submodules)
- `nlohmann/json` - JSON serialization (already in submodules)

#### Current Status
- ✅ Basic HTTP request handling (GET/POST/PUT/DELETE)
- ✅ JSON response generation
- ✅ Stubbed API endpoints
- 🟡 **PARTIAL** - Needs automatic route generation, OpenAPI, RPC integration

---

## 3. QUIC Transport

### Objective
Implement high-performance, low-latency transport using QUIC protocol (UDP-based with built-in TLS).

### Requirements

#### Protocol Features
- QUIC protocol (RFC 9000) over UDP
- Integrated TLS 1.3 encryption
- Multiple independent streams per connection
- 0-RTT connection establishment
- Connection migration (IP/port changes)
- Congestion control and flow control

#### Advantages Over TCP
- **Lower Latency**: Reduced connection establishment time (0-RTT or 1-RTT vs. TCP's 3-way handshake + TLS)
- **No Head-of-Line Blocking**: Independent streams don't block each other
- **Better Loss Recovery**: Faster recovery from packet loss
- **Connection Migration**: Survives network changes (WiFi ↔ Cellular)

#### Transport Implementation

```cpp
namespace quic {
    class quic_transport : public rpc::transport {
        // QUIC connection (ngtcp2 or msquic library)
        quic_connection_ptr connection_;

        // Multiple streams for parallel RPC calls
        std::map<uint64_t, quic_stream_ptr> active_streams_;

        // Bi-modal support
        CORO_TASK(int) send(destination_zone dest_zone,
                            caller_zone caller_zone_id,
                            uint64_t destination_channel_id,
                            const std::vector<char>& buffer,
                            const std::vector<rpc::back_channel_entry>& in_back_channel,
                            std::vector<char>& out_buffer,
                            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        // Stream management
        CORO_TASK(quic_stream_ptr) create_stream();
        void close_stream(uint64_t stream_id);
    };
}
```

#### Integration Features
- **Stream Multiplexing**: Each RPC call can use independent QUIC stream
- **Priority Management**: Critical RPC calls use high-priority streams
- **Built-in Encryption**: TLS 1.3 integrated into protocol (no separate SSL layer)
- **NAT Traversal**: Better than TCP for peer-to-peer scenarios
- **Configurable Congestion Control**: BBR, Cubic, etc.

#### Use Cases
- High-frequency trading systems (ultra-low latency)
- Mobile applications (connection migration)
- Real-time gaming (low jitter, fast recovery)
- Video streaming with RPC control channel
- IoT devices with unreliable networks
- Peer-to-peer distributed systems

#### Dependencies (choose one)
- `ngtcp2` + `nghttp3` - IETF QUIC implementation
- `msquic` - Microsoft QUIC implementation
- `quiche` - Cloudflare QUIC implementation

#### Current Status
- 📋 **NOT STARTED** - Planned for future implementation

---

## Implementation Phases

### Phase 1: WebSocket Transport (4-6 weeks)
1. ✅ Week 1-2: Basic WebSocket server/client with llhttp + wslay
2. ✅ Week 2-3: Message framing and RPC serialization integration
3. 🟡 Week 3-4: Bi-modal support (sync and async modes)
4. 🟡 Week 4-5: Reconnection, heartbeat, error handling
5. 🟡 Week 5-6: Testing, documentation, example client (JavaScript)

### Phase 2: REST Transport (3-4 weeks)
1. ✅ Week 1: HTTP request handling with llhttp, basic routing
2. ✅ Week 2: JSON serialization/deserialization for RPC methods
3. 🟡 Week 2-3: Automatic route generation from IDL
4. 🟡 Week 3-4: OpenAPI generation, authentication, CORS, testing

### Phase 3: QUIC Transport (6-8 weeks)
1. 🟡 Week 1-2: Evaluate QUIC libraries (ngtcp2 vs msquic vs quiche)
2. 🟡 Week 2-4: Basic QUIC connection establishment and stream creation
3. 🟡 Week 4-6: RPC integration with stream multiplexing
4. 🟡 Week 6-8: Bi-modal support, performance tuning, testing

---

## Remaining Work

### WebSocket Transport
- [ ] Implement bi-modal support (sync and async modes)
- [ ] Add automatic reconnection with exponential backoff
- [ ] Implement ping/pong heartbeat mechanism
- [ ] Add per-message compression support
- [ ] Create JavaScript/TypeScript client example
- [ ] Comprehensive testing and error handling
- [ ] Documentation and usage guide

### REST Transport
- [ ] Implement automatic route generation from IDL
- [ ] Generate OpenAPI/Swagger documentation
- [ ] Add authentication (Bearer tokens, API keys)
- [ ] Implement rate limiting
- [ ] Add CORS support
- [ ] Map HTTP status codes to RPC error codes
- [ ] Full RPC integration
- [ ] Comprehensive testing

### QUIC Transport
- [ ] Evaluate and select QUIC library
- [ ] Implement basic connection establishment
- [ ] Create stream management system
- [ ] Integrate with RPC serialization
- [ ] Add bi-modal support
- [ ] Performance tuning and optimization
- [ ] Comprehensive testing
- [ ] Documentation
