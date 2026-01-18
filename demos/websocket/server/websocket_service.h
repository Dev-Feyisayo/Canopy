// Copyright (c) 2026 Edward Boggis-Rolfe
// All rights reserved.

#pragma once

#include <rpc/rpc.h>
#include <websocket_demo/websocket_demo.h>
#include "demo.h"

namespace websocket_demo
{
    class websocket_service : public rpc::service
    {
        rpc::shared_ptr<websocket_demo::v1::i_calculator> demo_;

    public:
        websocket_service(std::string name, rpc::zone zone_id, std::shared_ptr<coro::io_scheduler> scheduler)
            : rpc::service(name.data(), zone_id, std::move(scheduler))
        {
            demo_ = create_websocket_demo_instance();
        }

        virtual ~websocket_service() DEFAULT_DESTRUCTOR;

        rpc::shared_ptr<websocket_demo::v1::i_calculator> get_demo_instance() { return demo_; }
    };
}
