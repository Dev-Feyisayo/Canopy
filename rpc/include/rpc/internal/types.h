/*
 *   Copyright (c) 2026 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once
#include <string>
#include <stdint.h>
#include <functional>

#include "rpc/internal/coroutine_support.h"
#include <rpc/internal/serialiser.h>

namespace std
{
    // Remove the old template for type_id, and provide overloads for each affected class:
    inline std::string to_string(const rpc::zone& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::destination_zone& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::caller_zone& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::known_direction_zone& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::object& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::interface_ordinal& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::method& val)
    {
        return std::to_string(val.get_val());
    }

    template<> struct hash<rpc::zone>
    {
        auto operator()(const rpc::zone& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::destination_zone>
    {
        auto operator()(const rpc::destination_zone& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::caller_zone>
    {
        auto operator()(const rpc::caller_zone& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::known_direction_zone>
    {
        auto operator()(const rpc::known_direction_zone& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::interface_ordinal>
    {
        auto operator()(const rpc::interface_ordinal& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::object>
    {
        auto operator()(const rpc::object& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::method>
    {
        auto operator()(const rpc::method& item) const noexcept { return (std::size_t)item.get_val(); }
    };
}
