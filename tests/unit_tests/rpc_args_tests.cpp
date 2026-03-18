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

#include "gtest/gtest.h"
#include "rpc/rpc_args.h"
#include <boost/program_options.hpp>

namespace
{
  namespace po = boost::program_options;

  // Helper to create a variables_map from command-line-like arguments
  po::variables_map make_vm(const std::vector<std::string>& args, bool any_cert = false)
  {
    po::options_description desc("Test options");
    cryptonote::rpc_args::init_options(desc, any_cert);

    po::variables_map vm;
    po::store(po::command_line_parser(args).options(desc).run(), vm);
    po::notify(vm);
    return vm;
  }
}

// ---- init_options ----

TEST(RpcArgs, InitOptionsDoesNotThrow)
{
  po::options_description desc("Test");
  EXPECT_NO_THROW(cryptonote::rpc_args::init_options(desc));
}

TEST(RpcArgs, InitOptionsWithAnyCert)
{
  po::options_description desc("Test");
  EXPECT_NO_THROW(cryptonote::rpc_args::init_options(desc, true));
}

// ---- Default options ----

TEST(RpcArgs, DefaultValues)
{
  auto vm = make_vm({});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_EQ("127.0.0.1", result->bind_ip);
  EXPECT_EQ("::1", result->bind_ipv6_address);
  EXPECT_EQ("127.0.0.1", result->restricted_bind_ip);
  EXPECT_EQ("::1", result->restricted_bind_ipv6_address);
  EXPECT_FALSE(result->use_ipv6);
  EXPECT_TRUE(result->require_ipv4);
  EXPECT_TRUE(result->access_control_origins.empty());
  EXPECT_FALSE(bool(result->login));
  EXPECT_FALSE(result->disable_rpc_ban);
}

// ---- Bind IP parsing ----

TEST(RpcArgs, BindIPLoopback)
{
  auto vm = make_vm({"--rpc-bind-ip=127.0.0.1"});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_EQ("127.0.0.1", result->bind_ip);
}

TEST(RpcArgs, BindIPExternalRequiresConfirm)
{
  auto vm = make_vm({"--rpc-bind-ip=0.0.0.0"});
  auto result = cryptonote::rpc_args::process(vm);
  // Should fail without confirm-external-bind
  EXPECT_FALSE(bool(result));
}

TEST(RpcArgs, BindIPExternalWithConfirm)
{
  auto vm = make_vm({"--rpc-bind-ip=0.0.0.0", "--confirm-external-bind"});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_EQ("0.0.0.0", result->bind_ip);
}

TEST(RpcArgs, BindIPInvalidAddress)
{
  auto vm = make_vm({"--rpc-bind-ip=not_an_ip"});
  auto result = cryptonote::rpc_args::process(vm);
  EXPECT_FALSE(bool(result));
}

// ---- IPv6 bind address ----

TEST(RpcArgs, BindIPv6Default)
{
  auto vm = make_vm({});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_EQ("::1", result->bind_ipv6_address);
}

TEST(RpcArgs, BindIPv6Loopback)
{
  auto vm = make_vm({"--rpc-bind-ipv6-address=::1"});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_EQ("::1", result->bind_ipv6_address);
}

TEST(RpcArgs, BindIPv6WithBrackets)
{
  auto vm = make_vm({"--rpc-bind-ipv6-address=[::1]"});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  // Brackets should be stripped
  EXPECT_EQ("::1", result->bind_ipv6_address);
}

TEST(RpcArgs, BindIPv6Invalid)
{
  auto vm = make_vm({"--rpc-bind-ipv6-address=not_ipv6"});
  auto result = cryptonote::rpc_args::process(vm);
  EXPECT_FALSE(bool(result));
}

TEST(RpcArgs, BindIPv6ExternalRequiresConfirm)
{
  auto vm = make_vm({"--rpc-bind-ipv6-address=2001:db8::1"});
  auto result = cryptonote::rpc_args::process(vm);
  EXPECT_FALSE(bool(result));
}

TEST(RpcArgs, BindIPv6ExternalWithConfirm)
{
  auto vm = make_vm({"--rpc-bind-ipv6-address=2001:db8::1", "--confirm-external-bind"});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_EQ("2001:db8::1", result->bind_ipv6_address);
}

// ---- Use IPv6 flag ----

TEST(RpcArgs, UseIPv6Flag)
{
  auto vm = make_vm({"--rpc-use-ipv6"});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_TRUE(result->use_ipv6);
}

// ---- Ignore IPv4 flag ----

TEST(RpcArgs, IgnoreIPv4Flag)
{
  auto vm = make_vm({"--rpc-ignore-ipv4"});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_FALSE(result->require_ipv4);
}

// ---- Access control origins ----

TEST(RpcArgs, AccessControlOriginsSingle)
{
  auto vm = make_vm({"--rpc-access-control-origins=http://localhost:8080"});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  ASSERT_EQ(1u, result->access_control_origins.size());
  EXPECT_EQ("http://localhost:8080", result->access_control_origins[0]);
}

TEST(RpcArgs, AccessControlOriginsMultiple)
{
  auto vm = make_vm({"--rpc-access-control-origins=http://a.com,http://b.com,http://c.com"});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  ASSERT_EQ(3u, result->access_control_origins.size());
  EXPECT_EQ("http://a.com", result->access_control_origins[0]);
  EXPECT_EQ("http://b.com", result->access_control_origins[1]);
  EXPECT_EQ("http://c.com", result->access_control_origins[2]);
}

TEST(RpcArgs, AccessControlOriginsEmpty)
{
  auto vm = make_vm({});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_TRUE(result->access_control_origins.empty());
}

// ---- Disable RPC ban ----

TEST(RpcArgs, DisableRpcBanDefault)
{
  auto vm = make_vm({});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_FALSE(result->disable_rpc_ban);
}

TEST(RpcArgs, DisableRpcBanEnabled)
{
  auto vm = make_vm({"--disable-rpc-ban"});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_TRUE(result->disable_rpc_ban);
}

// ---- SSL processing ----

TEST(RpcArgs, ProcessSSLDefaults)
{
  auto vm = make_vm({});
  auto ssl = cryptonote::rpc_args::process_ssl(vm);
  ASSERT_TRUE(bool(ssl));
}

TEST(RpcArgs, ProcessSSLDisabled)
{
  auto vm = make_vm({"--rpc-ssl=disabled"});
  auto ssl = cryptonote::rpc_args::process_ssl(vm);
  ASSERT_TRUE(bool(ssl));
  EXPECT_EQ(epee::net_utils::ssl_support_t::e_ssl_support_disabled, ssl->support);
}

TEST(RpcArgs, ProcessSSLEnabled)
{
  auto vm = make_vm({"--rpc-ssl=enabled"});
  auto ssl = cryptonote::rpc_args::process_ssl(vm);
  ASSERT_TRUE(bool(ssl));
  EXPECT_EQ(epee::net_utils::ssl_support_t::e_ssl_support_enabled, ssl->support);
}

TEST(RpcArgs, ProcessSSLAutodetect)
{
  auto vm = make_vm({"--rpc-ssl=autodetect"});
  auto ssl = cryptonote::rpc_args::process_ssl(vm);
  ASSERT_TRUE(bool(ssl));
  EXPECT_EQ(epee::net_utils::ssl_support_t::e_ssl_support_autodetect, ssl->support);
}

TEST(RpcArgs, ProcessSSLInvalidValue)
{
  auto vm = make_vm({"--rpc-ssl=invalid"});
  auto ssl = cryptonote::rpc_args::process_ssl(vm);
  EXPECT_FALSE(bool(ssl));
}

// ---- Restricted bind IP ----

TEST(RpcArgs, RestrictedBindIPDefault)
{
  auto vm = make_vm({});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_EQ("127.0.0.1", result->restricted_bind_ip);
}

TEST(RpcArgs, RestrictedBindIPInvalid)
{
  auto vm = make_vm({"--rpc-restricted-bind-ip=garbage"});
  auto result = cryptonote::rpc_args::process(vm);
  EXPECT_FALSE(bool(result));
}

TEST(RpcArgs, RestrictedBindIPv6Invalid)
{
  auto vm = make_vm({"--rpc-restricted-bind-ipv6-address=garbage"});
  auto result = cryptonote::rpc_args::process(vm);
  EXPECT_FALSE(bool(result));
}

TEST(RpcArgs, RestrictedBindIPv6WithBrackets)
{
  auto vm = make_vm({"--rpc-restricted-bind-ipv6-address=[::1]"});
  auto result = cryptonote::rpc_args::process(vm);
  ASSERT_TRUE(bool(result));
  EXPECT_EQ("::1", result->restricted_bind_ipv6_address);
}
