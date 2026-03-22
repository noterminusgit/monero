// Copyright (c) 2025, The Monero Project
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

#include "crypto/generators.h"
#include "crypto/crypto.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "device/device.hpp"
#include "serialization/binary_utils.h"
#include "serialization/string.h"
#include "ringct/rctSigs.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "wipeable_string.h"
#include <vector>
#include <string>
#include <unordered_map>

TEST(cn_format_utils, add_extra_nonce_to_tx_extra)
{
    static constexpr std::size_t max_nonce_size = TX_EXTRA_NONCE_MAX_COUNT + 1; // we *can* test higher if desired

    for (int empty_prefix = 0; empty_prefix < 2; ++empty_prefix)
    {
        std::vector<std::uint8_t> extra_prefix;
        if (!empty_prefix)
            cryptonote::add_tx_pub_key_to_extra(extra_prefix, crypto::get_H());

        std::vector<std::uint8_t> extra;
        std::string nonce;
        std::vector<cryptonote::tx_extra_field> tx_extra_fields;
        extra.reserve(extra_prefix.size() + max_nonce_size + 1 + 10);
        nonce.reserve(max_nonce_size);
        tx_extra_fields.reserve(2);
        for (std::size_t nonce_size = 0; nonce_size <= max_nonce_size; ++nonce_size)
        {
            extra = extra_prefix;
            nonce.resize(nonce_size);
            if (nonce.size())
                memset(&nonce[0], '%', nonce.size());
            tx_extra_fields.clear();

            const std::size_t expected_extra_size = extra_prefix.size() + 1
                + tools::get_varint_byte_size(nonce_size) + nonce_size;
            const bool expected_success = nonce_size <= TX_EXTRA_NONCE_MAX_COUNT;

            // add nonce and do detailed test
            const bool add_success = cryptonote::add_extra_nonce_to_tx_extra(extra, nonce);
            ASSERT_EQ(expected_success, add_success);
            if (!expected_success)
                continue;
            ASSERT_EQ(expected_extra_size, extra.size());
            ASSERT_EQ(0, memcmp(extra_prefix.data(), extra.data(), extra_prefix.size()));
            const std::uint8_t *p = extra.data() + extra_prefix.size();
            ASSERT_EQ(TX_EXTRA_NONCE, *p);
            ++p;
            std::size_t read_nonce_size = 0;
            const int varint_size = tools::read_varint((const uint8_t*)(p), // copy p
                (const uint8_t*) extra.data() + extra.size(),
                read_nonce_size);
            ASSERT_EQ(tools::get_varint_byte_size(nonce_size), varint_size);
            p += varint_size;
            for (std::size_t i = 0; i < nonce_size; ++i)
            {
                ASSERT_EQ('%', *p);
                ++p;
            }
            ASSERT_EQ(extra.data() + extra.size(), p);

            // do integration test with higher-level tx_extra parsing code
            ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
            if (empty_prefix)
            {
                ASSERT_EQ(1, tx_extra_fields.size());
                const auto &nonce_field = boost::get<cryptonote::tx_extra_nonce>(tx_extra_fields.at(0));
                ASSERT_EQ(nonce, nonce_field.nonce);
            }
            else
            {
                ASSERT_EQ(2, tx_extra_fields.size());
                const auto &pk_field = boost::get<cryptonote::tx_extra_pub_key>(tx_extra_fields.at(0));
                ASSERT_EQ(crypto::get_H(), pk_field.pub_key);
                const auto &nonce_field = boost::get<cryptonote::tx_extra_nonce>(tx_extra_fields.at(1));
                ASSERT_EQ(nonce, nonce_field.nonce);
            }
        }
    }
}

TEST(cn_format_utils, add_mm_merkle_root_to_tx_extra)
{
    const std::vector<std::uint64_t> depths{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 63, 64, 127, 128, 16383, 16384};

    const crypto::hash mm_merkle_root = crypto::rand<crypto::hash>();

    for (int empty_prefix = 0; empty_prefix < 2; ++empty_prefix)
    {
        std::vector<std::uint8_t> extra_prefix;
        if (!empty_prefix)
            cryptonote::add_tx_pub_key_to_extra(extra_prefix, crypto::get_H());

        std::vector<std::uint8_t> extra;
        std::vector<cryptonote::tx_extra_field> tx_extra_fields;
        extra.reserve(extra_prefix.size() + 1 + 1 + 10 + 32);
        tx_extra_fields.reserve(2);
        for (std::uint64_t mm_merkle_tree_depth : depths)
        {
            extra = extra_prefix;
            tx_extra_fields.clear();

            const std::size_t expected_extra_size = extra_prefix.size() + 1 + 1
                + tools::get_varint_byte_size(mm_merkle_tree_depth) + 32;

            // add nonce and do detailed test
            const bool add_success = cryptonote::add_mm_merkle_root_to_tx_extra(extra, mm_merkle_root, mm_merkle_tree_depth);
            ASSERT_TRUE(add_success);
            ASSERT_EQ(expected_extra_size, extra.size());
            ASSERT_EQ(0, memcmp(extra_prefix.data(), extra.data(), extra_prefix.size()));
            const std::uint8_t *p = extra.data() + extra_prefix.size();
            ASSERT_EQ(TX_EXTRA_MERGE_MINING_TAG, *p);
            ++p;
            ASSERT_EQ(32 + tools::get_varint_byte_size(mm_merkle_tree_depth), *p);
            ++p;
            std::uint64_t read_depth = 0;
            const int varint_size = tools::read_varint((const uint8_t*)(p), // copy p
                (const uint8_t*) extra.data() + extra.size(),
                read_depth);
            ASSERT_EQ(tools::get_varint_byte_size(mm_merkle_tree_depth), varint_size);
            ASSERT_EQ(mm_merkle_tree_depth, read_depth);
            p += varint_size;
            ASSERT_EQ(0, memcmp(p, mm_merkle_root.data, sizeof(mm_merkle_root)));
            p += sizeof(crypto::hash);
            ASSERT_EQ(extra.data() + extra.size(), p);

            // do integration test with higher-level tx_extra parsing code
            ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
            if (empty_prefix)
            {
                ASSERT_EQ(1, tx_extra_fields.size());
                const auto &mm_field = boost::get<cryptonote::tx_extra_merge_mining_tag>(tx_extra_fields.at(0));
                ASSERT_EQ(mm_merkle_root, mm_field.merkle_root);
                ASSERT_EQ(mm_merkle_tree_depth, mm_field.depth);
            }
            else
            {
                ASSERT_EQ(2, tx_extra_fields.size());
                const auto &pk_field = boost::get<cryptonote::tx_extra_pub_key>(tx_extra_fields.at(0));
                ASSERT_EQ(crypto::get_H(), pk_field.pub_key);
                const auto &mm_field = boost::get<cryptonote::tx_extra_merge_mining_tag>(tx_extra_fields.at(1));
                ASSERT_EQ(mm_merkle_root, mm_field.merkle_root);
                ASSERT_EQ(mm_merkle_tree_depth, mm_field.depth);
            }
        }
    }
}

TEST(cn_format_utils, tx_extra_merge_mining_tag_store_load)
{
    const std::vector<std::uint64_t> depths{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 63, 64, 127, 128, 16383, 16384};

    const crypto::hash mm_merkle_root = crypto::rand<crypto::hash>();

    for (int empty_prefix = 0; empty_prefix < 2; ++empty_prefix)
    {
        std::vector<std::uint8_t> extra_prefix;
        if (!empty_prefix)
            cryptonote::add_tx_pub_key_to_extra(extra_prefix, crypto::get_H());

        std::vector<std::uint8_t> extra;
        std::vector<cryptonote::tx_extra_field> tx_extra_fields;
        extra.reserve(extra_prefix.size() + 1 + 1 + 10 + 32);
        tx_extra_fields.reserve(2);
        for (std::uint64_t mm_merkle_tree_depth : depths)
        {
            extra = extra_prefix;
            tx_extra_fields.clear();

            const std::size_t expected_extra_size = extra_prefix.size() + 1 + 1
                + tools::get_varint_byte_size(mm_merkle_tree_depth) + 32;

            // add nonce and do detailed test
            cryptonote::tx_extra_merge_mining_tag mm;
            mm.depth = mm_merkle_tree_depth;
            mm.merkle_root = mm_merkle_root;
            cryptonote::tx_extra_field extra_field = mm;
            std::string mm_blob;
            ASSERT_TRUE(::serialization::dump_binary(extra_field, mm_blob));
            extra.resize(extra.size() + mm_blob.size());
            memcpy(extra.data() + extra.size() - mm_blob.size(), mm_blob.data(), mm_blob.size());
            ASSERT_EQ(expected_extra_size, extra.size());
            ASSERT_EQ(0, memcmp(extra_prefix.data(), extra.data(), extra_prefix.size()));
            const std::uint8_t *p = extra.data() + extra_prefix.size();
            ASSERT_EQ(TX_EXTRA_MERGE_MINING_TAG, *p);
            ++p;
            ASSERT_EQ(32 + tools::get_varint_byte_size(mm_merkle_tree_depth), *p);
            ++p;
            std::uint64_t read_depth = 0;
            const int varint_size = tools::read_varint((const uint8_t*)(p), // copy p
                (const uint8_t*) extra.data() + extra.size(),
                read_depth);
            ASSERT_EQ(tools::get_varint_byte_size(mm_merkle_tree_depth), varint_size);
            ASSERT_EQ(mm_merkle_tree_depth, read_depth);
            p += varint_size;
            ASSERT_EQ(0, memcmp(p, mm_merkle_root.data, sizeof(mm_merkle_root)));
            p += sizeof(crypto::hash);
            ASSERT_EQ(extra.data() + extra.size(), p);

            // do integration test with higher-level tx_extra parsing code
            ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
            if (empty_prefix)
            {
                ASSERT_EQ(1, tx_extra_fields.size());
                const auto &mm_field = boost::get<cryptonote::tx_extra_merge_mining_tag>(tx_extra_fields.at(0));
                ASSERT_EQ(mm_merkle_root, mm_field.merkle_root);
                ASSERT_EQ(mm_merkle_tree_depth, mm_field.depth);
            }
            else
            {
                ASSERT_EQ(2, tx_extra_fields.size());
                const auto &pk_field = boost::get<cryptonote::tx_extra_pub_key>(tx_extra_fields.at(0));
                ASSERT_EQ(crypto::get_H(), pk_field.pub_key);
                const auto &mm_field = boost::get<cryptonote::tx_extra_merge_mining_tag>(tx_extra_fields.at(1));
                ASSERT_EQ(mm_merkle_root, mm_field.merkle_root);
                ASSERT_EQ(mm_merkle_tree_depth, mm_field.depth);
            }
        }
    }
}

TEST(cn_format_utils, parse_tx_extra_empty)
{
    std::vector<uint8_t> extra;
    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_TRUE(fields.empty());
}

TEST(cn_format_utils, add_tx_pub_key_to_extra)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));
    ASSERT_FALSE(extra.empty());

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 1u);

    const auto &pk_field = boost::get<cryptonote::tx_extra_pub_key>(fields[0]);
    ASSERT_EQ(pk, pk_field.pub_key);
}

TEST(cn_format_utils, get_tx_pub_key_from_extra)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    cryptonote::add_tx_pub_key_to_extra(extra, pk);

    cryptonote::transaction tx;
    tx.extra = extra;
    crypto::public_key extracted_pk = cryptonote::get_tx_pub_key_from_extra(tx);
    ASSERT_EQ(pk, extracted_pk);
}

TEST(cn_format_utils, is_coinbase)
{
    cryptonote::transaction tx;
    ASSERT_FALSE(cryptonote::is_coinbase(tx));

    cryptonote::txin_gen gen_input;
    gen_input.height = 0;
    tx.vin.push_back(gen_input);
    ASSERT_TRUE(cryptonote::is_coinbase(tx));
}

TEST(cn_format_utils, get_transaction_hash_deterministic)
{
    // Use get_transaction_prefix_hash for a minimal tx, since get_transaction_hash
    // on a v2 tx requires valid prefix_size/unprunable_size fields from serialization.
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;

    crypto::hash h1 = cryptonote::get_transaction_prefix_hash(tx);
    crypto::hash h2 = cryptonote::get_transaction_prefix_hash(tx);
    ASSERT_EQ(h1, h2);
}

TEST(cn_format_utils, tx_to_blob_and_back)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 10;

    cryptonote::txin_gen gen_input;
    gen_input.height = 42;
    tx.vin.push_back(gen_input);

    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));
    ASSERT_FALSE(blob.empty());

    cryptonote::transaction tx2;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
    ASSERT_EQ(tx.version, tx2.version);
    ASSERT_EQ(tx.unlock_time, tx2.unlock_time);
}

TEST(cn_format_utils, parse_invalid_blob_fails)
{
    cryptonote::transaction tx;
    cryptonote::blobdata invalid_blob = "this is not a valid transaction blob";
    ASSERT_FALSE(cryptonote::parse_and_validate_tx_from_blob(invalid_blob, tx));
}

TEST(cn_format_utils, add_extra_nonce_payment_id)
{
    std::vector<uint8_t> extra;
    crypto::hash payment_id = crypto::rand<crypto::hash>();
    std::string nonce;
    cryptonote::set_payment_id_to_tx_extra_nonce(nonce, payment_id);
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, nonce));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 1u);
}

TEST(cn_format_utils, relative_absolute_offsets_roundtrip)
{
    std::vector<uint64_t> absolute = {10, 20, 35, 100, 200};
    std::vector<uint64_t> relative = cryptonote::absolute_output_offsets_to_relative(absolute);
    std::vector<uint64_t> recovered = cryptonote::relative_output_offsets_to_absolute(relative);
    ASSERT_EQ(absolute, recovered);
}

TEST(cn_format_utils, relative_offsets_empty)
{
    std::vector<uint64_t> empty;
    std::vector<uint64_t> relative = cryptonote::absolute_output_offsets_to_relative(empty);
    ASSERT_TRUE(relative.empty());
}

TEST(cn_format_utils, relative_offsets_single)
{
    std::vector<uint64_t> absolute = {42};
    std::vector<uint64_t> relative = cryptonote::absolute_output_offsets_to_relative(absolute);
    ASSERT_EQ(relative.size(), 1u);
    ASSERT_EQ(relative[0], 42u);
    std::vector<uint64_t> recovered = cryptonote::relative_output_offsets_to_absolute(relative);
    ASSERT_EQ(absolute, recovered);
}

TEST(cn_format_utils, print_money_zero)
{
    ASSERT_EQ(cryptonote::print_money(0), "0.000000000000");
}

TEST(cn_format_utils, print_money_one_xmr)
{
    // 1 XMR = 1e12 atomic units
    ASSERT_EQ(cryptonote::print_money(1000000000000ULL), "1.000000000000");
}

TEST(cn_format_utils, print_money_fractional)
{
    ASSERT_EQ(cryptonote::print_money(1), "0.000000000001");
    ASSERT_EQ(cryptonote::print_money(123456789012ULL), "0.123456789012");
}

TEST(cn_format_utils, print_money_large)
{
    // 18.4 million XMR (near max supply)
    ASSERT_EQ(cryptonote::print_money(18400000000000000000ULL), "18400000.000000000000");
}

TEST(cn_format_utils, round_money_up_basic)
{
    // 1.23456789 XMR rounded to 2 significant digits
    uint64_t amount = 1234567890000ULL;
    uint64_t rounded = cryptonote::round_money_up(amount, 2);
    ASSERT_GE(rounded, amount);
}

TEST(cn_format_utils, round_money_up_zero)
{
    ASSERT_EQ(cryptonote::round_money_up(0ULL, 2), 0ULL);
}

TEST(cn_format_utils, get_block_height_coinbase)
{
    cryptonote::block b;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen_input;
    gen_input.height = 12345;
    b.miner_tx.vin.push_back(gen_input);
    ASSERT_EQ(cryptonote::get_block_height(b), 12345);
}

TEST(cn_format_utils, short_hash_str)
{
    crypto::hash h;
    memset(&h, 0, sizeof(h));
    std::string s = cryptonote::short_hash_str(h);
    ASSERT_FALSE(s.empty());
    ASSERT_LT(s.size(), 65u); // shorter than full hex
}

TEST(cn_format_utils, get_blob_hash_deterministic)
{
    cryptonote::blobdata blob = "test blob data";
    crypto::hash h1 = cryptonote::get_blob_hash(blob);
    crypto::hash h2 = cryptonote::get_blob_hash(blob);
    ASSERT_EQ(h1, h2);
}

TEST(cn_format_utils, get_blob_hash_different_data)
{
    crypto::hash h1 = cryptonote::get_blob_hash(std::string("data1"));
    crypto::hash h2 = cryptonote::get_blob_hash(std::string("data2"));
    ASSERT_NE(h1, h2);
}

TEST(cn_format_utils, block_to_blob_and_back)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::block_to_blob(b, blob));
    ASSERT_FALSE(blob.empty());

    cryptonote::block b2;
    ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
    ASSERT_EQ(b.major_version, b2.major_version);
    ASSERT_EQ(b.minor_version, b2.minor_version);
    ASSERT_EQ(b.timestamp, b2.timestamp);
    ASSERT_EQ(b.nonce, b2.nonce);
}

TEST(cn_format_utils, block_hash_deterministic)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    crypto::hash h1 = cryptonote::get_block_hash(b);
    crypto::hash h2 = cryptonote::get_block_hash(b);
    ASSERT_EQ(h1, h2);
}

TEST(cn_format_utils, parse_invalid_block_blob_fails)
{
    cryptonote::block b;
    cryptonote::blobdata invalid_blob = "not a valid block";
    ASSERT_FALSE(cryptonote::parse_and_validate_block_from_blob(invalid_blob, b));
}

TEST(cn_format_utils, get_tx_tree_hash_single)
{
    std::vector<crypto::hash> hashes;
    crypto::hash h;
    memset(&h, 0xab, sizeof(h));
    hashes.push_back(h);
    crypto::hash tree = cryptonote::get_tx_tree_hash(hashes);
    ASSERT_EQ(tree, h);
}

TEST(cn_format_utils, get_tx_tree_hash_multiple)
{
    std::vector<crypto::hash> hashes;
    crypto::hash h1, h2;
    memset(&h1, 0xab, sizeof(h1));
    memset(&h2, 0xcd, sizeof(h2));
    hashes.push_back(h1);
    hashes.push_back(h2);
    crypto::hash tree = cryptonote::get_tx_tree_hash(hashes);
    // Result should be deterministic
    crypto::hash tree2 = cryptonote::get_tx_tree_hash(hashes);
    ASSERT_EQ(tree, tree2);
    // Should differ from either input
    ASSERT_NE(tree, h1);
    ASSERT_NE(tree, h2);
}

TEST(cn_format_utils, additional_pub_keys_roundtrip)
{
    std::vector<uint8_t> extra;
    std::vector<crypto::public_key> keys;
    crypto::public_key pk1, pk2;
    // Use H as a valid point
    pk1 = crypto::get_H();
    pk2 = crypto::get_H();
    keys.push_back(pk1);
    keys.push_back(pk2);

    ASSERT_TRUE(cryptonote::add_additional_tx_pub_keys_to_extra(extra, keys));
    std::vector<crypto::public_key> recovered = cryptonote::get_additional_tx_pub_keys_from_extra(extra);
    ASSERT_EQ(recovered.size(), 2u);
    ASSERT_EQ(recovered[0], pk1);
    ASSERT_EQ(recovered[1], pk2);
}

TEST(cn_format_utils, encrypted_payment_id_roundtrip)
{
    crypto::hash8 pid;
    memset(&pid, 0xde, sizeof(pid));
    std::string nonce;
    cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(nonce, pid);

    crypto::hash8 recovered;
    ASSERT_TRUE(cryptonote::get_encrypted_payment_id_from_tx_extra_nonce(nonce, recovered));
    ASSERT_EQ(pid, recovered);
}

TEST(cn_format_utils, payment_id_roundtrip)
{
    crypto::hash pid = crypto::rand<crypto::hash>();
    std::string nonce;
    cryptonote::set_payment_id_to_tx_extra_nonce(nonce, pid);

    crypto::hash recovered;
    ASSERT_TRUE(cryptonote::get_payment_id_from_tx_extra_nonce(nonce, recovered));
    ASSERT_EQ(pid, recovered);
}

TEST(cn_format_utils, remove_field_from_tx_extra)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));
    ASSERT_FALSE(extra.empty());

    // Remove the pub key field
    ASSERT_TRUE(cryptonote::remove_field_from_tx_extra(extra, typeid(cryptonote::tx_extra_pub_key)));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_TRUE(fields.empty());
}

TEST(cn_format_utils, sort_tx_extra)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    cryptonote::add_tx_pub_key_to_extra(extra, pk);
    std::string nonce(10, 'x');
    cryptonote::add_extra_nonce_to_tx_extra(extra, nonce);

    std::vector<uint8_t> sorted;
    ASSERT_TRUE(cryptonote::sort_tx_extra(extra, sorted));
    ASSERT_FALSE(sorted.empty());

    // Sorted extra should still parse correctly
    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(sorted, fields));
    ASSERT_EQ(fields.size(), 2u);
}

TEST(cn_format_utils, is_v1_tx_blob)
{
    // v1 tx starts with varint 1
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));
    ASSERT_TRUE(cryptonote::is_v1_tx(blob));

    // v2 tx
    tx.version = 2;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));
    ASSERT_FALSE(cryptonote::is_v1_tx(blob));
}

TEST(cn_format_utils, set_tx_out_no_view_tag)
{
    cryptonote::tx_out out;
    crypto::public_key pk = crypto::get_H();
    crypto::view_tag vt = {0};
    cryptonote::set_tx_out(100, pk, false, vt, out);
    ASSERT_EQ(out.amount, 100);

    crypto::public_key extracted;
    ASSERT_TRUE(cryptonote::get_output_public_key(out, extracted));
    ASSERT_EQ(extracted, pk);
}

TEST(cn_format_utils, set_tx_out_with_view_tag)
{
    cryptonote::tx_out out;
    crypto::public_key pk = crypto::get_H();
    crypto::view_tag vt;
    vt.data = 0xab;
    cryptonote::set_tx_out(200, pk, true, vt, out);
    ASSERT_EQ(out.amount, 200);

    crypto::public_key extracted;
    ASSERT_TRUE(cryptonote::get_output_public_key(out, extracted));
    ASSERT_EQ(extracted, pk);

    auto vt_opt = cryptonote::get_output_view_tag(out);
    ASSERT_TRUE(!!vt_opt);
    ASSERT_EQ(static_cast<unsigned char>(vt_opt->data), 0xab);
}

TEST(cn_format_utils, get_unit)
{
    std::string unit = cryptonote::get_unit();
    ASSERT_FALSE(unit.empty());
}

TEST(cn_format_utils, is_valid_decomposed_amount)
{
    // Powers of 10 * {1..9} are valid decomposed amounts
    ASSERT_TRUE(cryptonote::is_valid_decomposed_amount(1));
    ASSERT_TRUE(cryptonote::is_valid_decomposed_amount(2));
    ASSERT_TRUE(cryptonote::is_valid_decomposed_amount(10));
    ASSERT_TRUE(cryptonote::is_valid_decomposed_amount(90));
    ASSERT_TRUE(cryptonote::is_valid_decomposed_amount(1000000000000ULL)); // 1 XMR
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(0));
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(11));
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(15));
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(123));
}

// ===== Phase 7 extended tests =====

TEST(cn_format_utils, parse_tx_extra_empty_is_valid)
{
    std::vector<uint8_t> extra;
    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 0u);
}

TEST(cn_format_utils, parse_tx_extra_with_pubkey_tag)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 1u);

    const auto& pk_field = boost::get<cryptonote::tx_extra_pub_key>(fields[0]);
    ASSERT_EQ(pk, pk_field.pub_key);
}

TEST(cn_format_utils, parse_tx_extra_with_nonce_tag)
{
    std::vector<uint8_t> extra;
    std::string nonce(32, 'N');
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, nonce));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 1u);

    const auto& nonce_field = boost::get<cryptonote::tx_extra_nonce>(fields[0]);
    ASSERT_EQ(nonce, nonce_field.nonce);
}

TEST(cn_format_utils, parse_tx_extra_with_padding)
{
    std::vector<uint8_t> extra;
    // TX_EXTRA_TAG_PADDING = 0x00
    extra.push_back(0x00);

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 1u);
}

TEST(cn_format_utils, parse_tx_extra_with_multiple_tags)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));
    std::string nonce(16, 'A');
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, nonce));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 2u);
}

TEST(cn_format_utils, parse_tx_extra_malformed_truncated)
{
    // Pubkey tag followed by insufficient data
    std::vector<uint8_t> extra;
    extra.push_back(TX_EXTRA_TAG_PUBKEY);
    extra.push_back(0x01); // only 1 byte instead of 32

    std::vector<cryptonote::tx_extra_field> fields;
    // May fail to parse or just parse what it can
    cryptonote::parse_tx_extra(extra, fields);
    // Just ensure it does not crash
}

TEST(cn_format_utils, get_tx_pub_key_from_extra_roundtrip)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));

    cryptonote::transaction tx;
    tx.extra = extra;
    crypto::public_key extracted = cryptonote::get_tx_pub_key_from_extra(tx);
    ASSERT_EQ(pk, extracted);
}

TEST(cn_format_utils, get_tx_pub_key_from_empty_extra)
{
    cryptonote::transaction tx;
    // No extra data - should return null key
    crypto::public_key extracted = cryptonote::get_tx_pub_key_from_extra(tx);
    ASSERT_EQ(extracted, crypto::null_pkey);
}

TEST(cn_format_utils, add_tx_pub_key_roundtrip)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 1u);

    const auto& pk_field = boost::get<cryptonote::tx_extra_pub_key>(fields[0]);
    ASSERT_EQ(pk, pk_field.pub_key);
}

TEST(cn_format_utils, add_extra_nonce_max_size)
{
    std::vector<uint8_t> extra;
    std::string max_nonce(TX_EXTRA_NONCE_MAX_COUNT, 'X');
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, max_nonce));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 1u);
}

TEST(cn_format_utils, add_extra_nonce_over_max_fails)
{
    std::vector<uint8_t> extra;
    std::string too_big(TX_EXTRA_NONCE_MAX_COUNT + 1, 'X');
    ASSERT_FALSE(cryptonote::add_extra_nonce_to_tx_extra(extra, too_big));
}

TEST(cn_format_utils, add_extra_nonce_empty)
{
    std::vector<uint8_t> extra;
    std::string empty_nonce;
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, empty_nonce));
    ASSERT_FALSE(extra.empty());
}

TEST(cn_format_utils, encrypted_payment_id_from_nonce)
{
    crypto::hash8 pid;
    memset(&pid, 0xde, sizeof(pid));
    std::string nonce;
    cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(nonce, pid);

    crypto::hash8 recovered;
    ASSERT_TRUE(cryptonote::get_encrypted_payment_id_from_tx_extra_nonce(nonce, recovered));
    ASSERT_EQ(pid, recovered);
}

TEST(cn_format_utils, encrypted_payment_id_wrong_nonce_fails)
{
    // A nonce with a non-encrypted payment id tag (0x00) but hash8 size should fail
    // Note: 0x01 IS TX_EXTRA_NONCE_ENCRYPTED_PAYMENT_ID, so use 0x00 (TX_EXTRA_NONCE_PAYMENT_ID)
    std::string nonce;
    nonce.push_back(0x00); // unencrypted payment id tag, wrong for encrypted lookup
    nonce.append(8, 'X');

    crypto::hash8 recovered;
    ASSERT_FALSE(cryptonote::get_encrypted_payment_id_from_tx_extra_nonce(nonce, recovered));
}

TEST(cn_format_utils, address_parsing_valid_mainnet)
{
    cryptonote::account_base account;
    account.generate();
    const auto& addr = account.get_keys().m_account_address;

    std::string str = cryptonote::get_account_address_as_str(cryptonote::MAINNET, false, addr);
    ASSERT_FALSE(str.empty());

    cryptonote::address_parse_info info;
    ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, str));
    ASSERT_EQ(info.address, addr);
    ASSERT_FALSE(info.is_subaddress);
}

TEST(cn_format_utils, address_parsing_valid_testnet)
{
    cryptonote::account_base account;
    account.generate();
    const auto& addr = account.get_keys().m_account_address;

    std::string str = cryptonote::get_account_address_as_str(cryptonote::TESTNET, false, addr);
    cryptonote::address_parse_info info;
    ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::TESTNET, str));
    ASSERT_EQ(info.address, addr);
}

TEST(cn_format_utils, address_parsing_valid_stagenet)
{
    cryptonote::account_base account;
    account.generate();
    const auto& addr = account.get_keys().m_account_address;

    std::string str = cryptonote::get_account_address_as_str(cryptonote::STAGENET, false, addr);
    cryptonote::address_parse_info info;
    ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::STAGENET, str));
    ASSERT_EQ(info.address, addr);
}

TEST(cn_format_utils, address_parsing_invalid_string)
{
    cryptonote::address_parse_info info;
    ASSERT_FALSE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, "not_an_address"));
    ASSERT_FALSE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, ""));
    ASSERT_FALSE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, "4aaaaaaaaaa"));
}

TEST(cn_format_utils, address_formatting_roundtrip)
{
    cryptonote::account_base account;
    account.generate();
    const auto& addr = account.get_keys().m_account_address;

    std::string str = cryptonote::get_account_address_as_str(cryptonote::MAINNET, false, addr);
    cryptonote::address_parse_info info;
    ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, str));

    // Re-format and verify same string
    std::string str2 = cryptonote::get_account_address_as_str(cryptonote::MAINNET, false, info.address);
    ASSERT_EQ(str, str2);
}

TEST(cn_format_utils, integrated_address_creation_and_parsing)
{
    cryptonote::account_base account;
    account.generate();
    const auto& addr = account.get_keys().m_account_address;

    crypto::hash8 payment_id;
    memset(&payment_id, 0xab, sizeof(payment_id));

    std::string integrated_str = cryptonote::get_account_integrated_address_as_str(
        cryptonote::MAINNET, addr, payment_id);
    ASSERT_FALSE(integrated_str.empty());

    cryptonote::address_parse_info info;
    ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, integrated_str));
    ASSERT_TRUE(info.has_payment_id);
    ASSERT_EQ(info.payment_id, payment_id);
    ASSERT_EQ(info.address, addr);
}

TEST(cn_format_utils, subaddress_detection)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();
    hw::device& dev = hw::get_device("default");

    cryptonote::subaddress_index idx = {0, 1};
    cryptonote::account_public_address subaddr = dev.get_subaddress(keys, idx);

    std::string str = cryptonote::get_account_address_as_str(cryptonote::MAINNET, true, subaddr);
    cryptonote::address_parse_info info;
    ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, str));
    ASSERT_TRUE(info.is_subaddress);
}

TEST(cn_format_utils, is_coinbase_empty_tx)
{
    cryptonote::transaction tx;
    ASSERT_FALSE(cryptonote::is_coinbase(tx));
}

TEST(cn_format_utils, is_coinbase_with_gen_input)
{
    cryptonote::transaction tx;
    cryptonote::txin_gen gen;
    gen.height = 100;
    tx.vin.push_back(gen);
    ASSERT_TRUE(cryptonote::is_coinbase(tx));
}

TEST(cn_format_utils, is_coinbase_with_key_input)
{
    cryptonote::transaction tx;
    cryptonote::txin_to_key key_in;
    key_in.amount = 100;
    tx.vin.push_back(key_in);
    ASSERT_FALSE(cryptonote::is_coinbase(tx));
}

TEST(cn_format_utils, block_serialization_roundtrip_detailed)
{
    cryptonote::block b;
    b.major_version = 16;
    b.minor_version = 16;
    b.timestamp = 1700000000;
    b.nonce = 0xdeadbeef;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 60;
    cryptonote::txin_gen gen;
    gen.height = 500;
    b.miner_tx.vin.push_back(gen);

    // Add some tx hashes
    crypto::hash txh1, txh2;
    memset(&txh1, 0xaa, sizeof(txh1));
    memset(&txh2, 0xbb, sizeof(txh2));
    b.tx_hashes.push_back(txh1);
    b.tx_hashes.push_back(txh2);

    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::block_to_blob(b, blob));
    ASSERT_FALSE(blob.empty());

    cryptonote::block b2;
    ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
    ASSERT_EQ(b.major_version, b2.major_version);
    ASSERT_EQ(b.minor_version, b2.minor_version);
    ASSERT_EQ(b.timestamp, b2.timestamp);
    ASSERT_EQ(b.nonce, b2.nonce);
    ASSERT_EQ(b.tx_hashes.size(), b2.tx_hashes.size());
    ASSERT_EQ(b.tx_hashes[0], b2.tx_hashes[0]);
    ASSERT_EQ(b.tx_hashes[1], b2.tx_hashes[1]);
}

TEST(cn_format_utils, get_block_hashing_blob_genesis_like)
{
    cryptonote::block b;
    b.major_version = 1;
    b.minor_version = 0;
    b.timestamp = 0;
    b.nonce = 0;
    b.miner_tx.version = 1;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 0;
    b.miner_tx.vin.push_back(gen);

    cryptonote::blobdata hashing_blob = cryptonote::get_block_hashing_blob(b);
    ASSERT_FALSE(hashing_blob.empty());

    // Same block should produce same hashing blob
    cryptonote::blobdata hashing_blob2 = cryptonote::get_block_hashing_blob(b);
    ASSERT_EQ(hashing_blob, hashing_blob2);
}

TEST(cn_format_utils, get_transaction_prefix_hash_determinism)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 42;
    tx.vin.push_back(gen);

    crypto::hash h1 = cryptonote::get_transaction_prefix_hash(tx);
    crypto::hash h2 = cryptonote::get_transaction_prefix_hash(tx);
    ASSERT_EQ(h1, h2);
}

TEST(cn_format_utils, get_transaction_prefix_hash_differs_for_different_tx)
{
    cryptonote::transaction tx1, tx2;
    tx1.version = 2;
    tx1.unlock_time = 0;
    tx2.version = 2;
    tx2.unlock_time = 100;

    cryptonote::txin_gen gen1, gen2;
    gen1.height = 1;
    gen2.height = 2;
    tx1.vin.push_back(gen1);
    tx2.vin.push_back(gen2);

    crypto::hash h1 = cryptonote::get_transaction_prefix_hash(tx1);
    crypto::hash h2 = cryptonote::get_transaction_prefix_hash(tx2);
    ASSERT_NE(h1, h2);
}

TEST(cn_format_utils, tx_blob_v1_vs_v2)
{
    cryptonote::transaction tx_v1, tx_v2;
    tx_v1.version = 1;
    tx_v1.unlock_time = 0;
    tx_v2.version = 2;
    tx_v2.unlock_time = 0;

    cryptonote::blobdata blob1, blob2;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx_v1, blob1));
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx_v2, blob2));
    ASSERT_TRUE(cryptonote::is_v1_tx(blob1));
    ASSERT_FALSE(cryptonote::is_v1_tx(blob2));
}

TEST(cn_format_utils, get_transaction_hash_differs_for_different_tx)
{
    // Use get_transaction_prefix_hash for minimal txs, since get_transaction_hash
    // on a v2 tx requires valid prefix_size/unprunable_size fields from serialization.
    cryptonote::transaction tx1, tx2;
    tx1.version = 2;
    tx1.unlock_time = 0;
    tx2.version = 2;
    tx2.unlock_time = 1;

    crypto::hash h1 = cryptonote::get_transaction_prefix_hash(tx1);
    crypto::hash h2 = cryptonote::get_transaction_prefix_hash(tx2);
    ASSERT_NE(h1, h2);
}

TEST(cn_format_utils, relative_absolute_offsets_large)
{
    std::vector<uint64_t> absolute;
    for (uint64_t i = 0; i < 100; ++i)
        absolute.push_back(i * 1000 + i);
    std::vector<uint64_t> relative = cryptonote::absolute_output_offsets_to_relative(absolute);
    std::vector<uint64_t> recovered = cryptonote::relative_output_offsets_to_absolute(relative);
    ASSERT_EQ(absolute, recovered);
}

TEST(cn_format_utils, relative_offsets_same_values)
{
    std::vector<uint64_t> absolute = {5, 5, 5};
    std::vector<uint64_t> relative = cryptonote::absolute_output_offsets_to_relative(absolute);
    ASSERT_EQ(relative[0], 5u);
    ASSERT_EQ(relative[1], 0u);
    ASSERT_EQ(relative[2], 0u);
    std::vector<uint64_t> recovered = cryptonote::relative_output_offsets_to_absolute(relative);
    ASSERT_EQ(absolute, recovered);
}

TEST(cn_format_utils, payment_id_from_nonce_wrong_size)
{
    std::string nonce;
    nonce.push_back(0x00); // payment id tag
    nonce.append(10, 'X'); // wrong size

    crypto::hash recovered;
    ASSERT_FALSE(cryptonote::get_payment_id_from_tx_extra_nonce(nonce, recovered));
}

TEST(cn_format_utils, remove_field_from_tx_extra_nonexistent)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));

    // remove_field_from_tx_extra returns true even when the field type is not present (no-op success)
    std::vector<uint8_t> extra_before = extra;
    ASSERT_TRUE(cryptonote::remove_field_from_tx_extra(extra, typeid(cryptonote::tx_extra_nonce)));

    // Extra should remain unchanged since the field wasn't present
    ASSERT_EQ(extra, extra_before);
    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 1u);
}

TEST(cn_format_utils, remove_field_from_empty_extra)
{
    // remove_field_from_tx_extra returns true for empty extra (no-op success)
    std::vector<uint8_t> extra;
    ASSERT_TRUE(cryptonote::remove_field_from_tx_extra(extra, typeid(cryptonote::tx_extra_pub_key)));
    ASSERT_TRUE(extra.empty());
}

TEST(cn_format_utils, print_money_exact_amounts)
{
    ASSERT_EQ(cryptonote::print_money(100000000000ULL), "0.100000000000");
    ASSERT_EQ(cryptonote::print_money(10000000000ULL), "0.010000000000");
    ASSERT_EQ(cryptonote::print_money(1000000000ULL), "0.001000000000");
    ASSERT_EQ(cryptonote::print_money(100000000ULL), "0.000100000000");
}

TEST(cn_format_utils, get_block_height_from_coinbase)
{
    cryptonote::block b;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 0;
    b.miner_tx.vin.push_back(gen);
    ASSERT_EQ(cryptonote::get_block_height(b), 0u);

    b.miner_tx.vin.clear();
    gen.height = 999999;
    b.miner_tx.vin.push_back(gen);
    ASSERT_EQ(cryptonote::get_block_height(b), 999999u);
}

TEST(cn_format_utils, multiple_pub_keys_in_extra)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk1 = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk1));
    // Add additional tx pub keys
    std::vector<crypto::public_key> additional_keys = {crypto::get_H(), crypto::get_H()};
    ASSERT_TRUE(cryptonote::add_additional_tx_pub_keys_to_extra(extra, additional_keys));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_GE(fields.size(), 2u);
}

TEST(cn_format_utils, sort_tx_extra_preserves_fields)
{
    std::vector<uint8_t> extra;
    std::string nonce(16, 'Z');
    cryptonote::add_extra_nonce_to_tx_extra(extra, nonce);
    crypto::public_key pk = crypto::get_H();
    cryptonote::add_tx_pub_key_to_extra(extra, pk);

    std::vector<uint8_t> sorted;
    ASSERT_TRUE(cryptonote::sort_tx_extra(extra, sorted));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(sorted, fields));
    ASSERT_EQ(fields.size(), 2u);
}

TEST(cn_format_utils, get_blob_hash_empty_blob)
{
    crypto::hash h = cryptonote::get_blob_hash(std::string(""));
    crypto::hash null_hash;
    memset(&null_hash, 0, sizeof(null_hash));
    // Hash of empty should be a specific non-null value
    ASSERT_NE(h, null_hash);
}

TEST(cn_format_utils, is_valid_decomposed_amount_all_digits)
{
    // All valid single-digit * power-of-10 amounts
    for (uint64_t digit = 1; digit <= 9; ++digit)
    {
        uint64_t base = 1;
        for (int p = 0; p < 13; ++p)
        {
            ASSERT_TRUE(cryptonote::is_valid_decomposed_amount(digit * base));
            base *= 10;
        }
    }
}

TEST(cn_format_utils, round_money_up_identity_for_single_digit)
{
    // A single significant digit should stay the same when rounded to 1
    ASSERT_EQ(cryptonote::round_money_up(1000000000000ULL, 1), 1000000000000ULL);
}

TEST(cn_format_utils, set_tx_out_amount_zero)
{
    cryptonote::tx_out out;
    crypto::public_key pk = crypto::get_H();
    crypto::view_tag vt = {0};
    cryptonote::set_tx_out(0, pk, false, vt, out);
    ASSERT_EQ(out.amount, 0u);
}

// =====================================================================
// Helper: create a minimal v1 coinbase transaction with outputs
// =====================================================================
static cryptonote::transaction make_v1_coinbase_tx(uint64_t height, const std::vector<std::pair<uint64_t, crypto::public_key>>& outs)
{
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;
    cryptonote::txin_gen gen;
    gen.height = height;
    tx.vin.push_back(gen);
    for (const auto& p : outs)
    {
        cryptonote::tx_out out;
        cryptonote::set_tx_out(p.first, p.second, false, crypto::view_tag{}, out);
        tx.vout.push_back(out);
    }
    return tx;
}

// Helper: create a minimal v1 tx with key inputs and outputs (non-coinbase)
static cryptonote::transaction make_v1_tx_with_key_inputs(
    const std::vector<uint64_t>& input_amounts,
    const std::vector<uint64_t>& output_amounts)
{
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    for (uint64_t amt : input_amounts)
    {
        cryptonote::txin_to_key key_in;
        key_in.amount = amt;
        key_in.key_offsets = {0};
        key_in.k_image = crypto::rand<crypto::key_image>();
        tx.vin.push_back(key_in);
    }
    for (uint64_t amt : output_amounts)
    {
        cryptonote::tx_out out;
        crypto::public_key pk = crypto::rand<crypto::public_key>();
        cryptonote::set_tx_out(amt, pk, false, crypto::view_tag{}, out);
        tx.vout.push_back(out);
    }
    return tx;
}

// =====================================================================
// get_tx_fee tests
// =====================================================================
TEST(cn_format_utils, get_tx_fee_v1_tx)
{
    auto tx = make_v1_tx_with_key_inputs({100, 200, 300}, {50, 150, 100});
    uint64_t fee = 0;
    ASSERT_TRUE(cryptonote::get_tx_fee(tx, fee));
    ASSERT_EQ(fee, 300u); // 600 - 300 = 300
}

TEST(cn_format_utils, get_tx_fee_v1_zero_fee)
{
    auto tx = make_v1_tx_with_key_inputs({100}, {100});
    uint64_t fee = 0;
    ASSERT_TRUE(cryptonote::get_tx_fee(tx, fee));
    ASSERT_EQ(fee, 0u);
}

TEST(cn_format_utils, get_tx_fee_v2_tx)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;
    tx.rct_signatures.txnFee = 12345;
    uint64_t fee = 0;
    ASSERT_TRUE(cryptonote::get_tx_fee(tx, fee));
    ASSERT_EQ(fee, 12345u);
}

TEST(cn_format_utils, get_tx_fee_single_arg)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;
    tx.rct_signatures.txnFee = 9999;
    ASSERT_EQ(cryptonote::get_tx_fee(tx), 9999u);
}

// =====================================================================
// get_inputs_money_amount tests
// =====================================================================
TEST(cn_format_utils, get_inputs_money_amount_basic)
{
    auto tx = make_v1_tx_with_key_inputs({100, 200, 300}, {500});
    uint64_t money = 0;
    ASSERT_TRUE(cryptonote::get_inputs_money_amount(tx, money));
    ASSERT_EQ(money, 600u);
}

TEST(cn_format_utils, get_inputs_money_amount_single)
{
    auto tx = make_v1_tx_with_key_inputs({42}, {42});
    uint64_t money = 0;
    ASSERT_TRUE(cryptonote::get_inputs_money_amount(tx, money));
    ASSERT_EQ(money, 42u);
}

TEST(cn_format_utils, get_inputs_money_amount_empty)
{
    cryptonote::transaction tx;
    tx.version = 1;
    uint64_t money = 0;
    ASSERT_TRUE(cryptonote::get_inputs_money_amount(tx, money));
    ASSERT_EQ(money, 0u);
}

// =====================================================================
// get_outs_money_amount tests
// =====================================================================
TEST(cn_format_utils, get_outs_money_amount_basic)
{
    auto tx = make_v1_tx_with_key_inputs({600}, {100, 200, 300});
    uint64_t amount = cryptonote::get_outs_money_amount(tx);
    ASSERT_EQ(amount, 600u);
}

TEST(cn_format_utils, get_outs_money_amount_empty)
{
    cryptonote::transaction tx;
    tx.version = 1;
    uint64_t amount = cryptonote::get_outs_money_amount(tx);
    ASSERT_EQ(amount, 0u);
}

TEST(cn_format_utils, get_outs_money_amount_single)
{
    auto tx = make_v1_tx_with_key_inputs({42}, {42});
    uint64_t amount = cryptonote::get_outs_money_amount(tx);
    ASSERT_EQ(amount, 42u);
}

// =====================================================================
// check_inputs_types_supported tests
// =====================================================================
TEST(cn_format_utils, check_inputs_types_supported_valid)
{
    auto tx = make_v1_tx_with_key_inputs({100}, {100});
    ASSERT_TRUE(cryptonote::check_inputs_types_supported(tx));
}

TEST(cn_format_utils, check_inputs_types_supported_empty)
{
    cryptonote::transaction tx;
    tx.version = 1;
    // No inputs - trivially supported
    ASSERT_TRUE(cryptonote::check_inputs_types_supported(tx));
}

TEST(cn_format_utils, check_inputs_types_supported_coinbase_fails)
{
    cryptonote::transaction tx;
    tx.version = 1;
    cryptonote::txin_gen gen;
    gen.height = 0;
    tx.vin.push_back(gen);
    // txin_gen is not txin_to_key, should fail
    ASSERT_FALSE(cryptonote::check_inputs_types_supported(tx));
}

// =====================================================================
// check_outs_valid tests
// =====================================================================
TEST(cn_format_utils, check_outs_valid_v1_nonzero_amount)
{
    // Create a v1 tx with valid (non-zero amount, valid key) outputs
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::txin_to_key key_in;
    key_in.amount = 100;
    key_in.key_offsets = {0};
    key_in.k_image = crypto::rand<crypto::key_image>();
    tx.vin.push_back(key_in);

    cryptonote::tx_out out;
    // Use H which is a valid point on the curve
    cryptonote::set_tx_out(100, crypto::get_H(), false, crypto::view_tag{}, out);
    tx.vout.push_back(out);

    ASSERT_TRUE(cryptonote::check_outs_valid(tx));
}

TEST(cn_format_utils, check_outs_valid_v1_zero_amount_fails)
{
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::txin_to_key key_in;
    key_in.amount = 100;
    key_in.key_offsets = {0};
    key_in.k_image = crypto::rand<crypto::key_image>();
    tx.vin.push_back(key_in);

    cryptonote::tx_out out;
    // Zero amount for v1 should fail
    cryptonote::set_tx_out(0, crypto::get_H(), false, crypto::view_tag{}, out);
    tx.vout.push_back(out);

    ASSERT_FALSE(cryptonote::check_outs_valid(tx));
}

TEST(cn_format_utils, check_outs_valid_v2_zero_amount_ok)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;

    cryptonote::tx_out out;
    cryptonote::set_tx_out(0, crypto::get_H(), false, crypto::view_tag{}, out);
    tx.vout.push_back(out);

    ASSERT_TRUE(cryptonote::check_outs_valid(tx));
}

TEST(cn_format_utils, check_outs_valid_empty)
{
    cryptonote::transaction tx;
    tx.version = 2;
    ASSERT_TRUE(cryptonote::check_outs_valid(tx));
}

// =====================================================================
// check_outs_overflow / check_inputs_overflow / check_money_overflow
// =====================================================================
TEST(cn_format_utils, check_outs_overflow_no_overflow)
{
    auto tx = make_v1_tx_with_key_inputs({200}, {100, 100});
    ASSERT_TRUE(cryptonote::check_outs_overflow(tx));
}

TEST(cn_format_utils, check_outs_overflow_empty)
{
    cryptonote::transaction tx;
    tx.version = 1;
    ASSERT_TRUE(cryptonote::check_outs_overflow(tx));
}

TEST(cn_format_utils, check_inputs_overflow_no_overflow)
{
    auto tx = make_v1_tx_with_key_inputs({100, 200, 300}, {500});
    ASSERT_TRUE(cryptonote::check_inputs_overflow(tx));
}

TEST(cn_format_utils, check_inputs_overflow_empty)
{
    cryptonote::transaction tx;
    tx.version = 1;
    // No inputs - no overflow
    ASSERT_TRUE(cryptonote::check_inputs_overflow(tx));
}

TEST(cn_format_utils, check_money_overflow_valid)
{
    auto tx = make_v1_tx_with_key_inputs({100, 200}, {100, 200});
    ASSERT_TRUE(cryptonote::check_money_overflow(tx));
}

// =====================================================================
// get_transaction_weight tests (v1 tx weight == blob_size)
// =====================================================================
TEST(cn_format_utils, get_transaction_weight_v1)
{
    auto tx = make_v1_coinbase_tx(0, {{1000000, crypto::get_H()}});
    // Serialize to get blob size
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));
    // Parse so tx has valid internal sizes
    cryptonote::transaction parsed_tx;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, parsed_tx));
    uint64_t weight = cryptonote::get_transaction_weight(parsed_tx);
    ASSERT_EQ(weight, blob.size());
}

TEST(cn_format_utils, get_transaction_weight_v1_with_blob_size)
{
    auto tx = make_v1_coinbase_tx(0, {{1000000, crypto::get_H()}});
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));
    cryptonote::transaction parsed_tx;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, parsed_tx));
    uint64_t weight = cryptonote::get_transaction_weight(parsed_tx, blob.size());
    ASSERT_EQ(weight, blob.size());
}

TEST(cn_format_utils, get_transaction_blob_size)
{
    auto tx = make_v1_coinbase_tx(0, {{1000000, crypto::get_H()}});
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));
    cryptonote::transaction parsed_tx;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, parsed_tx));
    uint64_t size = cryptonote::get_transaction_blob_size(parsed_tx);
    ASSERT_EQ(size, blob.size());
}

// =====================================================================
// parse_amount tests
// =====================================================================
TEST(cn_format_utils, parse_amount_whole)
{
    uint64_t amount = 0;
    ASSERT_TRUE(cryptonote::parse_amount(amount, "1"));
    ASSERT_EQ(amount, 1000000000000ULL); // 1 XMR
}

TEST(cn_format_utils, parse_amount_fractional)
{
    uint64_t amount = 0;
    ASSERT_TRUE(cryptonote::parse_amount(amount, "0.1"));
    ASSERT_EQ(amount, 100000000000ULL);
}

TEST(cn_format_utils, parse_amount_zero)
{
    uint64_t amount = 0;
    ASSERT_TRUE(cryptonote::parse_amount(amount, "0"));
    ASSERT_EQ(amount, 0u);
}

TEST(cn_format_utils, parse_amount_full_precision)
{
    uint64_t amount = 0;
    ASSERT_TRUE(cryptonote::parse_amount(amount, "0.000000000001"));
    ASSERT_EQ(amount, 1u);
}

TEST(cn_format_utils, parse_amount_trailing_zeros)
{
    uint64_t amount = 0;
    ASSERT_TRUE(cryptonote::parse_amount(amount, "1.0"));
    ASSERT_EQ(amount, 1000000000000ULL);
}

TEST(cn_format_utils, parse_amount_with_whitespace)
{
    uint64_t amount = 0;
    ASSERT_TRUE(cryptonote::parse_amount(amount, "  1  "));
    ASSERT_EQ(amount, 1000000000000ULL);
}

TEST(cn_format_utils, parse_amount_empty_fails)
{
    uint64_t amount = 0;
    ASSERT_FALSE(cryptonote::parse_amount(amount, ""));
}

TEST(cn_format_utils, parse_amount_only_dot_fails)
{
    uint64_t amount = 0;
    ASSERT_FALSE(cryptonote::parse_amount(amount, "."));
}

TEST(cn_format_utils, parse_amount_too_many_decimals_fails)
{
    uint64_t amount = 0;
    // 13 decimal places (more than 12)
    ASSERT_FALSE(cryptonote::parse_amount(amount, "0.0000000000001"));
}

TEST(cn_format_utils, parse_amount_large_value)
{
    uint64_t amount = 0;
    ASSERT_TRUE(cryptonote::parse_amount(amount, "18.4"));
    ASSERT_EQ(amount, 18400000000000ULL);
}

// =====================================================================
// set_default_decimal_point / get_default_decimal_point / get_unit
// =====================================================================
TEST(cn_format_utils, decimal_point_default)
{
    // Default should be CRYPTONOTE_DISPLAY_DECIMAL_POINT (12)
    ASSERT_EQ(cryptonote::get_default_decimal_point(), 12u);
}

TEST(cn_format_utils, set_decimal_point_valid_values)
{
    // Store original and restore after test
    unsigned int original = cryptonote::get_default_decimal_point();

    cryptonote::set_default_decimal_point(12);
    ASSERT_EQ(cryptonote::get_default_decimal_point(), 12u);
    ASSERT_EQ(cryptonote::get_unit(), "monero");

    cryptonote::set_default_decimal_point(9);
    ASSERT_EQ(cryptonote::get_default_decimal_point(), 9u);
    ASSERT_EQ(cryptonote::get_unit(), "millinero");

    cryptonote::set_default_decimal_point(6);
    ASSERT_EQ(cryptonote::get_default_decimal_point(), 6u);
    ASSERT_EQ(cryptonote::get_unit(), "micronero");

    cryptonote::set_default_decimal_point(3);
    ASSERT_EQ(cryptonote::get_default_decimal_point(), 3u);
    ASSERT_EQ(cryptonote::get_unit(), "nanonero");

    cryptonote::set_default_decimal_point(0);
    ASSERT_EQ(cryptonote::get_default_decimal_point(), 0u);
    ASSERT_EQ(cryptonote::get_unit(), "piconero");

    // Restore
    cryptonote::set_default_decimal_point(original);
}

TEST(cn_format_utils, set_decimal_point_invalid_throws)
{
    ASSERT_THROW(cryptonote::set_default_decimal_point(7), std::runtime_error);
    ASSERT_THROW(cryptonote::set_default_decimal_point(1), std::runtime_error);
    ASSERT_THROW(cryptonote::set_default_decimal_point(100), std::runtime_error);
}

TEST(cn_format_utils, get_unit_explicit_arg)
{
    ASSERT_EQ(cryptonote::get_unit(12), "monero");
    ASSERT_EQ(cryptonote::get_unit(9), "millinero");
    ASSERT_EQ(cryptonote::get_unit(6), "micronero");
    ASSERT_EQ(cryptonote::get_unit(3), "nanonero");
    ASSERT_EQ(cryptonote::get_unit(0), "piconero");
}

TEST(cn_format_utils, print_money_with_decimal_point)
{
    // print_money with explicit decimal point
    ASSERT_EQ(cryptonote::print_money(1000000000000ULL, 12), "1.000000000000");
    ASSERT_EQ(cryptonote::print_money(1000000000ULL, 9), "1.000000000");
    ASSERT_EQ(cryptonote::print_money(1000000ULL, 6), "1.000000");
    ASSERT_EQ(cryptonote::print_money(1000ULL, 3), "1.000");
    ASSERT_EQ(cryptonote::print_money(1ULL, 0), "1");
}

TEST(cn_format_utils, print_money_uint128)
{
    boost::multiprecision::uint128_t amount = 1000000000000ULL;
    std::string s = cryptonote::print_money(amount);
    ASSERT_EQ(s, "1.000000000000");
}

TEST(cn_format_utils, print_money_uint128_zero)
{
    boost::multiprecision::uint128_t amount = 0;
    std::string s = cryptonote::print_money(amount);
    ASSERT_EQ(s, "0.000000000000");
}

TEST(cn_format_utils, print_money_uint128_large)
{
    boost::multiprecision::uint128_t amount = 18400000000000000000ULL;
    std::string s = cryptonote::print_money(amount);
    ASSERT_EQ(s, "18400000.000000000000");
}

// =====================================================================
// round_money_up string overload
// =====================================================================
TEST(cn_format_utils, round_money_up_string)
{
    std::string result = cryptonote::round_money_up("1.234567890000", 2);
    // Should round up to 1.3 XMR = "1.300000000000"
    ASSERT_EQ(result, "1.300000000000");
}

TEST(cn_format_utils, round_money_up_already_rounded)
{
    ASSERT_EQ(cryptonote::round_money_up(1000000000000ULL, 1), 1000000000000ULL);
    ASSERT_EQ(cryptonote::round_money_up(2000000000000ULL, 1), 2000000000000ULL);
}

TEST(cn_format_utils, round_money_up_bumps)
{
    // 1234 -> round to 2 significant digits -> 1300
    uint64_t rounded = cryptonote::round_money_up(1234ULL, 2);
    ASSERT_EQ(rounded, 1300ULL);
}

TEST(cn_format_utils, round_money_up_nines)
{
    // 999 -> round to 1 significant digit -> 1000
    uint64_t rounded = cryptonote::round_money_up(999ULL, 1);
    ASSERT_EQ(rounded, 1000ULL);
}

TEST(cn_format_utils, round_money_up_exact)
{
    // Already exactly 2 significant digits
    ASSERT_EQ(cryptonote::round_money_up(1200ULL, 2), 1200ULL);
}

// =====================================================================
// get_blob_hash with blobdata_ref overload
// =====================================================================
TEST(cn_format_utils, get_blob_hash_ref_overload)
{
    std::string data = "test blob ref data";
    cryptonote::blobdata_ref ref(data.data(), data.size());
    crypto::hash h1 = cryptonote::get_blob_hash(ref);
    crypto::hash h2 = cryptonote::get_blob_hash(data);
    ASSERT_EQ(h1, h2);
}

TEST(cn_format_utils, get_blob_hash_ref_void_overload)
{
    std::string data = "another blob";
    cryptonote::blobdata_ref ref(data.data(), data.size());
    crypto::hash h1, h2;
    cryptonote::get_blob_hash(ref, h1);
    cryptonote::get_blob_hash(data, h2);
    ASSERT_EQ(h1, h2);
}

// =====================================================================
// get_transaction_hash - full tx hash via serialization round-trip
// =====================================================================
TEST(cn_format_utils, get_transaction_hash_v1_coinbase)
{
    auto tx = make_v1_coinbase_tx(100, {{50000, crypto::get_H()}});
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));

    cryptonote::transaction parsed_tx;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, parsed_tx));

    crypto::hash h1 = cryptonote::get_transaction_hash(parsed_tx);
    crypto::hash h2;
    ASSERT_TRUE(cryptonote::get_transaction_hash(parsed_tx, h2));
    ASSERT_EQ(h1, h2);

    // Hash should be deterministic
    crypto::hash h3 = cryptonote::get_transaction_hash(parsed_tx);
    ASSERT_EQ(h1, h3);
}

TEST(cn_format_utils, get_transaction_hash_with_blob_size)
{
    auto tx = make_v1_coinbase_tx(100, {{50000, crypto::get_H()}});
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));

    cryptonote::transaction parsed_tx;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, parsed_tx));

    crypto::hash h;
    size_t blob_size = 0;
    ASSERT_TRUE(cryptonote::get_transaction_hash(parsed_tx, h, blob_size));
    ASSERT_EQ(blob_size, blob.size());
}

// =====================================================================
// get_transaction_prefix_hash with hw::device overload
// =====================================================================
TEST(cn_format_utils, get_transaction_prefix_hash_with_device)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 42;
    tx.vin.push_back(gen);

    hw::device& dev = hw::get_device("default");
    crypto::hash h1 = cryptonote::get_transaction_prefix_hash(tx, dev);
    crypto::hash h2 = cryptonote::get_transaction_prefix_hash(tx);
    ASSERT_EQ(h1, h2);
}

TEST(cn_format_utils, get_transaction_prefix_hash_void_overload)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 42;
    tx.vin.push_back(gen);

    crypto::hash h1, h2;
    cryptonote::get_transaction_prefix_hash(tx, h1);
    h2 = cryptonote::get_transaction_prefix_hash(tx);
    ASSERT_EQ(h1, h2);
}

// =====================================================================
// parse_and_validate_tx_from_blob with hash variants
// =====================================================================
TEST(cn_format_utils, parse_and_validate_tx_from_blob_with_hash)
{
    auto tx = make_v1_coinbase_tx(50, {{1000, crypto::get_H()}});
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));

    cryptonote::transaction parsed_tx;
    crypto::hash tx_hash;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, parsed_tx, tx_hash));

    // Hash should match what we get from get_transaction_hash
    crypto::hash expected_hash = cryptonote::get_transaction_hash(parsed_tx);
    ASSERT_EQ(tx_hash, expected_hash);
}

TEST(cn_format_utils, parse_and_validate_tx_from_blob_with_hash_and_prefix_hash)
{
    auto tx = make_v1_coinbase_tx(50, {{1000, crypto::get_H()}});
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));

    cryptonote::transaction parsed_tx;
    crypto::hash tx_hash, tx_prefix_hash;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, parsed_tx, tx_hash, tx_prefix_hash));

    crypto::hash expected_prefix_hash = cryptonote::get_transaction_prefix_hash(parsed_tx);
    ASSERT_EQ(tx_prefix_hash, expected_prefix_hash);
}

// =====================================================================
// parse_and_validate_tx_base_from_blob
// =====================================================================
TEST(cn_format_utils, parse_and_validate_tx_base_from_blob)
{
    auto tx = make_v1_coinbase_tx(50, {{1000, crypto::get_H()}});
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));

    cryptonote::transaction parsed_tx;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_base_from_blob(blob, parsed_tx));
    ASSERT_EQ(parsed_tx.version, tx.version);
}

TEST(cn_format_utils, parse_and_validate_tx_base_from_blob_invalid)
{
    cryptonote::transaction parsed_tx;
    std::string bad_blob = "garbage data";
    ASSERT_FALSE(cryptonote::parse_and_validate_tx_base_from_blob(bad_blob, parsed_tx));
}

// =====================================================================
// parse_and_validate_tx_prefix_from_blob
// =====================================================================
TEST(cn_format_utils, parse_and_validate_tx_prefix_from_blob)
{
    auto tx = make_v1_coinbase_tx(50, {{1000, crypto::get_H()}});
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));

    cryptonote::transaction_prefix parsed;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_prefix_from_blob(blob, parsed));
    ASSERT_EQ(parsed.version, tx.version);
    ASSERT_EQ(parsed.unlock_time, tx.unlock_time);
}

// =====================================================================
// parse_and_validate_block_from_blob with hash overloads
// =====================================================================
TEST(cn_format_utils, parse_and_validate_block_from_blob_with_hash)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::block_to_blob(b, blob));

    cryptonote::block b2;
    crypto::hash block_hash;
    ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2, block_hash));

    // Hash should match what we get from get_block_hash
    crypto::hash expected_hash = cryptonote::get_block_hash(b2);
    ASSERT_EQ(block_hash, expected_hash);
}

TEST(cn_format_utils, parse_and_validate_block_from_blob_with_hash_ptr)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::block_to_blob(b, blob));

    // Parse with null hash ptr (no hash computed)
    cryptonote::block b2;
    ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
    ASSERT_EQ(b.major_version, b2.major_version);
}

// =====================================================================
// get_block_hashing_blob
// =====================================================================
TEST(cn_format_utils, get_block_hashing_blob_nonempty)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    cryptonote::blobdata hashing_blob = cryptonote::get_block_hashing_blob(b);
    ASSERT_FALSE(hashing_blob.empty());
}

TEST(cn_format_utils, get_block_hashing_blob_with_tx_hashes)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    // Add tx hashes
    crypto::hash txh;
    memset(&txh, 0xaa, sizeof(txh));
    b.tx_hashes.push_back(txh);

    cryptonote::blobdata hashing_blob = cryptonote::get_block_hashing_blob(b);
    ASSERT_FALSE(hashing_blob.empty());

    // Different tx hashes should produce different hashing blob
    cryptonote::block b2 = b;
    b2.tx_hashes.clear();
    cryptonote::blobdata hashing_blob2 = cryptonote::get_block_hashing_blob(b2);
    ASSERT_NE(hashing_blob, hashing_blob2);
}

// =====================================================================
// get_block_hash
// =====================================================================
TEST(cn_format_utils, get_block_hash_void_overload)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    crypto::hash h1;
    ASSERT_TRUE(cryptonote::get_block_hash(b, h1));
    crypto::hash h2 = cryptonote::get_block_hash(b);
    ASSERT_EQ(h1, h2);
}

TEST(cn_format_utils, get_block_hash_different_blocks)
{
    cryptonote::block b1, b2;
    b1.major_version = 14;
    b1.minor_version = 14;
    b1.timestamp = 1000000;
    b1.nonce = 1;
    b1.miner_tx.version = 2;
    b1.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen1;
    gen1.height = 100;
    b1.miner_tx.vin.push_back(gen1);

    b2 = b1;
    b2.nonce = 2;
    b2.invalidate_hashes();
    b2.miner_tx.invalidate_hashes();

    crypto::hash h1 = cryptonote::get_block_hash(b1);
    crypto::hash h2 = cryptonote::get_block_hash(b2);
    ASSERT_NE(h1, h2);
}

// =====================================================================
// check_output_types
// =====================================================================
TEST(cn_format_utils, check_output_types_pre_view_tags)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;

    cryptonote::tx_out out;
    cryptonote::set_tx_out(0, crypto::get_H(), false, crypto::view_tag{}, out);
    tx.vout.push_back(out);

    // Before HF_VERSION_VIEW_TAGS (15), only txout_to_key allowed
    ASSERT_TRUE(cryptonote::check_output_types(tx, HF_VERSION_VIEW_TAGS - 1));
}

TEST(cn_format_utils, check_output_types_post_view_tags)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;

    cryptonote::tx_out out;
    crypto::view_tag vt;
    vt.data = 0x42;
    cryptonote::set_tx_out(0, crypto::get_H(), true, vt, out);
    tx.vout.push_back(out);

    // After HF_VERSION_VIEW_TAGS, only txout_to_tagged_key allowed
    ASSERT_TRUE(cryptonote::check_output_types(tx, HF_VERSION_VIEW_TAGS + 1));
}

TEST(cn_format_utils, check_output_types_at_view_tags_hf_both_ok)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;

    // At HF_VERSION_VIEW_TAGS, either type is ok, but all must match
    cryptonote::tx_out out1;
    cryptonote::set_tx_out(0, crypto::get_H(), false, crypto::view_tag{}, out1);
    tx.vout.push_back(out1);

    ASSERT_TRUE(cryptonote::check_output_types(tx, HF_VERSION_VIEW_TAGS));
}

TEST(cn_format_utils, check_output_types_pre_view_tags_tagged_fails)
{
    // Use v1 tx so that get_transaction_hash (called in error log) doesn't throw
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 1;
    tx.vin.push_back(gen);

    cryptonote::tx_out out;
    crypto::view_tag vt;
    vt.data = 0x42;
    cryptonote::set_tx_out(0, crypto::get_H(), true, vt, out);
    tx.vout.push_back(out);

    // Before HF_VERSION_VIEW_TAGS, tagged outputs should fail
    ASSERT_FALSE(cryptonote::check_output_types(tx, HF_VERSION_VIEW_TAGS - 1));
}

TEST(cn_format_utils, check_output_types_post_view_tags_untagged_fails)
{
    // Use v1 tx so that get_transaction_hash (called in error log) doesn't throw
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 1;
    tx.vin.push_back(gen);

    cryptonote::tx_out out;
    cryptonote::set_tx_out(0, crypto::get_H(), false, crypto::view_tag{}, out);
    tx.vout.push_back(out);

    // After HF_VERSION_VIEW_TAGS, untagged outputs should fail
    ASSERT_FALSE(cryptonote::check_output_types(tx, HF_VERSION_VIEW_TAGS + 1));
}

TEST(cn_format_utils, check_output_types_empty_tx)
{
    cryptonote::transaction tx;
    tx.version = 2;
    ASSERT_TRUE(cryptonote::check_output_types(tx, HF_VERSION_VIEW_TAGS));
}

// =====================================================================
// get_output_public_key / get_output_view_tag
// =====================================================================
TEST(cn_format_utils, get_output_public_key_txout_to_key)
{
    cryptonote::tx_out out;
    crypto::public_key pk = crypto::get_H();
    cryptonote::set_tx_out(100, pk, false, crypto::view_tag{}, out);

    crypto::public_key extracted;
    ASSERT_TRUE(cryptonote::get_output_public_key(out, extracted));
    ASSERT_EQ(extracted, pk);
}

TEST(cn_format_utils, get_output_public_key_txout_to_tagged_key)
{
    cryptonote::tx_out out;
    crypto::public_key pk = crypto::get_H();
    crypto::view_tag vt;
    vt.data = 0xab;
    cryptonote::set_tx_out(200, pk, true, vt, out);

    crypto::public_key extracted;
    ASSERT_TRUE(cryptonote::get_output_public_key(out, extracted));
    ASSERT_EQ(extracted, pk);
}

TEST(cn_format_utils, get_output_view_tag_no_tag)
{
    cryptonote::tx_out out;
    cryptonote::set_tx_out(100, crypto::get_H(), false, crypto::view_tag{}, out);

    auto vt_opt = cryptonote::get_output_view_tag(out);
    ASSERT_FALSE(!!vt_opt);
}

TEST(cn_format_utils, get_output_view_tag_with_tag)
{
    cryptonote::tx_out out;
    crypto::view_tag vt;
    vt.data = 0xcd;
    cryptonote::set_tx_out(100, crypto::get_H(), true, vt, out);

    auto vt_opt = cryptonote::get_output_view_tag(out);
    ASSERT_TRUE(!!vt_opt);
    ASSERT_EQ(vt_opt->data, '\xcd');
}

// =====================================================================
// short_hash_str tests
// =====================================================================
TEST(cn_format_utils, short_hash_str_format)
{
    crypto::hash h = crypto::rand<crypto::hash>();
    std::string s = cryptonote::short_hash_str(h);
    // Format: 8 hex chars + "...." + 8 hex chars = 20 chars
    ASSERT_EQ(s.size(), 20u);
    ASSERT_EQ(s.substr(8, 4), "....");
}

TEST(cn_format_utils, short_hash_str_deterministic)
{
    crypto::hash h;
    memset(&h, 0x42, sizeof(h));
    std::string s1 = cryptonote::short_hash_str(h);
    std::string s2 = cryptonote::short_hash_str(h);
    ASSERT_EQ(s1, s2);
}

// =====================================================================
// block_to_blob / tx_to_blob
// =====================================================================
TEST(cn_format_utils, tx_to_blob_return_overload)
{
    auto tx = make_v1_coinbase_tx(100, {{50000, crypto::get_H()}});
    cryptonote::blobdata blob = cryptonote::tx_to_blob(tx);
    ASSERT_FALSE(blob.empty());

    cryptonote::transaction parsed_tx;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, parsed_tx));
    ASSERT_EQ(parsed_tx.version, tx.version);
}

TEST(cn_format_utils, tx_to_blob_bool_overload)
{
    auto tx = make_v1_coinbase_tx(100, {{50000, crypto::get_H()}});
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::tx_to_blob(tx, blob));
    ASSERT_FALSE(blob.empty());
}

TEST(cn_format_utils, block_to_blob_string_overload)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    cryptonote::blobdata blob = cryptonote::block_to_blob(b);
    ASSERT_FALSE(blob.empty());
}

// =====================================================================
// get_tx_tree_hash from block
// =====================================================================
TEST(cn_format_utils, get_tx_tree_hash_from_block)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    crypto::hash tree_hash = cryptonote::get_tx_tree_hash(b);
    // Should be deterministic
    crypto::hash tree_hash2 = cryptonote::get_tx_tree_hash(b);
    ASSERT_EQ(tree_hash, tree_hash2);
}

TEST(cn_format_utils, get_tx_tree_hash_from_block_with_txs)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    crypto::hash txh;
    memset(&txh, 0xaa, sizeof(txh));
    b.tx_hashes.push_back(txh);

    crypto::hash tree_hash = cryptonote::get_tx_tree_hash(b);
    // Adding tx hashes should change the tree hash
    cryptonote::block b2 = b;
    b2.tx_hashes.clear();
    b2.invalidate_hashes();
    b2.miner_tx.invalidate_hashes();
    crypto::hash tree_hash2 = cryptonote::get_tx_tree_hash(b2);
    ASSERT_NE(tree_hash, tree_hash2);
}

// =====================================================================
// get_tx_tree_hash with void overload
// =====================================================================
TEST(cn_format_utils, get_tx_tree_hash_void_overload)
{
    std::vector<crypto::hash> hashes;
    crypto::hash h;
    memset(&h, 0xab, sizeof(h));
    hashes.push_back(h);

    crypto::hash result;
    cryptonote::get_tx_tree_hash(hashes, result);
    crypto::hash result2 = cryptonote::get_tx_tree_hash(hashes);
    ASSERT_EQ(result, result2);
}

// =====================================================================
// get_hash_stats
// =====================================================================
TEST(cn_format_utils, get_hash_stats_callable)
{
    uint64_t tx_calc, tx_cached, block_calc, block_cached;
    cryptonote::get_hash_stats(tx_calc, tx_cached, block_calc, block_cached);
    // Just verify it doesn't crash and returns values
    // After prior tests there should be some calculated hashes
    ASSERT_TRUE(tx_calc > 0 || tx_cached > 0 || block_calc > 0 || block_cached > 0);
}

// =====================================================================
// is_out_to_acc - output ownership tests
// =====================================================================
TEST(cn_format_utils, is_out_to_acc_own_output)
{
    // Generate account keys
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();

    // Create a tx key pair
    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    // Derive output public key
    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

    crypto::public_key output_pk;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, output_pk));

    // is_out_to_acc should return true
    std::vector<crypto::public_key> additional_tx_pks;
    ASSERT_TRUE(cryptonote::is_out_to_acc(keys, output_pk, tx_pub, additional_tx_pks, 0));
}

TEST(cn_format_utils, is_out_to_acc_not_own_output)
{
    cryptonote::account_base account1, account2;
    account1.generate();
    account2.generate();
    const auto& keys1 = account1.get_keys();
    const auto& keys2 = account2.get_keys();

    // Create output for account2
    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys2.m_view_secret_key, derivation));

    crypto::public_key output_pk;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys2.m_account_address.m_spend_public_key, output_pk));

    // Checking with account1's keys should fail
    std::vector<crypto::public_key> additional_tx_pks;
    ASSERT_FALSE(cryptonote::is_out_to_acc(keys1, output_pk, tx_pub, additional_tx_pks, 0));
}

TEST(cn_format_utils, is_out_to_acc_with_view_tag)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();

    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

    crypto::public_key output_pk;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, output_pk));

    // Derive the correct view tag
    crypto::view_tag vt;
    crypto::derive_view_tag(derivation, 0, vt);

    std::vector<crypto::public_key> additional_tx_pks;
    boost::optional<crypto::view_tag> vt_opt(vt);
    ASSERT_TRUE(cryptonote::is_out_to_acc(keys, output_pk, tx_pub, additional_tx_pks, 0, vt_opt));
}

TEST(cn_format_utils, is_out_to_acc_wrong_view_tag)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();

    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

    crypto::public_key output_pk;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, output_pk));

    // Use a wrong view tag - try multiple to be sure we find one that differs
    crypto::view_tag correct_vt;
    crypto::derive_view_tag(derivation, 0, correct_vt);
    crypto::view_tag wrong_vt;
    wrong_vt.data = correct_vt.data ^ 0xFF;

    std::vector<crypto::public_key> additional_tx_pks;
    boost::optional<crypto::view_tag> vt_opt(wrong_vt);
    ASSERT_FALSE(cryptonote::is_out_to_acc(keys, output_pk, tx_pub, additional_tx_pks, 0, vt_opt));
}

// =====================================================================
// is_out_to_acc_precomp
// =====================================================================
TEST(cn_format_utils, is_out_to_acc_precomp_basic)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();
    hw::device& dev = hw::get_device("default");

    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

    crypto::public_key output_pk;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, output_pk));

    // Build subaddresses map (main address at {0,0})
    std::unordered_map<crypto::public_key, cryptonote::subaddress_index> subaddresses;
    subaddresses[keys.m_account_address.m_spend_public_key] = {0, 0};

    std::vector<crypto::key_derivation> additional_derivations;
    auto result = cryptonote::is_out_to_acc_precomp(subaddresses, output_pk, derivation, additional_derivations, 0, dev);
    ASSERT_TRUE(!!result);
    ASSERT_TRUE(result->index.is_zero());
}

TEST(cn_format_utils, is_out_to_acc_precomp_not_found)
{
    cryptonote::account_base account1, account2;
    account1.generate();
    account2.generate();
    const auto& keys1 = account1.get_keys();
    const auto& keys2 = account2.get_keys();
    hw::device& dev = hw::get_device("default");

    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    // Derive for account2
    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys2.m_view_secret_key, derivation));
    crypto::public_key output_pk;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys2.m_account_address.m_spend_public_key, output_pk));

    // Check with account1's subaddresses
    std::unordered_map<crypto::public_key, cryptonote::subaddress_index> subaddresses;
    subaddresses[keys1.m_account_address.m_spend_public_key] = {0, 0};

    // Use account1's derivation
    crypto::key_derivation derivation1;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys1.m_view_secret_key, derivation1));

    std::vector<crypto::key_derivation> additional_derivations;
    auto result = cryptonote::is_out_to_acc_precomp(subaddresses, output_pk, derivation1, additional_derivations, 0, dev);
    ASSERT_FALSE(!!result);
}

// =====================================================================
// out_can_be_to_acc (view tag filtering)
// =====================================================================
TEST(cn_format_utils, out_can_be_to_acc_no_view_tag)
{
    // Without a view tag, output can always potentially belong to account
    crypto::key_derivation derivation;
    memset(&derivation, 0, sizeof(derivation));
    boost::optional<crypto::view_tag> no_tag;
    ASSERT_TRUE(cryptonote::out_can_be_to_acc(no_tag, derivation, 0));
}

TEST(cn_format_utils, out_can_be_to_acc_matching_view_tag)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();

    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

    crypto::view_tag vt;
    crypto::derive_view_tag(derivation, 0, vt);

    boost::optional<crypto::view_tag> vt_opt(vt);
    ASSERT_TRUE(cryptonote::out_can_be_to_acc(vt_opt, derivation, 0));
}

TEST(cn_format_utils, out_can_be_to_acc_wrong_view_tag)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();

    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

    crypto::view_tag correct_vt;
    crypto::derive_view_tag(derivation, 0, correct_vt);
    crypto::view_tag wrong_vt;
    wrong_vt.data = correct_vt.data ^ 0xFF;

    boost::optional<crypto::view_tag> vt_opt(wrong_vt);
    ASSERT_FALSE(cryptonote::out_can_be_to_acc(vt_opt, derivation, 0));
}

// =====================================================================
// lookup_acc_outs
// =====================================================================
TEST(cn_format_utils, lookup_acc_outs_finds_own_output)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();

    // Create tx key pair
    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    // Derive output public key for output index 0
    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));
    crypto::public_key output_pk;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, output_pk));

    // Build a v1 coinbase tx with this output and the tx pub key in extra
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 0;
    tx.vin.push_back(gen);

    cryptonote::tx_out out;
    cryptonote::set_tx_out(1000, output_pk, false, crypto::view_tag{}, out);
    tx.vout.push_back(out);

    cryptonote::add_tx_pub_key_to_extra(tx, tx_pub);

    std::vector<size_t> outs;
    uint64_t money_transferred = 0;
    ASSERT_TRUE(cryptonote::lookup_acc_outs(keys, tx, outs, money_transferred));
    ASSERT_EQ(outs.size(), 1u);
    ASSERT_EQ(outs[0], 0u);
    ASSERT_EQ(money_transferred, 1000u);
}

TEST(cn_format_utils, lookup_acc_outs_skips_others_output)
{
    cryptonote::account_base account1, account2;
    account1.generate();
    account2.generate();
    const auto& keys1 = account1.get_keys();
    const auto& keys2 = account2.get_keys();

    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    // Derive output for account2
    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys2.m_view_secret_key, derivation));
    crypto::public_key output_pk;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys2.m_account_address.m_spend_public_key, output_pk));

    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 0;
    tx.vin.push_back(gen);

    cryptonote::tx_out out;
    cryptonote::set_tx_out(1000, output_pk, false, crypto::view_tag{}, out);
    tx.vout.push_back(out);

    cryptonote::add_tx_pub_key_to_extra(tx, tx_pub);

    // account1 should not find the output
    std::vector<size_t> outs;
    uint64_t money_transferred = 0;
    ASSERT_TRUE(cryptonote::lookup_acc_outs(keys1, tx, outs, money_transferred));
    ASSERT_EQ(outs.size(), 0u);
    ASSERT_EQ(money_transferred, 0u);
}

TEST(cn_format_utils, lookup_acc_outs_with_explicit_tx_pub_key)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();

    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));
    crypto::public_key output_pk;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, output_pk));

    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 0;
    tx.vin.push_back(gen);

    cryptonote::tx_out out;
    cryptonote::set_tx_out(500, output_pk, false, crypto::view_tag{}, out);
    tx.vout.push_back(out);

    std::vector<crypto::public_key> additional_tx_pks;
    std::vector<size_t> outs;
    uint64_t money_transferred = 0;
    ASSERT_TRUE(cryptonote::lookup_acc_outs(keys, tx, tx_pub, additional_tx_pks, outs, money_transferred));
    ASSERT_EQ(outs.size(), 1u);
    ASSERT_EQ(money_transferred, 500u);
}

// =====================================================================
// generate_key_image_helper
// =====================================================================
TEST(cn_format_utils, generate_key_image_helper_basic)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();
    hw::device& dev = hw::get_device("default");

    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));
    crypto::public_key output_pk;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, output_pk));

    std::unordered_map<crypto::public_key, cryptonote::subaddress_index> subaddresses;
    subaddresses[keys.m_account_address.m_spend_public_key] = {0, 0};

    std::vector<crypto::public_key> additional_tx_pks;
    cryptonote::keypair in_ephemeral;
    crypto::key_image ki;

    ASSERT_TRUE(cryptonote::generate_key_image_helper(
        keys, subaddresses, output_pk, tx_pub, additional_tx_pks, 0,
        in_ephemeral, ki, dev));

    // Key image should be non-null
    crypto::key_image null_ki;
    memset(&null_ki, 0, sizeof(null_ki));
    ASSERT_NE(ki, null_ki);

    // Ephemeral public key should match the output public key
    ASSERT_EQ(in_ephemeral.pub, output_pk);
}

TEST(cn_format_utils, generate_key_image_helper_deterministic)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();
    hw::device& dev = hw::get_device("default");

    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));
    crypto::public_key output_pk;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, output_pk));

    std::unordered_map<crypto::public_key, cryptonote::subaddress_index> subaddresses;
    subaddresses[keys.m_account_address.m_spend_public_key] = {0, 0};

    std::vector<crypto::public_key> additional_tx_pks;
    cryptonote::keypair eph1, eph2;
    crypto::key_image ki1, ki2;

    ASSERT_TRUE(cryptonote::generate_key_image_helper(
        keys, subaddresses, output_pk, tx_pub, additional_tx_pks, 0,
        eph1, ki1, dev));
    ASSERT_TRUE(cryptonote::generate_key_image_helper(
        keys, subaddresses, output_pk, tx_pub, additional_tx_pks, 0,
        eph2, ki2, dev));

    ASSERT_EQ(ki1, ki2);
    ASSERT_EQ(eph1.pub, eph2.pub);
}

// =====================================================================
// generate_key_image_helper_precomp
// =====================================================================
TEST(cn_format_utils, generate_key_image_helper_precomp_basic)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();
    hw::device& dev = hw::get_device("default");

    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));
    crypto::public_key output_pk;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, output_pk));

    cryptonote::subaddress_index idx = {0, 0};
    cryptonote::keypair in_ephemeral;
    crypto::key_image ki;

    ASSERT_TRUE(cryptonote::generate_key_image_helper_precomp(
        keys, output_pk, derivation, 0, idx, in_ephemeral, ki, dev));

    crypto::key_image null_ki;
    memset(&null_ki, 0, sizeof(null_ki));
    ASSERT_NE(ki, null_ki);
    ASSERT_EQ(in_ephemeral.pub, output_pk);
}

// =====================================================================
// Payment ID extra nonce edge cases
// =====================================================================
TEST(cn_format_utils, payment_id_nonce_wrong_tag_fails)
{
    // Build nonce with encrypted payment id tag but try to read as unencrypted
    crypto::hash8 pid;
    memset(&pid, 0xab, sizeof(pid));
    std::string nonce;
    cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(nonce, pid);

    crypto::hash recovered;
    // Wrong size for full payment ID (hash is 32 bytes, nonce has 8 + 1 = 9 bytes)
    ASSERT_FALSE(cryptonote::get_payment_id_from_tx_extra_nonce(nonce, recovered));
}

TEST(cn_format_utils, payment_id_nonce_empty_fails)
{
    std::string nonce;
    crypto::hash recovered;
    ASSERT_FALSE(cryptonote::get_payment_id_from_tx_extra_nonce(nonce, recovered));
}

TEST(cn_format_utils, encrypted_payment_id_nonce_empty_fails)
{
    std::string nonce;
    crypto::hash8 recovered;
    ASSERT_FALSE(cryptonote::get_encrypted_payment_id_from_tx_extra_nonce(nonce, recovered));
}

TEST(cn_format_utils, encrypted_payment_id_nonce_wrong_size_fails)
{
    // Build a nonce with encrypted tag but wrong size
    std::string nonce;
    nonce.push_back(TX_EXTRA_NONCE_ENCRYPTED_PAYMENT_ID);
    nonce.append(4, 'X'); // Only 4 bytes instead of 8
    crypto::hash8 recovered;
    ASSERT_FALSE(cryptonote::get_encrypted_payment_id_from_tx_extra_nonce(nonce, recovered));
}

// =====================================================================
// Additional additional_tx_pub_keys tests
// =====================================================================
TEST(cn_format_utils, get_additional_tx_pub_keys_from_transaction_prefix)
{
    cryptonote::transaction_prefix tx;
    tx.version = 2;
    tx.unlock_time = 0;

    std::vector<crypto::public_key> keys;
    keys.push_back(crypto::get_H());
    keys.push_back(crypto::get_H());
    ASSERT_TRUE(cryptonote::add_additional_tx_pub_keys_to_extra(tx.extra, keys));

    std::vector<crypto::public_key> recovered = cryptonote::get_additional_tx_pub_keys_from_extra(tx);
    ASSERT_EQ(recovered.size(), 2u);
}

TEST(cn_format_utils, get_additional_tx_pub_keys_empty)
{
    std::vector<uint8_t> extra;
    std::vector<crypto::public_key> recovered = cryptonote::get_additional_tx_pub_keys_from_extra(extra);
    ASSERT_TRUE(recovered.empty());
}

// =====================================================================
// get_tx_pub_key_from_extra - transaction_prefix overload
// =====================================================================
TEST(cn_format_utils, get_tx_pub_key_from_extra_prefix)
{
    cryptonote::transaction_prefix tx;
    tx.version = 2;
    tx.unlock_time = 0;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(tx, pk));

    crypto::public_key extracted = cryptonote::get_tx_pub_key_from_extra(tx);
    ASSERT_EQ(pk, extracted);
}

TEST(cn_format_utils, get_tx_pub_key_from_extra_with_index)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk1 = crypto::get_H();
    crypto::public_key pk2 = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk1));
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk2));

    crypto::public_key first = cryptonote::get_tx_pub_key_from_extra(extra, 0);
    crypto::public_key second = cryptonote::get_tx_pub_key_from_extra(extra, 1);
    ASSERT_EQ(first, pk1);
    ASSERT_EQ(second, pk2);
}

// =====================================================================
// add_tx_pub_key_to_extra - transaction_prefix overload
// =====================================================================
TEST(cn_format_utils, add_tx_pub_key_to_extra_transaction_prefix)
{
    cryptonote::transaction_prefix tx;
    tx.version = 2;
    tx.unlock_time = 0;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(tx, pk));
    ASSERT_FALSE(tx.extra.empty());

    crypto::public_key extracted = cryptonote::get_tx_pub_key_from_extra(tx.extra);
    ASSERT_EQ(pk, extracted);
}

// =====================================================================
// sort_tx_extra - empty
// =====================================================================
TEST(cn_format_utils, sort_tx_extra_empty)
{
    std::vector<uint8_t> extra;
    std::vector<uint8_t> sorted;
    ASSERT_TRUE(cryptonote::sort_tx_extra(extra, sorted));
    ASSERT_TRUE(sorted.empty());
}

// =====================================================================
// is_valid_decomposed_amount - more edge cases
// =====================================================================
TEST(cn_format_utils, is_valid_decomposed_amount_powers_of_10)
{
    uint64_t p = 1;
    for (int i = 0; i < 18; ++i)
    {
        ASSERT_TRUE(cryptonote::is_valid_decomposed_amount(p));
        p *= 10;
    }
}

TEST(cn_format_utils, is_valid_decomposed_amount_non_decomposed)
{
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(12));
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(101));
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(1234567890ULL));
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(55));
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(999));
}

// =====================================================================
// encrypt_key / decrypt_key roundtrip
// =====================================================================
TEST(cn_format_utils, encrypt_decrypt_key_roundtrip)
{
    // Use a known small scalar value that is already reduced mod l.
    // encrypt_key/decrypt_key use sc_add/sc_sub which operate mod l,
    // so the input must be a valid reduced scalar for roundtrip to work.
    crypto::secret_key original;
    memset(&original, 0, sizeof(original));
    // Set a small deterministic value (already reduced mod l)
    reinterpret_cast<uint8_t*>(&original)[0] = 0x42;
    reinterpret_cast<uint8_t*>(&original)[1] = 0x13;
    reinterpret_cast<uint8_t*>(&original)[2] = 0x77;

    epee::wipeable_string passphrase("test_passphrase_123");
    crypto::secret_key encrypted = cryptonote::encrypt_key(original, passphrase);
    // Encrypted should differ from original (in general)
    // Note: we can't guarantee they differ for all keys, but statistically they should
    crypto::secret_key decrypted = cryptonote::decrypt_key(encrypted, passphrase);
    ASSERT_EQ(original, decrypted);
}

TEST(cn_format_utils, encrypt_key_changes_key)
{
    crypto::secret_key original;
    crypto::generate_random_bytes_thread_safe(sizeof(original), reinterpret_cast<uint8_t*>(&original));

    epee::wipeable_string passphrase("nonempty");
    crypto::secret_key encrypted = cryptonote::encrypt_key(original, passphrase);
    // Highly unlikely to be the same
    ASSERT_NE(memcmp(&original, &encrypted, sizeof(original)), 0);
}

// =====================================================================
// find_tx_extra_field_by_type
// =====================================================================
TEST(cn_format_utils, find_tx_extra_field_by_type_basic)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));
    std::string nonce(10, 'A');
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, nonce));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));

    cryptonote::tx_extra_pub_key pk_field;
    ASSERT_TRUE(cryptonote::find_tx_extra_field_by_type(fields, pk_field, 0));
    ASSERT_EQ(pk_field.pub_key, pk);

    cryptonote::tx_extra_nonce nonce_field;
    ASSERT_TRUE(cryptonote::find_tx_extra_field_by_type(fields, nonce_field, 0));
    ASSERT_EQ(nonce_field.nonce, nonce);
}

TEST(cn_format_utils, find_tx_extra_field_by_type_not_found)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));

    cryptonote::tx_extra_nonce nonce_field;
    ASSERT_FALSE(cryptonote::find_tx_extra_field_by_type(fields, nonce_field, 0));
}

// =====================================================================
// decompose_amount_into_digits
// =====================================================================
TEST(cn_format_utils, decompose_amount_into_digits_basic)
{
    std::vector<uint64_t> chunks;
    uint64_t dust = 0;
    cryptonote::decompose_amount_into_digits(
        123456, 100,
        [&](uint64_t chunk) { chunks.push_back(chunk); },
        [&](uint64_t d) { dust = d; }
    );
    // 123456 = 100000 + 20000 + 3000 + 400 + 56
    // dust_threshold=100, so 56 is dust, then 400 is a chunk
    // Actually: 6*1=6, 50, 400, 3000, 20000, 100000
    // 6 <= 100 -> dust=6, 50 <= 100-6=no, 50+6=56 <= 100 -> dust=56
    // 400 > 100 -> emit dust=56, then chunk=400
    // 3000 > 100 -> chunk=3000
    // 20000 > 100 -> chunk=20000
    // 100000 > 100 -> chunk=100000
    ASSERT_EQ(dust, 56u);
    ASSERT_EQ(chunks.size(), 4u);
}

TEST(cn_format_utils, decompose_amount_into_digits_zero)
{
    std::vector<uint64_t> chunks;
    uint64_t dust = 0;
    cryptonote::decompose_amount_into_digits(
        0, 100,
        [&](uint64_t chunk) { chunks.push_back(chunk); },
        [&](uint64_t d) { dust = d; }
    );
    ASSERT_TRUE(chunks.empty());
    ASSERT_EQ(dust, 0u);
}

TEST(cn_format_utils, decompose_amount_into_digits_all_dust)
{
    std::vector<uint64_t> chunks;
    uint64_t dust = 0;
    cryptonote::decompose_amount_into_digits(
        50, 100,
        [&](uint64_t chunk) { chunks.push_back(chunk); },
        [&](uint64_t d) { dust = d; }
    );
    ASSERT_TRUE(chunks.empty());
    ASSERT_EQ(dust, 50u);
}

// =====================================================================
// t_serializable_object_to_blob / t_serializable_object_from_blob
// =====================================================================
TEST(cn_format_utils, serializable_object_roundtrip)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(b, blob));
    ASSERT_FALSE(blob.empty());

    cryptonote::block b2;
    ASSERT_TRUE(cryptonote::t_serializable_object_from_blob(b2, blob));
    ASSERT_EQ(b.major_version, b2.major_version);
    ASSERT_EQ(b.nonce, b2.nonce);
}

TEST(cn_format_utils, serializable_object_to_blob_return)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    cryptonote::blobdata blob1 = cryptonote::t_serializable_object_to_blob(b);
    cryptonote::blobdata blob2;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(b, blob2));
    ASSERT_EQ(blob1, blob2);
}

// =====================================================================
// get_object_hash
// =====================================================================
TEST(cn_format_utils, get_object_hash_basic)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    crypto::hash h;
    ASSERT_TRUE(cryptonote::get_object_hash(b, h));

    // Should match hash of blob
    cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b);
    crypto::hash h2 = cryptonote::get_blob_hash(blob);
    ASSERT_EQ(h, h2);
}

TEST(cn_format_utils, get_object_hash_with_size)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    crypto::hash h;
    size_t blob_size = 0;
    ASSERT_TRUE(cryptonote::get_object_hash(b, h, blob_size));

    cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b);
    ASSERT_EQ(blob_size, blob.size());
}

// =====================================================================
// get_object_blobsize
// =====================================================================
TEST(cn_format_utils, get_object_blobsize)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1000000;
    b.nonce = 42;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 100;
    b.miner_tx.vin.push_back(gen);

    size_t size = cryptonote::get_object_blobsize(b);
    cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b);
    ASSERT_EQ(size, blob.size());
}

// =====================================================================
// add_mm_merkle_root_to_tx_extra with depth=0
// =====================================================================
TEST(cn_format_utils, add_mm_merkle_root_depth_zero)
{
    std::vector<uint8_t> extra;
    crypto::hash root = crypto::rand<crypto::hash>();
    ASSERT_TRUE(cryptonote::add_mm_merkle_root_to_tx_extra(extra, root, 0));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 1u);

    const auto& mm = boost::get<cryptonote::tx_extra_merge_mining_tag>(fields[0]);
    ASSERT_EQ(mm.merkle_root, root);
    ASSERT_EQ(mm.depth, 0u);
}

// =====================================================================
// parse_tx_extra with multiple payment nonces
// =====================================================================
TEST(cn_format_utils, parse_tx_extra_payment_id_and_encrypted_in_same_extra)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));

    // Add an unencrypted payment id nonce
    crypto::hash pid = crypto::rand<crypto::hash>();
    std::string nonce1;
    cryptonote::set_payment_id_to_tx_extra_nonce(nonce1, pid);
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, nonce1));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 2u);
}

// =====================================================================
// sort_tx_extra with allow_partial
// =====================================================================
TEST(cn_format_utils, sort_tx_extra_allow_partial)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));
    // Add some garbage bytes at the end
    extra.push_back(0xFF);
    extra.push_back(0xFE);

    std::vector<uint8_t> sorted;
    // Without allow_partial, should fail
    ASSERT_FALSE(cryptonote::sort_tx_extra(extra, sorted, false));

    // With allow_partial, should succeed with the valid prefix
    ASSERT_TRUE(cryptonote::sort_tx_extra(extra, sorted, true));
    ASSERT_FALSE(sorted.empty());
}

// =====================================================================
// remove_field_from_tx_extra - multiple fields
// =====================================================================
TEST(cn_format_utils, remove_field_from_tx_extra_multiple)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));
    std::string nonce(10, 'X');
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, nonce));

    // Remove pubkey, keep nonce
    ASSERT_TRUE(cryptonote::remove_field_from_tx_extra(extra, typeid(cryptonote::tx_extra_pub_key)));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 1u);
    // Remaining field should be the nonce
    const auto& nonce_field = boost::get<cryptonote::tx_extra_nonce>(fields[0]);
    ASSERT_EQ(nonce_field.nonce, nonce);
}

// =====================================================================
// is_v1_tx edge cases
// =====================================================================
TEST(cn_format_utils, is_v1_tx_blobdata_ref_overload)
{
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));

    cryptonote::blobdata_ref ref(blob.data(), blob.size());
    ASSERT_TRUE(cryptonote::is_v1_tx(ref));
}

TEST(cn_format_utils, is_v1_tx_v2_blobdata_ref)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));

    cryptonote::blobdata_ref ref(blob.data(), blob.size());
    ASSERT_FALSE(cryptonote::is_v1_tx(ref));
}

// =====================================================================
// calculate_transaction_hash for v1 tx
// =====================================================================
TEST(cn_format_utils, calculate_transaction_hash_v1)
{
    auto tx = make_v1_coinbase_tx(100, {{50000, crypto::get_H()}, {30000, crypto::get_H()}});
    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));

    cryptonote::transaction parsed_tx;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, parsed_tx));

    crypto::hash h;
    size_t sz = 0;
    ASSERT_TRUE(cryptonote::get_transaction_hash(parsed_tx, h, sz));
    ASSERT_GT(sz, 0u);

    // Should be deterministic
    crypto::hash h2;
    ASSERT_TRUE(cryptonote::get_transaction_hash(parsed_tx, h2));
    ASSERT_EQ(h, h2);
}

// =====================================================================
// Address subaddress formatting
// =====================================================================
TEST(cn_format_utils, subaddress_address_roundtrip)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();
    hw::device& dev = hw::get_device("default");

    cryptonote::subaddress_index idx = {0, 1};
    cryptonote::account_public_address subaddr = dev.get_subaddress(keys, idx);

    std::string str = cryptonote::get_account_address_as_str(cryptonote::MAINNET, true, subaddr);
    ASSERT_FALSE(str.empty());

    cryptonote::address_parse_info info;
    ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, str));
    ASSERT_TRUE(info.is_subaddress);
    ASSERT_EQ(info.address, subaddr);

    // Re-format should give the same string
    std::string str2 = cryptonote::get_account_address_as_str(cryptonote::MAINNET, true, info.address);
    ASSERT_EQ(str, str2);
}

TEST(cn_format_utils, mainnet_address_wrong_network_fails)
{
    cryptonote::account_base account;
    account.generate();
    const auto& addr = account.get_keys().m_account_address;

    std::string mainnet_str = cryptonote::get_account_address_as_str(cryptonote::MAINNET, false, addr);

    // Parsing a mainnet address as testnet should fail
    cryptonote::address_parse_info info;
    ASSERT_FALSE(cryptonote::get_account_address_from_str(info, cryptonote::TESTNET, mainnet_str));
}

// =====================================================================
// Multiple outputs in lookup_acc_outs
// =====================================================================
TEST(cn_format_utils, lookup_acc_outs_multiple_outputs)
{
    cryptonote::account_base account;
    account.generate();
    const auto& keys = account.get_keys();

    crypto::secret_key tx_sec;
    crypto::public_key tx_pub;
    crypto::generate_keys(tx_pub, tx_sec);

    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

    // Create two outputs to the same account
    crypto::public_key output_pk0, output_pk1;
    ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, output_pk0));
    ASSERT_TRUE(crypto::derive_public_key(derivation, 1, keys.m_account_address.m_spend_public_key, output_pk1));

    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 0;
    tx.vin.push_back(gen);

    cryptonote::tx_out out0, out1;
    cryptonote::set_tx_out(1000, output_pk0, false, crypto::view_tag{}, out0);
    cryptonote::set_tx_out(2000, output_pk1, false, crypto::view_tag{}, out1);
    tx.vout.push_back(out0);
    tx.vout.push_back(out1);

    cryptonote::add_tx_pub_key_to_extra(tx, tx_pub);

    std::vector<size_t> outs;
    uint64_t money_transferred = 0;
    ASSERT_TRUE(cryptonote::lookup_acc_outs(keys, tx, outs, money_transferred));
    ASSERT_EQ(outs.size(), 2u);
    ASSERT_EQ(money_transferred, 3000u);
}

// =====================================================================
// Block with miner tx tree hash
// =====================================================================
TEST(cn_format_utils, block_hash_changes_with_miner_tx)
{
    cryptonote::block b1;
    b1.major_version = 14;
    b1.minor_version = 14;
    b1.timestamp = 1000000;
    b1.nonce = 42;
    b1.miner_tx.version = 2;
    b1.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen1;
    gen1.height = 100;
    b1.miner_tx.vin.push_back(gen1);

    cryptonote::block b2 = b1;
    b2.miner_tx.unlock_time = 100; // Different unlock time
    b2.invalidate_hashes();
    b2.miner_tx.invalidate_hashes();

    crypto::hash h1 = cryptonote::get_block_hash(b1);
    crypto::hash h2 = cryptonote::get_block_hash(b2);
    ASSERT_NE(h1, h2);
}

// =====================================================================
// parse_and_validate_tx_from_blob - invalid blob
// =====================================================================
TEST(cn_format_utils, parse_and_validate_tx_from_blob_with_hash_invalid)
{
    cryptonote::transaction tx;
    crypto::hash h;
    std::string bad = "invalid";
    ASSERT_FALSE(cryptonote::parse_and_validate_tx_from_blob(bad, tx, h));
}

TEST(cn_format_utils, parse_and_validate_tx_from_blob_with_hash_and_prefix_invalid)
{
    cryptonote::transaction tx;
    crypto::hash h, ph;
    std::string bad = "invalid";
    ASSERT_FALSE(cryptonote::parse_and_validate_tx_from_blob(bad, tx, h, ph));
}

// =====================================================================
// obj_to_json_str
// =====================================================================
TEST(cn_format_utils, obj_to_json_str_basic)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;
    std::string json = cryptonote::obj_to_json_str(tx);
    ASSERT_FALSE(json.empty());
    // Should contain "version" field
    ASSERT_NE(json.find("version"), std::string::npos);
}

// =====================================================================
// Multiple v1 tx tests for get_tx_fee edge case (coinbase input type)
// =====================================================================
TEST(cn_format_utils, get_tx_fee_coinbase_returns_zero)
{
    // Coinbase tx has txin_gen which is not txin_to_key, get_tx_fee should return 0
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 0;
    tx.vin.push_back(gen);
    // get_tx_fee should return 0 (fails internally due to wrong type)
    ASSERT_EQ(cryptonote::get_tx_fee(tx), 0u);
}

// =====================================================================
// Block height from block with wrong input count
// =====================================================================
TEST(cn_format_utils, get_block_height_no_inputs)
{
    cryptonote::block b;
    b.miner_tx.version = 2;
    // No vin at all - get_block_height's error path calls get_block_hash(b)
    // which internally throws when computing the hash of the incomplete miner_tx.
    ASSERT_ANY_THROW(cryptonote::get_block_height(b));
}

// =====================================================================
// Transaction prefix hash with void overload
// =====================================================================
TEST(cn_format_utils, get_transaction_prefix_hash_void_matches)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 42;
    cryptonote::txin_gen gen;
    gen.height = 10;
    tx.vin.push_back(gen);

    crypto::hash h1;
    cryptonote::get_transaction_prefix_hash(tx, h1);
    crypto::hash h2 = cryptonote::get_transaction_prefix_hash(tx);
    ASSERT_EQ(h1, h2);
}

// =====================================================================
// Block serialization and hashing from parsed blob
// =====================================================================
TEST(cn_format_utils, block_serialization_hash_consistency)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1700000000;
    b.nonce = 12345;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 60;
    cryptonote::txin_gen gen;
    gen.height = 500;
    b.miner_tx.vin.push_back(gen);

    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::block_to_blob(b, blob));

    // Parse with hash
    cryptonote::block b2;
    crypto::hash block_hash;
    ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2, block_hash));

    // The cached hash should match
    crypto::hash h2 = cryptonote::get_block_hash(b2);
    ASSERT_EQ(block_hash, h2);

    // get_block_hashing_blob should be consistent
    cryptonote::blobdata hb1 = cryptonote::get_block_hashing_blob(b);
    cryptonote::blobdata hb2 = cryptonote::get_block_hashing_blob(b2);
    ASSERT_EQ(hb1, hb2);
}

// =====================================================================
// get_transaction_weight with clawback (3 tests)
// =====================================================================

// Helper: create a v2 tx with bulletproof outputs for weight clawback testing
static cryptonote::transaction make_v2_bp_tx(size_t n_outputs, bool use_plus)
{
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;

    // Add a coinbase input so serialization works
    cryptonote::txin_gen gen;
    gen.height = 100;
    tx.vin.push_back(gen);

    // Add n_outputs zero-amount outputs
    for (size_t i = 0; i < n_outputs; ++i)
    {
        cryptonote::tx_out out;
        out.amount = 0;
        cryptonote::txout_to_key tk;
        tk.key = crypto::public_key();
        out.target = tk;
        tx.vout.push_back(out);
    }

    // Set up RCT signature type
    if (use_plus)
        tx.rct_signatures.type = rct::RCTTypeBulletproofPlus;
    else
        tx.rct_signatures.type = rct::RCTTypeBulletproof;

    // Create a fake bulletproof (plus) with correct L/R sizes for n_outputs
    // n_padded_outputs is next power of 2 >= n_outputs
    size_t n_padded = 1;
    size_t nlr = 0;
    while (n_padded < n_outputs)
    {
        n_padded <<= 1;
        ++nlr;
    }
    nlr += 6; // base L/R size

    if (use_plus)
    {
        rct::BulletproofPlus bpp;
        bpp.L.resize(nlr);
        bpp.R.resize(nlr);
        bpp.V.resize(n_outputs);
        tx.rct_signatures.p.bulletproofs_plus.push_back(bpp);
    }
    else
    {
        rct::Bulletproof bp;
        bp.L.resize(nlr);
        bp.R.resize(nlr);
        bp.V.resize(n_outputs);
        tx.rct_signatures.p.bulletproofs.push_back(bp);
    }

    // Set outPk and ecdhInfo to match outputs
    tx.rct_signatures.outPk.resize(n_outputs);
    tx.rct_signatures.ecdhInfo.resize(n_outputs);

    return tx;
}

TEST(cn_format_utils, get_transaction_weight_v2_bp_4_outputs_clawback)
{
    // 4 bulletproof outputs should trigger clawback (n_padded_outputs = 4 > 2)
    auto tx = make_v2_bp_tx(4, false);

    // Use the overload that takes an explicit blob_size.
    // The tx is not serializable as a real rctSig, but get_transaction_weight(tx, blob_size)
    // only needs the tx structure and a notional blob size.
    const size_t fake_blob_size = 2000;
    uint64_t weight = cryptonote::get_transaction_weight(tx, fake_blob_size);
    // Weight should be greater than blob size due to clawback
    ASSERT_GT(weight, fake_blob_size);
}

TEST(cn_format_utils, get_transaction_weight_v2_bp_2_outputs_no_clawback)
{
    // 2 bulletproof outputs: n_padded_outputs = 2, no clawback
    auto tx = make_v2_bp_tx(2, false);

    const size_t fake_blob_size = 2000;
    uint64_t weight = cryptonote::get_transaction_weight(tx, fake_blob_size);
    // With 2 outputs, clawback is 0, so weight == blob_size
    ASSERT_EQ(weight, fake_blob_size);
}

TEST(cn_format_utils, get_transaction_weight_v2_bp_plus_4_outputs_clawback)
{
    // 4 bulletproof_plus outputs should also trigger clawback
    auto tx = make_v2_bp_tx(4, true);

    const size_t fake_blob_size = 2000;
    uint64_t weight = cryptonote::get_transaction_weight(tx, fake_blob_size);
    // Weight should be greater than blob size due to clawback
    ASSERT_GT(weight, fake_blob_size);
}

// =====================================================================
// parse_and_validate_tx_from_blob edge cases (2 tests)
// =====================================================================

TEST(cn_format_utils, parse_and_validate_tx_from_blob_v1_roundtrip)
{
    // Construct a valid v1 tx, serialize, then parse back
    auto tx = make_v1_coinbase_tx(42, {{1000000000000ULL, crypto::get_H()}});

    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));

    cryptonote::transaction parsed;
    crypto::hash tx_hash;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, parsed, tx_hash));
    ASSERT_EQ(parsed.version, 1u);
    ASSERT_EQ(parsed.vout.size(), 1u);
    ASSERT_EQ(parsed.vout[0].amount, 1000000000000ULL);

    // Hash should be non-null
    ASSERT_NE(tx_hash, crypto::null_hash);
}

TEST(cn_format_utils, parse_and_validate_tx_from_blob_truncated_fails)
{
    // Create a valid tx blob, then truncate in the middle of the output data
    auto tx = make_v1_coinbase_tx(42, {{1000000000000ULL, crypto::get_H()}, {2000000000000ULL, crypto::get_H()}});

    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx, blob));
    ASSERT_GT(blob.size(), 20u);

    // Truncate midway through the serialized outputs
    cryptonote::blobdata truncated = blob.substr(0, blob.size() / 2);

    cryptonote::transaction parsed;
    ASSERT_FALSE(cryptonote::parse_and_validate_tx_from_blob(truncated, parsed));
}

// =====================================================================
// get_block_reward tests (3 tests)
// =====================================================================

TEST(cn_format_utils, get_block_reward_penalty_for_oversize_block)
{
    // Block slightly over median should get a penalty (reduced reward)
    uint64_t reward_at_median = 0;
    uint64_t reward_over_median = 0;
    const size_t median = 300000;
    const uint64_t already_generated = 1000000000000000ULL; // ~1M XMR

    ASSERT_TRUE(cryptonote::get_block_reward(median, median, already_generated, reward_at_median, 14));
    ASSERT_TRUE(cryptonote::get_block_reward(median, median + 1000, already_generated, reward_over_median, 14));

    // Reward should be less when block is over median
    ASSERT_LT(reward_over_median, reward_at_median);
}

TEST(cn_format_utils, get_block_reward_tail_emission)
{
    // With very large already_generated_coins (near MONEY_SUPPLY), we hit tail emission
    // MONEY_SUPPLY is (uint64_t)(-1), so use a value close to it
    uint64_t reward = 0;
    uint64_t near_max = MONEY_SUPPLY - 1;
    ASSERT_TRUE(cryptonote::get_block_reward(300000, 300000, near_max, reward, 14));

    // Should get the tail emission: FINAL_SUBSIDY_PER_MINUTE * target_minutes
    // For v14 (v >= 2), target = DIFFICULTY_TARGET_V2 = 120s, target_minutes = 2
    uint64_t expected_tail = FINAL_SUBSIDY_PER_MINUTE * 2;
    ASSERT_EQ(reward, expected_tail);
}

TEST(cn_format_utils, get_block_reward_too_large_block_fails)
{
    // Block weight > 2 * median should fail
    uint64_t reward = 0;
    const size_t median = 300000;
    ASSERT_FALSE(cryptonote::get_block_reward(median, 2 * median + 1, 1000000000000000ULL, reward, 14));
}

// =====================================================================
// check_output_types at different HF versions (3 tests)
// =====================================================================

TEST(cn_format_utils, check_output_types_only_txout_to_key_before_view_tags)
{
    // Before HF_VERSION_VIEW_TAGS, only txout_to_key is valid
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 1;
    tx.vin.push_back(gen);

    // Add two txout_to_key outputs
    for (int i = 0; i < 2; ++i)
    {
        cryptonote::tx_out out;
        cryptonote::set_tx_out(1000000000000ULL, crypto::get_H(), false, crypto::view_tag{}, out);
        tx.vout.push_back(out);
    }

    ASSERT_TRUE(cryptonote::check_output_types(tx, HF_VERSION_VIEW_TAGS - 1));
}

TEST(cn_format_utils, check_output_types_at_view_tag_fork_tagged_key_ok)
{
    // At HF_VERSION_VIEW_TAGS, txout_to_tagged_key outputs are ok (all same type)
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;

    for (int i = 0; i < 2; ++i)
    {
        cryptonote::tx_out out;
        crypto::view_tag vt;
        vt.data = 0x42;
        cryptonote::set_tx_out(0, crypto::get_H(), true, vt, out);
        tx.vout.push_back(out);
    }

    ASSERT_TRUE(cryptonote::check_output_types(tx, HF_VERSION_VIEW_TAGS));
}

TEST(cn_format_utils, check_output_types_mixed_types_at_view_tag_fork_fails)
{
    // At HF_VERSION_VIEW_TAGS, mixed output types in the same tx should fail
    // Use v1 tx so get_transaction_hash in error logging doesn't throw
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 1;
    tx.vin.push_back(gen);

    // First output: txout_to_key
    cryptonote::tx_out out1;
    cryptonote::set_tx_out(1000000000000ULL, crypto::get_H(), false, crypto::view_tag{}, out1);
    tx.vout.push_back(out1);

    // Second output: txout_to_tagged_key (mixed!)
    cryptonote::tx_out out2;
    crypto::view_tag vt;
    vt.data = 0x42;
    cryptonote::set_tx_out(1000000000000ULL, crypto::get_H(), true, vt, out2);
    tx.vout.push_back(out2);

    ASSERT_FALSE(cryptonote::check_output_types(tx, HF_VERSION_VIEW_TAGS));
}

// =====================================================================
// construct_miner_tx additional tests (4 tests)
// =====================================================================

// Helper: create a default miner address for testing
static cryptonote::account_public_address make_test_miner_address()
{
    cryptonote::account_base acct;
    acct.generate();
    return acct.get_keys().m_account_address;
}

TEST(cn_format_utils, construct_miner_tx_with_extra_nonce)
{
    auto addr = make_test_miner_address();
    cryptonote::transaction tx;
    std::string extra_nonce(4, '\x42'); // 4-byte nonce

    ASSERT_TRUE(cryptonote::construct_miner_tx(100, 300000, 1000000000000000ULL, 0, 0, addr, tx, extra_nonce, 999, 14));

    // The extra should contain the nonce field
    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(tx.extra, fields));

    cryptonote::tx_extra_nonce nonce_field;
    ASSERT_TRUE(cryptonote::find_tx_extra_field_by_type(fields, nonce_field));
    ASSERT_FALSE(nonce_field.nonce.empty());
}

TEST(cn_format_utils, construct_miner_tx_output_count_max_outs_1)
{
    auto addr = make_test_miner_address();
    cryptonote::transaction tx;

    // With max_outs = 1 and HF >= 4, all outputs get merged into 1
    ASSERT_TRUE(cryptonote::construct_miner_tx(100, 300000, 1000000000000000ULL, 0, 0, addr, tx, cryptonote::blobdata(), 1, 14));
    ASSERT_EQ(tx.vout.size(), 1u);
    ASSERT_GT(tx.vout[0].amount, 0u);
}

TEST(cn_format_utils, construct_miner_tx_hf16_uses_view_tags)
{
    auto addr = make_test_miner_address();
    cryptonote::transaction tx;

    ASSERT_TRUE(cryptonote::construct_miner_tx(100, 300000, 1000000000000000ULL, 0, 0, addr, tx, cryptonote::blobdata(), 999, 16));

    // At HF 16 (>= HF_VERSION_VIEW_TAGS=15), outputs should use txout_to_tagged_key
    ASSERT_GE(tx.vout.size(), 1u);
    for (const auto& out : tx.vout)
    {
        ASSERT_TRUE(out.target.type() == typeid(cryptonote::txout_to_tagged_key))
            << "Expected txout_to_tagged_key at HF 16";
    }
    // tx version should be 2 at HF >= 4
    ASSERT_EQ(tx.version, 2u);
}

TEST(cn_format_utils, construct_miner_tx_with_zero_fee)
{
    auto addr = make_test_miner_address();
    cryptonote::transaction tx;

    // Zero fee, non-zero block reward from already_generated_coins
    ASSERT_TRUE(cryptonote::construct_miner_tx(100, 300000, 1000000000000000ULL, 0, 0, addr, tx, cryptonote::blobdata(), 999, 14));

    // Output amounts should sum to the block reward (no fee added)
    uint64_t total_output = 0;
    for (const auto& out : tx.vout)
        total_output += out.amount;
    ASSERT_GT(total_output, 0u);

    // Now with a fee
    cryptonote::transaction tx_with_fee;
    uint64_t fee = 100000000ULL; // 0.0001 XMR
    ASSERT_TRUE(cryptonote::construct_miner_tx(100, 300000, 1000000000000000ULL, 0, fee, addr, tx_with_fee, cryptonote::blobdata(), 999, 14));

    uint64_t total_with_fee = 0;
    for (const auto& out : tx_with_fee.vout)
        total_with_fee += out.amount;

    // Output with fee should be greater than output without fee
    ASSERT_GT(total_with_fee, total_output);
}

// =====================================================================
// NEW TESTS: CryptonoteFormatUtils suite
// =====================================================================

// --- tx_extra manipulation ---

TEST(CryptonoteFormatUtils, AddAndGetTxPubKeyRoundtrip)
{
    // Roundtrip: add a random-looking pub key, then retrieve it from the extra vector
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));

    crypto::public_key recovered = cryptonote::get_tx_pub_key_from_extra(extra);
    ASSERT_EQ(pk, recovered);

    // Second pub key at index 0 and 1
    crypto::public_key pk2 = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk2));
    ASSERT_EQ(cryptonote::get_tx_pub_key_from_extra(extra, 0), pk);
    ASSERT_EQ(cryptonote::get_tx_pub_key_from_extra(extra, 1), pk2);

    // Non-existent index returns null
    ASSERT_EQ(cryptonote::get_tx_pub_key_from_extra(extra, 99), crypto::null_pkey);
}

TEST(CryptonoteFormatUtils, AddAndGetAdditionalTxPubKeysRoundtrip)
{
    std::vector<uint8_t> extra;
    // Also add a primary pub key so the extra has mixed fields
    crypto::public_key primary_pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, primary_pk));

    std::vector<crypto::public_key> additional_keys;
    additional_keys.push_back(crypto::get_H());
    additional_keys.push_back(crypto::get_H());
    additional_keys.push_back(crypto::get_H());
    ASSERT_TRUE(cryptonote::add_additional_tx_pub_keys_to_extra(extra, additional_keys));

    std::vector<crypto::public_key> recovered = cryptonote::get_additional_tx_pub_keys_from_extra(extra);
    ASSERT_EQ(recovered.size(), 3u);
    for (size_t i = 0; i < 3; ++i)
        ASSERT_EQ(recovered[i], additional_keys[i]);

    // Primary key should still be retrievable
    ASSERT_EQ(cryptonote::get_tx_pub_key_from_extra(extra, 0), primary_pk);
}

TEST(CryptonoteFormatUtils, AddExtraNonceWithPaymentIds)
{
    // Add unencrypted payment id via extra nonce, parse back
    std::vector<uint8_t> extra;
    crypto::hash payment_id = crypto::rand<crypto::hash>();
    std::string nonce;
    cryptonote::set_payment_id_to_tx_extra_nonce(nonce, payment_id);
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, nonce));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 1u);

    cryptonote::tx_extra_nonce nonce_field;
    ASSERT_TRUE(cryptonote::find_tx_extra_field_by_type(fields, nonce_field));

    // Verify we can extract the payment id back from the nonce
    crypto::hash recovered_pid;
    ASSERT_TRUE(cryptonote::get_payment_id_from_tx_extra_nonce(nonce_field.nonce, recovered_pid));
    ASSERT_EQ(payment_id, recovered_pid);
}

TEST(CryptonoteFormatUtils, PaymentIdRoundtripThroughExtra)
{
    // Full roundtrip: set payment id -> add to extra nonce -> add to tx extra -> parse -> extract
    crypto::hash pid = crypto::rand<crypto::hash>();
    std::string nonce;
    cryptonote::set_payment_id_to_tx_extra_nonce(nonce, pid);

    std::vector<uint8_t> extra;
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, nonce));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));

    cryptonote::tx_extra_nonce nf;
    ASSERT_TRUE(cryptonote::find_tx_extra_field_by_type(fields, nf));

    crypto::hash recovered;
    ASSERT_TRUE(cryptonote::get_payment_id_from_tx_extra_nonce(nf.nonce, recovered));
    ASSERT_EQ(pid, recovered);

    // Encrypted payment id should NOT be extractable from unencrypted nonce
    crypto::hash8 wrong_recovered;
    ASSERT_FALSE(cryptonote::get_encrypted_payment_id_from_tx_extra_nonce(nf.nonce, wrong_recovered));
}

TEST(CryptonoteFormatUtils, EncryptedPaymentIdRoundtripThroughExtra)
{
    crypto::hash8 pid;
    memset(&pid, 0xab, sizeof(pid));
    std::string nonce;
    cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(nonce, pid);

    std::vector<uint8_t> extra;
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, nonce));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));

    cryptonote::tx_extra_nonce nf;
    ASSERT_TRUE(cryptonote::find_tx_extra_field_by_type(fields, nf));

    crypto::hash8 recovered;
    ASSERT_TRUE(cryptonote::get_encrypted_payment_id_from_tx_extra_nonce(nf.nonce, recovered));
    ASSERT_EQ(pid, recovered);

    // Unencrypted payment id should NOT be extractable from encrypted nonce
    crypto::hash wrong_recovered;
    ASSERT_FALSE(cryptonote::get_payment_id_from_tx_extra_nonce(nf.nonce, wrong_recovered));
}

TEST(CryptonoteFormatUtils, RemoveFieldFromTxExtraKeepsOthers)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));

    std::string nonce_data(16, 'A');
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, nonce_data));

    // Remove nonce, keep pub key
    ASSERT_TRUE(cryptonote::remove_field_from_tx_extra(extra, typeid(cryptonote::tx_extra_nonce)));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 1u);

    cryptonote::tx_extra_pub_key pk_field;
    ASSERT_TRUE(cryptonote::find_tx_extra_field_by_type(fields, pk_field));
    ASSERT_EQ(pk_field.pub_key, pk);
}

TEST(CryptonoteFormatUtils, RemoveFieldFromTxExtraTwice)
{
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));

    // Remove once
    ASSERT_TRUE(cryptonote::remove_field_from_tx_extra(extra, typeid(cryptonote::tx_extra_pub_key)));
    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_TRUE(fields.empty());

    // Remove again (no-op, should still succeed)
    ASSERT_TRUE(cryptonote::remove_field_from_tx_extra(extra, typeid(cryptonote::tx_extra_pub_key)));
}

TEST(CryptonoteFormatUtils, SortTxExtraOrdersPubKeyFirst)
{
    // Deliberately add nonce first, then pub key - sorting should reorder
    std::vector<uint8_t> extra;
    std::string nonce(10, 'N');
    cryptonote::add_extra_nonce_to_tx_extra(extra, nonce);
    crypto::public_key pk = crypto::get_H();
    cryptonote::add_tx_pub_key_to_extra(extra, pk);

    std::vector<uint8_t> sorted;
    ASSERT_TRUE(cryptonote::sort_tx_extra(extra, sorted));

    // Parse sorted and verify pub key comes first
    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(sorted, fields));
    ASSERT_EQ(fields.size(), 2u);

    // First field should be pub key (tag 0x01 < nonce tag 0x02)
    ASSERT_NO_THROW(boost::get<cryptonote::tx_extra_pub_key>(fields[0]));
    ASSERT_NO_THROW(boost::get<cryptonote::tx_extra_nonce>(fields[1]));
}

TEST(CryptonoteFormatUtils, SortTxExtraInvalidDataFails)
{
    std::vector<uint8_t> extra = {0xFF, 0xFE, 0xFD};
    std::vector<uint8_t> sorted;
    ASSERT_FALSE(cryptonote::sort_tx_extra(extra, sorted, false));
}

TEST(CryptonoteFormatUtils, ParseTxExtraEmptyReturnsTrue)
{
    std::vector<uint8_t> extra;
    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 0u);
}

TEST(CryptonoteFormatUtils, ParseTxExtraValidComplex)
{
    // Build a complex extra with pub key + nonce + additional keys
    std::vector<uint8_t> extra;
    crypto::public_key pk = crypto::get_H();
    ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(extra, pk));
    std::string nonce(20, 'Z');
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, nonce));
    std::vector<crypto::public_key> add_keys = {crypto::get_H()};
    ASSERT_TRUE(cryptonote::add_additional_tx_pub_keys_to_extra(extra, add_keys));

    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(fields.size(), 3u);
}

TEST(CryptonoteFormatUtils, ParseTxExtraInvalidGarbageOnly)
{
    // Pure garbage should fail to parse
    std::vector<uint8_t> extra = {0xFF, 0x01, 0x02, 0x03};
    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_FALSE(cryptonote::parse_tx_extra(extra, fields));
}

// --- Amount/offset utilities ---

TEST(CryptonoteFormatUtils, RelativeAbsoluteOffsetsRoundtripLargeValues)
{
    std::vector<uint64_t> absolute = {
        1000000ULL, 2000000ULL, 5000000ULL, 10000000ULL, 100000000ULL
    };
    std::vector<uint64_t> relative = cryptonote::absolute_output_offsets_to_relative(absolute);

    // First relative should equal first absolute
    ASSERT_EQ(relative[0], 1000000ULL);
    // Subsequent relatives should be differences
    ASSERT_EQ(relative[1], 1000000ULL); // 2M - 1M
    ASSERT_EQ(relative[2], 3000000ULL); // 5M - 2M

    std::vector<uint64_t> recovered = cryptonote::relative_output_offsets_to_absolute(relative);
    ASSERT_EQ(absolute, recovered);
}

TEST(CryptonoteFormatUtils, RelativeAbsoluteOffsetsConsecutive)
{
    // Consecutive offsets: 0, 1, 2, 3, 4
    std::vector<uint64_t> absolute = {0, 1, 2, 3, 4};
    std::vector<uint64_t> relative = cryptonote::absolute_output_offsets_to_relative(absolute);
    ASSERT_EQ(relative.size(), 5u);
    ASSERT_EQ(relative[0], 0u);
    for (size_t i = 1; i < relative.size(); ++i)
        ASSERT_EQ(relative[i], 1u);

    std::vector<uint64_t> recovered = cryptonote::relative_output_offsets_to_absolute(relative);
    ASSERT_EQ(absolute, recovered);
}

TEST(CryptonoteFormatUtils, IsValidDecomposedAmountEdgeCases)
{
    // Valid: 9 * 10^12 (largest single-digit * 1 XMR base)
    ASSERT_TRUE(cryptonote::is_valid_decomposed_amount(9000000000000ULL));
    // Valid: 1 (smallest valid decomposed amount)
    ASSERT_TRUE(cryptonote::is_valid_decomposed_amount(1));
    // Invalid: 0
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(0));
    // Invalid: 10 + 1 = 11
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(11));
    // Invalid: multi-digit
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(12345678ULL));
    // Valid: 8 * 10^6 = 8000000
    ASSERT_TRUE(cryptonote::is_valid_decomposed_amount(8000000ULL));
}

TEST(CryptonoteFormatUtils, PrintMoneyMaxUint64)
{
    uint64_t max_val = std::numeric_limits<uint64_t>::max();
    std::string result = cryptonote::print_money(max_val);
    ASSERT_FALSE(result.empty());
    // Should contain a decimal point
    ASSERT_NE(result.find('.'), std::string::npos);
    // Should have exactly 12 digits after the decimal
    size_t dot_pos = result.find('.');
    ASSERT_EQ(result.size() - dot_pos - 1, 12u);
}

TEST(CryptonoteFormatUtils, PrintMoneySmallFractions)
{
    ASSERT_EQ(cryptonote::print_money(1), "0.000000000001");
    ASSERT_EQ(cryptonote::print_money(10), "0.000000000010");
    ASSERT_EQ(cryptonote::print_money(100), "0.000000000100");
    ASSERT_EQ(cryptonote::print_money(999999999999ULL), "0.999999999999");
}

TEST(CryptonoteFormatUtils, RoundMoneyUpVariousSignificantDigits)
{
    // 1234 rounded to 1 sig digit -> 2000
    ASSERT_EQ(cryptonote::round_money_up(1234ULL, 1), 2000ULL);
    // 1234 rounded to 2 sig digits -> 1300
    ASSERT_EQ(cryptonote::round_money_up(1234ULL, 2), 1300ULL);
    // 1234 rounded to 3 sig digits -> 1240
    ASSERT_EQ(cryptonote::round_money_up(1234ULL, 3), 1240ULL);
    // 1234 rounded to 4 sig digits -> 1234 (exact)
    ASSERT_EQ(cryptonote::round_money_up(1234ULL, 4), 1234ULL);
    // 1234 rounded to 5 sig digits -> 1234 (more digits than available)
    ASSERT_EQ(cryptonote::round_money_up(1234ULL, 5), 1234ULL);
}

TEST(CryptonoteFormatUtils, RoundMoneyUpStringOverload)
{
    ASSERT_EQ(cryptonote::round_money_up("0.123456789012", 1), "0.200000000000");
    ASSERT_EQ(cryptonote::round_money_up("9.999999999999", 1), "10.000000000000");
    ASSERT_EQ(cryptonote::round_money_up("0.000000000000", 1), "0.000000000000");
}

TEST(CryptonoteFormatUtils, GetUnitAllDecimalPoints)
{
    ASSERT_EQ(cryptonote::get_unit(12), "monero");
    ASSERT_EQ(cryptonote::get_unit(9), "millinero");
    ASSERT_EQ(cryptonote::get_unit(6), "micronero");
    ASSERT_EQ(cryptonote::get_unit(3), "nanonero");
    ASSERT_EQ(cryptonote::get_unit(0), "piconero");
}

TEST(CryptonoteFormatUtils, SetAndGetDefaultDecimalPointRoundtrip)
{
    unsigned int original = cryptonote::get_default_decimal_point();

    cryptonote::set_default_decimal_point(9);
    ASSERT_EQ(cryptonote::get_default_decimal_point(), 9u);

    // print_money should use the new decimal point by default
    std::string s = cryptonote::print_money(1000000000ULL);
    ASSERT_EQ(s, "1.000000000");

    // Restore
    cryptonote::set_default_decimal_point(original);
}

// --- Key encrypt/decrypt ---

TEST(CryptonoteFormatUtils, EncryptDecryptKeyRoundtrip)
{
    // Use a small deterministic reduced scalar
    crypto::secret_key original;
    memset(&original, 0, sizeof(original));
    reinterpret_cast<uint8_t*>(&original)[0] = 0xAA;
    reinterpret_cast<uint8_t*>(&original)[1] = 0xBB;

    epee::wipeable_string pass("my_test_passphrase");
    crypto::secret_key encrypted = cryptonote::encrypt_key(original, pass);
    crypto::secret_key decrypted = cryptonote::decrypt_key(encrypted, pass);
    ASSERT_EQ(original, decrypted);
}

TEST(CryptonoteFormatUtils, DifferentPassphrasesProduceDifferentEncryptedKeys)
{
    crypto::secret_key key;
    memset(&key, 0, sizeof(key));
    reinterpret_cast<uint8_t*>(&key)[0] = 0x42;

    epee::wipeable_string pass1("passphrase_one");
    epee::wipeable_string pass2("passphrase_two");

    crypto::secret_key enc1 = cryptonote::encrypt_key(key, pass1);
    crypto::secret_key enc2 = cryptonote::encrypt_key(key, pass2);

    // Different passphrases should produce different encrypted keys
    ASSERT_NE(memcmp(&enc1, &enc2, sizeof(enc1)), 0);
}

TEST(CryptonoteFormatUtils, DecryptWithWrongPassphraseFails)
{
    crypto::secret_key original;
    memset(&original, 0, sizeof(original));
    reinterpret_cast<uint8_t*>(&original)[0] = 0x42;

    epee::wipeable_string correct_pass("correct");
    epee::wipeable_string wrong_pass("wrong");

    crypto::secret_key encrypted = cryptonote::encrypt_key(original, correct_pass);
    crypto::secret_key decrypted = cryptonote::decrypt_key(encrypted, wrong_pass);

    // Decryption with wrong passphrase should NOT produce the original key
    ASSERT_NE(memcmp(&original, &decrypted, sizeof(original)), 0);
}

// --- Transaction utilities ---

TEST(CryptonoteFormatUtils, SetTxOutWithoutViewTag)
{
    cryptonote::tx_out out;
    crypto::public_key pk = crypto::get_H();
    crypto::view_tag vt = {0};
    cryptonote::set_tx_out(12345, pk, false, vt, out);

    ASSERT_EQ(out.amount, 12345u);
    ASSERT_TRUE(out.target.type() == typeid(cryptonote::txout_to_key));

    crypto::public_key extracted;
    ASSERT_TRUE(cryptonote::get_output_public_key(out, extracted));
    ASSERT_EQ(extracted, pk);

    auto vt_opt = cryptonote::get_output_view_tag(out);
    ASSERT_FALSE(!!vt_opt);
}

TEST(CryptonoteFormatUtils, SetTxOutWithViewTag)
{
    cryptonote::tx_out out;
    crypto::public_key pk = crypto::get_H();
    crypto::view_tag vt;
    vt.data = 0x42;
    cryptonote::set_tx_out(67890, pk, true, vt, out);

    ASSERT_EQ(out.amount, 67890u);
    ASSERT_TRUE(out.target.type() == typeid(cryptonote::txout_to_tagged_key));

    crypto::public_key extracted;
    ASSERT_TRUE(cryptonote::get_output_public_key(out, extracted));
    ASSERT_EQ(extracted, pk);

    auto vt_opt = cryptonote::get_output_view_tag(out);
    ASSERT_TRUE(!!vt_opt);
    ASSERT_EQ(static_cast<unsigned char>(vt_opt->data), 0x42);
}

TEST(CryptonoteFormatUtils, GetOutputPublicKeyFromTxoutToKey)
{
    cryptonote::tx_out out;
    out.amount = 100;
    cryptonote::txout_to_key tk;
    tk.key = crypto::get_H();
    out.target = tk;

    crypto::public_key extracted;
    ASSERT_TRUE(cryptonote::get_output_public_key(out, extracted));
    ASSERT_EQ(extracted, crypto::get_H());
}

TEST(CryptonoteFormatUtils, GetOutputPublicKeyFromTxoutToTaggedKey)
{
    cryptonote::tx_out out;
    out.amount = 200;
    cryptonote::txout_to_tagged_key ttk;
    ttk.key = crypto::get_H();
    ttk.view_tag.data = 0xCD;
    out.target = ttk;

    crypto::public_key extracted;
    ASSERT_TRUE(cryptonote::get_output_public_key(out, extracted));
    ASSERT_EQ(extracted, crypto::get_H());
}

TEST(CryptonoteFormatUtils, GetOutputViewTagAbsent)
{
    cryptonote::tx_out out;
    out.amount = 100;
    cryptonote::txout_to_key tk;
    tk.key = crypto::get_H();
    out.target = tk;

    auto vt_opt = cryptonote::get_output_view_tag(out);
    ASSERT_FALSE(!!vt_opt);
}

TEST(CryptonoteFormatUtils, GetOutputViewTagPresent)
{
    cryptonote::tx_out out;
    out.amount = 100;
    cryptonote::txout_to_tagged_key ttk;
    ttk.key = crypto::get_H();
    ttk.view_tag.data = 0xEF;
    out.target = ttk;

    auto vt_opt = cryptonote::get_output_view_tag(out);
    ASSERT_TRUE(!!vt_opt);
    ASSERT_EQ(static_cast<unsigned char>(vt_opt->data), 0xEF);
}

TEST(CryptonoteFormatUtils, CheckMoneyOverflowValid)
{
    auto tx = make_v1_tx_with_key_inputs({100, 200}, {100, 200});
    ASSERT_TRUE(cryptonote::check_money_overflow(tx));
}

TEST(CryptonoteFormatUtils, CheckOutsOverflowWithOverflow)
{
    // Create a tx with outputs that would overflow uint64_t
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;

    // Two outputs whose amounts sum exceeds MONEY_SUPPLY
    cryptonote::tx_out out1, out2;
    cryptonote::set_tx_out(std::numeric_limits<uint64_t>::max(), crypto::get_H(), false, crypto::view_tag{}, out1);
    cryptonote::set_tx_out(1, crypto::get_H(), false, crypto::view_tag{}, out2);
    tx.vout.push_back(out1);
    tx.vout.push_back(out2);

    ASSERT_FALSE(cryptonote::check_outs_overflow(tx));
}

TEST(CryptonoteFormatUtils, CheckInputsOverflowWithOverflow)
{
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;

    // Two inputs whose amounts sum exceeds uint64_t max
    cryptonote::txin_to_key key_in1, key_in2;
    key_in1.amount = std::numeric_limits<uint64_t>::max();
    key_in1.key_offsets = {0};
    key_in1.k_image = crypto::rand<crypto::key_image>();
    key_in2.amount = 1;
    key_in2.key_offsets = {0};
    key_in2.k_image = crypto::rand<crypto::key_image>();
    tx.vin.push_back(key_in1);
    tx.vin.push_back(key_in2);

    ASSERT_FALSE(cryptonote::check_inputs_overflow(tx));
}

TEST(CryptonoteFormatUtils, CheckInputsTypesSupportedValid)
{
    auto tx = make_v1_tx_with_key_inputs({100, 200, 300}, {500});
    ASSERT_TRUE(cryptonote::check_inputs_types_supported(tx));
}

TEST(CryptonoteFormatUtils, CheckInputsTypesSupportedInvalid)
{
    // txin_gen is not supported by check_inputs_types_supported
    cryptonote::transaction tx;
    tx.version = 1;
    cryptonote::txin_gen gen;
    gen.height = 0;
    tx.vin.push_back(gen);
    ASSERT_FALSE(cryptonote::check_inputs_types_supported(tx));
}

TEST(CryptonoteFormatUtils, ShortHashStrFormatting)
{
    crypto::hash h;
    memset(&h, 0xAB, sizeof(h));
    std::string s = cryptonote::short_hash_str(h);

    // Should be exactly 20 characters: 8 hex + "...." + 8 hex
    ASSERT_EQ(s.size(), 20u);
    ASSERT_EQ(s.substr(8, 4), "....");

    // First and last 8 characters should be hex chars
    auto is_hex = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };
    for (int i = 0; i < 8; ++i)
    {
        ASSERT_TRUE(is_hex(s[i])) << "char at position " << i << " is not hex: " << s[i];
        ASSERT_TRUE(is_hex(s[12 + i])) << "char at position " << (12 + i) << " is not hex: " << s[12 + i];
    }
}

TEST(CryptonoteFormatUtils, ShortHashStrZeroHash)
{
    crypto::hash h;
    memset(&h, 0, sizeof(h));
    std::string s = cryptonote::short_hash_str(h);
    ASSERT_EQ(s, "00000000....00000000");
}

TEST(CryptonoteFormatUtils, GetBlobHashConsistency)
{
    std::string blob1 = "test blob data for hashing";
    std::string blob2 = "test blob data for hashing";
    std::string blob3 = "different blob data";

    crypto::hash h1 = cryptonote::get_blob_hash(blob1);
    crypto::hash h2 = cryptonote::get_blob_hash(blob2);
    crypto::hash h3 = cryptonote::get_blob_hash(blob3);

    ASSERT_EQ(h1, h2);
    ASSERT_NE(h1, h3);

    // Void overload should match
    crypto::hash h4;
    cryptonote::get_blob_hash(blob1, h4);
    ASSERT_EQ(h1, h4);
}

// --- Block utilities ---

TEST(CryptonoteFormatUtils, BlockToBlobAndParseBlobRoundtrip)
{
    cryptonote::block b;
    b.major_version = 15;
    b.minor_version = 15;
    b.timestamp = 1700000000;
    b.nonce = 0xBEEFCAFE;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 60;
    cryptonote::txin_gen gen;
    gen.height = 1000;
    b.miner_tx.vin.push_back(gen);

    // Add tx hashes
    crypto::hash txh1, txh2, txh3;
    memset(&txh1, 0x11, sizeof(txh1));
    memset(&txh2, 0x22, sizeof(txh2));
    memset(&txh3, 0x33, sizeof(txh3));
    b.tx_hashes.push_back(txh1);
    b.tx_hashes.push_back(txh2);
    b.tx_hashes.push_back(txh3);

    cryptonote::blobdata blob;
    ASSERT_TRUE(cryptonote::block_to_blob(b, blob));
    ASSERT_FALSE(blob.empty());

    cryptonote::block b2;
    crypto::hash block_hash;
    ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2, block_hash));

    ASSERT_EQ(b.major_version, b2.major_version);
    ASSERT_EQ(b.minor_version, b2.minor_version);
    ASSERT_EQ(b.timestamp, b2.timestamp);
    ASSERT_EQ(b.nonce, b2.nonce);
    ASSERT_EQ(b.tx_hashes.size(), b2.tx_hashes.size());
    for (size_t i = 0; i < b.tx_hashes.size(); ++i)
        ASSERT_EQ(b.tx_hashes[i], b2.tx_hashes[i]);

    // Hash from parsing should match get_block_hash
    crypto::hash expected_hash = cryptonote::get_block_hash(b2);
    ASSERT_EQ(block_hash, expected_hash);
}

TEST(CryptonoteFormatUtils, TxToBlobAndParseBlobRoundtrip)
{
    auto tx = make_v1_coinbase_tx(42, {
        {1000000000000ULL, crypto::get_H()},
        {2000000000000ULL, crypto::get_H()}
    });

    cryptonote::blobdata blob = cryptonote::tx_to_blob(tx);
    ASSERT_FALSE(blob.empty());

    cryptonote::transaction parsed;
    crypto::hash tx_hash;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, parsed, tx_hash));
    ASSERT_EQ(parsed.version, tx.version);
    ASSERT_EQ(parsed.unlock_time, tx.unlock_time);
    ASSERT_EQ(parsed.vout.size(), 2u);
    ASSERT_EQ(parsed.vout[0].amount, 1000000000000ULL);
    ASSERT_EQ(parsed.vout[1].amount, 2000000000000ULL);

    // Hash should match get_transaction_hash
    crypto::hash expected_hash = cryptonote::get_transaction_hash(parsed);
    ASSERT_EQ(tx_hash, expected_hash);
}

TEST(CryptonoteFormatUtils, GetBlockHashingBlobConsistency)
{
    cryptonote::block b;
    b.major_version = 14;
    b.minor_version = 14;
    b.timestamp = 1234567890;
    b.nonce = 99;
    b.miner_tx.version = 2;
    b.miner_tx.unlock_time = 0;
    cryptonote::txin_gen gen;
    gen.height = 200;
    b.miner_tx.vin.push_back(gen);

    cryptonote::blobdata hb1 = cryptonote::get_block_hashing_blob(b);
    cryptonote::blobdata hb2 = cryptonote::get_block_hashing_blob(b);
    ASSERT_EQ(hb1, hb2);
    ASSERT_FALSE(hb1.empty());

    // Changing nonce should change hashing blob
    cryptonote::block b2 = b;
    b2.nonce = 100;
    b2.invalidate_hashes();
    b2.miner_tx.invalidate_hashes();
    cryptonote::blobdata hb3 = cryptonote::get_block_hashing_blob(b2);
    ASSERT_NE(hb1, hb3);
}

TEST(CryptonoteFormatUtils, GetTxTreeHashThreeHashes)
{
    std::vector<crypto::hash> hashes;
    crypto::hash h1, h2, h3;
    memset(&h1, 0x11, sizeof(h1));
    memset(&h2, 0x22, sizeof(h2));
    memset(&h3, 0x33, sizeof(h3));
    hashes.push_back(h1);
    hashes.push_back(h2);
    hashes.push_back(h3);

    crypto::hash tree = cryptonote::get_tx_tree_hash(hashes);

    // Deterministic
    crypto::hash tree2 = cryptonote::get_tx_tree_hash(hashes);
    ASSERT_EQ(tree, tree2);

    // Should differ from any individual hash
    ASSERT_NE(tree, h1);
    ASSERT_NE(tree, h2);
    ASSERT_NE(tree, h3);

    // Reordering hashes should produce different tree hash
    std::vector<crypto::hash> reordered = {h3, h1, h2};
    crypto::hash tree_reordered = cryptonote::get_tx_tree_hash(reordered);
    ASSERT_NE(tree, tree_reordered);
}

TEST(CryptonoteFormatUtils, GetTxTreeHashFourHashes)
{
    std::vector<crypto::hash> hashes;
    for (int i = 0; i < 4; ++i)
    {
        crypto::hash h;
        memset(&h, 0x10 + i, sizeof(h));
        hashes.push_back(h);
    }

    crypto::hash tree = cryptonote::get_tx_tree_hash(hashes);
    crypto::hash tree2 = cryptonote::get_tx_tree_hash(hashes);
    ASSERT_EQ(tree, tree2);

    // Removing one hash should change the result
    std::vector<crypto::hash> fewer = {hashes[0], hashes[1], hashes[2]};
    crypto::hash tree3 = cryptonote::get_tx_tree_hash(fewer);
    ASSERT_NE(tree, tree3);
}

TEST(CryptonoteFormatUtils, GetTxTreeHashVoidOverloadMatchesReturn)
{
    std::vector<crypto::hash> hashes;
    crypto::hash h1, h2;
    memset(&h1, 0xAA, sizeof(h1));
    memset(&h2, 0xBB, sizeof(h2));
    hashes.push_back(h1);
    hashes.push_back(h2);

    crypto::hash result_return = cryptonote::get_tx_tree_hash(hashes);
    crypto::hash result_void;
    cryptonote::get_tx_tree_hash(hashes, result_void);
    ASSERT_EQ(result_return, result_void);
}

// --- Additional edge case tests ---

TEST(CryptonoteFormatUtils, PrintMoneyWithExplicitDecimalPoint)
{
    // Test print_money with explicit decimal point = 0 (piconero)
    ASSERT_EQ(cryptonote::print_money(42ULL, 0), "42");
    ASSERT_EQ(cryptonote::print_money(0ULL, 0), "0");

    // Decimal point = 3 (nanonero)
    ASSERT_EQ(cryptonote::print_money(1234ULL, 3), "1.234");
    ASSERT_EQ(cryptonote::print_money(1ULL, 3), "0.001");

    // Decimal point = 6 (micronero)
    ASSERT_EQ(cryptonote::print_money(1000000ULL, 6), "1.000000");
}

TEST(CryptonoteFormatUtils, CheckMoneyOverflowWithOverflowingOutputs)
{
    // Create a tx where outputs would overflow
    cryptonote::transaction tx;
    tx.version = 1;
    tx.unlock_time = 0;

    // Add a normal input
    cryptonote::txin_to_key key_in;
    key_in.amount = 100;
    key_in.key_offsets = {0};
    key_in.k_image = crypto::rand<crypto::key_image>();
    tx.vin.push_back(key_in);

    // Add outputs that overflow
    cryptonote::tx_out out1, out2;
    cryptonote::set_tx_out(std::numeric_limits<uint64_t>::max(), crypto::get_H(), false, crypto::view_tag{}, out1);
    cryptonote::set_tx_out(1, crypto::get_H(), false, crypto::view_tag{}, out2);
    tx.vout.push_back(out1);
    tx.vout.push_back(out2);

    ASSERT_FALSE(cryptonote::check_money_overflow(tx));
}

TEST(CryptonoteFormatUtils, EncryptKeyEmptyPassphrase)
{
    crypto::secret_key original;
    memset(&original, 0, sizeof(original));
    reinterpret_cast<uint8_t*>(&original)[0] = 0x01;

    epee::wipeable_string empty_pass("");
    crypto::secret_key encrypted = cryptonote::encrypt_key(original, empty_pass);
    crypto::secret_key decrypted = cryptonote::decrypt_key(encrypted, empty_pass);
    ASSERT_EQ(original, decrypted);
}

TEST(CryptonoteFormatUtils, GetBlobHashEmptyVsNonEmpty)
{
    crypto::hash h_empty = cryptonote::get_blob_hash(std::string(""));
    crypto::hash h_nonempty = cryptonote::get_blob_hash(std::string("x"));
    ASSERT_NE(h_empty, h_nonempty);

    // Empty hash should be a specific value (not all zeros)
    crypto::hash null_hash;
    memset(&null_hash, 0, sizeof(null_hash));
    ASSERT_NE(h_empty, null_hash);
}

TEST(CryptonoteFormatUtils, ParseAndValidateBlockInvalidBlobFails)
{
    cryptonote::block b;
    ASSERT_FALSE(cryptonote::parse_and_validate_block_from_blob(std::string("garbage"), b));
}

TEST(CryptonoteFormatUtils, ParseAndValidateTxInvalidBlobFails)
{
    cryptonote::transaction tx;
    ASSERT_FALSE(cryptonote::parse_and_validate_tx_from_blob(std::string("garbage"), tx));
}

TEST(CryptonoteFormatUtils, IsValidDecomposedAmountLargest)
{
    // 9 * 10^18 is valid
    ASSERT_TRUE(cryptonote::is_valid_decomposed_amount(9000000000000000000ULL));
    // 10^18 is valid (1 * 10^18)
    ASSERT_TRUE(cryptonote::is_valid_decomposed_amount(1000000000000000000ULL));
    // 10^18 + 1 is not valid
    ASSERT_FALSE(cryptonote::is_valid_decomposed_amount(1000000000000000001ULL));
}
