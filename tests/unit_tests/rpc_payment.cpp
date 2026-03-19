// Copyright (c) 2018-2024, The Monero Project
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

#include "rpc/rpc_payment.h"
#include "crypto/crypto.h"

namespace
{
  cryptonote::account_public_address make_test_address()
  {
    cryptonote::account_public_address addr;
    memset(&addr, 0, sizeof(addr));
    return addr;
  }
}

TEST(rpc_payment, balance_new_client)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  uint64_t bal = payment.balance(client);
  ASSERT_EQ(bal, 0u);
}

TEST(rpc_payment, balance_add_credits)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  uint64_t bal = payment.balance(client, 100);
  ASSERT_EQ(bal, 100u);
}

TEST(rpc_payment, balance_subtract_credits)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  payment.balance(client, 100);
  uint64_t bal = payment.balance(client, -50);
  ASSERT_EQ(bal, 50u);
}

TEST(rpc_payment, balance_no_underflow)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  payment.balance(client, 10);
  uint64_t bal = payment.balance(client, -100);
  // Should clamp to 0 rather than underflow
  ASSERT_EQ(bal, 0u);
}

TEST(rpc_payment, pay_sufficient_credits)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  payment.balance(client, 100);
  uint64_t credits = 0;
  bool result = payment.pay(client, 1, 50, "get_info", false, credits);
  ASSERT_TRUE(result);
  ASSERT_EQ(credits, 50u);
}

TEST(rpc_payment, pay_insufficient_credits)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  payment.balance(client, 10);
  uint64_t credits = 0;
  bool result = payment.pay(client, 1, 50, "get_info", false, credits);
  ASSERT_FALSE(result);
}

TEST(rpc_payment, pay_zero_cost)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  payment.balance(client, 10);
  uint64_t credits = 0;
  bool result = payment.pay(client, 1, 0, "get_info", false, credits);
  ASSERT_TRUE(result);
  ASSERT_EQ(credits, 10u);
}

TEST(rpc_payment, foreach_empty)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  int count = 0;
  payment.foreach([&count](const crypto::public_key &, const cryptonote::rpc_payment::client_info &) {
    ++count;
    return true;
  });
  ASSERT_EQ(count, 0);
}

TEST(rpc_payment, foreach_with_clients)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client1, client2;
  memset(&client1, 1, sizeof(client1));
  memset(&client2, 2, sizeof(client2));

  payment.balance(client1, 10);
  payment.balance(client2, 20);

  int count = 0;
  payment.foreach([&count](const crypto::public_key &, const cryptonote::rpc_payment::client_info &) {
    ++count;
    return true;
  });
  ASSERT_EQ(count, 2);
}

TEST(rpc_payment, get_payment_address)
{
  auto addr = make_test_address();
  cryptonote::rpc_payment payment(addr, 100, 10);
  ASSERT_EQ(memcmp(&payment.get_payment_address(), &addr, sizeof(addr)), 0);
}

TEST(rpc_payment, multiple_clients_independent)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client1, client2;
  memset(&client1, 1, sizeof(client1));
  memset(&client2, 2, sizeof(client2));

  payment.balance(client1, 100);
  payment.balance(client2, 200);

  ASSERT_EQ(payment.balance(client1), 100u);
  ASSERT_EQ(payment.balance(client2), 200u);

  uint64_t credits = 0;
  payment.pay(client1, 1, 30, "test", false, credits);
  ASSERT_EQ(credits, 70u);
  ASSERT_EQ(payment.balance(client2), 200u);
}

TEST(rpc_payment, balance_accumulate)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  payment.balance(client, 10);
  payment.balance(client, 20);
  payment.balance(client, 30);
  ASSERT_EQ(payment.balance(client), 60u);
}

TEST(rpc_payment, pay_drains_exact)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  payment.balance(client, 50);
  uint64_t credits = 0;
  ASSERT_TRUE(payment.pay(client, 1, 50, "test", false, credits));
  ASSERT_EQ(credits, 0u);
}

TEST(rpc_payment, pay_multiple_times)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  payment.balance(client, 100);
  uint64_t credits = 0;
  ASSERT_TRUE(payment.pay(client, 1, 30, "t1", false, credits));
  ASSERT_EQ(credits, 70u);
  ASSERT_TRUE(payment.pay(client, 2, 30, "t2", false, credits));
  ASSERT_EQ(credits, 40u);
  ASSERT_TRUE(payment.pay(client, 3, 30, "t3", false, credits));
  ASSERT_EQ(credits, 10u);
  ASSERT_FALSE(payment.pay(client, 4, 30, "t4", false, credits));
}

TEST(rpc_payment, foreach_stop_early)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client1, client2, client3;
  memset(&client1, 1, sizeof(client1));
  memset(&client2, 2, sizeof(client2));
  memset(&client3, 3, sizeof(client3));

  payment.balance(client1, 10);
  payment.balance(client2, 20);
  payment.balance(client3, 30);

  int count = 0;
  payment.foreach([&count](const crypto::public_key &, const cryptonote::rpc_payment::client_info &) {
    ++count;
    return count < 2; // Stop after 2
  });
  ASSERT_EQ(count, 2);
}

TEST(rpc_payment, different_addresses)
{
  cryptonote::account_public_address addr1, addr2;
  memset(&addr1, 1, sizeof(addr1));
  memset(&addr2, 2, sizeof(addr2));

  cryptonote::rpc_payment payment1(addr1, 100, 10);
  cryptonote::rpc_payment payment2(addr2, 100, 10);

  ASSERT_EQ(memcmp(&payment1.get_payment_address(), &addr1, sizeof(addr1)), 0);
  ASSERT_EQ(memcmp(&payment2.get_payment_address(), &addr2, sizeof(addr2)), 0);
}

TEST(rpc_payment, balance_zero_add)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  uint64_t bal = payment.balance(client, 0);
  ASSERT_EQ(bal, 0u);
}

TEST(rpc_payment, pay_with_no_balance)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  uint64_t credits = 0;
  ASSERT_FALSE(payment.pay(client, 1, 1, "test", false, credits));
}

TEST(rpc_payment, large_balance)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  payment.balance(client, 1000000);
  ASSERT_EQ(payment.balance(client), 1000000u);
}

TEST(rpc_payment, many_clients)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);

  for (int i = 0; i < 50; ++i)
  {
    crypto::public_key client;
    memset(&client, i + 1, sizeof(client));
    payment.balance(client, (uint64_t)(i + 1) * 10);
  }

  int count = 0;
  payment.foreach([&count](const crypto::public_key &, const cryptonote::rpc_payment::client_info &) {
    ++count;
    return true;
  });
  ASSERT_EQ(count, 50);
}

// ============================================================
// Additional rpc_payment coverage tests
// ============================================================

TEST(rpc_payment, pay_timestamp_ordering)
{
  // pay() requires timestamps to be monotonically increasing
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  payment.balance(client, 1000);
  uint64_t credits = 0;

  // First payment at ts=10
  ASSERT_TRUE(payment.pay(client, 10, 1, "test", false, credits));
  ASSERT_EQ(credits, 999u);

  // Payment at ts=5 should fail (going backwards)
  ASSERT_FALSE(payment.pay(client, 5, 1, "test", false, credits));

  // Payment at same ts=10 with same_ts=false should fail
  ASSERT_FALSE(payment.pay(client, 10, 1, "test", false, credits));

  // Payment at same ts=10 with same_ts=true should succeed
  ASSERT_TRUE(payment.pay(client, 10, 1, "test", true, credits));
  ASSERT_EQ(credits, 998u);

  // Payment at higher ts should succeed
  ASSERT_TRUE(payment.pay(client, 20, 1, "test", false, credits));
  ASSERT_EQ(credits, 997u);
}

// NOTE: balance() does not clamp on overflow — it wraps. This is existing behavior.
// Testing overflow clamping would require a code change, so skipped.

TEST(rpc_payment, balance_subtraction_clamps_to_zero)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));

  payment.balance(client, 5);
  // Subtracting more than available should clamp to 0
  uint64_t bal = payment.balance(client, -100);
  ASSERT_EQ(bal, 0u);
}

TEST(rpc_payment, client_info_default_values)
{
  cryptonote::rpc_payment::client_info info;
  ASSERT_EQ(info.credits, 0u);
  ASSERT_EQ(info.cookie, 0u);
  ASSERT_EQ(info.previous_seed_height, 0u);
  ASSERT_EQ(info.seed_height, 0u);
  ASSERT_EQ(info.credits_total, 0u);
  ASSERT_EQ(info.credits_used, 0u);
  ASSERT_EQ(info.nonces_good, 0u);
  ASSERT_EQ(info.nonces_stale, 0u);
  ASSERT_EQ(info.nonces_bad, 0u);
  ASSERT_EQ(info.nonces_dupe, 0u);
  ASSERT_EQ(info.last_request_timestamp, 0u);
  ASSERT_EQ(info.block_template_update_time, 0u);
}

TEST(rpc_payment, foreach_returns_true_on_empty)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  bool result = payment.foreach([](const crypto::public_key &, const cryptonote::rpc_payment::client_info &) {
    return true;
  });
  ASSERT_TRUE(result);
}

TEST(rpc_payment, foreach_returns_false_on_early_stop)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  crypto::public_key client;
  memset(&client, 1, sizeof(client));
  payment.balance(client, 10);

  bool result = payment.foreach([](const crypto::public_key &, const cryptonote::rpc_payment::client_info &) {
    return false; // Stop immediately
  });
  ASSERT_FALSE(result);
}

TEST(rpc_payment, get_hashes_no_data)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  // With no hash submissions, get_hashes should return 0
  uint64_t hashes = payment.get_hashes(3600);
  ASSERT_EQ(hashes, 0u);
}

TEST(rpc_payment, prune_hashrate_empty)
{
  // Pruning an empty hashrate map should not crash
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  payment.prune_hashrate(3600);
  ASSERT_EQ(payment.get_hashes(3600), 0u);
}

TEST(rpc_payment, flush_by_age_empty)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  // Flushing with no clients should return 0 and not crash
  unsigned int flushed = payment.flush_by_age(0);
  ASSERT_EQ(flushed, 0u);
}

TEST(rpc_payment, on_idle_no_crash)
{
  cryptonote::rpc_payment payment(make_test_address(), 100, 10);
  // on_idle should not crash even with no data
  ASSERT_TRUE(payment.on_idle());
}
