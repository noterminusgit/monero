// Copyright (c) 2019-2024, The Monero Project
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

#include "cryptonote_protocol/levin_notify.h"
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/random_generator.hpp>

TEST(levin_notify_basic, default_construct)
{
  // Default constructed notify should be inactive
  cryptonote::levin::notify notifier;
  auto status = notifier.get_status();
  ASSERT_FALSE(status.has_noise);
  ASSERT_FALSE(status.connections_filled);
  ASSERT_FALSE(status.has_outgoing);
}

TEST(levin_notify_basic, default_send_fails)
{
  cryptonote::levin::notify notifier;
  std::vector<cryptonote::blobdata> txs;
  txs.push_back("test_tx_blob");
  boost::uuids::uuid source = boost::uuids::nil_uuid();
  // Sending on a default notifier should fail gracefully
  bool result = notifier.send_txs(std::move(txs), source, cryptonote::relay_method::local);
  ASSERT_FALSE(result);
}

TEST(levin_notify_basic, move_construct)
{
  cryptonote::levin::notify notifier1;
  cryptonote::levin::notify notifier2(std::move(notifier1));
  auto status = notifier2.get_status();
  ASSERT_FALSE(status.has_noise);
}

TEST(levin_notify_basic, move_assign)
{
  cryptonote::levin::notify notifier1;
  cryptonote::levin::notify notifier2;
  notifier2 = std::move(notifier1);
  auto status = notifier2.get_status();
  ASSERT_FALSE(status.has_noise);
}

// --- Additional levin_notify tests ---

TEST(levin_notify_basic, multiple_send_txs_on_default)
{
  cryptonote::levin::notify notifier;
  boost::uuids::uuid source = boost::uuids::nil_uuid();

  std::vector<cryptonote::blobdata> txs1;
  txs1.push_back("tx_blob_1");
  ASSERT_FALSE(notifier.send_txs(std::move(txs1), source, cryptonote::relay_method::local));

  std::vector<cryptonote::blobdata> txs2;
  txs2.push_back("tx_blob_2");
  ASSERT_FALSE(notifier.send_txs(std::move(txs2), source, cryptonote::relay_method::local));

  std::vector<cryptonote::blobdata> txs3;
  txs3.push_back("tx_blob_3");
  ASSERT_FALSE(notifier.send_txs(std::move(txs3), source, cryptonote::relay_method::local));
}

TEST(levin_notify_basic, get_status_fields_default)
{
  cryptonote::levin::notify notifier;
  auto status = notifier.get_status();
  ASSERT_FALSE(status.has_noise);
  ASSERT_FALSE(status.connections_filled);
  ASSERT_FALSE(status.has_outgoing);
}

TEST(levin_notify_basic, move_construct_preserves_inactive)
{
  cryptonote::levin::notify original;
  // Verify original is inactive
  auto status_orig = original.get_status();
  ASSERT_FALSE(status_orig.has_noise);
  ASSERT_FALSE(status_orig.connections_filled);
  ASSERT_FALSE(status_orig.has_outgoing);

  cryptonote::levin::notify moved(std::move(original));
  auto status_moved = moved.get_status();
  ASSERT_FALSE(status_moved.has_noise);
  ASSERT_FALSE(status_moved.connections_filled);
  ASSERT_FALSE(status_moved.has_outgoing);
}

TEST(levin_notify_basic, move_assign_from_active_to_inactive)
{
  cryptonote::levin::notify notifier1;
  cryptonote::levin::notify notifier2;

  // Both are inactive/default
  notifier1 = std::move(notifier2);

  auto status = notifier1.get_status();
  ASSERT_FALSE(status.has_noise);
  ASSERT_FALSE(status.connections_filled);
  ASSERT_FALSE(status.has_outgoing);
}

TEST(levin_notify_basic, send_txs_empty_tx_list)
{
  cryptonote::levin::notify notifier;
  boost::uuids::uuid source = boost::uuids::nil_uuid();

  std::vector<cryptonote::blobdata> empty_txs;
  // send_txs returns true for an empty tx list (early return, nothing to send)
  bool result = notifier.send_txs(std::move(empty_txs), source, cryptonote::relay_method::fluff);
  ASSERT_TRUE(result);
}

TEST(levin_notify_basic, send_txs_multiple_txs)
{
  cryptonote::levin::notify notifier;
  boost::uuids::uuid source = boost::uuids::nil_uuid();

  std::vector<cryptonote::blobdata> txs;
  txs.push_back("tx_data_a");
  txs.push_back("tx_data_b");
  txs.push_back("tx_data_c");
  txs.push_back("tx_data_d");
  txs.push_back("tx_data_e");

  bool result = notifier.send_txs(std::move(txs), source, cryptonote::relay_method::fluff);
  ASSERT_FALSE(result);
}

TEST(levin_notify_basic, send_txs_relay_local)
{
  cryptonote::levin::notify notifier;
  boost::uuids::uuid source = boost::uuids::nil_uuid();

  std::vector<cryptonote::blobdata> txs;
  txs.push_back("local_tx");
  ASSERT_FALSE(notifier.send_txs(std::move(txs), source, cryptonote::relay_method::local));
}

TEST(levin_notify_basic, send_txs_relay_stem)
{
  cryptonote::levin::notify notifier;
  boost::uuids::uuid source = boost::uuids::nil_uuid();

  std::vector<cryptonote::blobdata> txs;
  txs.push_back("stem_tx");
  ASSERT_FALSE(notifier.send_txs(std::move(txs), source, cryptonote::relay_method::stem));
}

TEST(levin_notify_basic, send_txs_relay_fluff)
{
  cryptonote::levin::notify notifier;
  boost::uuids::uuid source = boost::uuids::nil_uuid();

  std::vector<cryptonote::blobdata> txs;
  txs.push_back("fluff_tx");
  ASSERT_FALSE(notifier.send_txs(std::move(txs), source, cryptonote::relay_method::fluff));
}

TEST(levin_notify_basic, send_txs_relay_block)
{
  cryptonote::levin::notify notifier;
  boost::uuids::uuid source = boost::uuids::nil_uuid();

  std::vector<cryptonote::blobdata> txs;
  txs.push_back("block_tx");
  ASSERT_FALSE(notifier.send_txs(std::move(txs), source, cryptonote::relay_method::block));
}

TEST(levin_notify_basic, send_txs_relay_none)
{
  cryptonote::levin::notify notifier;
  boost::uuids::uuid source = boost::uuids::nil_uuid();

  std::vector<cryptonote::blobdata> txs;
  txs.push_back("none_tx");
  ASSERT_FALSE(notifier.send_txs(std::move(txs), source, cryptonote::relay_method::none));
}

TEST(levin_notify_basic, send_txs_relay_forward)
{
  cryptonote::levin::notify notifier;
  boost::uuids::uuid source = boost::uuids::nil_uuid();

  std::vector<cryptonote::blobdata> txs;
  txs.push_back("forward_tx");
  ASSERT_FALSE(notifier.send_txs(std::move(txs), source, cryptonote::relay_method::forward));
}

TEST(levin_notify_basic, status_after_default_construction)
{
  cryptonote::levin::notify notifier;
  auto status = notifier.get_status();
  // All status fields should be false for a default-constructed notifier
  ASSERT_FALSE(status.has_noise);
  ASSERT_FALSE(status.connections_filled);
  ASSERT_FALSE(status.has_outgoing);
}

TEST(levin_notify_basic, status_after_move_construction)
{
  cryptonote::levin::notify original;
  cryptonote::levin::notify moved(std::move(original));

  auto status = moved.get_status();
  ASSERT_FALSE(status.has_noise);
  ASSERT_FALSE(status.connections_filled);
  ASSERT_FALSE(status.has_outgoing);
}

TEST(levin_notify_basic, nil_uuid_source)
{
  cryptonote::levin::notify notifier;
  boost::uuids::uuid nil_source = boost::uuids::nil_uuid();
  ASSERT_TRUE(nil_source.is_nil());

  std::vector<cryptonote::blobdata> txs;
  txs.push_back("tx_from_nil");
  ASSERT_FALSE(notifier.send_txs(std::move(txs), nil_source, cryptonote::relay_method::local));
}

TEST(levin_notify_basic, random_uuid_source)
{
  cryptonote::levin::notify notifier;
  boost::uuids::random_generator gen;
  boost::uuids::uuid random_source = gen();
  ASSERT_FALSE(random_source.is_nil());

  std::vector<cryptonote::blobdata> txs;
  txs.push_back("tx_from_random");
  ASSERT_FALSE(notifier.send_txs(std::move(txs), random_source, cryptonote::relay_method::fluff));
}

TEST(levin_notify_basic, move_assign_self_like)
{
  // Move from one default to another, then use the target
  cryptonote::levin::notify a;
  cryptonote::levin::notify b;
  a = std::move(b);

  // a should still be valid and usable
  auto status = a.get_status();
  ASSERT_FALSE(status.has_noise);

  std::vector<cryptonote::blobdata> txs;
  txs.push_back("after_move");
  boost::uuids::uuid source = boost::uuids::nil_uuid();
  ASSERT_FALSE(a.send_txs(std::move(txs), source, cryptonote::relay_method::local));
}

TEST(levin_notify_basic, send_large_tx_blob)
{
  cryptonote::levin::notify notifier;
  boost::uuids::uuid source = boost::uuids::nil_uuid();

  std::vector<cryptonote::blobdata> txs;
  txs.push_back(std::string(100000, 'X')); // 100KB tx blob
  ASSERT_FALSE(notifier.send_txs(std::move(txs), source, cryptonote::relay_method::fluff));
}
