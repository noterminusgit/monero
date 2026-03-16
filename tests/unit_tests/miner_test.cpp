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

  // A simple block hash function for testing
  bool test_get_block_hash(const cryptonote::block& b, uint64_t height, const crypto::hash* seed_hash, unsigned int threads, crypto::hash& hash)
  {
    return cryptonote::get_block_longhash(NULL, b, hash, height, seed_hash, threads);
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
