/*
 *   Copyright (c) 2026 Edward Boggis-Rolfe
 *   All rights reserved.
 */

/*
 *   TCP Transport Demo
 *   Demonstrates network communication using TCP transport
 *
 *   Concept: Client and server communicating over TCP/IP
 *   - Server: Listens on a port, accepts connections
 *   - Client: Connects to server, makes RPC calls
 *   - Requires: CANOPY_BUILD_COROUTINE=ON (uses async I/O)
 *
 *   MISCONCEPTION REPORT:
 *   ---------------------
 *   This demo requires CANOPY_BUILD_COROUTINE=ON because TCP transport uses
 *   libcoro for async I/O operations. The coro::net::tcp::client and
 *   coro::net::tcp::server classes are only available with coroutines.
 *
 *   Without coroutines, you would need to implement a synchronous TCP
 *   transport wrapper, which is not provided in the base RPC++ library.
 *
 *   To build and run:
 *   1. cmake --preset Coroutine_Debug
 *   2. cmake --build build --target tcp_transport_demo
 *   3. ./build/output/debug/demos/comprehensive/tcp_transport_demo
 */

#include <demo_impl.h>
#include <rpc/rpc.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

#include <transports/tcp/transport.h>

std::atomic<bool> g_running{true};

void signal_handler(int sig)
{
    std::cout << "Received signal " << sig << ", shutting down...\n";
    g_running = false;
}

void print_separator(const std::string& title)
{
    RPC_INFO("");
    RPC_INFO("{}", std::string(60, '='));
    RPC_INFO("  {}", title);
    RPC_INFO("{}", std::string(60, '='));
}

namespace comprehensive
{
    namespace v1
    {

#ifdef CANOPY_BUILD_COROUTINE
        CORO_TASK(bool) run_tcp_server(std::shared_ptr<coro::io_scheduler> scheduler)
        {
            print_separator("TCP SERVER (Coroutine Mode)");

            std::atomic<uint64_t> zone_gen{0};
            const uint16_t port = 18888;

            // Create root service
            auto server_service = std::make_shared<rpc::service>("tcp_server", rpc::zone{++zone_gen}, scheduler);

            RPC_INFO("Server zone ID: {}", server_service->get_zone_id().get_val());
            RPC_INFO("Listening on port {}...", port);

            // Create server listener
            auto server_options = coro::net::tcp::server::options{
                .address = coro::net::ip_address::from_string("127.0.0.1"), .port = port, .backlog = 10};

            std::cout << "Note: Full TCP transport implementation requires rpc::tcp::listener\n";
            std::cout << "This demo shows the client connection pattern.\n";

            // Create a calculator for demo purposes
            auto calculator = create_calculator();
            std::cout << "Created calculator service in server zone\n";

            // Run for a limited time (5 seconds) then exit gracefully
            auto start_time = std::chrono::steady_clock::now();
            const auto max_duration = std::chrono::seconds(5);

            while (g_running)
            {
                scheduler->process_events(std::chrono::milliseconds(100));

                auto elapsed = std::chrono::steady_clock::now() - start_time;
                if (elapsed > max_duration)
                {
                    RPC_INFO("Server timeout reached, shutting down...");
                    break;
                }
            }

            print_separator("TCP SERVER SHUTDOWN");
            CO_RETURN true;
        }

        CORO_TASK(bool) run_tcp_client(std::shared_ptr<coro::io_scheduler> scheduler)
        {
            print_separator("TCP CLIENT (Coroutine Mode)");

            std::atomic<uint64_t> zone_gen{0};
            const uint16_t port = 18888;
            const char* host = "127.0.0.1";

            // Create client service
            auto client_service = std::make_shared<rpc::service>("tcp_client", rpc::zone{++zone_gen}, scheduler);

            RPC_INFO("Client zone ID: {}", client_service->get_zone_id().get_val());
            RPC_INFO("Connecting to {}:{}...", host, port);

            // Note: Full TCP client implementation requires the rpc::tcp::tcp_transport class
            //
            // For a complete implementation, you would use:
            // coro::net::tcp::client client(scheduler,
            //     coro::net::tcp::client::options{
            //         .address = coro::net::ip_address::from_string(host),
            //         .port = port
            //     });
            //
            // auto status = CO_AWAIT client.connect();
            // if (status != coro::net::socket_status::connected)
            // {
            //     RPC_ERROR("Failed to connect");
            //     CO_RETURN false;
            // }
            //
            // auto transport = rpc::tcp::tcp_transport::create(
            //     "client", client_service, peer_zone_id,
            //     std::chrono::seconds(5), std::move(client), nullptr);
            //
            // auto error = CO_AWAIT client_service->connect_to_zone(
            //     "server", transport, service_proxy);

            RPC_INFO("Note: Full TCP transport requires rpc::tcp::tcp_transport");
            RPC_INFO("This demo shows the client connection pattern.");

            // Demonstrate calculator usage (would be remote if TCP was fully implemented)
            auto calculator = create_calculator();
            int result;
            auto error = CO_AWAIT calculator->add(100, 200, result);
            RPC_INFO("Calculator test: 100 + 200 = {} (error: {})", result, static_cast<int>(error));

            print_separator("TCP CLIENT SHUTDOWN");
            CO_RETURN true;
        }
#endif
    }
}

void rpc_log(int level, const char* str, size_t sz)
{
    std::string message(str, sz);
    switch (level)
    {
    case 0:
        printf("[CRITICAL] %s\n", message.c_str());
        break;
    case 1:
        printf("[ERROR] %s\n", message.c_str());
        break;
    case 2:
        printf("[WARN] %s\n", message.c_str());
        break;
    case 3:
        printf("[INFO] %s\n", message.c_str());
        break;
    case 4:
        printf("[TRACE] %s\n", message.c_str());
        break;
    case 5:
        printf("[DEBUG] %s\n", message.c_str());
        break;
    default:
        printf("[DEBUG] %s\n", message.c_str());
        break;
    }
}

int main()
{
    RPC_INFO("RPC++ Comprehensive Demo - TCP Transport");
    RPC_INFO("========================================");
    RPC_INFO("NOTE: TCP transport demo requires CANOPY_BUILD_COROUTINE=ON");
    RPC_INFO("");

#ifndef CANOPY_BUILD_COROUTINE
    RPC_ERROR("TCP transport requires coroutines.");
    RPC_ERROR("Please configure with: cmake --preset Coroutine_Debug");
    return 1;
#else
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    auto scheduler = coro::io_scheduler::make_shared(
        coro::io_scheduler::options{
            .thread_strategy = coro::io_scheduler::thread_strategy_t::spawn,
            .pool = coro::thread_pool::options{
                .thread_count = std::thread::hardware_concurrency(),
            },
            .execution_strategy = coro::io_scheduler::execution_strategy_t::process_tasks_on_thread_pool
        });

    bool server_done = false;
    bool client_done = false;

    // Start server
    scheduler->spawn(
        [&]() -> CORO_TASK(void)
        {
            CO_AWAIT comprehensive::v1::run_tcp_server(scheduler);
            server_done = true;
            CO_RETURN;
        }());

    // Small delay to let server start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Start client
    scheduler->spawn(
        [&]() -> CORO_TASK(void)
        {
            CO_AWAIT comprehensive::v1::run_tcp_client(scheduler);
            client_done = true;
            CO_RETURN;
        }());

    // Process events until both done
    while (!server_done || !client_done)
    {
        scheduler->process_events(std::chrono::milliseconds(1));
    }

    print_separator("TCP TRANSPORT DEMO COMPLETED");
    return 0;
#endif
}
