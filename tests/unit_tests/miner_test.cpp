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

#include "cryptonote_basic/miner.h"
#include "cryptonote_basic/cryptonote_format_utils.h"

namespace
{
  struct test_miner_handler : public cryptonote::i_miner_handler
  {
    bool handle_block_found(cryptonote::block& b, cryptonote::block_verification_context &bvc) override
    {
      found_block = true;
      return true;
    }

    bool get_block_template(cryptonote::block& b, const cryptonote::account_public_address& adr,
                            cryptonote::difficulty_type& diffic, uint64_t& height, uint64_t& expected_reward,
                            uint64_t &cumulative_weight, const cryptonote::blobdata& ex_nonce,
                            uint64_t &seed_height, crypto::hash &seed_hash) override
    {
      return true;
    }

    bool found_block = false;
  };

  // A simple block hash function for testing - returns the block hash via standard hashing
  bool test_get_block_hash(const cryptonote::block& b, uint64_t height, const crypto::hash* seed_hash, unsigned int threads, crypto::hash& hash)
  {
    return cryptonote::get_block_hash(b, hash);
  }
}

TEST(miner, find_nonce_difficulty_one)
{
  // With difficulty 1, any nonce should work
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;

  cryptonote::block bl = {};
  bl.major_version = 1;
  bl.minor_version = 0;
  bl.timestamp = 0;
  bl.nonce = 0;

  // Difficulty 1 means any hash passes
  ASSERT_TRUE(cryptonote::miner::find_nonce_for_given_block(gbh, bl, 1, 0));
}

TEST(miner, create_and_check_not_mining)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  ASSERT_FALSE(m.is_mining());
}

TEST(miner, pause_resume)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  // Pause and resume should be balanced
  m.pause();
  m.resume();
  // Multiple pauses
  m.pause();
  m.pause();
  m.resume();
  m.resume();
}

TEST(miner, get_speed_when_not_mining)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  ASSERT_EQ(m.get_speed(), 0u);
}

TEST(miner, background_mining_defaults)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  ASSERT_EQ(cryptonote::miner::BACKGROUND_MINING_DEFAULT_IDLE_THRESHOLD_PERCENTAGE, 90);
  ASSERT_EQ(cryptonote::miner::BACKGROUND_MINING_DEFAULT_MINING_TARGET_PERCENTAGE, 40);
  ASSERT_EQ(cryptonote::miner::BACKGROUND_MINING_DEFAULT_MIN_IDLE_INTERVAL_IN_SECONDS, 10);
}

// --- Additional miner tests ---

TEST(miner, find_nonce_difficulty_one_timestamp_zero)
{
  cryptonote::get_block_hash_t gbh = test_get_block_hash;

  cryptonote::block bl = {};
  bl.major_version = 1;
  bl.minor_version = 0;
  bl.timestamp = 0;
  bl.nonce = 0;

  ASSERT_TRUE(cryptonote::miner::find_nonce_for_given_block(gbh, bl, 1, 0));
}

TEST(miner, find_nonce_difficulty_one_timestamp_current)
{
  cryptonote::get_block_hash_t gbh = test_get_block_hash;

  cryptonote::block bl = {};
  bl.major_version = 1;
  bl.minor_version = 0;
  bl.timestamp = static_cast<uint64_t>(time(nullptr));
  bl.nonce = 0;

  ASSERT_TRUE(cryptonote::miner::find_nonce_for_given_block(gbh, bl, 1, 0));
}

TEST(miner, find_nonce_difficulty_one_timestamp_large)
{
  cryptonote::get_block_hash_t gbh = test_get_block_hash;

  cryptonote::block bl = {};
  bl.major_version = 1;
  bl.minor_version = 0;
  bl.timestamp = 1700000000;
  bl.nonce = 0;

  ASSERT_TRUE(cryptonote::miner::find_nonce_for_given_block(gbh, bl, 1, 0));
}

TEST(miner, find_nonce_difficulty_one_timestamp_one)
{
  cryptonote::get_block_hash_t gbh = test_get_block_hash;

  cryptonote::block bl = {};
  bl.major_version = 1;
  bl.minor_version = 0;
  bl.timestamp = 1;
  bl.nonce = 0;

  ASSERT_TRUE(cryptonote::miner::find_nonce_for_given_block(gbh, bl, 1, 0));
}

TEST(miner, find_nonce_difficulty_two)
{
  cryptonote::get_block_hash_t gbh = test_get_block_hash;

  cryptonote::block bl = {};
  bl.major_version = 1;
  bl.minor_version = 0;
  bl.timestamp = 0;
  bl.nonce = 0;

  // Difficulty 2 may require trying a few nonces but should succeed
  ASSERT_TRUE(cryptonote::miner::find_nonce_for_given_block(gbh, bl, 2, 0));
}

TEST(miner, find_nonce_difficulty_one_at_height_100)
{
  cryptonote::get_block_hash_t gbh = test_get_block_hash;

  cryptonote::block bl = {};
  bl.major_version = 1;
  bl.minor_version = 0;
  bl.timestamp = 0;
  bl.nonce = 0;

  ASSERT_TRUE(cryptonote::miner::find_nonce_for_given_block(gbh, bl, 1, 100));
}

TEST(miner, construction_with_null_handler)
{
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  // Constructing with nullptr should not crash
  cryptonote::miner m(nullptr, gbh);
  ASSERT_FALSE(m.is_mining());
}

TEST(miner, is_mining_false_before_start)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  ASSERT_FALSE(m.is_mining());
}

TEST(miner, get_speed_zero_when_not_mining)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  ASSERT_EQ(m.get_speed(), 0u);
}

TEST(miner, get_speed_zero_after_construction)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  uint64_t speed = m.get_speed();
  ASSERT_EQ(speed, 0u);
}

TEST(miner, pause_resume_single)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  m.pause();
  m.resume();
  // Should not crash and miner should still not be mining
  ASSERT_FALSE(m.is_mining());
}

TEST(miner, pause_resume_multiple_balanced)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  m.pause();
  m.pause();
  m.pause();
  m.resume();
  m.resume();
  m.resume();

  ASSERT_FALSE(m.is_mining());
}

TEST(miner, pause_resume_interleaved)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  m.pause();
  m.resume();
  m.pause();
  m.resume();
  m.pause();
  m.resume();

  ASSERT_FALSE(m.is_mining());
}

TEST(miner, background_mining_idle_threshold_range)
{
  ASSERT_LE(cryptonote::miner::BACKGROUND_MINING_MIN_IDLE_THRESHOLD_PERCENTAGE,
            cryptonote::miner::BACKGROUND_MINING_DEFAULT_IDLE_THRESHOLD_PERCENTAGE);
  ASSERT_LE(cryptonote::miner::BACKGROUND_MINING_DEFAULT_IDLE_THRESHOLD_PERCENTAGE,
            cryptonote::miner::BACKGROUND_MINING_MAX_IDLE_THRESHOLD_PERCENTAGE);
}

TEST(miner, background_mining_target_range)
{
  ASSERT_LE(cryptonote::miner::BACKGROUND_MINING_MIN_MINING_TARGET_PERCENTAGE,
            cryptonote::miner::BACKGROUND_MINING_DEFAULT_MINING_TARGET_PERCENTAGE);
  ASSERT_LE(cryptonote::miner::BACKGROUND_MINING_DEFAULT_MINING_TARGET_PERCENTAGE,
            cryptonote::miner::BACKGROUND_MINING_MAX_MINING_TARGET_PERCENTAGE);
}

TEST(miner, background_mining_idle_interval_range)
{
  ASSERT_LE(cryptonote::miner::BACKGROUND_MINING_MIN_MIN_IDLE_INTERVAL_IN_SECONDS,
            cryptonote::miner::BACKGROUND_MINING_DEFAULT_MIN_IDLE_INTERVAL_IN_SECONDS);
  ASSERT_LE(cryptonote::miner::BACKGROUND_MINING_DEFAULT_MIN_IDLE_INTERVAL_IN_SECONDS,
            cryptonote::miner::BACKGROUND_MINING_MAX_MIN_IDLE_INTERVAL_IN_SECONDS);
}

TEST(miner, background_mining_extra_sleep_positive)
{
  ASSERT_GT(cryptonote::miner::BACKGROUND_MINING_DEFAULT_MINER_EXTRA_SLEEP_MILLIS, 0u);
}

TEST(miner, background_mining_monitor_interval_positive)
{
  ASSERT_GT(cryptonote::miner::BACKGROUND_MINING_MINER_MONITOR_INVERVAL_IN_SECONDS, 0u);
}

TEST(miner, stop_when_not_started)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  // Stopping when not mining should not crash
  ASSERT_FALSE(m.is_mining());
  m.stop();
  ASSERT_FALSE(m.is_mining());
}

TEST(miner, send_stop_signal_when_not_started)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  // send_stop_signal when not mining should not crash
  m.send_stop_signal();
  ASSERT_FALSE(m.is_mining());
}

TEST(miner, get_threads_count_when_not_mining)
{
  test_miner_handler handler;
  cryptonote::get_block_hash_t gbh = test_get_block_hash;
  cryptonote::miner m(&handler, gbh);

  ASSERT_EQ(m.get_threads_count(), 0u);
}
