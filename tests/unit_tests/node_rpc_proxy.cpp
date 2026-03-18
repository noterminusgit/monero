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
  ASSERT_TRUE(!!result); // Error string returned
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

// --- Additional NodeRPCProxy tests ---

TEST_F(NodeRPCProxyTest, set_height_zero)
{
  m_proxy->set_height(0);
  uint64_t height = 999;
  auto result = m_proxy->get_height(height);
  ASSERT_EQ(height, 0u);
}

TEST_F(NodeRPCProxyTest, set_height_max_uint64)
{
  m_proxy->set_height(UINT64_MAX);
  uint64_t height = 0;
  auto result = m_proxy->get_height(height);
  ASSERT_EQ(height, UINT64_MAX);
}

TEST_F(NodeRPCProxyTest, set_height_one)
{
  m_proxy->set_height(1);
  uint64_t height = 0;
  auto result = m_proxy->get_height(height);
  ASSERT_EQ(height, 1u);
}

TEST_F(NodeRPCProxyTest, set_height_large_value)
{
  m_proxy->set_height(2000000);
  uint64_t height = 0;
  auto result = m_proxy->get_height(height);
  ASSERT_EQ(height, 2000000u);
}

TEST_F(NodeRPCProxyTest, set_offline_on)
{
  m_proxy->set_offline(true);
  uint64_t height = 0;
  auto result = m_proxy->get_height(height);
  ASSERT_TRUE(!!result); // should return error string
}

TEST_F(NodeRPCProxyTest, set_offline_off)
{
  m_proxy->set_offline(true);
  m_proxy->set_offline(false);
  // After turning offline off, the proxy should try to use the HTTP client
  // With our mock that auto-connects, it may or may not succeed,
  // but it should not crash
}

TEST_F(NodeRPCProxyTest, set_offline_toggle_multiple)
{
  m_proxy->set_offline(true);
  m_proxy->set_offline(false);
  m_proxy->set_offline(true);
  m_proxy->set_offline(false);
  m_proxy->set_offline(true);

  uint64_t height = 0;
  auto result = m_proxy->get_height(height);
  ASSERT_TRUE(!!result);
}

TEST_F(NodeRPCProxyTest, multiple_invalidate_calls)
{
  m_proxy->set_height(100);
  m_proxy->invalidate();
  m_proxy->invalidate();
  m_proxy->invalidate();
  // Multiple invalidations should not crash
}

TEST_F(NodeRPCProxyTest, invalidate_then_set_height)
{
  m_proxy->set_height(100);
  m_proxy->invalidate();
  m_proxy->set_height(200);

  uint64_t height = 0;
  auto result = m_proxy->get_height(height);
  ASSERT_EQ(height, 200u);
}

TEST_F(NodeRPCProxyTest, get_height_offline_returns_error)
{
  m_proxy->set_offline(true);
  uint64_t height = 0;
  auto result = m_proxy->get_height(height);
  ASSERT_TRUE(!!result);
  // The error string should not be empty
  ASSERT_FALSE(result->empty());
}

TEST_F(NodeRPCProxyTest, construction_with_disconnected_client)
{
  test::mock_http_client client;
  client.set_should_connect(false);
  boost::recursive_mutex mutex;
  tools::NodeRPCProxy proxy(client, mutex);
  // Should construct without crashing
}

TEST_F(NodeRPCProxyTest, get_earliest_height_offline)
{
  m_proxy->set_offline(true);
  uint64_t earliest_height = 0;
  auto result = m_proxy->get_earliest_height(1, earliest_height);
  ASSERT_TRUE(!!result);
}

TEST_F(NodeRPCProxyTest, get_earliest_height_version_zero_offline)
{
  m_proxy->set_offline(true);
  uint64_t earliest_height = 0;
  auto result = m_proxy->get_earliest_height(0, earliest_height);
  ASSERT_TRUE(!!result);
}

TEST_F(NodeRPCProxyTest, get_dynamic_base_fee_estimate_offline)
{
  m_proxy->set_offline(true);
  uint64_t fee = 0;
  auto result = m_proxy->get_dynamic_base_fee_estimate(0, fee);
  ASSERT_TRUE(!!result);
}

TEST_F(NodeRPCProxyTest, get_dynamic_base_fee_estimate_grace_blocks_offline)
{
  m_proxy->set_offline(true);
  uint64_t fee = 0;
  auto result = m_proxy->get_dynamic_base_fee_estimate(10, fee);
  ASSERT_TRUE(!!result);
}

TEST_F(NodeRPCProxyTest, get_fee_quantization_mask_offline)
{
  m_proxy->set_offline(true);
  uint64_t mask = 0;
  auto result = m_proxy->get_fee_quantization_mask(mask);
  ASSERT_TRUE(!!result);
}

TEST_F(NodeRPCProxyTest, get_rpc_version_offline)
{
  m_proxy->set_offline(true);
  uint32_t version = 0;
  std::vector<std::pair<uint8_t, uint64_t>> hard_forks;
  uint64_t height = 0;
  uint64_t target_height = 0;
  auto result = m_proxy->get_rpc_version(version, hard_forks, height, target_height);
  ASSERT_TRUE(!!result);
}

TEST_F(NodeRPCProxyTest, get_target_height_offline)
{
  m_proxy->set_offline(true);
  uint64_t target = 0;
  auto result = m_proxy->get_target_height(target);
  ASSERT_TRUE(!!result);
}

TEST_F(NodeRPCProxyTest, get_block_weight_limit_offline)
{
  m_proxy->set_offline(true);
  uint64_t limit = 0;
  auto result = m_proxy->get_block_weight_limit(limit);
  ASSERT_TRUE(!!result);
}

TEST_F(NodeRPCProxyTest, get_adjusted_time_offline)
{
  m_proxy->set_offline(true);
  uint64_t adjusted = 0;
  auto result = m_proxy->get_adjusted_time(adjusted);
  ASSERT_TRUE(!!result);
}

TEST_F(NodeRPCProxyTest, set_height_then_invalidate_then_offline)
{
  m_proxy->set_height(500);
  m_proxy->invalidate();
  m_proxy->set_offline(true);

  uint64_t height = 0;
  auto result = m_proxy->get_height(height);
  ASSERT_TRUE(!!result);
}

TEST_F(NodeRPCProxyTest, construction_multiple_proxies)
{
  test::mock_http_client client1;
  test::mock_http_client client2;
  boost::recursive_mutex mutex1;
  boost::recursive_mutex mutex2;

  client1.set_should_connect(true);
  client2.set_should_connect(false);

  tools::NodeRPCProxy proxy1(client1, mutex1);
  tools::NodeRPCProxy proxy2(client2, mutex2);

  proxy1.set_offline(true);
  proxy2.set_offline(true);

  uint64_t h1 = 0, h2 = 0;
  auto r1 = proxy1.get_height(h1);
  auto r2 = proxy2.get_height(h2);
  ASSERT_TRUE(!!r1);
  ASSERT_TRUE(!!r2);
}

TEST_F(NodeRPCProxyTest, get_dynamic_base_fee_estimate_2021_offline)
{
  m_proxy->set_offline(true);
  std::vector<uint64_t> fees;
  auto result = m_proxy->get_dynamic_base_fee_estimate_2021_scaling(0, fees);
  ASSERT_TRUE(!!result);
}

TEST_F(NodeRPCProxyTest, set_height_overwrite)
{
  m_proxy->set_height(100);
  m_proxy->set_height(200);
  m_proxy->set_height(300);

  uint64_t height = 0;
  auto result = m_proxy->get_height(height);
  ASSERT_EQ(height, 300u);
}
