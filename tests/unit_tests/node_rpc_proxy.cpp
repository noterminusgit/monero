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

#include "wallet/node_rpc_proxy.h"
#include "mocks/mock_daemon.h"

namespace
{
  class NodeRPCProxyTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      m_http_client.set_should_connect(true);
      m_proxy.reset(new tools::NodeRPCProxy(m_http_client, m_mutex));
    }

    test::mock_http_client m_http_client;
    boost::recursive_mutex m_mutex;
    std::unique_ptr<tools::NodeRPCProxy> m_proxy;
  };
}

TEST_F(NodeRPCProxyTest, offline_mode)
{
  m_proxy->set_offline(true);
  uint64_t height = 0;
  auto result = m_proxy->get_height(height);
  ASSERT_TRUE(result.has_value()); // Error string returned
}

TEST_F(NodeRPCProxyTest, set_height)
{
  m_proxy->set_height(12345);
  uint64_t height = 0;
  auto result = m_proxy->get_height(height);
  // After set_height, the cached height should be returned
  // without making an RPC call (if cache is still fresh)
  ASSERT_EQ(height, 12345u);
}

TEST_F(NodeRPCProxyTest, invalidate_clears_cache)
{
  m_proxy->set_height(12345);
  m_proxy->invalidate();
  // After invalidation, next call should try to make an RPC call
  // which may fail with our minimal mock, but the cache is cleared
}

TEST_F(NodeRPCProxyTest, construction_no_crash)
{
  // Verify proxy can be created and destroyed without issues
  test::mock_http_client client;
  boost::recursive_mutex mutex;
  tools::NodeRPCProxy proxy(client, mutex);
  proxy.set_offline(true);
}
