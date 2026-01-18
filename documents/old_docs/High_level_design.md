<!--
Copyright (c) 2026 Edward Boggis-Rolfe
All rights reserved.
-->

# Architectural Specification: Hierarchical Node Tree and Distributed Object Mesh

This document outlines the architecture for a high-performance RPC framework that manages the lifecycle and communication of objects distributed across a strictly hierarchical tree of nodes. It balances deterministic C++ memory management with the resilience required for distributed systems.

---

## 1. Structural Overview

The system is defined by two overlapping topologies:

### The Node Tree (Physical Topology)

Nodes are arranged in a **strict tree structure**. Each node represents a logical or physical boundary (e.g., a DLL, an embedded device, or a virtual nested device).

* **Ownership:** Children hold a **strong reference** (`std::shared_ptr`) to their parent.
* **Lifecycle Constraint:** A parent node is guaranteed to outlive its children. A node only begins its destruction sequence once all child nodes and internal service references are released.

### The Object Mesh (Logical Topology)

Within these nodes, objects form a complex, arbitrary graph. These objects communicate across node boundaries via "magical" edges that are established during node creation or passed as parameters.

* **Decoupled Location:** Objects do not need to know the physical location of their peers; the transport layer handles routing transparently.

---

## 2. Core Architectural Components

Based on the system headers, the following types and classes facilitate this architecture:

### Lifecycle & Management

* **`rpc::service`**: The central authority within a node. It maintains the triple-count (Inbound, Outbound, Passthrough) and coordinates the node's "death" once amnesia is achieved.
* **`rpc::service_proxy`**: Represents a remote service within a local node, facilitating cross-zone communication.
* **`stdex::member_ptr`**: A thread-safe wrapper for `shared_ptr` using a `shared_mutex` to ensure safe access to objects during concurrent RPC calls or teardown events.

### Proxies and Stubs

* **`rpc::object_stub`**: The server-side representative of a local object, responsible for demarshalling incoming calls.
* **`rpc::object_proxy`**: The client-side representative of a remote object.
* **`rpc::casting_interface`**: The base class for all interfaces, providing version-independent interface matching and access to underlying proxies or services.
* **`rpc::interface_proxy`**: A template class that provides a concrete implementation of an interface by wrapping an `object_proxy`.

### Transport and Communication

* **`rpc::i_marshaller`**: The interface defining the transport requirements, including `send`, `post`, `add_ref`, `release`, and lifecycle notifications.
* **`rpc::interface_descriptor`**: An encapsulation containing the `object_id` and `destination_zone_id` required to route a call.
* **`rpc::back_channel_entry`**: Used to manage auxiliary data or control signals alongside standard RPC payloads.

---

## 3. Reference and Edge Semantics

The system utilizes two distinct types of edges to manage object lifetimes across nodes:

1. **Shared Pointers (`rpc::shared_ptr`)**:
* Implements **RAII-based ownership**.
* Keeps the remote object and the intervening transport chain (passthroughs) alive.
* Failures result in `rpc::error::OBJECT_NOT_FOUND`.


2. **Optimistic Pointers**:
* Maintains a callable channel but does **not** keep the remote object alive.
* Once the object is released by all shared pointers, the optimistic pointer becomes invalid.
* Subsequent calls return `rpc::error::OBJECT_GONE`.



---

## 4. The "Death" Sequence (Amnesia & Teardown)

A node dies only when it reaches a state of **logical amnesia**. The sequence is managed as follows:

1. **Count Exhaustion**: The `rpc::service` triple-count (Inbound stubs, Outbound proxies, and Passthrough objects) reaches zero.
2. **Service Signaling**: The service sends an asynchronous message to the **Edge Transport** to initiate teardown.
3. **Transport Cleanup**: The transport cleans up local resources (DLL handles, sockets, etc.).
4. **Parent Release**: The child transport releases its `shared_ptr` to the parent node.
5. **Amnesia Notification**:
* **Graceful**: `object_released` is sent to notify optimistic pointers.
* **Abrupt**: `transport_down` is sent in the event of a network or hardware failure, triggering local RAII cleanup storms.



---

## 5. Error States and Resilience

The framework distinguishes between lifecycle events and transport failures through specific error codes defined in `error_codes.h`:

* **`OK`**: Successful operation.
* **`OBJECT_GONE`**: The target object has been gracefully released (Optimistic pointer failure).
* **`OBJECT_NOT_FOUND`**: The object ID is invalid or the path is broken (Shared pointer failure).
* **`TRANSPORT_ERROR` / `SERVICE_PROXY_LOST_CONNECTION**`: Physical or logical link failure.

Applications can register **Repair Hooks** to intercept these errors and attempt to re-establish broken edges or gracefully degrade functionality.
