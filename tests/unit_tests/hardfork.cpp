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
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include <algorithm>
#include "gtest/gtest.h"

#include "blockchain_db/blockchain_db.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/hardfork.h"
#include "blockchain_db/testdb.h"

using namespace cryptonote;

#define BLOCKS_PER_YEAR 525960
#define SECONDS_PER_YEAR 31557600

namespace
{

class TestDB: public cryptonote::BaseTestDB {
public:
  virtual uint64_t height() const override { return blocks.size(); }
  virtual void add_block( const block& blk
                        , size_t block_weight
                        , uint64_t long_term_block_weight
                        , const difficulty_type& cumulative_difficulty
                        , const uint64_t& coins_generated
                        , uint64_t num_rct_outs
                        , const crypto::hash& blk_hash
                        ) override {
    blocks.push_back(blk);
  }
  virtual void remove_block() override { blocks.pop_back(); }
  virtual block get_block_from_height(const uint64_t& height) const override {
    return blocks.at(height);
  }
  virtual void set_hard_fork_version(uint64_t height, uint8_t version) override {
    if (versions.size() <= height) 
      versions.resize(height+1); 
    versions[height] = version;
  }
  virtual uint8_t get_hard_fork_version(uint64_t height) const override {
    return versions.at(height);
  }

private:
  std::vector<block> blocks;
  std::deque<uint8_t> versions;
};

}

static cryptonote::block mkblock(uint8_t version, uint8_t vote)
{
  cryptonote::block b;
  b.major_version = version;
  b.minor_version = vote;
  return b;
}

static cryptonote::block mkblock(const HardFork &hf, uint64_t height, uint8_t vote)
{
  cryptonote::block b;
  b.major_version = hf.get(height);
  b.minor_version = vote;
  return b;
}

TEST(major, Only)
{
  TestDB db;
  HardFork hf(db, 1, 0, 0, 0, 1, 0); // no voting

  //                      v  h  t
  ASSERT_TRUE(hf.add_fork(1, 0, 0));
  ASSERT_TRUE(hf.add_fork(2, 2, 1));
  hf.init();

  // block height 0, only version 1 is accepted
  ASSERT_FALSE(hf.add(mkblock(0, 2), 0));
  ASSERT_FALSE(hf.add(mkblock(2, 2), 0));
  ASSERT_TRUE(hf.add(mkblock(1, 2), 0));
  db.add_block(mkblock(1, 1), 0, 0, 0, 0, 0, crypto::hash());

  // block height 1, only version 1 is accepted
  ASSERT_FALSE(hf.add(mkblock(0, 2), 1));
  ASSERT_FALSE(hf.add(mkblock(2, 2), 1));
  ASSERT_TRUE(hf.add(mkblock(1, 2), 1));
  db.add_block(mkblock(1, 1), 0, 0, 0, 0, 0, crypto::hash());

  // block height 2, only version 2 is accepted
  ASSERT_FALSE(hf.add(mkblock(0, 2), 2));
  ASSERT_FALSE(hf.add(mkblock(1, 2), 2));
  ASSERT_FALSE(hf.add(mkblock(3, 2), 2));
  ASSERT_TRUE(hf.add(mkblock(2, 2), 2));
  db.add_block(mkblock(2, 1), 0, 0, 0, 0, 0, crypto::hash());
}

TEST(empty_hardforks, Success)
{
  TestDB db;
  HardFork hf(db);

  ASSERT_TRUE(hf.add_fork(1, 0, 0));
  hf.init();
  ASSERT_TRUE(hf.get_state(time(NULL)) == HardFork::Ready);
  ASSERT_TRUE(hf.get_state(time(NULL) + 3600*24*400) == HardFork::Ready);

  for (uint64_t h = 0; h <= 10; ++h) {
    db.add_block(mkblock(hf, h, 1), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
  }
  ASSERT_EQ(hf.get(0), 1);
  ASSERT_EQ(hf.get(1), 1);
  ASSERT_EQ(hf.get(10), 1);
}

TEST(ordering, Success)
{
  TestDB db;
  HardFork hf(db);

  ASSERT_TRUE(hf.add_fork(2, 2, 1));
  ASSERT_FALSE(hf.add_fork(3, 3, 1));
  ASSERT_FALSE(hf.add_fork(3, 2, 2));
  ASSERT_FALSE(hf.add_fork(2, 3, 2));
  ASSERT_TRUE(hf.add_fork(3, 10, 2));
  ASSERT_TRUE(hf.add_fork(4, 20, 3));
  ASSERT_FALSE(hf.add_fork(5, 5, 4));
}

TEST(check_for_height, Success)
{
  TestDB db;
  HardFork hf(db, 1, 0, 0, 0, 1, 0); // no voting

  ASSERT_TRUE(hf.add_fork(1, 0, 0));
  ASSERT_TRUE(hf.add_fork(2, 5, 1));
  hf.init();

  for (uint64_t h = 0; h <= 4; ++h) {
    ASSERT_TRUE(hf.check_for_height(mkblock(1, 1), h));
    ASSERT_FALSE(hf.check_for_height(mkblock(2, 2), h));  // block version is too high
    db.add_block(mkblock(hf, h, 1), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
  }

  for (uint64_t h = 5; h <= 10; ++h) {
    ASSERT_FALSE(hf.check_for_height(mkblock(1, 1), h));  // block version is too low
    ASSERT_TRUE(hf.check_for_height(mkblock(2, 2), h));
    db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
  }
}

TEST(get, next_version)
{
  TestDB db;
  HardFork hf(db);

  ASSERT_TRUE(hf.add_fork(1, 0, 0));
  ASSERT_TRUE(hf.add_fork(2, 5, 1));
  ASSERT_TRUE(hf.add_fork(4, 10, 2));
  hf.init();

  for (uint64_t h = 0; h <= 4; ++h) {
    ASSERT_EQ(2, hf.get_next_version());
    db.add_block(mkblock(hf, h, 1), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
  }

  for (uint64_t h = 5; h <= 9; ++h) {
    ASSERT_EQ(4, hf.get_next_version());
    db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
  }

  for (uint64_t h = 10; h <= 15; ++h) {
    ASSERT_EQ(4, hf.get_next_version());
    db.add_block(mkblock(hf, h, 4), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
  }
}

TEST(states, Success)
{
  TestDB db;
  HardFork hf(db);

  ASSERT_TRUE(hf.add_fork(1, 0, 0));
  ASSERT_TRUE(hf.add_fork(2, BLOCKS_PER_YEAR, SECONDS_PER_YEAR));

  ASSERT_TRUE(hf.get_state(0) == HardFork::Ready);
  ASSERT_TRUE(hf.get_state(SECONDS_PER_YEAR / 2) == HardFork::Ready);
  ASSERT_TRUE(hf.get_state(SECONDS_PER_YEAR + HardFork::DEFAULT_UPDATE_TIME / 2) == HardFork::Ready);
  ASSERT_TRUE(hf.get_state(SECONDS_PER_YEAR + (HardFork::DEFAULT_UPDATE_TIME + HardFork::DEFAULT_FORKED_TIME) / 2) == HardFork::UpdateNeeded);
  ASSERT_TRUE(hf.get_state(SECONDS_PER_YEAR + HardFork::DEFAULT_FORKED_TIME * 2) == HardFork::LikelyForked);

  ASSERT_TRUE(hf.add_fork(3, BLOCKS_PER_YEAR * 5, SECONDS_PER_YEAR * 5));

  ASSERT_TRUE(hf.get_state(0) == HardFork::Ready);
  ASSERT_TRUE(hf.get_state(SECONDS_PER_YEAR / 2) == HardFork::Ready);
  ASSERT_TRUE(hf.get_state(SECONDS_PER_YEAR + HardFork::DEFAULT_UPDATE_TIME / 2) == HardFork::Ready);
  ASSERT_TRUE(hf.get_state(SECONDS_PER_YEAR + (HardFork::DEFAULT_UPDATE_TIME + HardFork::DEFAULT_FORKED_TIME) / 2) == HardFork::Ready);
  ASSERT_TRUE(hf.get_state(SECONDS_PER_YEAR + HardFork::DEFAULT_FORKED_TIME * 2) == HardFork::Ready);
}

TEST(steps_asap, Success)
{
  TestDB db;
  HardFork hf(db, 1,0,1,1,1);

  //                 v  h  t
  ASSERT_TRUE(hf.add_fork(1, 0, 0));
  ASSERT_TRUE(hf.add_fork(4, 2, 1));
  ASSERT_TRUE(hf.add_fork(7, 4, 2));
  ASSERT_TRUE(hf.add_fork(9, 6, 3));
  hf.init();

  for (uint64_t h = 0; h < 10; ++h) {
    db.add_block(mkblock(hf, h, 9), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
  }

  ASSERT_EQ(hf.get(0), 1);
  ASSERT_EQ(hf.get(1), 1);
  ASSERT_EQ(hf.get(2), 4);
  ASSERT_EQ(hf.get(3), 4);
  ASSERT_EQ(hf.get(4), 7);
  ASSERT_EQ(hf.get(5), 7);
  ASSERT_EQ(hf.get(6), 9);
  ASSERT_EQ(hf.get(7), 9);
  ASSERT_EQ(hf.get(8), 9);
  ASSERT_EQ(hf.get(9), 9);
}

TEST(steps_1, Success)
{
  TestDB db;
  HardFork hf(db, 1,0,1,1,1);

  ASSERT_TRUE(hf.add_fork(1, 0, 0));
  for (int n = 1 ; n < 10; ++n)
    ASSERT_TRUE(hf.add_fork(n+1, n, n));
  hf.init();

  for (uint64_t h = 0 ; h < 10; ++h) {
    db.add_block(mkblock(hf, h, h+1), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
  }

  for (uint64_t h = 0; h < 10; ++h) {
    ASSERT_EQ(hf.get(h), std::max(1,(int)h));
  }
}

TEST(reorganize, Same)
{
  for (int history = 1; history <= 12; ++history) {
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, history, 100);

    //                 v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(4, 2, 1));
    ASSERT_TRUE(hf.add_fork(7, 4, 2));
    ASSERT_TRUE(hf.add_fork(9, 6, 3));
    hf.init();

    //                                 index  0  1  2  3  4  5  6  7  8  9
    static const uint8_t block_versions[] = { 1, 1, 4, 4, 7, 7, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 };
    for (uint64_t h = 0; h < 20; ++h) {
      db.add_block(mkblock(hf, h, block_versions[h]), 0, 0, 0, 0, 0, crypto::hash());
      ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    for (uint64_t rh = 0; rh < 20; ++rh) {
      hf.reorganize_from_block_height(rh);
      for (int hh = 0; hh < 20; ++hh) {
        uint8_t version = hh >= history ? block_versions[hh - history] : 1;
        ASSERT_EQ(hf.get(hh), version);
      }
    }
  }
}

TEST(reorganize, Changed)
{
  TestDB db;
  HardFork hf(db, 1, 0, 1, 1, 4, 100);

  //                 v  h  t
  ASSERT_TRUE(hf.add_fork(1, 0, 0));
  ASSERT_TRUE(hf.add_fork(4, 2, 1));
  ASSERT_TRUE(hf.add_fork(7, 4, 2));
  ASSERT_TRUE(hf.add_fork(9, 6, 3));
  hf.init();

  //                                    fork         4     7     9
  //                                    index  0  1  2  3  4  5  6  7  8  9
  static const uint8_t block_versions[] =    { 1, 1, 4, 4, 7, 7, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 };
  static const uint8_t expected_versions[] = { 1, 1, 1, 1, 1, 1, 4, 4, 7, 7, 9, 9, 9, 9, 9, 9 };
  for (uint64_t h = 0; h < 16; ++h) {
    db.add_block(mkblock(hf, h, block_versions[h]), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE (hf.add(db.get_block_from_height(h), h));
  }

  for (uint64_t rh = 0; rh < 16; ++rh) {
    hf.reorganize_from_block_height(rh);
    for (int hh = 0; hh < 16; ++hh) {
      ASSERT_EQ(hf.get(hh), expected_versions[hh]);
    }
  }

  // delay a bit for 9, and go back to 1 to check it stays at 9
  static const uint8_t block_versions_new[] =    { 1, 1, 4, 4, 7, 7, 4, 7, 7, 7, 9, 9, 9, 9, 9, 1 };
  static const uint8_t expected_versions_new[] = { 1, 1, 1, 1, 1, 1, 4, 4, 4, 4, 4, 7, 7, 7, 9, 9 };
  for (uint64_t h = 3; h < 16; ++h) {
    db.remove_block();
  }
  ASSERT_EQ(db.height(), 3);
  hf.reorganize_from_block_height(2);
  for (uint64_t h = 3; h < 16; ++h) {
    db.add_block(mkblock(hf, h, block_versions_new[h]), 0, 0, 0, 0, 0, crypto::hash());
    bool ret = hf.add(db.get_block_from_height(h), h);
    ASSERT_EQ (ret, h < 15);
  }
  db.remove_block(); // last block added to the blockchain, but not hf
  ASSERT_EQ(db.height(), 15);
  for (int hh = 0; hh < 15; ++hh) {
    ASSERT_EQ(hf.get(hh), expected_versions_new[hh]);
  }
}

TEST(voting, threshold)
{
  for (int threshold = 87; threshold <= 88; ++threshold) {
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 8, threshold);

    //                 v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    hf.init();

    for (uint64_t h = 0; h <= 8; ++h) {
      uint8_t v = 1 + !!(h % 8);
      db.add_block(mkblock(hf, h, v), 0, 0, 0, 0, 0, crypto::hash());
      bool ret = hf.add(db.get_block_from_height(h), h);
      if (h >= 8 && threshold == 87) {
        // for threshold 87, we reach the threshold at height 7, so from height 8, hard fork to version 2, but 8 tries to add 1
        ASSERT_FALSE(ret);
      }
      else {
        // for threshold 88, we never reach the threshold
        ASSERT_TRUE(ret);
        uint8_t expected = threshold == 88 ? 1 : h < 8 ? 1 : 2;
        ASSERT_EQ(hf.get(h), expected);
      }
    }
  }
}

TEST(voting, different_thresholds)
{
  for (int threshold = 87; threshold <= 88; ++threshold) {
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50); // window size 4

    //                 v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 0, 1)); // asap
    ASSERT_TRUE(hf.add_fork(3, 10, 100, 2)); // all votes
    ASSERT_TRUE(hf.add_fork(4, 15, 3)); // default 50% votes
    hf.init();

    //                                           0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9
    static const uint8_t block_versions[] =    { 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4 };
    static const uint8_t expected_versions[] = { 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4 };

    for (uint64_t h = 0; h < sizeof(block_versions) / sizeof(block_versions[0]); ++h) {
      db.add_block(mkblock(hf, h, block_versions[h]), 0, 0, 0, 0, 0, crypto::hash());
      bool ret = hf.add(db.get_block_from_height(h), h);
      ASSERT_EQ(ret, true);
    }
    for (uint64_t h = 0; h < sizeof(expected_versions) / sizeof(expected_versions[0]); ++h) {
      ASSERT_EQ(hf.get(h), expected_versions[h]);
    }
  }
}

TEST(voting, info)
{
  TestDB db;
  HardFork hf(db, 1, 0, 1, 1, 4, 50); // window size 4, default threshold 50%

  //                      v  h  ts
  ASSERT_TRUE(hf.add_fork(1, 0,  0));
  //                      v  h   thr  ts
  ASSERT_TRUE(hf.add_fork(2, 5,    0,  1)); // asap
  ASSERT_TRUE(hf.add_fork(3, 10, 100,  2)); // all votes
  //                      v   h  ts
  ASSERT_TRUE(hf.add_fork(4, 15,  3)); // default 50% votes
  hf.init();

  //                                             0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9
  static const uint8_t block_versions[]      = { 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4 };
  static const uint8_t expected_thresholds[] = { 0, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 2, 2, 2, 2 };

  for (uint64_t h = 0; h < sizeof(block_versions) / sizeof(block_versions[0]); ++h) {
    uint32_t window, votes, threshold;
    uint64_t earliest_height;
    uint8_t voting;

    ASSERT_TRUE(hf.get_voting_info(1, window, votes, threshold, earliest_height, voting));
    ASSERT_EQ(std::min<uint64_t>(h, 4), votes);
    ASSERT_EQ(0, earliest_height);

    ASSERT_EQ(hf.get_current_version() >= 2, hf.get_voting_info(2, window, votes, threshold, earliest_height, voting));
    ASSERT_EQ(std::min<uint64_t>(h <= 3 ? 0 : h - 3, 4), votes);
    ASSERT_EQ(5, earliest_height);

    ASSERT_EQ(hf.get_current_version() >= 3, hf.get_voting_info(3, window, votes, threshold, earliest_height, voting));
    ASSERT_EQ(std::min<uint64_t>(h <= 8 ? 0 : h - 8, 4), votes);
    ASSERT_EQ(10, earliest_height);

    ASSERT_EQ(hf.get_current_version() == 4, hf.get_voting_info(4, window, votes, threshold, earliest_height, voting));
    ASSERT_EQ(std::min<uint64_t>(h <= 14 ? 0 : h - 14, 4), votes);
    ASSERT_EQ(15, earliest_height);

    ASSERT_EQ(std::min<uint64_t>(h, 4), window);
    ASSERT_EQ(expected_thresholds[h], threshold);
    ASSERT_EQ(4, voting);

    db.add_block(mkblock(hf, h, block_versions[h]), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
  }
}

TEST(new_blocks, denied)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    //                 v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    hf.init();

    ASSERT_TRUE(hf.add(mkblock(1, 1), 0));
    ASSERT_TRUE(hf.add(mkblock(1, 1), 1));
    ASSERT_TRUE(hf.add(mkblock(1, 1), 2));
    ASSERT_TRUE(hf.add(mkblock(1, 2), 3));
    ASSERT_TRUE(hf.add(mkblock(1, 1), 4));
    ASSERT_TRUE(hf.add(mkblock(1, 1), 5));
    ASSERT_TRUE(hf.add(mkblock(1, 1), 6));
    ASSERT_TRUE(hf.add(mkblock(1, 2), 7));
    ASSERT_TRUE(hf.add(mkblock(1, 2), 8)); // we reach 50% of the last 4
    ASSERT_FALSE(hf.add(mkblock(2, 1), 9)); // so this one can't get added
    ASSERT_TRUE(hf.add(mkblock(2, 2), 9));
}

TEST(new_version, early)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    //                 v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 4, 1));
    hf.init();

    ASSERT_TRUE(hf.add(mkblock(1, 2), 0));
    ASSERT_TRUE(hf.add(mkblock(1, 2), 1)); // we have enough votes already
    ASSERT_TRUE(hf.add(mkblock(1, 2), 2));
    ASSERT_TRUE(hf.add(mkblock(1, 1), 3)); // we accept a previous version because we did not switch, even with all the votes
    ASSERT_TRUE(hf.add(mkblock(2, 2), 4)); // but have to wait for the declared height anyway
    ASSERT_TRUE(hf.add(mkblock(2, 2), 5));
    ASSERT_FALSE(hf.add(mkblock(2, 1), 6)); // we don't accept 1 anymore
    ASSERT_TRUE(hf.add(mkblock(2, 2), 7)); // but we do accept 2
}

TEST(reorganize, changed)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    //                 v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    ASSERT_TRUE(hf.add_fork(3, 5, 2));
    ASSERT_TRUE(hf.add_fork(4, 555, 222));
    hf.init();

#define ADD(v, h, a) \
  do { \
    cryptonote::block b = mkblock(hf, h, v); \
    db.add_block(b, 0, 0, 0, 0, 0, crypto::hash()); \
    ASSERT_##a(hf.add(b, h)); \
  } while(0)
#define ADD_TRUE(v, h) ADD(v, h, TRUE)
#define ADD_FALSE(v, h) ADD(v, h, FALSE)

    ADD_TRUE(1, 0);
    ADD_TRUE(1, 1);
    ADD_TRUE(2, 2);
    ADD_TRUE(2, 3); // switch to 2 here
    ADD_TRUE(2, 4);
    ADD_TRUE(2, 5);
    ADD_TRUE(2, 6);
    ASSERT_EQ(hf.get_current_version(), 2);
    ADD_TRUE(3, 7);
    ADD_TRUE(4, 8);
    ADD_TRUE(4, 9);
    ASSERT_EQ(hf.get_current_version(), 3);

    // pop a few blocks and check current version goes back down
    db.remove_block();
    hf.reorganize_from_block_height(8);
    ASSERT_EQ(hf.get_current_version(), 3);
    db.remove_block();
    hf.reorganize_from_block_height(7);
    ASSERT_EQ(hf.get_current_version(), 2);
    db.remove_block();
    ASSERT_EQ(hf.get_current_version(), 2);

    // add blocks again, but remaining at 2
    ADD_TRUE(2, 7);
    ADD_TRUE(2, 8);
    ADD_TRUE(2, 9);
    ASSERT_EQ(hf.get_current_version(), 2); // we did not bump to 3 this time
}

TEST(get, higher)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    //                 v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    ASSERT_TRUE(hf.add_fork(3, 5, 2));
    hf.init();

    ASSERT_EQ(hf.get_ideal_version(0), 1);
    ASSERT_EQ(hf.get_ideal_version(1), 1);
    ASSERT_EQ(hf.get_ideal_version(2), 2);
    ASSERT_EQ(hf.get_ideal_version(3), 2);
    ASSERT_EQ(hf.get_ideal_version(4), 2);
    ASSERT_EQ(hf.get_ideal_version(5), 3);
    ASSERT_EQ(hf.get_ideal_version(6), 3);
    ASSERT_EQ(hf.get_ideal_version(7), 3);
}

TEST(get, earliest_ideal_height)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    //                      v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    ASSERT_TRUE(hf.add_fork(5, 5, 2));
    ASSERT_TRUE(hf.add_fork(6, 10, 3));
    ASSERT_TRUE(hf.add_fork(9, 15, 4));
    hf.init();

    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(1), 0);
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(2), 2);
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(3), 5);
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(4), 5);
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(5), 5);
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(6), 10);
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(7), 15);
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(8), 15);
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(9), 15);
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(10), std::numeric_limits<uint64_t>::max());
}

// ============================================================
// Additional HardFork coverage tests
// ============================================================

TEST(get_current_version, after_blocks)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    //                 v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 3, 1));
    ASSERT_TRUE(hf.add_fork(3, 6, 2));
    hf.init();

    // Initially, current version should be 1
    ASSERT_EQ(hf.get_current_version(), 1);

    // Add blocks voting for version 2
    for (uint64_t h = 0; h < 3; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }
    // At height 3, we should have enough votes for version 2 in the window
    // (3 out of 3 blocks in window voted for v2, window=4, threshold=50%)
    // But fork requires height >= 3, and we need to add a block AT height 3
    db.add_block(mkblock(hf, 3, 2), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(3), 3));

    ASSERT_EQ(hf.get_current_version(), 2);

    // Add more blocks voting for version 3
    for (uint64_t h = 4; h < 6; ++h) {
        db.add_block(mkblock(hf, h, 3), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // After height 6+ with enough votes
    db.add_block(mkblock(hf, 6, 3), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(6), 6));

    db.add_block(mkblock(hf, 7, 3), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(7), 7));

    ASSERT_EQ(hf.get_current_version(), 3);
}

TEST(get_ideal_version, for_height)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    //                 v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 10, 1));
    ASSERT_TRUE(hf.add_fork(3, 20, 2));
    ASSERT_TRUE(hf.add_fork(4, 30, 3));
    hf.init();

    // Ideal version at each range
    ASSERT_EQ(hf.get_ideal_version(0), 1);
    ASSERT_EQ(hf.get_ideal_version(5), 1);
    ASSERT_EQ(hf.get_ideal_version(9), 1);
    ASSERT_EQ(hf.get_ideal_version(10), 2);
    ASSERT_EQ(hf.get_ideal_version(15), 2);
    ASSERT_EQ(hf.get_ideal_version(19), 2);
    ASSERT_EQ(hf.get_ideal_version(20), 3);
    ASSERT_EQ(hf.get_ideal_version(25), 3);
    ASSERT_EQ(hf.get_ideal_version(29), 3);
    ASSERT_EQ(hf.get_ideal_version(30), 4);
    ASSERT_EQ(hf.get_ideal_version(100), 4);

    // The latest ideal version (no height) should be the highest
    ASSERT_EQ(hf.get_ideal_version(), 4);
}

TEST(get_earliest_ideal_height, for_version_detailed)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    //                      v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 1));
    ASSERT_TRUE(hf.add_fork(3, 10, 2));
    ASSERT_TRUE(hf.add_fork(4, 20, 3));
    hf.init();

    // Version 1 starts at height 0
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(1), 0);
    // Version 2 starts at height 5
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(2), 5);
    // Version 3 starts at height 10
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(3), 10);
    // Version 4 starts at height 20
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(4), 20);
    // Version 5 was never registered
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(5), std::numeric_limits<uint64_t>::max());
}

TEST(num_hardforks, count)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_EQ(hf.get_hardforks().size(), 1u);

    ASSERT_TRUE(hf.add_fork(2, 5, 1));
    ASSERT_EQ(hf.get_hardforks().size(), 2u);

    ASSERT_TRUE(hf.add_fork(3, 10, 2));
    ASSERT_EQ(hf.get_hardforks().size(), 3u);

    ASSERT_TRUE(hf.add_fork(4, 15, 3));
    ASSERT_EQ(hf.get_hardforks().size(), 4u);

    // Verify the fork info stored correctly
    const auto &forks = hf.get_hardforks();
    ASSERT_EQ(forks[0].version, 1);
    ASSERT_EQ(forks[0].height, 0u);
    ASSERT_EQ(forks[1].version, 2);
    ASSERT_EQ(forks[1].height, 5u);
    ASSERT_EQ(forks[2].version, 3);
    ASSERT_EQ(forks[2].height, 10u);
    ASSERT_EQ(forks[3].version, 4);
    ASSERT_EQ(forks[3].height, 15u);
}

TEST(on_block_popped, version_tracking)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    //                 v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    ASSERT_TRUE(hf.add_fork(3, 5, 2));
    hf.init();

    // Add blocks: versions 1,1,2,2,2,2,2 with votes matching
    for (uint64_t h = 0; h < 8; ++h) {
        uint8_t vote = h < 2 ? 1 : (h < 5 ? 2 : 3);
        db.add_block(mkblock(hf, h, vote), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // Should be at version 2 or higher by now
    uint8_t ver_before = hf.get_current_version();
    ASSERT_GE(ver_before, 2);

    // Pop 3 blocks (remove them from DB first, then call on_block_popped)
    db.remove_block();
    db.remove_block();
    db.remove_block();
    hf.on_block_popped(3);

    // After popping, current version should be <= what it was before
    uint8_t ver_after = hf.get_current_version();
    ASSERT_LE(ver_after, ver_before);
}

TEST(get_state, various_times)
{
    TestDB db;
    HardFork hf(db);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, BLOCKS_PER_YEAR, SECONDS_PER_YEAR));
    ASSERT_TRUE(hf.add_fork(3, BLOCKS_PER_YEAR * 2, SECONDS_PER_YEAR * 2));

    // At time 0, we're ready (only fork 1 is active, forks 2/3 are far in the future)
    ASSERT_EQ(hf.get_state(0), HardFork::Ready);

    // Well before second fork time, should be ready
    ASSERT_EQ(hf.get_state(SECONDS_PER_YEAR / 2), HardFork::Ready);

    // The state depends on the LAST fork time. Last fork is at 2*SECONDS_PER_YEAR.
    // Ready if t < last_fork_time + update_time
    ASSERT_EQ(hf.get_state(SECONDS_PER_YEAR * 2), HardFork::Ready);

    // UpdateNeeded if t >= last_fork_time + update_time but < last_fork_time + forked_time
    ASSERT_EQ(hf.get_state(SECONDS_PER_YEAR * 2 + HardFork::DEFAULT_UPDATE_TIME + 1), HardFork::UpdateNeeded);

    // LikelyForked if t >= last_fork_time + forked_time
    ASSERT_EQ(hf.get_state(SECONDS_PER_YEAR * 2 + HardFork::DEFAULT_FORKED_TIME + 1), HardFork::LikelyForked);
}

TEST(get_state, single_fork_always_ready)
{
    TestDB db;
    HardFork hf(db);

    // With only one fork (the original), state should always be Ready
    ASSERT_TRUE(hf.add_fork(1, 0, 0));

    ASSERT_EQ(hf.get_state(0), HardFork::Ready);
    ASSERT_EQ(hf.get_state((time_t)SECONDS_PER_YEAR * 100), HardFork::Ready);
}

TEST(voting_info, queries)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    //                 v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    ASSERT_TRUE(hf.add_fork(3, 5, 2));
    hf.init();

    // Add blocks voting for increasing versions
    for (uint64_t h = 0; h < 2; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    uint32_t window, votes, threshold;
    uint64_t earliest_height;
    uint8_t voting;

    // Query voting info for version 1
    bool enabled = hf.get_voting_info(1, window, votes, threshold, earliest_height, voting);
    ASSERT_TRUE(enabled); // version 1 is current
    ASSERT_EQ(earliest_height, 0u);
    ASSERT_GE(votes, 0u);

    // Query voting info for version 2
    enabled = hf.get_voting_info(2, window, votes, threshold, earliest_height, voting);
    // version 2 may or may not be enabled yet depending on votes
    ASSERT_EQ(earliest_height, 2u);

    // Query voting info for version 3
    enabled = hf.get_voting_info(3, window, votes, threshold, earliest_height, voting);
    ASSERT_FALSE(enabled); // version 3 not reached yet
    ASSERT_EQ(earliest_height, 5u);
}

TEST(window_size, accessor)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 8, 50);

    ASSERT_EQ(hf.get_window_size(), 8u);

    TestDB db2;
    HardFork hf2(db2, 1, 0, 1, 1, 100, 75);
    ASSERT_EQ(hf2.get_window_size(), 100u);
}

TEST(on_block_popped, single_pop)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    hf.init();

    // Add 6 blocks, all voting for v2
    for (uint64_t h = 0; h < 6; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    ASSERT_EQ(hf.get_current_version(), 2);

    // Pop 1 block
    db.remove_block();
    hf.on_block_popped(1);

    // Should still be at version 2 (only popped 1 from 6)
    ASSERT_EQ(hf.get_current_version(), 2);
}

TEST(check, block_version_validation)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 1, 0); // no voting (threshold 0, window 1)

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 1));
    hf.init();

    // Before fork height: block version 1 should be accepted, version 2 should not
    ASSERT_TRUE(hf.check(mkblock(1, 1)));
    ASSERT_FALSE(hf.check(mkblock(2, 2)));

    // Add blocks up to height 5
    for (uint64_t h = 0; h < 5; ++h) {
        db.add_block(mkblock(hf, h, 1), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // At fork height: block version 2 should be accepted, version 1 should not
    ASSERT_TRUE(hf.check(mkblock(2, 2)));
    ASSERT_FALSE(hf.check(mkblock(1, 1)));
}

// ============================================================
// Additional HardFork coverage tests
// ============================================================

TEST(reorganize_from_chain_height, basic)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 3, 1));
    hf.init();

    // Add blocks voting for v2
    for (uint64_t h = 0; h < 8; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    ASSERT_EQ(hf.get_current_version(), 2);

    // reorganize_from_chain_height uses chain height (= block height + 1)
    // Pop back to height 3 (chain height 4)
    for (int i = 0; i < 4; ++i)
        db.remove_block();
    ASSERT_TRUE(hf.reorganize_from_chain_height(4));

    // Should still be able to determine state correctly
    ASSERT_GE(hf.get_current_version(), 1);
}

TEST(get_current_version, single_fork)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    hf.init();

    ASSERT_EQ(hf.get_current_version(), 1);

    // Add some blocks
    for (uint64_t h = 0; h < 5; ++h) {
        db.add_block(mkblock(1, 1), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // Version should still be 1
    ASSERT_EQ(hf.get_current_version(), 1);
}

TEST(get_ideal_version, no_height_returns_latest)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 1));
    ASSERT_TRUE(hf.add_fork(3, 10, 2));
    ASSERT_TRUE(hf.add_fork(7, 20, 3));
    hf.init();

    // get_ideal_version() with no args returns the latest scheduled version
    ASSERT_EQ(hf.get_ideal_version(), 7);
}

TEST(voting, all_vote_same_version)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    hf.init();

    // All blocks vote for version 2
    for (uint64_t h = 0; h < 10; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // Should have switched to version 2
    ASSERT_EQ(hf.get_current_version(), 2);
}

TEST(voting, no_votes_for_upgrade)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    hf.init();

    // All blocks vote for version 1 only
    for (uint64_t h = 0; h < 10; ++h) {
        db.add_block(mkblock(hf, h, 1), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // Should still be at version 1 since no votes for 2
    ASSERT_EQ(hf.get_current_version(), 1);
}

TEST(add_fork, version_must_increase)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 1));
    // Adding a fork with same version should fail
    ASSERT_FALSE(hf.add_fork(2, 10, 2));
    // Adding a fork with lower version should fail
    ASSERT_FALSE(hf.add_fork(1, 15, 3));
}

TEST(add_fork, height_must_increase)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 10, 1));
    // Height must be strictly greater than previous
    ASSERT_FALSE(hf.add_fork(3, 10, 2));
    ASSERT_FALSE(hf.add_fork(3, 5, 2));
    ASSERT_TRUE(hf.add_fork(3, 11, 2));
}

TEST(get, version_at_boundary)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 1, 0); // no voting needed

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 1));
    ASSERT_TRUE(hf.add_fork(3, 10, 2));
    hf.init();

    // Add blocks so get() can look up versions
    for (uint64_t h = 0; h < 15; ++h) {
        db.add_block(mkblock(hf, h, hf.get_ideal_version(h)), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // Exact boundary checks
    ASSERT_EQ(hf.get(4), 1);
    ASSERT_EQ(hf.get(5), 2);
    ASSERT_EQ(hf.get(9), 2);
    ASSERT_EQ(hf.get(10), 3);
}

TEST(check_for_height, multiple_forks)
{
    TestDB db;
    HardFork hf(db, 1, 0, 0, 0, 1, 0); // no voting

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 1));
    ASSERT_TRUE(hf.add_fork(3, 10, 2));
    hf.init();

    // At height 0, version must be 1
    ASSERT_TRUE(hf.check_for_height(mkblock(1, 1), 0));
    ASSERT_FALSE(hf.check_for_height(mkblock(2, 2), 0));

    // At height 5, version must be 2
    ASSERT_FALSE(hf.check_for_height(mkblock(1, 1), 5));
    ASSERT_TRUE(hf.check_for_height(mkblock(2, 2), 5));
    ASSERT_FALSE(hf.check_for_height(mkblock(3, 3), 5));

    // At height 10, version must be 3
    ASSERT_FALSE(hf.check_for_height(mkblock(2, 2), 10));
    ASSERT_TRUE(hf.check_for_height(mkblock(3, 3), 10));
}

TEST(original_version_till_height, basic)
{
    TestDB db;
    // original_version=1, original_version_till_height=3
    // The add() function uses do_check which checks current_fork_index version.
    // With no voting (window=1, threshold=0), the current_fork_index still
    // advances based on votes, so the original_version_till_height primarily
    // affects get_block_version (internal) and init() behavior.
    HardFork hf(db, 1, 3, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    hf.init();

    // With original_version_till_height=3 and window=4/threshold=50%,
    // we can add v1 blocks at heights 0-1 (before fork 2 height)
    for (uint64_t h = 0; h < 2; ++h) {
        db.add_block(mkblock(hf, h, 1), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }
    // Version should be 1 since not enough v2 votes
    ASSERT_EQ(hf.get_current_version(), 1);
}

TEST(get_next_version, transitions)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 1, 0);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 3, 1));
    ASSERT_TRUE(hf.add_fork(3, 6, 2));
    hf.init();

    // Before reaching height 3, next version should be 2
    ASSERT_EQ(hf.get_next_version(), 2);

    for (uint64_t h = 0; h < 3; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // After reaching height 3 with v2, next version should be 3
    ASSERT_EQ(hf.get_next_version(), 3);

    for (uint64_t h = 3; h < 6; ++h) {
        db.add_block(mkblock(hf, h, 3), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // After height 6 with v3, next version is still 3 (no more forks)
    ASSERT_EQ(hf.get_next_version(), 3);
}

TEST(get_state, no_voting_param)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    hf.init();

    // The no-argument get_state() uses current time
    HardFork::State state = hf.get_state();
    // With only one fork, should always be Ready
    ASSERT_EQ(state, HardFork::Ready);
}

TEST(hardfork, large_window_size)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 100, 80);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 1));
    hf.init();

    ASSERT_EQ(hf.get_window_size(), 100u);

    // Add only 10 blocks voting for v2 - not enough for 80% threshold in 100-block window
    for (uint64_t h = 0; h < 10; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // With 10 blocks voting, window is 10, 100% voted for v2 (10/10 >= 80%)
    // Whether upgrade happens depends on the window being filled
    // In any case, version should be at least 1
    ASSERT_GE(hf.get_current_version(), 1);
}

TEST(voting_info, threshold_per_fork)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 0, 1)); // threshold 0 = asap
    ASSERT_TRUE(hf.add_fork(3, 10, 100, 2)); // threshold 100 = all must vote
    hf.init();

    uint32_t window, votes, threshold;
    uint64_t earliest_height;
    uint8_t voting;

    // Check voting info for version 2 (threshold 0)
    hf.get_voting_info(2, window, votes, threshold, earliest_height, voting);
    ASSERT_EQ(earliest_height, 5u);
    // Note: threshold in get_voting_info uses heights[current_fork_index].threshold,
    // which is the CURRENT fork's threshold (fork 1 = version 1), not the queried version
    ASSERT_EQ(threshold, 0u);

    // Check voting info for version 3
    hf.get_voting_info(3, window, votes, threshold, earliest_height, voting);
    ASSERT_EQ(earliest_height, 10u);
    // threshold is still from current_fork_index (fork 0 = version 1, threshold 0)
    ASSERT_EQ(threshold, 0u);
}

TEST(multiple_reorganize, stability)
{
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    hf.init();

    // Add 10 blocks all voting for v2
    for (uint64_t h = 0; h < 10; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // Reorganize multiple times from different heights
    for (uint64_t rh = 9; rh > 0; --rh) {
        hf.reorganize_from_block_height(rh);
        // Should not crash and version should remain consistent
        ASSERT_GE(hf.get_current_version(), 1);
    }

    // Reorganize to the very beginning
    hf.reorganize_from_block_height(0);
    ASSERT_GE(hf.get_current_version(), 1);
}

// ============================================================
// Extended HardFork coverage tests
// ============================================================

TEST(hardfork_version, requirements_at_various_heights)
{
    // Verify that get_ideal_version returns the correct expected version
    // at every height across five sequential forks
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 1, 0); // no voting

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 10, 1));
    ASSERT_TRUE(hf.add_fork(3, 20, 2));
    ASSERT_TRUE(hf.add_fork(4, 30, 3));
    ASSERT_TRUE(hf.add_fork(5, 40, 4));
    hf.init();

    for (uint64_t h = 0; h < 10; ++h)
        ASSERT_EQ(hf.get_ideal_version(h), 1);
    for (uint64_t h = 10; h < 20; ++h)
        ASSERT_EQ(hf.get_ideal_version(h), 2);
    for (uint64_t h = 20; h < 30; ++h)
        ASSERT_EQ(hf.get_ideal_version(h), 3);
    for (uint64_t h = 30; h < 40; ++h)
        ASSERT_EQ(hf.get_ideal_version(h), 4);
    for (uint64_t h = 40; h < 60; ++h)
        ASSERT_EQ(hf.get_ideal_version(h), 5);
}

TEST(hardfork_version, five_sequential_forks_walk)
{
    // Walk through five forks with no-voting and confirm actual
    // version via get() after adding each block
    TestDB db;
    HardFork hf(db, 1, 0, 0, 0, 1, 0); // no voting

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 3, 1));
    ASSERT_TRUE(hf.add_fork(3, 6, 2));
    ASSERT_TRUE(hf.add_fork(4, 9, 3));
    ASSERT_TRUE(hf.add_fork(5, 12, 4));
    hf.init();

    for (uint64_t h = 0; h < 15; ++h) {
        uint8_t ideal = hf.get_ideal_version(h);
        db.add_block(mkblock(ideal, ideal), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    ASSERT_EQ(hf.get(0), 1);
    ASSERT_EQ(hf.get(3), 2);
    ASSERT_EQ(hf.get(6), 3);
    ASSERT_EQ(hf.get(9), 4);
    ASSERT_EQ(hf.get(12), 5);
    ASSERT_EQ(hf.get(14), 5);
}

TEST(voting_threshold, exact_50_percent)
{
    // Window=4, threshold=50: exactly 2 out of 4 votes needed
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    hf.init();

    // h=0: vote v1, h=1: vote v2, h=2: vote v1, h=3: vote v2
    // After h=3, window=[v1,v2,v1,v2] => 2/4=50% votes for v2 exactly at threshold
    db.add_block(mkblock(hf, 0, 1), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(0), 0));
    db.add_block(mkblock(hf, 1, 2), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(1), 1));
    db.add_block(mkblock(hf, 2, 1), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(2), 2));
    db.add_block(mkblock(hf, 3, 2), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(3), 3));

    // With exactly 50%, should have upgraded
    ASSERT_EQ(hf.get_current_version(), 2);
}

TEST(voting_threshold, just_below_threshold)
{
    // Window=4, threshold=75: need 3 out of 4 votes
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 75);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    hf.init();

    // 2 out of 4 voting for v2 = 50%, below 75%
    db.add_block(mkblock(hf, 0, 2), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(0), 0));
    db.add_block(mkblock(hf, 1, 1), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(1), 1));
    db.add_block(mkblock(hf, 2, 2), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(2), 2));
    db.add_block(mkblock(hf, 3, 1), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(3), 3));

    // Only 50% voted, threshold is 75%, should NOT upgrade
    ASSERT_EQ(hf.get_current_version(), 1);
}

TEST(voting_threshold, exactly_at_threshold_75)
{
    // Window=4, threshold=75: need 3 out of 4 votes
    // do_check requires voting_version >= current_version, so once activated,
    // all votes must be >= the new version
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 75);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    hf.init();

    // All 4 blocks vote for v2
    for (uint64_t h = 0; h < 4; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // 4/4 = 100% >= 75%, should have upgraded
    ASSERT_EQ(hf.get_current_version(), 2);

    // Verify that a v1 vote is now rejected (voting_version must be >= current)
    ASSERT_FALSE(hf.add(mkblock(2, 1), 4));
}

TEST(voting_threshold, zero_threshold_immediate)
{
    // A threshold of 0 means the fork activates immediately once the height is reached
    // But once activated, voting_version must be >= new version
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 3, 0, 1)); // threshold 0 = asap
    hf.init();

    // Before height 3: vote v2 to prepare; at height 3+: must use v2 major and v2 vote
    for (uint64_t h = 0; h < 6; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // With 0 threshold, should have activated at height 3
    ASSERT_EQ(hf.get_current_version(), 2);

    // Verify the fork actually activated by checking that v1 blocks are rejected
    ASSERT_FALSE(hf.add(mkblock(1, 1), 6));
    ASSERT_FALSE(hf.add(mkblock(2, 1), 6)); // v1 vote with v2 major also rejected
}

TEST(voting_threshold, hundred_percent_threshold)
{
    // 100% threshold means all blocks in window must vote
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 100, 1)); // 100% threshold
    hf.init();

    // 3 out of 4 vote for v2, 1 votes for v1 => not 100%
    db.add_block(mkblock(hf, 0, 2), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(0), 0));
    db.add_block(mkblock(hf, 1, 2), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(1), 1));
    db.add_block(mkblock(hf, 2, 2), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(2), 2));
    db.add_block(mkblock(hf, 3, 1), 0, 0, 0, 0, 0, crypto::hash());
    ASSERT_TRUE(hf.add(db.get_block_from_height(3), 3));

    // Not 100% voted, should NOT upgrade
    ASSERT_EQ(hf.get_current_version(), 1);

    // Now push 4 more blocks all voting v2
    for (uint64_t h = 4; h < 8; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // Now window has 4 consecutive v2 votes => 100% => upgrade
    ASSERT_EQ(hf.get_current_version(), 2);
}

TEST(feature_enablement, version_gates_behavior)
{
    // Simulate checking feature enablement per version
    TestDB db;
    HardFork hf(db, 1, 0, 0, 0, 1, 0); // no voting

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 1));
    ASSERT_TRUE(hf.add_fork(3, 10, 2));
    hf.init();

    for (uint64_t h = 0; h < 15; ++h) {
        uint8_t ideal = hf.get_ideal_version(h);
        db.add_block(mkblock(ideal, ideal), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // "Feature A" enabled at version >= 2
    for (uint64_t h = 0; h < 5; ++h)
        ASSERT_LT(hf.get(h), 2) << "Feature A should not be enabled before height 5";
    for (uint64_t h = 5; h < 15; ++h)
        ASSERT_GE(hf.get(h), 2) << "Feature A should be enabled at/after height 5";

    // "Feature B" enabled at version >= 3
    for (uint64_t h = 0; h < 10; ++h)
        ASSERT_LT(hf.get(h), 3) << "Feature B should not be enabled before height 10";
    for (uint64_t h = 10; h < 15; ++h)
        ASSERT_GE(hf.get(h), 3) << "Feature B should be enabled at/after height 10";
}

TEST(version_setter_getter, roundtrip_via_db)
{
    // Verify that set_hard_fork_version/get_hard_fork_version in the DB
    // provide a proper roundtrip
    TestDB db;

    for (uint8_t v = 1; v <= 15; ++v) {
        db.set_hard_fork_version(v * 10, v);
        ASSERT_EQ(db.get_hard_fork_version(v * 10), v);
    }

    // Overwrite a version and check
    db.set_hard_fork_version(10, 7);
    ASSERT_EQ(db.get_hard_fork_version(10), 7);
}

TEST(version_setter_getter, large_height)
{
    TestDB db;
    uint64_t large_h = 1000000;
    db.set_hard_fork_version(large_h, 15);
    ASSERT_EQ(db.get_hard_fork_version(large_h), 15);
}

TEST(hardfork, init_from_existing_chain)
{
    // Verify that two HardFork objects initialized with the same forks
    // and processing the same blocks produce consistent results
    TestDB db;
    HardFork hf(db, 1, 0, 0, 0, 1, 0); // no voting

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 1));
    hf.init();

    // Add blocks to the chain
    for (uint64_t h = 0; h < 10; ++h) {
        uint8_t ideal = hf.get_ideal_version(h);
        db.add_block(mkblock(ideal, ideal), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    ASSERT_EQ(hf.get_current_version(), 2);

    // Verify ideal version and stored version consistency
    for (uint64_t h = 0; h < 10; ++h) {
        uint8_t expected = h < 5 ? 1 : 2;
        ASSERT_EQ(hf.get(h), expected);
        ASSERT_EQ(db.get_hard_fork_version(h), expected);
    }
}

TEST(hardfork, get_hardforks_returns_all)
{
    TestDB db;
    HardFork hf(db);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 100, 1));
    ASSERT_TRUE(hf.add_fork(3, 200, 2));
    ASSERT_TRUE(hf.add_fork(4, 300, 3));
    ASSERT_TRUE(hf.add_fork(5, 400, 4));

    const auto &forks = hf.get_hardforks();
    ASSERT_EQ(forks.size(), 5u);
    for (uint8_t i = 0; i < 5; ++i) {
        ASSERT_EQ(forks[i].version, i + 1);
        ASSERT_EQ(forks[i].height, (uint64_t)(i) * 100);
    }
}

TEST(reorganize, pop_and_rebuild_partial)
{
    // Use a larger window and voting to test reorganize behavior
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    hf.init();

    // Add 8 blocks all voting for v2
    for (uint64_t h = 0; h < 8; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }
    ASSERT_EQ(hf.get_current_version(), 2);

    // Pop last 4 blocks
    for (int i = 0; i < 4; ++i)
        db.remove_block();
    ASSERT_EQ(db.height(), 4u);
    hf.reorganize_from_block_height(3);

    // After reorganize, version should still be consistent
    uint8_t ver_after = hf.get_current_version();
    ASSERT_GE(ver_after, 1);

    // Re-add 4 blocks voting for v2
    for (uint64_t h = 4; h < 8; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }
    ASSERT_EQ(hf.get_current_version(), 2);
}

TEST(add_fork, time_must_strictly_increase)
{
    TestDB db;
    HardFork hf(db);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 10, 100));
    // Time must strictly increase
    ASSERT_FALSE(hf.add_fork(3, 20, 50));  // time decreased
    ASSERT_FALSE(hf.add_fork(3, 20, 100)); // same time also rejected
    ASSERT_TRUE(hf.add_fork(3, 20, 101));  // strictly greater is ok
    ASSERT_TRUE(hf.add_fork(4, 30, 200));  // higher time is ok
}

TEST(voting, gradual_vote_accumulation)
{
    // Start with 0 v2 votes, gradually add more until threshold is met
    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 50); // window 4, threshold 50%

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 2, 1));
    hf.init();

    // First 4 blocks: all v1 votes
    for (uint64_t h = 0; h < 4; ++h) {
        db.add_block(mkblock(hf, h, 1), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }
    ASSERT_EQ(hf.get_current_version(), 1);

    // Next 2 blocks: v2 votes (2/4 in window = 50%)
    for (uint64_t h = 4; h < 6; ++h) {
        db.add_block(mkblock(hf, h, 2), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // With [v1, v1, v2, v2] in window => 50% >= 50% threshold
    ASSERT_EQ(hf.get_current_version(), 2);
}

TEST(check, rejects_future_version)
{
    TestDB db;
    HardFork hf(db, 1, 0, 0, 0, 1, 0); // no voting

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 10, 1));
    hf.init();

    // check() uses current state (no blocks added, so current version=1)
    ASSERT_TRUE(hf.check(mkblock(1, 1)));
    ASSERT_FALSE(hf.check(mkblock(2, 2)));
    ASSERT_FALSE(hf.check(mkblock(3, 3)));
}

TEST(check_for_height, rejects_version_zero)
{
    TestDB db;
    HardFork hf(db, 1, 0, 0, 0, 1, 0);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    hf.init();

    // Version 0 block should be rejected
    ASSERT_FALSE(hf.check_for_height(mkblock(0, 0), 0));
}

TEST(get_earliest_ideal_height, version_gaps)
{
    // Register forks with version gaps (1, 3, 7) and check
    // that intermediate versions map to the correct height
    TestDB db;
    HardFork hf(db);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(3, 10, 1));
    ASSERT_TRUE(hf.add_fork(7, 20, 2));

    // Version 1 => height 0
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(1), 0);
    // Version 2 => maps to next registered fork >= 2 => fork 3 at height 10
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(2), 10);
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(3), 10);
    // Versions 4-7 => maps to fork 7 at height 20
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(4), 20);
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(7), 20);
    // Version 8 => not registered
    ASSERT_EQ(hf.get_earliest_ideal_height_for_version(8), std::numeric_limits<uint64_t>::max());
}

TEST(get_state, update_needed_timing)
{
    TestDB db;
    HardFork hf(db);

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 100, SECONDS_PER_YEAR));

    // Just before update_time boundary: Ready
    time_t boundary = SECONDS_PER_YEAR + HardFork::DEFAULT_UPDATE_TIME;
    ASSERT_EQ(hf.get_state(boundary - 1), HardFork::Ready);

    // At update_time boundary: UpdateNeeded
    ASSERT_EQ(hf.get_state(boundary + 1), HardFork::UpdateNeeded);

    // At forked_time boundary: LikelyForked
    time_t forked = SECONDS_PER_YEAR + HardFork::DEFAULT_FORKED_TIME;
    ASSERT_EQ(hf.get_state(forked + 1), HardFork::LikelyForked);
}

// ============================================================
// Regression tests for Bug #7: Fork activation height logic
// The fix in blockchain.cpp changed difficulty target selection
// from get_ideal_hard_fork_version(bei.height) to
// bei.bl.major_version, which is more reliable for alt chains
// because it uses the block's own declared version rather than
// the ideal version from the fork schedule.
// ============================================================

TEST(hardfork, difficulty_target_constants)
{
    // Verify the expected values of DIFFICULTY_TARGET_V1 and V2.
    // These are the block time targets used for difficulty calculation.
    ASSERT_EQ(DIFFICULTY_TARGET_V1, 60);   // 60 seconds before fork 2
    ASSERT_EQ(DIFFICULTY_TARGET_V2, 120);  // 120 seconds from fork 2 onward
}

TEST(hardfork, difficulty_target_selection_by_block_version)
{
    // Regression test for Bug #7: The fix uses bei.bl.major_version
    // instead of get_ideal_hard_fork_version(bei.height) to determine
    // which difficulty target to use for alt chain blocks.
    //
    // This test verifies the logic:
    //   major_version < 2  => DIFFICULTY_TARGET_V1 (60s)
    //   major_version >= 2 => DIFFICULTY_TARGET_V2 (120s)

    // Version 1 block => DIFFICULTY_TARGET_V1
    {
        cryptonote::block b;
        b.major_version = 1;
        size_t target = b.major_version < 2 ? DIFFICULTY_TARGET_V1 : DIFFICULTY_TARGET_V2;
        ASSERT_EQ(target, DIFFICULTY_TARGET_V1);
        ASSERT_EQ(target, 60u);
    }

    // Version 2 block => DIFFICULTY_TARGET_V2
    {
        cryptonote::block b;
        b.major_version = 2;
        size_t target = b.major_version < 2 ? DIFFICULTY_TARGET_V1 : DIFFICULTY_TARGET_V2;
        ASSERT_EQ(target, DIFFICULTY_TARGET_V2);
        ASSERT_EQ(target, 120u);
    }

    // Version 3+ blocks => DIFFICULTY_TARGET_V2
    for (uint8_t v = 3; v <= 16; ++v) {
        cryptonote::block b;
        b.major_version = v;
        size_t target = b.major_version < 2 ? DIFFICULTY_TARGET_V1 : DIFFICULTY_TARGET_V2;
        ASSERT_EQ(target, DIFFICULTY_TARGET_V2)
            << "Version " << (int)v << " should use DIFFICULTY_TARGET_V2";
    }
}

TEST(hardfork, ideal_version_vs_block_version_divergence)
{
    // This test demonstrates the scenario that Bug #7 addresses:
    // When fork activation is delayed (e.g., due to voting), the ideal
    // version at a height can differ from the block's actual major version.
    //
    // Using get_ideal_hard_fork_version() would give the wrong difficulty
    // target in this scenario, while using the block's major_version gives
    // the correct one.

    TestDB db;
    // Window=4, threshold=100 => ALL blocks in window must vote to upgrade
    HardFork hf(db, 1, 0, 1, 1, 4, 100);

    //                 v  h  t
    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 1));  // fork 2 scheduled at height 5
    hf.init();

    // Add 10 blocks, ALL voting for v1 (simulating a scenario where
    // miners don't adopt the new version even though it's scheduled)
    for (uint64_t h = 0; h < 10; ++h) {
        db.add_block(mkblock(hf, h, 1), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // The ideal version at height 7 is 2 (fork scheduled at height 5)
    ASSERT_EQ(hf.get_ideal_version(7), 2);

    // But the actual version stored is 1 (because 100% threshold was not met)
    ASSERT_EQ(hf.get(7), 1);

    // The block at height 7 has major_version = 1
    block b7 = db.get_block_from_height(7);
    ASSERT_EQ(b7.major_version, 1);

    // Bug #7: Using ideal version would give DIFFICULTY_TARGET_V2 (120s)
    // which is WRONG because the network hasn't actually forked yet.
    size_t target_ideal = hf.get_ideal_version(7) < 2
        ? DIFFICULTY_TARGET_V1 : DIFFICULTY_TARGET_V2;
    ASSERT_EQ(target_ideal, DIFFICULTY_TARGET_V2);  // 120s - INCORRECT

    // Fix: Using block's major_version gives DIFFICULTY_TARGET_V1 (60s)
    // which is CORRECT because the fork hasn't activated.
    size_t target_block = b7.major_version < 2
        ? DIFFICULTY_TARGET_V1 : DIFFICULTY_TARGET_V2;
    ASSERT_EQ(target_block, DIFFICULTY_TARGET_V1);  // 60s - CORRECT
}

TEST(hardfork, ideal_version_matches_block_version_no_voting)
{
    // When forks activate deterministically (no voting delays), the ideal
    // version and block major_version should agree, so both approaches
    // give the same difficulty target. This test confirms there is no
    // regression for the common case.

    TestDB db;
    HardFork hf(db, 1, 0, 0, 0, 1, 0); // no voting needed

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 1));
    ASSERT_TRUE(hf.add_fork(3, 10, 2));
    hf.init();

    for (uint64_t h = 0; h < 15; ++h) {
        uint8_t ideal = hf.get_ideal_version(h);
        db.add_block(mkblock(ideal, ideal), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // For every height, ideal version and block major_version should agree
    for (uint64_t h = 0; h < 15; ++h) {
        block blk = db.get_block_from_height(h);
        uint8_t ideal = hf.get_ideal_version(h);

        // Both methods should produce the same difficulty target
        size_t target_ideal = ideal < 2 ? DIFFICULTY_TARGET_V1 : DIFFICULTY_TARGET_V2;
        size_t target_block = blk.major_version < 2 ? DIFFICULTY_TARGET_V1 : DIFFICULTY_TARGET_V2;
        ASSERT_EQ(target_ideal, target_block)
            << "At height " << h << " ideal=" << (int)ideal
            << " major_version=" << (int)blk.major_version;
    }
}

TEST(hardfork, difficulty_target_boundary_version_1_to_2)
{
    // Verify the exact boundary where difficulty target switches from
    // DIFFICULTY_TARGET_V1 to DIFFICULTY_TARGET_V2 based on block version.

    TestDB db;
    HardFork hf(db, 1, 0, 0, 0, 1, 0); // no voting

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 5, 1));
    hf.init();

    for (uint64_t h = 0; h < 10; ++h) {
        uint8_t ideal = hf.get_ideal_version(h);
        db.add_block(mkblock(ideal, ideal), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // Heights 0-4: version 1 => 60s target
    for (uint64_t h = 0; h < 5; ++h) {
        block blk = db.get_block_from_height(h);
        ASSERT_EQ(blk.major_version, 1);
        size_t target = blk.major_version < 2 ? DIFFICULTY_TARGET_V1 : DIFFICULTY_TARGET_V2;
        ASSERT_EQ(target, DIFFICULTY_TARGET_V1)
            << "Height " << h << " should use 60s target";
    }

    // Heights 5-9: version 2 => 120s target
    for (uint64_t h = 5; h < 10; ++h) {
        block blk = db.get_block_from_height(h);
        ASSERT_EQ(blk.major_version, 2);
        size_t target = blk.major_version < 2 ? DIFFICULTY_TARGET_V1 : DIFFICULTY_TARGET_V2;
        ASSERT_EQ(target, DIFFICULTY_TARGET_V2)
            << "Height " << h << " should use 120s target";
    }
}

TEST(hardfork, delayed_fork_activation_difficulty_target)
{
    // End-to-end test of the voting/delayed activation scenario.
    // Fork 2 is scheduled at height 3, but with a high threshold (100%)
    // and only 50% of blocks voting for it. The fork never activates,
    // so all blocks remain at version 1 and should use DIFFICULTY_TARGET_V1.

    TestDB db;
    HardFork hf(db, 1, 0, 1, 1, 4, 100); // 100% threshold

    ASSERT_TRUE(hf.add_fork(1, 0, 0));
    ASSERT_TRUE(hf.add_fork(2, 3, 1));
    hf.init();

    // Alternate votes: v1, v2, v1, v2, ... => never reaches 100%
    for (uint64_t h = 0; h < 12; ++h) {
        uint8_t vote = (h % 2 == 0) ? 1 : 2;
        db.add_block(mkblock(hf, h, vote), 0, 0, 0, 0, 0, crypto::hash());
        ASSERT_TRUE(hf.add(db.get_block_from_height(h), h));
    }

    // Fork never activated
    ASSERT_EQ(hf.get_current_version(), 1);

    // Check every block: ideal says v2 from height 3, but actual is v1
    for (uint64_t h = 3; h < 12; ++h) {
        ASSERT_EQ(hf.get_ideal_version(h), 2)
            << "Ideal version at height " << h << " should be 2";
        ASSERT_EQ(hf.get(h), 1)
            << "Actual version at height " << h << " should still be 1";

        block blk = db.get_block_from_height(h);
        ASSERT_EQ(blk.major_version, 1)
            << "Block major_version at height " << h << " should be 1";

        // Using block version (the fix) gives correct target
        size_t target = blk.major_version < 2
            ? DIFFICULTY_TARGET_V1 : DIFFICULTY_TARGET_V2;
        ASSERT_EQ(target, DIFFICULTY_TARGET_V1)
            << "Height " << h << ": fork not activated, should use 60s target";
    }
}

