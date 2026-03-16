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
#include "serialization/binary_utils.h"
#include "serialization/string.h"
#include <vector>
#include <string>

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
    cryptonote::transaction tx;
    tx.version = 2;
    tx.unlock_time = 0;

    crypto::hash h1 = cryptonote::get_transaction_hash(tx);
    crypto::hash h2 = cryptonote::get_transaction_hash(tx);
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
