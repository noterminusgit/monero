// Copyright (c) 2017-2024, The Monero Project
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

#include "crypto/crypto.h"
#include "multisig/multisig_account.h"
#include "multisig/multisig_kex_msg.h"
#include "ringct/rctOps.h"
#include "wallet/wallet2.h"

#include "gtest/gtest.h"

#include <cstdint>

static const struct
{
  const char *address;
  const char *spendkey;
} test_addresses[] =
{
  {
    "9uvjbU54ZJb8j7Dcq1h3F1DnBRkxXdYUX4pbJ7mE3ghM8uF3fKzqRKRNAKYZXcNLqMg7MxjVVD2wKC2PALUwEveGSC3YSWD",
    "2dd6e34a234c3e8b5d29a371789e4601e96dee4ea6f7ef79224d1a2d91164c01"
  },
  {
    "9ywDBAyDbb6QKFiZxDJ4hHZqZEQXXCR5EaYNcndUpqPDeE7rEgs6neQdZnhcDrWbURYK8xUjhuG2mVjJdmknrZbcG7NnbaB",
    "fac47aecc948ce9d3531aa042abb18235b1df632087c55a361b632ffdd6ede0c"
  },
  {
    "9t6Hn946u3eah5cuncH1hB5hGzsTUoevtf4SY7MHN5NgJZh2SFWsyVt3vUhuHyRKyrCQvr71Lfc1AevG3BXE11PQFoXDtD8",
    "bbd3175ef9fd9f5eefdc43035f882f74ad14c4cf1799d8b6f9001bc197175d02"
  },
  {
    "9zmAWoNyNPbgnYSm3nJNpAKHm6fCcs3MR94gBWxp9MCDUiMUhyYFfyQETUDLPF7DP6ZsmNo6LRxwPP9VmhHNxKrER9oGigT",
    "f2efae45bef1917a7430cda8fcffc4ee010e3178761aa41d4628e23b1fe2d501"
  },
  {
    "9ue8NJMg3WzKxTtmjeXzWYF5KmU6dC7LHEt9wvYdPn2qMmoFUa8hJJHhSHvJ46UEwpDyy5jSboNMRaDBKwU54NT42YcNUp5",
    "a4cef54ed3fd61cd78a2ceb82ecf85a903ad2db9a86fb77ff56c35c56016280a"
  }
};

static const size_t KEYS_COUNT = 5;

static void make_wallet(unsigned int idx, tools::wallet2 &wallet)
{
  ASSERT_TRUE(idx < sizeof(test_addresses) / sizeof(test_addresses[0]));

  crypto::secret_key spendkey;
  epee::string_tools::hex_to_pod(test_addresses[idx].spendkey, spendkey);

  try
  {
    wallet.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
    wallet.set_subaddress_lookahead(1, 1);
    wallet.generate("", "", spendkey, true, false);
    ASSERT_TRUE(test_addresses[idx].address == wallet.get_account().get_public_address_str(cryptonote::TESTNET));
    wallet.decrypt_keys("");
    ASSERT_TRUE(test_addresses[idx].spendkey == epee::string_tools::pod_to_hex(unwrap(unwrap(wallet.get_account().get_keys().m_spend_secret_key))));
    wallet.encrypt_keys("");
  }
  catch (const std::exception &e)
  {
    MFATAL("Error creating test wallet: " << e.what());
    ASSERT_TRUE(0);
  }
}

static std::vector<std::string> exchange_round(std::vector<tools::wallet2>& wallets, const std::vector<std::string>& infos)
{
  std::vector<std::string> new_infos;
  new_infos.reserve(infos.size());

  for (size_t i = 0; i < wallets.size(); ++i)
    new_infos.push_back(wallets[i].exchange_multisig_keys("", infos));

  return new_infos;
}

static std::vector<std::string> exchange_round_force_update(std::vector<tools::wallet2>& wallets,
  const std::vector<std::string>& infos,
  const std::size_t round_in_progress)
{
  EXPECT_TRUE(wallets.size() == infos.size());
  std::vector<std::string> new_infos;
  std::vector<std::string> temp_force_update_infos;
  new_infos.reserve(infos.size());

  // when force-updating, we only need at most 'num_signers - 1 - (round - 1)' messages from other signers
  size_t num_other_messages_required{wallets.size() - 1 - (round_in_progress - 1)};
  if (num_other_messages_required > wallets.size())
    num_other_messages_required = 0;  //overflow case for post-kex verification round of 1-of-N

  for (size_t i = 0; i < wallets.size(); ++i)
  {
    temp_force_update_infos.clear();
    temp_force_update_infos.reserve(num_other_messages_required + 1);
    temp_force_update_infos.push_back(infos[i]);  //always include the local signer's message for this round

    size_t infos_collected{0};
    for (size_t wallet_index = 0; wallet_index < wallets.size(); ++wallet_index)
    {
      // skip the local signer's message
      if (wallet_index == i)
        continue;

      temp_force_update_infos.push_back(infos[wallet_index]);
      ++infos_collected;

      if (infos_collected == num_other_messages_required)
        break;
    }

    new_infos.push_back(wallets[i].exchange_multisig_keys("", temp_force_update_infos, true));
  }

  return new_infos;
}

static void check_results(const std::vector<std::string> &intermediate_infos,
  std::vector<tools::wallet2>& wallets,
  const std::uint32_t M)
{
  // check results
  std::unordered_set<crypto::secret_key> unique_privkeys;
  rct::key composite_pubkey = rct::identity();

  ASSERT_TRUE(wallets.size() > 0);
  wallets[0].decrypt_keys("");
  crypto::public_key spend_pubkey = wallets[0].get_account().get_keys().m_account_address.m_spend_public_key;
  crypto::secret_key view_privkey = wallets[0].get_account().get_keys().m_view_secret_key;
  crypto::public_key view_pubkey;
  EXPECT_TRUE(crypto::secret_key_to_public_key(view_privkey, view_pubkey));
  wallets[0].encrypt_keys("");

  // at the end of multisig kex, all wallets should emit a post-kex message with the same two pubkeys
  std::vector<crypto::public_key> post_kex_msg_pubkeys;
  ASSERT_TRUE(intermediate_infos.size() == wallets.size());
  for (const std::string &intermediate_info : intermediate_infos)
  {
    multisig::multisig_kex_msg post_kex_msg;
    EXPECT_TRUE(!intermediate_info.empty());
    EXPECT_NO_THROW(post_kex_msg = intermediate_info);

    if (post_kex_msg_pubkeys.size() != 0)
      EXPECT_TRUE(post_kex_msg_pubkeys == post_kex_msg.get_msg_pubkeys());  //assumes sorting is always the same
    else
      post_kex_msg_pubkeys = post_kex_msg.get_msg_pubkeys();

    EXPECT_TRUE(post_kex_msg_pubkeys.size() == 2);
  }

  // the post-kex pubkeys should equal the account's public view and spend keys
  EXPECT_TRUE(std::find(post_kex_msg_pubkeys.begin(), post_kex_msg_pubkeys.end(), spend_pubkey) != post_kex_msg_pubkeys.end());
  EXPECT_TRUE(std::find(post_kex_msg_pubkeys.begin(), post_kex_msg_pubkeys.end(), view_pubkey) != post_kex_msg_pubkeys.end());

  // each wallet should have the same state (private view key, public spend key), and the public spend key should be
  //   reproducible from the private spend keys found in each account
  for (tools::wallet2 &wallet : wallets)
  {
    wallet.decrypt_keys("");
    const multisig::multisig_account_status ms_status{wallet.get_multisig_status()};
    EXPECT_TRUE(ms_status.multisig_is_active);
    EXPECT_TRUE(ms_status.kex_is_done);
    EXPECT_TRUE(ms_status.is_ready);
    EXPECT_TRUE(ms_status.threshold == M);
    EXPECT_TRUE(ms_status.total == wallets.size());

    EXPECT_TRUE(wallets[0].get_account().get_public_address_str(cryptonote::TESTNET) ==
      wallet.get_account().get_public_address_str(cryptonote::TESTNET));
    
    EXPECT_EQ(spend_pubkey, wallet.get_account().get_keys().m_account_address.m_spend_public_key);
    EXPECT_EQ(view_privkey, wallet.get_account().get_keys().m_view_secret_key);
    EXPECT_EQ(view_pubkey, wallet.get_account().get_keys().m_account_address.m_view_public_key);

    // sum together unique multisig keys
    for (const auto &privkey : wallet.get_account().get_keys().m_multisig_keys)
    {
      EXPECT_NE(privkey, crypto::null_skey);

      if (unique_privkeys.find(privkey) == unique_privkeys.end())
      {
        unique_privkeys.insert(privkey);
        crypto::public_key pubkey;
        EXPECT_TRUE(crypto::secret_key_to_public_key(privkey, pubkey));
        EXPECT_NE(privkey, crypto::null_skey);
        EXPECT_NE(pubkey, crypto::null_pkey);
        EXPECT_NE(pubkey, rct::rct2pk(rct::identity()));
        rct::addKeys(composite_pubkey, composite_pubkey, rct::pk2rct(pubkey));
      }
    }
    wallet.encrypt_keys("");
  }

  // final key via sum of privkeys should equal the wallets' public spend key
  wallets[0].decrypt_keys("");
  EXPECT_EQ(wallets[0].get_account().get_keys().m_account_address.m_spend_public_key, rct::rct2pk(composite_pubkey));
  wallets[0].encrypt_keys("");
}

static void make_wallets(const unsigned int M, const unsigned int N, const bool force_update)
{
  std::vector<tools::wallet2> wallets(N);
  ASSERT_TRUE(wallets.size() > 1 && wallets.size() <= KEYS_COUNT);
  ASSERT_TRUE(M <= wallets.size());
  std::uint32_t total_rounds_required = multisig::multisig_setup_rounds_required(wallets.size(), M);
  std::uint32_t rounds_complete{0};

  // initialize wallets, get first round multisig kex msgs
  std::vector<std::string> initial_infos(wallets.size());

  for (size_t i = 0; i < wallets.size(); ++i)
  {
    make_wallet(i, wallets[i]);

    wallets[i].decrypt_keys("");
    initial_infos[i] = wallets[i].get_multisig_first_kex_msg();
    wallets[i].encrypt_keys("");
  }

  // wallets should not be multisig yet
  for (const auto& wallet: wallets)
    ASSERT_FALSE(wallet.get_multisig_status().multisig_is_active);

  // make wallets multisig, get second round kex messages (if appropriate)
  std::vector<std::string> intermediate_infos(wallets.size());

  for (size_t i = 0; i < wallets.size(); ++i)
  {
    intermediate_infos[i] = wallets[i].make_multisig("", initial_infos, M);
  }

  ++rounds_complete;

  // perform kex rounds until kex is complete
  multisig::multisig_account_status ms_status{wallets[0].get_multisig_status()};
  while (!ms_status.is_ready)
  {
    if (force_update)
      intermediate_infos = exchange_round_force_update(wallets, intermediate_infos, rounds_complete + 1);
    else
      intermediate_infos = exchange_round(wallets, intermediate_infos);

    ms_status = wallets[0].get_multisig_status();
    ++rounds_complete;
  }

  EXPECT_EQ(total_rounds_required, rounds_complete);

  check_results(intermediate_infos, wallets, M);
}

static void make_wallets_boosting(std::vector<tools::wallet2>& wallets, unsigned int M)
{
  ASSERT_TRUE(wallets.size() > 1 && wallets.size() <= KEYS_COUNT);
  ASSERT_TRUE(M <= wallets.size());
  std::uint32_t kex_rounds_required = multisig::multisig_kex_rounds_required(wallets.size(), M);
  std::uint32_t rounds_required = multisig::multisig_setup_rounds_required(wallets.size(), M);
  std::uint32_t rounds_complete{0};

  // initialize wallets, get first round multisig kex msgs
  std::vector<std::string> initial_infos(wallets.size());

  for (size_t i = 0; i < wallets.size(); ++i)
  {
    make_wallet(i, wallets[i]);

    wallets[i].decrypt_keys("");
    initial_infos[i] = wallets[i].get_multisig_first_kex_msg();
    wallets[i].encrypt_keys("");
  }

  // wallets should not be multisig yet
  for (const auto &wallet: wallets)
  {
    const multisig::multisig_account_status ms_status{wallet.get_multisig_status()};
    ASSERT_FALSE(ms_status.multisig_is_active);
  }

  // get round 2 booster messages for wallet0 (if appropriate)
  auto initial_infos_truncated = initial_infos;
  initial_infos_truncated.erase(initial_infos_truncated.begin());

  std::vector<std::string> wallet0_booster_infos;
  wallet0_booster_infos.reserve(wallets.size() - 1);

  if (rounds_complete + 1 < kex_rounds_required)
  {
    for (size_t i = 1; i < wallets.size(); ++i)
    {
      wallet0_booster_infos.push_back(
          wallets[i].get_multisig_key_exchange_booster("", initial_infos_truncated, M, wallets.size())
        );
    }
  }

  // make wallets multisig
  std::vector<std::string> intermediate_infos(wallets.size());

  for (size_t i = 0; i < wallets.size(); ++i)
    intermediate_infos[i] = wallets[i].make_multisig("", initial_infos, M);

  ++rounds_complete;

  // perform all kex rounds
  // boost wallet0 each round, so wallet0 is always 1 round ahead
  std::string wallet0_intermediate_info;
  std::vector<std::string> new_infos(intermediate_infos.size());
  multisig::multisig_account_status ms_status{wallets[0].get_multisig_status()};
  while (!ms_status.is_ready)
  {
    // use booster infos to update wallet0 'early'
    if (rounds_complete < kex_rounds_required)
      new_infos[0] = wallets[0].exchange_multisig_keys("", wallet0_booster_infos);
    else
    {
      // force update the post-kex round with wallet0's post-kex message since wallet0 is 'ahead' of the other wallets
      wallet0_booster_infos = {wallets[0].exchange_multisig_keys("", {})};
      new_infos[0] = wallets[0].exchange_multisig_keys("", wallet0_booster_infos, true);
    }

    // get wallet0 booster infos for next round
    if (rounds_complete + 1 < kex_rounds_required)
    {
      // remove wallet0 info for this round (so boosters have incomplete kex message set)
      auto intermediate_infos_truncated = intermediate_infos;
      intermediate_infos_truncated.erase(intermediate_infos_truncated.begin());

      // obtain booster messages from all other wallets
      for (size_t i = 1; i < wallets.size(); ++i)
      {
        wallet0_booster_infos[i-1] =
          wallets[i].get_multisig_key_exchange_booster("", intermediate_infos_truncated, M, wallets.size());
      }
    }

    // update other wallets
    for (size_t i = 1; i < wallets.size(); ++i)
        new_infos[i] = wallets[i].exchange_multisig_keys("", intermediate_infos);

    intermediate_infos = new_infos;
    ++rounds_complete;
    ms_status = wallets[0].get_multisig_status();
  }

  EXPECT_EQ(rounds_required, rounds_complete);

  check_results(intermediate_infos, wallets, M);
}

TEST(multisig, make_1_2)
{
  make_wallets(1, 2, false);
  make_wallets(1, 2, true);
}

TEST(multisig, make_1_3)
{
  make_wallets(1, 3, false);
  make_wallets(1, 3, true);
}

TEST(multisig, make_2_2)
{
  make_wallets(2, 2, false);
  make_wallets(2, 2, true);
}

TEST(multisig, make_3_3)
{
  make_wallets(3, 3, false);
  make_wallets(3, 3, true);
}

TEST(multisig, make_2_3)
{
  make_wallets(2, 3, false);
  make_wallets(2, 3, true);
}

TEST(multisig, make_2_4)
{
  make_wallets(2, 4, false);
  make_wallets(2, 4, true);
}

TEST(multisig, make_2_4_boosting)
{
  std::vector<tools::wallet2> wallets(4);
  make_wallets_boosting(wallets, 2);
}

TEST(multisig, multisig_kex_msg)
{
  using namespace multisig;

  crypto::public_key pubkey1;
  crypto::public_key pubkey2;
  crypto::public_key pubkey3;
  crypto::secret_key_to_public_key(rct::rct2sk(rct::skGen()), pubkey1);
  crypto::secret_key_to_public_key(rct::rct2sk(rct::skGen()), pubkey2);
  crypto::secret_key_to_public_key(rct::rct2sk(rct::skGen()), pubkey3);

  crypto::secret_key signing_skey = rct::rct2sk(rct::skGen());
  crypto::public_key signing_pubkey;
  while(!crypto::secret_key_to_public_key(signing_skey, signing_pubkey))
  {
    signing_skey = rct::rct2sk(rct::skGen());
  }

  const crypto::secret_key ancillary_skey{rct::rct2sk(rct::skGen())};

  // misc. edge cases
  EXPECT_NO_THROW((multisig_kex_msg{}));
  EXPECT_ANY_THROW((multisig_kex_msg{multisig_kex_msg{}.get_msg()}));
  EXPECT_ANY_THROW((multisig_kex_msg{"abc"}));
  EXPECT_ANY_THROW((multisig_kex_msg{0, crypto::null_skey, std::vector<crypto::public_key>{}, crypto::null_skey}));
  EXPECT_ANY_THROW((multisig_kex_msg{1, crypto::null_skey, std::vector<crypto::public_key>{}, crypto::null_skey}));
  EXPECT_ANY_THROW((multisig_kex_msg{1, signing_skey, std::vector<crypto::public_key>{}, crypto::null_skey}));
  EXPECT_ANY_THROW((multisig_kex_msg{1, crypto::null_skey, std::vector<crypto::public_key>{}, ancillary_skey}));

  // test that messages are both constructible and reversible

  // round 1
  EXPECT_NO_THROW((multisig_kex_msg{
      multisig_kex_msg{1, signing_skey, std::vector<crypto::public_key>{}, ancillary_skey}.get_msg()
    }));
  EXPECT_NO_THROW((multisig_kex_msg{
      multisig_kex_msg{1, signing_skey, std::vector<crypto::public_key>{pubkey1}, ancillary_skey}.get_msg()
    }));

  // round 2
  EXPECT_NO_THROW((multisig_kex_msg{
      multisig_kex_msg{2, signing_skey, std::vector<crypto::public_key>{pubkey1}, ancillary_skey}.get_msg()
    }));
  EXPECT_NO_THROW((multisig_kex_msg{
      multisig_kex_msg{2, signing_skey, std::vector<crypto::public_key>{pubkey1}, crypto::null_skey}.get_msg()
    }));
  EXPECT_NO_THROW((multisig_kex_msg{
      multisig_kex_msg{2, signing_skey, std::vector<crypto::public_key>{pubkey1, pubkey2}, ancillary_skey}.get_msg()
    }));
  EXPECT_NO_THROW((multisig_kex_msg{
      multisig_kex_msg{2, signing_skey, std::vector<crypto::public_key>{pubkey1, pubkey2, pubkey3}, crypto::null_skey}.get_msg()
    }));

  // test that keys can be recovered if stored in a message and the message's reverse

  // round 1
  const multisig_kex_msg msg_rnd1{1, signing_skey, std::vector<crypto::public_key>{pubkey1}, ancillary_skey};
  const multisig_kex_msg msg_rnd1_reverse{msg_rnd1.get_msg()};
  EXPECT_EQ(msg_rnd1.get_round(), 1);
  EXPECT_EQ(msg_rnd1.get_round(), msg_rnd1_reverse.get_round());
  EXPECT_EQ(msg_rnd1.get_signing_pubkey(), signing_pubkey);
  EXPECT_EQ(msg_rnd1.get_signing_pubkey(), msg_rnd1_reverse.get_signing_pubkey());
  EXPECT_EQ(msg_rnd1.get_msg_pubkeys().size(), 0);
  EXPECT_EQ(msg_rnd1.get_msg_pubkeys().size(), msg_rnd1_reverse.get_msg_pubkeys().size());
  EXPECT_EQ(msg_rnd1.get_msg_privkey(), ancillary_skey);
  EXPECT_EQ(msg_rnd1.get_msg_privkey(), msg_rnd1_reverse.get_msg_privkey());

  // round 2
  const multisig_kex_msg msg_rnd2{2, signing_skey, std::vector<crypto::public_key>{pubkey1, pubkey2}, ancillary_skey};
  const multisig_kex_msg msg_rnd2_reverse{msg_rnd2.get_msg()};
  EXPECT_EQ(msg_rnd2.get_round(), 2);
  EXPECT_EQ(msg_rnd2.get_round(), msg_rnd2_reverse.get_round());
  EXPECT_EQ(msg_rnd2.get_signing_pubkey(), signing_pubkey);
  EXPECT_EQ(msg_rnd2.get_signing_pubkey(), msg_rnd2_reverse.get_signing_pubkey());
  ASSERT_EQ(msg_rnd2.get_msg_pubkeys().size(), 2);
  ASSERT_EQ(msg_rnd2.get_msg_pubkeys().size(), msg_rnd2_reverse.get_msg_pubkeys().size());
  EXPECT_EQ(msg_rnd2.get_msg_pubkeys()[0], pubkey1);
  EXPECT_EQ(msg_rnd2.get_msg_pubkeys()[1], pubkey2);
  EXPECT_EQ(msg_rnd2.get_msg_pubkeys()[0], msg_rnd2_reverse.get_msg_pubkeys()[0]);
  EXPECT_EQ(msg_rnd2.get_msg_pubkeys()[1], msg_rnd2_reverse.get_msg_pubkeys()[1]);
  EXPECT_EQ(msg_rnd2.get_msg_privkey(), crypto::null_skey);
  EXPECT_EQ(msg_rnd2.get_msg_privkey(), msg_rnd2_reverse.get_msg_privkey());
}

TEST(multisig, kex_rounds_required)
{
  using namespace multisig;

  // Formula: num_signers - threshold + 1
  // 1-of-N: N rounds
  EXPECT_EQ(multisig_kex_rounds_required(2, 1), 2u);
  EXPECT_EQ(multisig_kex_rounds_required(3, 1), 3u);
  EXPECT_EQ(multisig_kex_rounds_required(5, 1), 5u);

  // N-of-N: 1 round
  EXPECT_EQ(multisig_kex_rounds_required(2, 2), 1u);
  EXPECT_EQ(multisig_kex_rounds_required(3, 3), 1u);
  EXPECT_EQ(multisig_kex_rounds_required(4, 4), 1u);

  // M-of-N: N - M + 1 rounds
  EXPECT_EQ(multisig_kex_rounds_required(3, 2), 2u);
  EXPECT_EQ(multisig_kex_rounds_required(4, 2), 3u);
  EXPECT_EQ(multisig_kex_rounds_required(4, 3), 2u);
}

TEST(multisig, setup_rounds_required)
{
  using namespace multisig;

  // Setup rounds = kex rounds + 1 (post-kex verification round)
  EXPECT_EQ(multisig_setup_rounds_required(2, 1), multisig_kex_rounds_required(2, 1) + 1);
  EXPECT_EQ(multisig_setup_rounds_required(3, 2), multisig_kex_rounds_required(3, 2) + 1);
  EXPECT_EQ(multisig_setup_rounds_required(4, 3), multisig_kex_rounds_required(4, 3) + 1);
  EXPECT_EQ(multisig_setup_rounds_required(4, 4), multisig_kex_rounds_required(4, 4) + 1);
}

TEST(multisig, kex_msg_tampered_signature)
{
  using namespace multisig;

  crypto::secret_key signing_skey = rct::rct2sk(rct::skGen());
  crypto::public_key signing_pubkey;
  while(!crypto::secret_key_to_public_key(signing_skey, signing_pubkey))
    signing_skey = rct::rct2sk(rct::skGen());

  const crypto::secret_key ancillary_skey{rct::rct2sk(rct::skGen())};

  // Create a valid message
  const multisig_kex_msg valid_msg{1, signing_skey, std::vector<crypto::public_key>{}, ancillary_skey};
  std::string msg_str = valid_msg.get_msg();

  // Tamper with the message (flip a character near the end, in the signature area)
  if (msg_str.size() > 10)
  {
    msg_str[msg_str.size() - 5] = (msg_str[msg_str.size() - 5] == 'A') ? 'B' : 'A';
    EXPECT_ANY_THROW((multisig_kex_msg{msg_str}));
  }
}

TEST(multisig, kex_msg_empty_string_throws)
{
  using namespace multisig;
  EXPECT_ANY_THROW((multisig_kex_msg{""}));
}

TEST(multisig, kex_msg_round_zero_throws)
{
  using namespace multisig;

  crypto::secret_key signing_skey = rct::rct2sk(rct::skGen());
  crypto::public_key signing_pubkey;
  while(!crypto::secret_key_to_public_key(signing_skey, signing_pubkey))
    signing_skey = rct::rct2sk(rct::skGen());

  const crypto::secret_key ancillary_skey{rct::rct2sk(rct::skGen())};

  // Round 0 should not be valid
  EXPECT_ANY_THROW((multisig_kex_msg{0, signing_skey, std::vector<crypto::public_key>{}, ancillary_skey}));
}

TEST(multisig, kex_msg_default_construct)
{
  using namespace multisig;
  multisig_kex_msg msg;
  // Default-constructed message should have empty state
  EXPECT_TRUE(msg.get_msg().empty());
}

TEST(multisig, make_3_4)
{
  make_wallets(3, 4, false);
  make_wallets(3, 4, true);
}

TEST(multisig, make_1_4)
{
  make_wallets(1, 4, false);
  make_wallets(1, 4, true);
}

// ---------- Direct multisig crypto function tests ----------

#include "multisig/multisig.h"
#include "cryptonote_basic/account.h"

TEST(multisig, blinded_secret_key_deterministic)
{
  // Same input should always produce the same blinded key
  crypto::secret_key sk = rct::rct2sk(rct::skGen());
  crypto::secret_key blinded1 = multisig::get_multisig_blinded_secret_key(sk);
  crypto::secret_key blinded2 = multisig::get_multisig_blinded_secret_key(sk);
  EXPECT_EQ(blinded1, blinded2);
}

TEST(multisig, blinded_secret_key_not_identity)
{
  crypto::secret_key sk = rct::rct2sk(rct::skGen());
  crypto::secret_key blinded = multisig::get_multisig_blinded_secret_key(sk);
  // The blinded key should differ from the input
  EXPECT_NE(blinded, sk);
  // Should not be null
  EXPECT_NE(blinded, crypto::null_skey);
}

TEST(multisig, blinded_secret_key_null_throws)
{
  EXPECT_ANY_THROW(multisig::get_multisig_blinded_secret_key(crypto::null_skey));
}

TEST(multisig, blinded_secret_key_different_inputs)
{
  crypto::secret_key sk1 = rct::rct2sk(rct::skGen());
  crypto::secret_key sk2 = rct::rct2sk(rct::skGen());
  EXPECT_NE(multisig::get_multisig_blinded_secret_key(sk1),
            multisig::get_multisig_blinded_secret_key(sk2));
}

TEST(multisig, generate_key_image_invalid_index)
{
  cryptonote::account_keys keys{};
  // No multisig keys set, so index 0 should fail
  crypto::key_image ki;
  crypto::public_key out_key;
  crypto::secret_key_to_public_key(rct::rct2sk(rct::skGen()), out_key);
  EXPECT_FALSE(multisig::generate_multisig_key_image(keys, 0, out_key, ki));
}

TEST(multisig, generate_key_image_valid_index)
{
  cryptonote::account_keys keys{};
  crypto::secret_key sk = rct::rct2sk(rct::skGen());
  keys.m_multisig_keys.push_back(sk);

  crypto::public_key out_key;
  crypto::secret_key_to_public_key(rct::rct2sk(rct::skGen()), out_key);

  crypto::key_image ki;
  EXPECT_TRUE(multisig::generate_multisig_key_image(keys, 0, out_key, ki));
  // Key image should not be zero
  crypto::key_image null_ki;
  memset(&null_ki, 0, sizeof(null_ki));
  EXPECT_NE(ki, null_ki);
}

TEST(multisig, generate_key_image_out_of_range)
{
  cryptonote::account_keys keys{};
  crypto::secret_key sk = rct::rct2sk(rct::skGen());
  keys.m_multisig_keys.push_back(sk);

  crypto::public_key out_key;
  crypto::secret_key_to_public_key(rct::rct2sk(rct::skGen()), out_key);

  crypto::key_image ki;
  // Index 1 is out of range (only 1 key at index 0)
  EXPECT_FALSE(multisig::generate_multisig_key_image(keys, 1, out_key, ki));
}

TEST(multisig, generate_LR_produces_valid_points)
{
  crypto::secret_key k = rct::rct2sk(rct::skGen());
  crypto::public_key pkey;
  crypto::secret_key_to_public_key(k, pkey);

  crypto::public_key L, R;
  multisig::generate_multisig_LR(pkey, k, L, R);

  // L = k*G, should equal the public key of k
  EXPECT_EQ(L, pkey);

  // R should not be null
  EXPECT_NE(R, crypto::null_pkey);
}

TEST(multisig, generate_LR_deterministic)
{
  crypto::secret_key k = rct::rct2sk(rct::skGen());
  crypto::public_key pkey;
  crypto::secret_key_to_public_key(rct::rct2sk(rct::skGen()), pkey);

  crypto::public_key L1, R1, L2, R2;
  multisig::generate_multisig_LR(pkey, k, L1, R1);
  multisig::generate_multisig_LR(pkey, k, L2, R2);

  EXPECT_EQ(L1, L2);
  EXPECT_EQ(R1, R2);
}

// ===== Phase 7 extended tests =====

TEST(multisig, kex_msg_construction_round1)
{
  // Generate a keypair for signing
  crypto::secret_key signing_sk = rct::rct2sk(rct::skGen());
  crypto::public_key signing_pk;
  crypto::secret_key_to_public_key(signing_sk, signing_pk);

  // Generate a msg private key (round 1 messages carry a private key, not pubkeys)
  crypto::secret_key msg_privkey = rct::rct2sk(rct::skGen());

  // Round 1 message includes a private key; pubkeys are NOT stored in m_msg_pubkeys
  multisig::multisig_kex_msg msg(1, signing_sk, std::vector<crypto::public_key>{}, msg_privkey);

  ASSERT_EQ(msg.get_round(), 1u);
  // Round 1 messages do not expose pubkeys; they carry a private key instead
  ASSERT_EQ(msg.get_msg_pubkeys().size(), 0u);
  ASSERT_EQ(msg.get_msg_privkey(), msg_privkey);
  ASSERT_EQ(msg.get_signing_pubkey(), signing_pk);
  ASSERT_FALSE(msg.get_msg().empty());
}

TEST(multisig, kex_msg_construction_round2)
{
  // Round 2+ messages do not include a private key
  crypto::secret_key signing_sk = rct::rct2sk(rct::skGen());
  crypto::public_key pk1, pk2;
  crypto::secret_key_to_public_key(rct::rct2sk(rct::skGen()), pk1);
  crypto::secret_key_to_public_key(rct::rct2sk(rct::skGen()), pk2);

  std::vector<crypto::public_key> msg_pubkeys = {pk1, pk2};

  multisig::multisig_kex_msg msg(2, signing_sk, msg_pubkeys);

  EXPECT_EQ(msg.get_round(), 2u);
  EXPECT_EQ(msg.get_msg_pubkeys().size(), 2u);
  EXPECT_FALSE(msg.get_msg().empty());
}

TEST(multisig, kex_msg_serialization_roundtrip)
{
  crypto::secret_key signing_sk = rct::rct2sk(rct::skGen());
  crypto::secret_key msg_privkey = rct::rct2sk(rct::skGen());
  crypto::public_key msg_pubkey;
  crypto::secret_key_to_public_key(msg_privkey, msg_pubkey);

  std::vector<crypto::public_key> msg_pubkeys = {msg_pubkey};

  multisig::multisig_kex_msg original(1, signing_sk, msg_pubkeys, msg_privkey);
  std::string msg_str = original.get_msg();

  // Parse back from string
  multisig::multisig_kex_msg parsed(msg_str);

  EXPECT_EQ(parsed.get_round(), original.get_round());
  EXPECT_EQ(parsed.get_msg_pubkeys(), original.get_msg_pubkeys());
  EXPECT_EQ(parsed.get_signing_pubkey(), original.get_signing_pubkey());
}

TEST(multisig, kex_msg_from_string_parsing)
{
  crypto::secret_key signing_sk = rct::rct2sk(rct::skGen());
  crypto::public_key pk;
  crypto::secret_key_to_public_key(rct::rct2sk(rct::skGen()), pk);

  multisig::multisig_kex_msg original(2, signing_sk, {pk});
  std::string serialized = original.get_msg();

  ASSERT_FALSE(serialized.empty());

  multisig::multisig_kex_msg parsed(serialized);
  EXPECT_EQ(parsed.get_round(), 2u);
  EXPECT_EQ(parsed.get_msg_pubkeys().size(), 1u);
}

TEST(multisig, kex_msg_invalid_string_throws)
{
  // Tampered/invalid string should throw
  EXPECT_ANY_THROW(multisig::multisig_kex_msg("invalid_kex_message"));
  EXPECT_ANY_THROW(multisig::multisig_kex_msg(""));
}

TEST(multisig, kex_msg_truncated_throws)
{
  crypto::secret_key signing_sk = rct::rct2sk(rct::skGen());
  crypto::public_key pk;
  crypto::secret_key_to_public_key(rct::rct2sk(rct::skGen()), pk);

  multisig::multisig_kex_msg original(2, signing_sk, {pk});
  std::string serialized = original.get_msg();

  // Just the magic prefix with no payload should fail to parse
  EXPECT_ANY_THROW(multisig::multisig_kex_msg(std::string("MultisigxV2Rn")));
}

TEST(multisig, multisig_account_default_construction)
{
  multisig::multisig_account account;
  EXPECT_FALSE(account.account_is_active());
  EXPECT_FALSE(account.main_kex_rounds_done());
  EXPECT_FALSE(account.multisig_is_ready());
  EXPECT_EQ(account.get_kex_rounds_complete(), 0u);
}

TEST(multisig, multisig_account_initialization)
{
  crypto::secret_key base_privkey = rct::rct2sk(rct::skGen());
  crypto::secret_key base_common_privkey = rct::rct2sk(rct::skGen());

  multisig::multisig_account account(base_privkey, base_common_privkey);

  // After construction with keys, should not yet be active (no kex done)
  EXPECT_FALSE(account.account_is_active());
  EXPECT_FALSE(account.multisig_is_ready());
  EXPECT_EQ(account.get_base_privkey(), base_privkey);
  EXPECT_EQ(account.get_base_common_privkey(), base_common_privkey);
  // Should have a first round kex message ready
  EXPECT_FALSE(account.get_next_kex_round_msg().empty());
}

TEST(multisig, kex_rounds_required_1_of_2)
{
  // 1-of-2: kex_rounds = N - M + 1 = 2 - 1 + 1 = 2
  EXPECT_EQ(multisig::multisig_kex_rounds_required(2, 1), 2u);
  // setup rounds = kex rounds + 1 = 3
  EXPECT_EQ(multisig::multisig_setup_rounds_required(2, 1), 3u);
}

TEST(multisig, kex_rounds_required_2_of_2)
{
  // 2-of-2: kex_rounds = 2 - 2 + 1 = 1
  EXPECT_EQ(multisig::multisig_kex_rounds_required(2, 2), 1u);
  EXPECT_EQ(multisig::multisig_setup_rounds_required(2, 2), 2u);
}

TEST(multisig, kex_rounds_required_2_of_3)
{
  // 2-of-3: kex_rounds = 3 - 2 + 1 = 2
  EXPECT_EQ(multisig::multisig_kex_rounds_required(3, 2), 2u);
  EXPECT_EQ(multisig::multisig_setup_rounds_required(3, 2), 3u);
}

TEST(multisig, kex_rounds_required_3_of_3)
{
  // 3-of-3: kex_rounds = 3 - 3 + 1 = 1
  EXPECT_EQ(multisig::multisig_kex_rounds_required(3, 3), 1u);
  EXPECT_EQ(multisig::multisig_setup_rounds_required(3, 3), 2u);
}

TEST(multisig, kex_rounds_required_n_of_n)
{
  // N-of-N always requires 1 kex round
  for (uint32_t n = 2; n <= 5; ++n)
  {
    EXPECT_EQ(multisig::multisig_kex_rounds_required(n, n), 1u);
    EXPECT_EQ(multisig::multisig_setup_rounds_required(n, n), 2u);
  }
}

TEST(multisig, kex_rounds_required_invalid_m_gt_n_throws)
{
  // M > N should throw
  EXPECT_ANY_THROW(multisig::multisig_kex_rounds_required(2, 3));
}

TEST(multisig, kex_rounds_required_zero_threshold_throws)
{
  // threshold = 0 should throw
  EXPECT_ANY_THROW(multisig::multisig_kex_rounds_required(2, 0));
}

TEST(multisig, kex_msg_multiple_pubkeys)
{
  crypto::secret_key signing_sk = rct::rct2sk(rct::skGen());
  std::vector<crypto::public_key> pubkeys;
  for (int i = 0; i < 5; ++i)
  {
    crypto::public_key pk;
    crypto::secret_key_to_public_key(rct::rct2sk(rct::skGen()), pk);
    pubkeys.push_back(pk);
  }

  multisig::multisig_kex_msg msg(2, signing_sk, pubkeys);
  EXPECT_EQ(msg.get_msg_pubkeys().size(), 5u);

  // Roundtrip
  multisig::multisig_kex_msg parsed(msg.get_msg());
  EXPECT_EQ(parsed.get_msg_pubkeys().size(), 5u);
}

TEST(multisig, multisig_account_base_pubkey_matches)
{
  crypto::secret_key base_privkey = rct::rct2sk(rct::skGen());
  crypto::secret_key base_common_privkey = rct::rct2sk(rct::skGen());

  multisig::multisig_account account(base_privkey, base_common_privkey);

  crypto::public_key expected_pubkey;
  crypto::secret_key_to_public_key(base_privkey, expected_pubkey);
  EXPECT_EQ(account.get_base_pubkey(), expected_pubkey);
}
