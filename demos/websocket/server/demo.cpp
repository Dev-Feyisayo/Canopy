// Copyright (c) 2026 Edward Boggis-Rolfe
// All rights reserved.

#include <rpc/rpc.h>

#include "websocket_demo/websocket_demo.h"

namespace websocket_demo
{
    class demo : public v1::i_calculator
    {
    public:
        ~demo() override = default;
        void* get_address() const override { return (void*)this; }
        const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
        {
            if (v1::i_calculator::get_id(rpc::VERSION_3) == interface_id)
            {
                return static_cast<const v1::i_calculator*>(this);
            }
            return nullptr;
        }

        CORO_TASK(int) add(double first_val, double second_val, double& response)
        {
            response = first_val + second_val;
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(int) subtract(double first_val, double second_val, double& response)
        {
            response = first_val - second_val;
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(int) multiply(double first_val, double second_val, double& response)
        {
            response = first_val * second_val;
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(int) divide(double first_val, double second_val, double& response)
        {
            response = first_val / second_val;
            CO_RETURN rpc::error::OK();
        }
    };

    rpc::shared_ptr<v1::i_calculator> create_websocket_demo_instance()
    {
        return rpc::make_shared<demo>();
    }
}
