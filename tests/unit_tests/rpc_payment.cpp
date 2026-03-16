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
