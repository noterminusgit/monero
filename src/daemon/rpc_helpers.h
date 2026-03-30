// Copyright (c) 2014-2024, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <string>
#include <ctime>
#include <cstdio>
#include <boost/lexical_cast.hpp>
#include "net/net_utils_base.h"
#include "rpc/core_rpc_server_commands_defs.h"

namespace daemonize
{
namespace rpc_helpers
{
  inline const char *get_address_type_name(epee::net_utils::address_type address_type)
  {
    switch (address_type)
    {
      default:
      case epee::net_utils::address_type::invalid: return "invalid";
      case epee::net_utils::address_type::ipv4: return "IPv4";
      case epee::net_utils::address_type::ipv6: return "IPv6";
      case epee::net_utils::address_type::i2p: return "I2P";
      case epee::net_utils::address_type::tor: return "Tor";
    }
  }

  inline std::string print_float(float f, int prec)
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%*.*f", prec, prec, f);
    return buf;
  }

  inline std::string get_human_time_ago(time_t t, time_t now)
  {
    if (t == now)
      return "now";
    time_t dt = t > now ? t - now : now - t;
    std::string s;
    if (dt < 90)
      s = boost::lexical_cast<std::string>(dt) + " seconds";
    else if (dt < 90 * 60)
      s = boost::lexical_cast<std::string>(dt/60) + " minutes";
    else if (dt < 36 * 3600)
      s = boost::lexical_cast<std::string>(dt/3600) + " hours";
    else
      s = boost::lexical_cast<std::string>(dt/(3600*24)) + " days";
    return s + " " + (t > now ? "in the future" : "ago");
  }

  inline std::string get_time_hms(time_t t)
  {
    unsigned int hours, minutes, seconds;
    char buffer[24];
    hours = t / 3600;
    t %= 3600;
    minutes = t / 60;
    t %= 60;
    seconds = t;
    snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", hours, minutes, seconds);
    return std::string(buffer);
  }

  inline std::string make_error(const std::string &base, const std::string &status)
  {
    if (status == CORE_RPC_STATUS_OK)
      return base;
    return base + " -- " + status;
  }

} // namespace rpc_helpers
} // namespace daemonize
