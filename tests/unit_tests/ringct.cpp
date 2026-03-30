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

#include "gtest/gtest.h"

#include <cstdint>
#include <algorithm>
#include <sstream>

#include "ringct/rctTypes.h"
#include "ringct/rctSigs.h"
#include "ringct/rctOps.h"
#include "ringct/bulletproofs.h"
#include "ringct/bulletproofs_plus.h"
#include "device/device.hpp"
#include "string_tools.h"

using namespace std;
using namespace crypto;
using namespace rct;

TEST(ringct, Borromean)
{
    int j = 0;

        //Tests for Borromean signatures
        //#boro true one, false one, C != sum Ci, and one out of the range..
        int N = 64;
        key64 xv;
        key64 P1v;
        key64 P2v;
        bits indi;

        for (j = 0 ; j < N ; j++) {
            indi[j] = (int)randXmrAmount(2);

            xv[j] = skGen();
            if ( (int)indi[j] == 0 ) {
                scalarmultBase(P1v[j], xv[j]);
            } else {
                addKeys1(P1v[j], xv[j], H2[j]);
            }
            subKeys(P2v[j], P1v[j], H2[j]);
        }

        //#true one
        boroSig bb = genBorromean(xv, P1v, P2v, indi);
        ASSERT_TRUE(verifyBorromean(bb, P1v, P2v));

        //#false one
        indi[3] = (indi[3] + 1) % 2;
        bb = genBorromean(xv, P1v, P2v, indi);
        ASSERT_FALSE(verifyBorromean(bb, P1v, P2v));

        //#true one again
        indi[3] = (indi[3] + 1) % 2;
        bb = genBorromean(xv, P1v, P2v, indi);
        ASSERT_TRUE(verifyBorromean(bb, P1v, P2v));

        //#false one
        bb = genBorromean(xv, P2v, P1v, indi);
        ASSERT_FALSE(verifyBorromean(bb, P1v, P2v));
}

TEST(ringct, MG_sigs)
{
    int j = 0;
    int N = 0;

        //Tests for MG Sigs
        //#MG sig: true one
        N = 3;// #cols
        int   R = 3;// #rows
        keyV xtmp = skvGen(R);
        keyM xm = keyMInit(R, N);// = [[None]*N] #just used to generate test public keys
        keyV sk = skvGen(R);
        keyM P  = keyMInit(R, N);// = keyM[[None]*N] #stores the public keys;
        int ind = 2;
        int i = 0;
        for (j = 0 ; j < R ; j++) {
            for (i = 0 ; i < N ; i++)
            {
                xm[i][j] = skGen();
                P[i][j] = scalarmultBase(xm[i][j]);
            }
        }
        for (j = 0 ; j < R ; j++) {
            sk[j] = xm[ind][j];
        }
        key message = identity();
        mgSig IIccss = MLSAG_Gen(message, P, sk, ind, R, hw::get_device("default"));
        ASSERT_TRUE(MLSAG_Ver(message, P, IIccss, R));

        //#MG sig: false one
        N = 3;// #cols
        R = 3;// #rows
        xtmp = skvGen(R);
        keyM xx(N, xtmp);// = [[None]*N] #just used to generate test public keys
        sk = skvGen(R);
        //P (N, xtmp);// = keyM[[None]*N] #stores the public keys;

        ind = 2;
        for (j = 0 ; j < R ; j++) {
            for (i = 0 ; i < N ; i++)
            {
                xx[i][j] = skGen();
                P[i][j] = scalarmultBase(xx[i][j]);
            }
            sk[j] = xx[ind][j];
        }
        sk[2] = skGen();//assume we don't know one of the private keys..
        IIccss = MLSAG_Gen(message, P, sk, ind, R, hw::get_device("default"));
        ASSERT_FALSE(MLSAG_Ver(message, P, IIccss, R));
}

TEST(ringct, CLSAG)
{
  const size_t N = 11;
  const size_t idx = 5;
  ctkeyV pubs;
  key p, t, t2, u;
  const key message = identity();
  ctkey backup;
  clsag clsag;

  for (size_t i = 0; i < N; ++i)
  {
    key sk;
    ctkey tmp;

    skpkGen(sk, tmp.dest);
    skpkGen(sk, tmp.mask);

    pubs.push_back(tmp);
  }

  // Set P[idx]
  skpkGen(p, pubs[idx].dest);

  // Set C[idx]
  t = skGen();
  u = skGen();
  addKeys2(pubs[idx].mask,t,u,H);

  // Set commitment offset
  key Cout;
  t2 = skGen();
  addKeys2(Cout,t2,u,H);

  // Prepare generation inputs
  ctkey insk;
  insk.dest = p;
  insk.mask = t;
  
  // bad message
  clsag = rct::proveRctCLSAGSimple(zero(),pubs,insk,t2,Cout,idx,hw::get_device("default"));
  ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));

  // bad index at creation
  try
  {
    clsag = rct::proveRctCLSAGSimple(message,pubs,insk,t2,Cout,(idx + 1) % N,hw::get_device("default"));
    ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  }
  catch (...) { /* either exception, or failure to verify above */ }

  // bad z at creation
  try
  {
    ctkey insk2;
    insk2.dest = insk.dest;
    insk2.mask = skGen();
    clsag = rct::proveRctCLSAGSimple(message,pubs,insk2,t2,Cout,idx,hw::get_device("default"));
    ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  }
  catch (...) { /* either exception, or failure to verify above */ }

  // bad C at creation
  backup = pubs[idx];
  pubs[idx].mask = scalarmultBase(skGen());
  try
  {
    clsag = rct::proveRctCLSAGSimple(message,pubs,insk,t2,Cout,idx,hw::get_device("default"));
    ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  }
  catch (...) { /* either exception, or failure to verify above */ }
  pubs[idx] = backup;

  // bad p at creation
  try
  {
    ctkey insk2;
    insk2.dest = skGen();
    insk2.mask = insk.mask;
    clsag = rct::proveRctCLSAGSimple(message,pubs,insk2,t2,Cout,idx,hw::get_device("default"));
    ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  }
  catch (...) { /* either exception, or failure to verify above */ }

  // bad P at creation
  backup = pubs[idx];
  pubs[idx].dest = scalarmultBase(skGen());
  try
  {
    clsag = rct::proveRctCLSAGSimple(message,pubs,insk,t2,Cout,idx,hw::get_device("default"));
    ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  }
  catch (...) { /* either exception, or failure to verify above */ }
  pubs[idx] = backup;

  // Test correct signature
  clsag = rct::proveRctCLSAGSimple(message,pubs,insk,t2,Cout,idx,hw::get_device("default"));
  ASSERT_TRUE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));

  // empty s
  auto sbackup = clsag.s;
  clsag.s.clear();
  ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  clsag.s = sbackup;

  // too few s elements
  key backup_key;
  backup_key = clsag.s.back();
  clsag.s.pop_back();
  ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  clsag.s.push_back(backup_key);

  // too many s elements
  clsag.s.push_back(skGen());
  ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  clsag.s.pop_back();

  // bad s in clsag at verification
  for (auto &s: clsag.s)
  {
    backup_key = s;
    s = skGen();
    ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
    s = backup_key;
  }

  // bad c1 in clsag at verification
  backup_key = clsag.c1;
  clsag.c1 = skGen();
  ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  clsag.c1 = backup_key;

  // bad I in clsag at verification
  backup_key = clsag.I;
  clsag.I = scalarmultBase(skGen());
  ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  clsag.I = backup_key;

  // bad D in clsag at verification
  backup_key = clsag.D;
  clsag.D = scalarmultBase(skGen());
  ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  clsag.D = backup_key;

  // D not in main subgroup in clsag at verification
  backup_key = clsag.D;
  rct::key x;
  ASSERT_TRUE(epee::string_tools::hex_to_pod("c7176a703d4dd84fba3c0b760d10670f2a2053fa2c39ccc64ec7fd7792ac03fa", x));
  clsag.D = rct::addKeys(clsag.D, x);
  ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  clsag.D = backup_key;

  // swapped I and D in clsag at verification
  std::swap(clsag.I, clsag.D);
  ASSERT_FALSE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
  std::swap(clsag.I, clsag.D);

  // check it's still good, in case we failed to restore
  ASSERT_TRUE(rct::verRctCLSAGSimple(message,clsag,pubs,Cout));
}

TEST(ringct, range_proofs)
{
        //Ring CT Stuff
        //ct range proofs
        ctkeyV sc, pc;
        ctkey sctmp, pctmp;
        std::vector<uint64_t> inamounts;
        //add fake input 6000
        inamounts.push_back(6000);
        tie(sctmp, pctmp) = ctskpkGen(inamounts.back());
        sc.push_back(sctmp);
        pc.push_back(pctmp);


        inamounts.push_back(7000);
        tie(sctmp, pctmp) = ctskpkGen(inamounts.back());
        sc.push_back(sctmp);
        pc.push_back(pctmp);
        vector<xmr_amount >amounts;
        rct::keyV amount_keys;
        key mask;

        //add output 500
        amounts.push_back(500);
        amount_keys.push_back(rct::hash_to_scalar(rct::zero()));
        keyV destinations;
        key Sk, Pk;
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);


        //add output for 12500
        amounts.push_back(12500);
        amount_keys.push_back(rct::hash_to_scalar(rct::zero()));
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);

        const rct::RCTConfig rct_config { RangeProofBorromean, 0 };

        //compute rct data with mixin 3 - should fail since full type with > 1 input
        bool ok = false;
        try { genRct(rct::zero(), sc, pc, destinations, amounts, amount_keys, 3, rct_config, hw::get_device("default")); }
        catch(...) { ok = true; }
        ASSERT_TRUE(ok);

        //compute rct data with mixin 3
        rctSig s = genRctSimple(rct::zero(), sc, pc, destinations, inamounts, amounts, amount_keys, 0, 3, rct_config, hw::get_device("default"));

        //verify rct data
        ASSERT_TRUE(verRctSimple(s));

        //decode received amount
        decodeRctSimple(s, amount_keys[1], 1, mask, hw::get_device("default"));

        // Ring CT with failing MG sig part should not verify!
        // Since sum of inputs != outputs

        amounts[1] = 12501;
        skpkGen(Sk, Pk);
        destinations[1] = Pk;


        //compute rct data with mixin 3
        s = genRctSimple(rct::zero(), sc, pc, destinations, inamounts, amounts, amount_keys, 0, 3, rct_config, hw::get_device("default"));

        //verify rct data
        ASSERT_FALSE(verRctSimple(s));

        //decode received amount
        decodeRctSimple(s, amount_keys[1], 1, mask, hw::get_device("default"));
}

TEST(ringct, range_proofs_with_fee)
{
        //Ring CT Stuff
        //ct range proofs
        ctkeyV sc, pc;
        ctkey sctmp, pctmp;
        std::vector<uint64_t> inamounts;
        //add fake input 6001
        inamounts.push_back(6001);
        tie(sctmp, pctmp) = ctskpkGen(inamounts.back());
        sc.push_back(sctmp);
        pc.push_back(pctmp);


        inamounts.push_back(7000);
        tie(sctmp, pctmp) = ctskpkGen(inamounts.back());
        sc.push_back(sctmp);
        pc.push_back(pctmp);
        vector<xmr_amount >amounts;
        keyV amount_keys;
        key mask;

        //add output 500
        amounts.push_back(500);
        amount_keys.push_back(rct::hash_to_scalar(rct::zero()));
        keyV destinations;
        key Sk, Pk;
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);

        //add output for 12500
        amounts.push_back(12500);
        amount_keys.push_back(hash_to_scalar(zero()));
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);

        const rct::RCTConfig rct_config { RangeProofBorromean, 0 };

        //compute rct data with mixin 3
        rctSig s = genRctSimple(rct::zero(), sc, pc, destinations, inamounts, amounts, amount_keys, 1, 3, rct_config, hw::get_device("default"));

        //verify rct data
        ASSERT_TRUE(verRctSimple(s));

        //decode received amount
        decodeRctSimple(s, amount_keys[1], 1, mask, hw::get_device("default"));

        // Ring CT with failing MG sig part should not verify!
        // Since sum of inputs != outputs

        amounts[1] = 12501;
        skpkGen(Sk, Pk);
        destinations[1] = Pk;


        //compute rct data with mixin 3
        s = genRctSimple(rct::zero(), sc, pc, destinations, inamounts, amounts, amount_keys, 500, 3, rct_config, hw::get_device("default"));

        //verify rct data
        ASSERT_FALSE(verRctSimple(s));

        //decode received amount
        decodeRctSimple(s, amount_keys[1], 1, mask, hw::get_device("default"));
}

TEST(ringct, simple)
{
        ctkeyV sc, pc;
        ctkey sctmp, pctmp;
        //this vector corresponds to output amounts
        vector<xmr_amount>outamounts;
       //this vector corresponds to input amounts
        vector<xmr_amount>inamounts;
        //this keyV corresponds to destination pubkeys
        keyV destinations;
        keyV amount_keys;
        key mask;

        //add fake input 3000
        //the sc is secret data
        //pc is public data
        tie(sctmp, pctmp) = ctskpkGen(3000);
        sc.push_back(sctmp);
        pc.push_back(pctmp);
        inamounts.push_back(3000);

        //add fake input 3000
        //the sc is secret data
        //pc is public data
        tie(sctmp, pctmp) = ctskpkGen(3000);
        sc.push_back(sctmp);
        pc.push_back(pctmp);
        inamounts.push_back(3000);

        //add output 5000
        outamounts.push_back(5000);
        amount_keys.push_back(rct::hash_to_scalar(rct::zero()));
        //add the corresponding destination pubkey
        key Sk, Pk;
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);

        //add output 999
        outamounts.push_back(999);
        amount_keys.push_back(rct::hash_to_scalar(rct::zero()));
        //add the corresponding destination pubkey
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);

        key message = skGen(); //real message later (hash of txn..)

        //compute sig with mixin 2
        xmr_amount txnfee = 1;

        const rct::RCTConfig rct_config { RangeProofBorromean, 0 };
        rctSig s = genRctSimple(message, sc, pc, destinations,inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

        //verify ring ct signature
        ASSERT_TRUE(verRctSimple(s));

        //decode received amount corresponding to output pubkey index 1
        decodeRctSimple(s, amount_keys[1], 1, mask,  hw::get_device("default"));
}

static rct::rctSig make_sample_rct_sig(int n_inputs, const uint64_t input_amounts[], int n_outputs, const uint64_t output_amounts[], bool last_is_fee)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount >amounts;
    keyV destinations;
    keyV amount_keys;
    key Sk, Pk;

    for (int n = 0; n < n_inputs; ++n) {
        tie(sctmp, pctmp) = ctskpkGen(input_amounts[n]);
        sc.push_back(sctmp);
        pc.push_back(pctmp);
    }

    for (int n = 0; n < n_outputs; ++n) {
        amounts.push_back(output_amounts[n]);
        skpkGen(Sk, Pk);
        if (n < n_outputs - 1 || !last_is_fee)
        {
          destinations.push_back(Pk);
          amount_keys.push_back(rct::hash_to_scalar(rct::zero()));
        }
    }

    const rct::RCTConfig rct_config { RangeProofBorromean, 0 };
    return genRct(rct::zero(), sc, pc, destinations, amounts, amount_keys, 3, rct_config, hw::get_device("default"));
}

static rct::rctSig make_sample_simple_rct_sig(int n_inputs, const uint64_t input_amounts[], int n_outputs, const uint64_t output_amounts[], uint64_t fee)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations;
    keyV amount_keys;
    key Sk, Pk;

    for (int n = 0; n < n_inputs; ++n) {
        inamounts.push_back(input_amounts[n]);
        tie(sctmp, pctmp) = ctskpkGen(input_amounts[n]);
        sc.push_back(sctmp);
        pc.push_back(pctmp);
    }

    for (int n = 0; n < n_outputs; ++n) {
        outamounts.push_back(output_amounts[n]);
        amount_keys.push_back(hash_to_scalar(zero()));
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);
    }

    const rct::RCTConfig rct_config { RangeProofBorromean, 0 };
    return genRctSimple(rct::zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, fee, 3, rct_config, hw::get_device("default"));
}

static bool range_proof_test(bool expected_valid,
    int n_inputs, const uint64_t input_amounts[], int n_outputs, const uint64_t output_amounts[], bool last_is_fee, bool simple)
{
    //compute rct data
    bool valid;
    try {
        rctSig s;
        // simple takes fee as a parameter, non-simple takes it as an extra element to output amounts
        if (simple) {
          s = make_sample_simple_rct_sig(n_inputs, input_amounts, last_is_fee ? n_outputs - 1 : n_outputs, output_amounts, last_is_fee ? output_amounts[n_outputs - 1] : 0);
          valid = verRctSimple(s);
        }
        else {
          s = make_sample_rct_sig(n_inputs, input_amounts, n_outputs, output_amounts, last_is_fee);
          valid = verRct(s);
        }
    }
    catch (const std::exception &e) {
        valid = false;
    }

    if (valid == expected_valid) {
        return testing::AssertionSuccess();
    }
    else {
        return testing::AssertionFailure();
    }
}

#define NELTS(array) (sizeof(array)/sizeof(array[0]))

TEST(ringct, range_proofs_reject_empty_outs)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_empty_outs_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_empty_ins)
{
  const uint64_t inputs[] = {};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_empty_ins_simple)
{
  const uint64_t inputs[] = {};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_all_empty)
{
  const uint64_t inputs[] = {};
  const uint64_t outputs[] = {};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_all_empty_simple)
{
  const uint64_t inputs[] = {};
  const uint64_t outputs[] = {};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_accept_zero_empty)
{
  const uint64_t inputs[] = {0};
  const uint64_t outputs[] = {};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_accept_zero_empty_simple)
{
  const uint64_t inputs[] = {0};
  const uint64_t outputs[] = {};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_empty_zero)
{
  const uint64_t inputs[] = {};
  const uint64_t outputs[] = {0};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_empty_zero_simple)
{
  const uint64_t inputs[] = {};
  const uint64_t outputs[] = {0};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_accept_zero_zero)
{
  const uint64_t inputs[] = {0};
  const uint64_t outputs[] = {0};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_accept_zero_zero_simple)
{
  const uint64_t inputs[] = {0};
  const uint64_t outputs[] = {0};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_accept_zero_out_first)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {0, 5000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_accept_zero_out_first_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {0, 5000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_accept_zero_out_last)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {5000, 0};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_accept_zero_out_last_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {5000, 0};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_accept_zero_out_middle)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {2500, 0, 2500};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_accept_zero_out_middle_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {2500, 0, 2500};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_accept_zero)
{
  const uint64_t inputs[] = {0};
  const uint64_t outputs[] = {0};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_accept_zero_in_first_simple)
{
  const uint64_t inputs[] = {0, 5000};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_accept_zero_in_last_simple)
{
  const uint64_t inputs[] = {5000, 0};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_accept_zero_in_middle_simple)
{
  const uint64_t inputs[] = {2500, 0, 2500};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_single_lower)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {1};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_single_lower_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {1};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_single_higher)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {5001};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_single_higher_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {5001};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_single_out_negative)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {(uint64_t)-1000ll};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_single_out_negative_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {(uint64_t)-1000ll};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_out_negative_first)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {(uint64_t)-1000ll, 6000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_out_negative_first_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {(uint64_t)-1000ll, 6000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_out_negative_last)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {6000, (uint64_t)-1000ll};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_out_negative_last_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {6000, (uint64_t)-1000ll};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_out_negative_middle)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {3000, (uint64_t)-1000ll, 3000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_out_negative_middle_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {3000, (uint64_t)-1000ll, 3000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_single_in_negative)
{
  const uint64_t inputs[] = {(uint64_t)-1000ll};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_single_in_negative_simple)
{
  const uint64_t inputs[] = {(uint64_t)-1000ll};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_in_negative_first)
{
  const uint64_t inputs[] = {(uint64_t)-1000ll, 6000};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_in_negative_first_simple)
{
  const uint64_t inputs[] = {(uint64_t)-1000ll, 6000};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_in_negative_last)
{
  const uint64_t inputs[] = {6000, (uint64_t)-1000ll};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_in_negative_last_simple)
{
  const uint64_t inputs[] = {6000, (uint64_t)-1000ll};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_in_negative_middle)
{
  const uint64_t inputs[] = {3000, (uint64_t)-1000ll, 3000};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_in_negative_middle_simple)
{
  const uint64_t inputs[] = {3000, (uint64_t)-1000ll, 3000};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_reject_higher_list)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {1000, 1000, 1000, 1000, 1000, 1000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_reject_higher_list_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {1000, 1000, 1000, 1000, 1000, 1000};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_accept_1_to_1)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_accept_1_to_1_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_accept_1_to_N)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {1000, 1000, 1000, 1000, 1000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, false));
}

TEST(ringct, range_proofs_accept_1_to_N_simple)
{
  const uint64_t inputs[] = {5000};
  const uint64_t outputs[] = {1000, 1000, 1000, 1000, 1000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false,true));
}

TEST(ringct, range_proofs_accept_N_to_1_simple)
{
  const uint64_t inputs[] = {1000, 1000, 1000, 1000, 1000};
  const uint64_t outputs[] = {5000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_accept_N_to_N_simple)
{
  const uint64_t inputs[] = {1000, 1000, 1000, 1000, 1000};
  const uint64_t outputs[] = {1000, 1000, 1000, 1000, 1000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, range_proofs_accept_very_long_simple)
{
  const size_t N=12;
  uint64_t inputs[N];
  uint64_t outputs[N];
  for (size_t n = 0; n < N; ++n) {
    inputs[n] = n;
    outputs[n] = n;
  }
  std::shuffle(inputs, inputs + N, crypto::random_device{});
  std::shuffle(outputs, outputs + N, crypto::random_device{});
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, false, true));
}

TEST(ringct, HPow2)
{
  key G = scalarmultBase(d2h(1));

  // Note that H is computed differently than standard hashing
  // This method is not guaranteed to return a curvepoint for all inputs
  // Don't use it elsewhere
  key H = cn_fast_hash(G);
  ge_p3 H_p3;
  int decode = ge_frombytes_vartime(&H_p3, H.bytes);
  ASSERT_EQ(decode, 0); // this is known to pass for the particular value G
  ge_p2 H_p2;
  ge_p3_to_p2(&H_p2, &H_p3);
  ge_p1p1 H8_p1p1;
  ge_mul8(&H8_p1p1, &H_p2);
  ge_p1p1_to_p3(&H_p3, &H8_p1p1);
  ge_p3_tobytes(H.bytes, &H_p3);

  for (int j = 0 ; j < ATOMS ; j++) {
    ASSERT_TRUE(equalKeys(H, H2[j]));
    addKeys(H, H, H);
  }
}

static const xmr_amount test_amounts[]={0, 1, 2, 3, 4, 5, 10000, 10000000000000000000ull, 10203040506070809000ull, 123456789123456789};

TEST(ringct, d2h)
{
  key k, P1;
  skpkGen(k, P1);
  for (auto amount: test_amounts) {
    d2h(k, amount);
    ASSERT_TRUE(amount == h2d(k));
  }
}

TEST(ringct, d2b)
{
  for (auto amount: test_amounts) {
    bits b;
    d2b(b, amount);
    ASSERT_TRUE(amount == b2d(b));
  }
}

TEST(ringct, prooveRange_is_non_deterministic)
{
  key C[2], mask[2];
  for (int n = 0; n < 2; ++n)
    proveRange(C[n], mask[n], 80);
  ASSERT_TRUE(memcmp(C[0].bytes, C[1].bytes, sizeof(C[0].bytes)));
  ASSERT_TRUE(memcmp(mask[0].bytes, mask[1].bytes, sizeof(mask[0].bytes)));
}

TEST(ringct, fee_0_valid)
{
  const uint64_t inputs[] = {2000};
  const uint64_t outputs[] = {2000, 0};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, true, false));
}

TEST(ringct, fee_0_valid_simple)
{
  const uint64_t inputs[] = {1000, 1000};
  const uint64_t outputs[] = {2000, 0};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, true, true));
}

TEST(ringct, fee_non_0_valid)
{
  const uint64_t inputs[] = {2000};
  const uint64_t outputs[] = {1900, 100};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, true, false));
}

TEST(ringct, fee_non_0_valid_simple)
{
  const uint64_t inputs[] = {1000, 1000};
  const uint64_t outputs[] = {1900, 100};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, true, true));
}

TEST(ringct, fee_non_0_invalid_higher)
{
  const uint64_t inputs[] = {1000, 1000};
  const uint64_t outputs[] = {1990, 100};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, true, false));
}

TEST(ringct, fee_non_0_invalid_higher_simple)
{
  const uint64_t inputs[] = {1000, 1000};
  const uint64_t outputs[] = {1990, 100};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, true, true));
}

TEST(ringct, fee_non_0_invalid_lower)
{
  const uint64_t inputs[] = {1000, 1000};
  const uint64_t outputs[] = {1000, 100};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, true, false));
}

TEST(ringct, fee_non_0_invalid_lower_simple)
{
  const uint64_t inputs[] = {1000, 1000};
  const uint64_t outputs[] = {1000, 100};
  EXPECT_TRUE(range_proof_test(false, NELTS(inputs), inputs, NELTS(outputs), outputs, true, true));
}

TEST(ringct, fee_burn_valid_one_out)
{
  const uint64_t inputs[] = {2000};
  const uint64_t outputs[] = {0, 2000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, true, false));
}

TEST(ringct, fee_burn_valid_one_out_simple)
{
  const uint64_t inputs[] = {1000, 1000};
  const uint64_t outputs[] = {0, 2000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, true, true));
}

TEST(ringct, fee_burn_valid_zero_out)
{
  const uint64_t inputs[] = {2000};
  const uint64_t outputs[] = {2000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, true, false));
}

TEST(ringct, fee_burn_valid_zero_out_simple)
{
  const uint64_t inputs[] = {1000, 1000};
  const uint64_t outputs[] = {2000};
  EXPECT_TRUE(range_proof_test(true, NELTS(inputs), inputs, NELTS(outputs), outputs, true, true));
}

static rctSig make_sig()
{
  static const uint64_t inputs[] = {2000};
  static const uint64_t outputs[] = {1000, 1000};
  static rct::rctSig sig = make_sample_rct_sig(NELTS(inputs), inputs, NELTS(outputs), outputs, true);
  return sig;
}

#define TEST_rctSig_elements(name, op) \
TEST(ringct, rctSig_##name) \
{ \
  rct::rctSig sig = make_sig(); \
  ASSERT_TRUE(rct::verRct(sig)); \
  op; \
  ASSERT_FALSE(rct::verRct(sig)); \
}

TEST_rctSig_elements(rangeSigs_empty, sig.p.rangeSigs.resize(0));
TEST_rctSig_elements(rangeSigs_too_many, sig.p.rangeSigs.push_back(sig.p.rangeSigs.back()));
TEST_rctSig_elements(rangeSigs_too_few, sig.p.rangeSigs.pop_back());
TEST_rctSig_elements(mgSig_MG_empty, sig.p.MGs.resize(0));
TEST_rctSig_elements(mgSig_ss_empty, sig.p.MGs[0].ss.resize(0));
TEST_rctSig_elements(mgSig_ss_too_many, sig.p.MGs[0].ss.push_back(sig.p.MGs[0].ss.back()));
TEST_rctSig_elements(mgSig_ss_too_few, sig.p.MGs[0].ss.pop_back());
TEST_rctSig_elements(mgSig_ss0_empty, sig.p.MGs[0].ss[0].resize(0));
TEST_rctSig_elements(mgSig_ss0_too_many, sig.p.MGs[0].ss[0].push_back(sig.p.MGs[0].ss[0].back()));
TEST_rctSig_elements(mgSig_ss0_too_few, sig.p.MGs[0].ss[0].pop_back());
TEST_rctSig_elements(mgSig_II_empty, sig.p.MGs[0].II.resize(0));
TEST_rctSig_elements(mgSig_II_too_many, sig.p.MGs[0].II.push_back(sig.p.MGs[0].II.back()));
TEST_rctSig_elements(mgSig_II_too_few, sig.p.MGs[0].II.pop_back());
TEST_rctSig_elements(mixRing_empty, sig.mixRing.resize(0));
TEST_rctSig_elements(mixRing_too_many, sig.mixRing.push_back(sig.mixRing.back()));
TEST_rctSig_elements(mixRing_too_few, sig.mixRing.pop_back());
TEST_rctSig_elements(mixRing0_empty, sig.mixRing[0].resize(0));
TEST_rctSig_elements(mixRing0_too_many, sig.mixRing[0].push_back(sig.mixRing[0].back()));
TEST_rctSig_elements(mixRing0_too_few, sig.mixRing[0].pop_back());
TEST_rctSig_elements(ecdhInfo_empty, sig.ecdhInfo.resize(0));
TEST_rctSig_elements(ecdhInfo_too_many, sig.ecdhInfo.push_back(sig.ecdhInfo.back()));
TEST_rctSig_elements(ecdhInfo_too_few, sig.ecdhInfo.pop_back());
TEST_rctSig_elements(outPk_empty, sig.outPk.resize(0));
TEST_rctSig_elements(outPk_too_many, sig.outPk.push_back(sig.outPk.back()));
TEST_rctSig_elements(outPk_too_few, sig.outPk.pop_back());

static rct::rctSig make_sig_simple()
{
  static const uint64_t inputs[] = {1000, 1000};
  static const uint64_t outputs[] = {1000};
  static rct::rctSig sig = make_sample_simple_rct_sig(NELTS(inputs), inputs, NELTS(outputs), outputs, 1000);
  return sig;
}

#define TEST_rctSig_elements_simple(name, op) \
TEST(ringct, rctSig_##name##_simple) \
{ \
  rct::rctSig sig = make_sig_simple(); \
  ASSERT_TRUE(rct::verRctSimple(sig)); \
  op; \
  ASSERT_FALSE(rct::verRctSimple(sig)); \
}

TEST_rctSig_elements_simple(rangeSigs_empty, sig.p.rangeSigs.resize(0));
TEST_rctSig_elements_simple(rangeSigs_too_many, sig.p.rangeSigs.push_back(sig.p.rangeSigs.back()));
TEST_rctSig_elements_simple(rangeSigs_too_few, sig.p.rangeSigs.pop_back());
TEST_rctSig_elements_simple(mgSig_empty, sig.p.MGs.resize(0));
TEST_rctSig_elements_simple(mgSig_too_many, sig.p.MGs.push_back(sig.p.MGs.back()));
TEST_rctSig_elements_simple(mgSig_too_few, sig.p.MGs.pop_back());
TEST_rctSig_elements_simple(mgSig0_ss_empty, sig.p.MGs[0].ss.resize(0));
TEST_rctSig_elements_simple(mgSig0_ss_too_many, sig.p.MGs[0].ss.push_back(sig.p.MGs[0].ss.back()));
TEST_rctSig_elements_simple(mgSig0_ss_too_few, sig.p.MGs[0].ss.pop_back());
TEST_rctSig_elements_simple(mgSig_ss0_empty, sig.p.MGs[0].ss[0].resize(0));
TEST_rctSig_elements_simple(mgSig_ss0_too_many, sig.p.MGs[0].ss[0].push_back(sig.p.MGs[0].ss[0].back()));
TEST_rctSig_elements_simple(mgSig_ss0_too_few, sig.p.MGs[0].ss[0].pop_back());
TEST_rctSig_elements_simple(mgSig0_II_empty, sig.p.MGs[0].II.resize(0));
TEST_rctSig_elements_simple(mgSig0_II_too_many, sig.p.MGs[0].II.push_back(sig.p.MGs[0].II.back()));
TEST_rctSig_elements_simple(mgSig0_II_too_few, sig.p.MGs[0].II.pop_back());
TEST_rctSig_elements_simple(mixRing_empty, sig.mixRing.resize(0));
TEST_rctSig_elements_simple(mixRing_too_many, sig.mixRing.push_back(sig.mixRing.back()));
TEST_rctSig_elements_simple(mixRing_too_few, sig.mixRing.pop_back());
TEST_rctSig_elements_simple(mixRing0_empty, sig.mixRing[0].resize(0));
TEST_rctSig_elements_simple(mixRing0_too_many, sig.mixRing[0].push_back(sig.mixRing[0].back()));
TEST_rctSig_elements_simple(mixRing0_too_few, sig.mixRing[0].pop_back());
TEST_rctSig_elements_simple(pseudoOuts_empty, sig.pseudoOuts.resize(0));
TEST_rctSig_elements_simple(pseudoOuts_too_many, sig.pseudoOuts.push_back(sig.pseudoOuts.back()));
TEST_rctSig_elements_simple(pseudoOuts_too_few, sig.pseudoOuts.pop_back());
TEST_rctSig_elements_simple(ecdhInfo_empty, sig.ecdhInfo.resize(0));
TEST_rctSig_elements_simple(ecdhInfo_too_many, sig.ecdhInfo.push_back(sig.ecdhInfo.back()));
TEST_rctSig_elements_simple(ecdhInfo_too_few, sig.ecdhInfo.pop_back());
TEST_rctSig_elements_simple(outPk_empty, sig.outPk.resize(0));
TEST_rctSig_elements_simple(outPk_too_many, sig.outPk.push_back(sig.outPk.back()));
TEST_rctSig_elements_simple(outPk_too_few, sig.outPk.pop_back());

TEST(ringct, reject_gen_simple_ver_non_simple)
{
  const uint64_t inputs[] = {1000, 1000};
  const uint64_t outputs[] = {1000};
  rct::rctSig sig = make_sample_simple_rct_sig(NELTS(inputs), inputs, NELTS(outputs), outputs, 1000);
  ASSERT_FALSE(rct::verRct(sig));
}

TEST(ringct, reject_gen_non_simple_ver_simple)
{
  const uint64_t inputs[] = {2000};
  const uint64_t outputs[] = {1000, 1000};
  rct::rctSig sig = make_sample_rct_sig(NELTS(inputs), inputs, NELTS(outputs), outputs, true);
  ASSERT_FALSE(rct::verRctSimple(sig));
}

TEST(ringct, key_ostream)
{
  std::stringstream out;
  out << "BEGIN" << rct::H << "END";
  EXPECT_EQ(
    std::string{"BEGIN<8b655970153799af2aeadc9ff1add0ea6c7251d54154cfa92c173a0dd39c1f94>END"},
    out.str()
  );
}

TEST(ringct, zeroCommmit)
{
  static const uint64_t amount = crypto::rand<uint64_t>();
  const rct::key z = rct::zeroCommit(amount);
  const rct::key a = rct::scalarmultBase(rct::identity());
  const rct::key b = rct::scalarmultH(rct::d2h(amount));
  const rct::key manual = rct::addKeys(a, b);
  ASSERT_EQ(z, manual);
}

static rct::key uncachedZeroCommit(uint64_t amount)
{
  const rct::key am = rct::d2h(amount);
  const rct::key bH = rct::scalarmultH(am);
  return rct::addKeys(rct::G, bH);
}

TEST(ringct, zeroCommitCache)
{
  ASSERT_EQ(rct::zeroCommit(0), uncachedZeroCommit(0));
  ASSERT_EQ(rct::zeroCommit(1), uncachedZeroCommit(1));
  ASSERT_EQ(rct::zeroCommit(2), uncachedZeroCommit(2));
  ASSERT_EQ(rct::zeroCommit(10), uncachedZeroCommit(10));
  ASSERT_EQ(rct::zeroCommit(200), uncachedZeroCommit(200));
  ASSERT_EQ(rct::zeroCommit(1000000000), uncachedZeroCommit(1000000000));
  ASSERT_EQ(rct::zeroCommit(3000000000000), uncachedZeroCommit(3000000000000));
  ASSERT_EQ(rct::zeroCommit(900000000000000), uncachedZeroCommit(900000000000000));
}

TEST(ringct, H)
{
  ge_p3 p3;
  ASSERT_EQ(ge_frombytes_vartime(&p3, rct::H.bytes), 0);
  ASSERT_EQ(memcmp(&p3, &ge_p3_H, sizeof(ge_p3)), 0);
}

TEST(ringct, mul8)
{
  ge_p3 p3;
  rct::key key;
  ASSERT_EQ(rct::scalarmult8(rct::identity()), rct::identity());
  rct::scalarmult8(p3,rct::identity());
  ge_p3_tobytes(key.bytes, &p3);
  ASSERT_EQ(key, rct::identity());
  ASSERT_EQ(rct::scalarmult8(rct::H), rct::scalarmultKey(rct::H, rct::EIGHT));
  rct::scalarmult8(p3,rct::H);
  ge_p3_tobytes(key.bytes, &p3);
  ASSERT_EQ(key, rct::scalarmultKey(rct::H, rct::EIGHT));
  ASSERT_EQ(rct::scalarmultKey(rct::scalarmultKey(rct::H, rct::INV_EIGHT), rct::EIGHT), rct::H);
}

TEST(ringct, aggregated)
{
  static const size_t N_PROOFS = 16;
  std::vector<rctSig> s(N_PROOFS);
  std::vector<const rctSig*> sp(N_PROOFS);

  for (size_t n = 0; n < N_PROOFS; ++n)
  {
    static const uint64_t inputs[] = {1000, 1000};
    static const uint64_t outputs[] = {500, 1500};
    s[n] = make_sample_simple_rct_sig(NELTS(inputs), inputs, NELTS(outputs), outputs, 0);
    sp[n] = &s[n];
  }

  ASSERT_TRUE(verRctSemanticsSimple(sp));
}

TEST(ringct, key_operations)
{
  // Test basic key operations
  key sk = skGen();
  key pk;
  scalarmultBase(pk, sk);

  // pk should not be identity
  ASSERT_NE(pk, identity());

  // sk * G == pk
  key pk2;
  scalarmultBase(pk2, sk);
  ASSERT_EQ(pk, pk2);
}

TEST(ringct, scalarmult_zero)
{
  key result;
  key zero;
  memset(&zero, 0, sizeof(zero));
  scalarmultBase(result, zero);
  ASSERT_EQ(result, identity());
}

TEST(ringct, key_add_commutative)
{
  // addKeys expects valid curve points, not raw scalars.
  // Generate points by doing scalarmultBase on random scalars.
  key sa = skGen();
  key sb = skGen();
  key a, b;
  scalarmultBase(a, sa);
  scalarmultBase(b, sb);
  key ab, ba;
  addKeys(ab, a, b);
  addKeys(ba, b, a);
  ASSERT_EQ(ab, ba);
}

TEST(ringct, sc_add_commutative)
{
  key a = skGen();
  key b = skGen();
  key ab, ba;
  sc_add(ab.bytes, a.bytes, b.bytes);
  sc_add(ba.bytes, b.bytes, a.bytes);
  ASSERT_EQ(ab, ba);
}

TEST(ringct, ecdh_encode_decode_roundtrip)
{
  ecdhTuple original;
  original.mask = skGen();
  original.amount = skGen();
  key sharedKey = skGen();

  ecdhTuple encoded = original;
  ecdhEncode(encoded, sharedKey, false);

  // Encoded should differ from original
  ASSERT_NE(original.mask, encoded.mask);

  ecdhDecode(encoded, sharedKey, false);
  ASSERT_EQ(original.mask, encoded.mask);
  ASSERT_EQ(original.amount, encoded.amount);
}

TEST(ringct, ecdh8_encode_decode_roundtrip)
{
  ecdhTuple original;
  original.mask = skGen();
  original.amount = skGen();
  // Zero out bytes 8-31 to match 8-byte mode
  memset(original.amount.bytes + 8, 0, 24);
  key sharedKey = skGen();

  ecdhTuple encoded = original;
  ecdhEncode(encoded, sharedKey, true);
  ecdhDecode(encoded, sharedKey, true);
  // Only first 8 bytes of amount should match
  ASSERT_EQ(memcmp(original.amount.bytes, encoded.amount.bytes, 8), 0);
}

TEST(ringct, d2h_h2d_roundtrip)
{
  for (uint64_t v : {0ULL, 1ULL, 42ULL, 1000000000ULL, 0xFFFFFFFFFFFFFFFFULL})
  {
    key k = d2h(v);
    uint64_t recovered = h2d(k);
    ASSERT_EQ(v, recovered);
  }
}

TEST(ringct, zero_commit)
{
  key commit = zeroCommit(0);
  // zeroCommit(0) should be a valid point (mask = I, amount = 0)
  ASSERT_NE(commit, identity());
}

TEST(ringct, corrupted_signature_fails)
{
  // Create a valid simple RCT sig and corrupt it
  static const uint64_t inputs[] = {1000};
  static const uint64_t outputs[] = {1000};
  rctSig sig = make_sample_simple_rct_sig(1, inputs, 1, outputs, 0);

  // Should verify correctly
  ASSERT_TRUE(verRctSemanticsSimple(sig));

  // Corrupt a pseudoOut
  if (!sig.pseudoOuts.empty())
  {
    sig.pseudoOuts[0] = skGen();
    ASSERT_FALSE(verRctSemanticsSimple(sig));
  }
}

// ============================================================================
// Borromean signature tests
// ============================================================================

TEST(ringct, borromean_all_zero_indices)
{
    // All indices are 0
    key64 xv, P1v, P2v;
    bits indi;

    for (int j = 0; j < 64; j++) {
        indi[j] = 0;
        xv[j] = skGen();
        scalarmultBase(P1v[j], xv[j]);
        subKeys(P2v[j], P1v[j], H2[j]);
    }

    boroSig bb = genBorromean(xv, P1v, P2v, indi);
    ASSERT_TRUE(verifyBorromean(bb, P1v, P2v));
}

TEST(ringct, borromean_all_one_indices)
{
    // All indices are 1
    key64 xv, P1v, P2v;
    bits indi;

    for (int j = 0; j < 64; j++) {
        indi[j] = 1;
        xv[j] = skGen();
        addKeys1(P1v[j], xv[j], H2[j]);
        subKeys(P2v[j], P1v[j], H2[j]);
    }

    boroSig bb = genBorromean(xv, P1v, P2v, indi);
    ASSERT_TRUE(verifyBorromean(bb, P1v, P2v));
}

TEST(ringct, borromean_tampered_s0)
{
    key64 xv, P1v, P2v;
    bits indi;

    for (int j = 0; j < 64; j++) {
        indi[j] = (int)randXmrAmount(2);
        xv[j] = skGen();
        if ((int)indi[j] == 0) {
            scalarmultBase(P1v[j], xv[j]);
        } else {
            addKeys1(P1v[j], xv[j], H2[j]);
        }
        subKeys(P2v[j], P1v[j], H2[j]);
    }

    boroSig bb = genBorromean(xv, P1v, P2v, indi);
    ASSERT_TRUE(verifyBorromean(bb, P1v, P2v));

    // Tamper with s0[0]
    bb.s0[0] = skGen();
    ASSERT_FALSE(verifyBorromean(bb, P1v, P2v));
}

TEST(ringct, borromean_tampered_s1)
{
    key64 xv, P1v, P2v;
    bits indi;

    for (int j = 0; j < 64; j++) {
        indi[j] = (int)randXmrAmount(2);
        xv[j] = skGen();
        if ((int)indi[j] == 0) {
            scalarmultBase(P1v[j], xv[j]);
        } else {
            addKeys1(P1v[j], xv[j], H2[j]);
        }
        subKeys(P2v[j], P1v[j], H2[j]);
    }

    boroSig bb = genBorromean(xv, P1v, P2v, indi);
    ASSERT_TRUE(verifyBorromean(bb, P1v, P2v));

    // Tamper with s1[10]
    bb.s1[10] = skGen();
    ASSERT_FALSE(verifyBorromean(bb, P1v, P2v));
}

TEST(ringct, borromean_tampered_ee)
{
    key64 xv, P1v, P2v;
    bits indi;

    for (int j = 0; j < 64; j++) {
        indi[j] = (int)randXmrAmount(2);
        xv[j] = skGen();
        if ((int)indi[j] == 0) {
            scalarmultBase(P1v[j], xv[j]);
        } else {
            addKeys1(P1v[j], xv[j], H2[j]);
        }
        subKeys(P2v[j], P1v[j], H2[j]);
    }

    boroSig bb = genBorromean(xv, P1v, P2v, indi);
    ASSERT_TRUE(verifyBorromean(bb, P1v, P2v));

    // Tamper with ee
    bb.ee = skGen();
    ASSERT_FALSE(verifyBorromean(bb, P1v, P2v));
}

// ============================================================================
// proveRange / verRange tests
// ============================================================================

TEST(ringct, proveRange_verRange_basic)
{
    key C, mask;
    rangeSig rs = proveRange(C, mask, 12345);
    ASSERT_TRUE(verRange(C, rs));
}

TEST(ringct, proveRange_verRange_zero)
{
    key C, mask;
    rangeSig rs = proveRange(C, mask, 0);
    ASSERT_TRUE(verRange(C, rs));
}

TEST(ringct, proveRange_verRange_large)
{
    key C, mask;
    rangeSig rs = proveRange(C, mask, 0xFFFFFFFFFFFFFFFFULL);
    ASSERT_TRUE(verRange(C, rs));
}

TEST(ringct, proveRange_verRange_tampered_commitment)
{
    key C, mask;
    rangeSig rs = proveRange(C, mask, 1000);
    ASSERT_TRUE(verRange(C, rs));

    // Tamper with C
    key badC = scalarmultBase(skGen());
    ASSERT_FALSE(verRange(badC, rs));
}

TEST(ringct, proveRange_verRange_tampered_Ci)
{
    key C, mask;
    rangeSig rs = proveRange(C, mask, 500);
    ASSERT_TRUE(verRange(C, rs));

    // Tamper with a Ci element
    rs.Ci[0] = scalarmultBase(skGen());
    ASSERT_FALSE(verRange(C, rs));
}

// ============================================================================
// Bulletproof tests
// ============================================================================

TEST(ringct, bulletproof_prove_verify_single)
{
    std::vector<uint64_t> amounts = {1000};
    keyV masks;
    masks.push_back(skGen());

    Bulletproof proof = bulletproof_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_prove_verify_multiple)
{
    std::vector<uint64_t> amounts = {1000, 2000};
    keyV masks;
    masks.push_back(skGen());
    masks.push_back(skGen());

    Bulletproof proof = bulletproof_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_batch_verify)
{
    std::vector<uint64_t> amounts1 = {500};
    keyV masks1;
    masks1.push_back(skGen());

    std::vector<uint64_t> amounts2 = {999};
    keyV masks2;
    masks2.push_back(skGen());

    Bulletproof p1 = bulletproof_PROVE(amounts1, masks1);
    Bulletproof p2 = bulletproof_PROVE(amounts2, masks2);

    std::vector<const Bulletproof*> proofs;
    proofs.push_back(&p1);
    proofs.push_back(&p2);
    ASSERT_TRUE(bulletproof_VERIFY(proofs));
}

TEST(ringct, bulletproof_tampered_fails)
{
    std::vector<uint64_t> amounts = {42};
    keyV masks;
    masks.push_back(skGen());

    Bulletproof proof = bulletproof_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    // Tamper with A
    proof.A = scalarmultBase(skGen());
    ASSERT_FALSE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_batch_with_tampered_fails)
{
    std::vector<uint64_t> amounts1 = {100};
    keyV masks1;
    masks1.push_back(skGen());

    std::vector<uint64_t> amounts2 = {200};
    keyV masks2;
    masks2.push_back(skGen());

    Bulletproof p1 = bulletproof_PROVE(amounts1, masks1);
    Bulletproof p2 = bulletproof_PROVE(amounts2, masks2);

    ASSERT_TRUE(bulletproof_VERIFY(p1));
    ASSERT_TRUE(bulletproof_VERIFY(p2));

    // Tamper with proof 2
    p2.S = scalarmultBase(skGen());

    std::vector<const Bulletproof*> proofs;
    proofs.push_back(&p1);
    proofs.push_back(&p2);
    ASSERT_FALSE(bulletproof_VERIFY(proofs));
}

TEST(ringct, bulletproof_prove_zero_amount)
{
    std::vector<uint64_t> amounts = {0};
    keyV masks;
    masks.push_back(skGen());

    Bulletproof proof = bulletproof_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_VERIFY(proof));
}

// ============================================================================
// BulletproofPlus tests
// ============================================================================

TEST(ringct, bulletproof_plus_prove_verify_single)
{
    std::vector<uint64_t> amounts = {7777};
    keyV masks;
    masks.push_back(skGen());

    BulletproofPlus proof = bulletproof_plus_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_prove_verify_multiple)
{
    std::vector<uint64_t> amounts = {1000, 2000, 3000};
    keyV masks;
    for (size_t i = 0; i < amounts.size(); ++i)
        masks.push_back(skGen());

    BulletproofPlus proof = bulletproof_plus_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_batch_verify)
{
    std::vector<uint64_t> amounts1 = {111};
    keyV masks1;
    masks1.push_back(skGen());

    std::vector<uint64_t> amounts2 = {222};
    keyV masks2;
    masks2.push_back(skGen());

    BulletproofPlus p1 = bulletproof_plus_PROVE(amounts1, masks1);
    BulletproofPlus p2 = bulletproof_plus_PROVE(amounts2, masks2);

    std::vector<const BulletproofPlus*> proofs;
    proofs.push_back(&p1);
    proofs.push_back(&p2);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proofs));
}

TEST(ringct, bulletproof_plus_tampered_fails)
{
    std::vector<uint64_t> amounts = {50000};
    keyV masks;
    masks.push_back(skGen());

    BulletproofPlus proof = bulletproof_plus_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));

    // Tamper with A
    proof.A = scalarmultBase(skGen());
    ASSERT_FALSE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_batch_with_tampered_fails)
{
    std::vector<uint64_t> amounts1 = {300};
    keyV masks1;
    masks1.push_back(skGen());

    std::vector<uint64_t> amounts2 = {400};
    keyV masks2;
    masks2.push_back(skGen());

    BulletproofPlus p1 = bulletproof_plus_PROVE(amounts1, masks1);
    BulletproofPlus p2 = bulletproof_plus_PROVE(amounts2, masks2);

    // Tamper with proof 1
    p1.B = scalarmultBase(skGen());

    std::vector<const BulletproofPlus*> proofs;
    proofs.push_back(&p1);
    proofs.push_back(&p2);
    ASSERT_FALSE(bulletproof_plus_VERIFY(proofs));
}

TEST(ringct, bulletproof_plus_prove_zero_amount)
{
    std::vector<uint64_t> amounts = {0};
    keyV masks;
    masks.push_back(skGen());

    BulletproofPlus proof = bulletproof_plus_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
}

// ============================================================================
// CLSAG tests - using CLSAG_Gen directly
// ============================================================================

TEST(ringct, CLSAG_Gen_with_device)
{
    // Test CLSAG_Gen(message, P, p, C, z, C_nonzero, C_offset, l, hwdev)
    const size_t N = 8;
    const size_t idx = 3;

    keyV P(N), C(N), C_nonzero(N);
    key p, z;

    // Generate random ring members
    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        skpkGen(sk, P[i]);
        key csk;
        skpkGen(csk, C_nonzero[i]);
    }

    // Set signing key
    skpkGen(p, P[idx]);

    // Set commitment keys
    z = skGen();
    key u = skGen();
    addKeys2(C_nonzero[idx], z, u, H);

    // C_offset
    key C_offset;
    key z2 = skGen();
    addKeys2(C_offset, z2, u, H);

    // Compute C = C_nonzero - C_offset for each ring member
    for (size_t i = 0; i < N; ++i)
        subKeys(C[i], C_nonzero[i], C_offset);

    // z for signing: the secret commitment key difference
    key z_sign;
    sc_sub(z_sign.bytes, z.bytes, z2.bytes);

    key message = skGen();

    clsag sig = CLSAG_Gen(message, P, p, C, z_sign, C_nonzero, C_offset, idx, hw::get_device("default"));

    // Verify via proveRctCLSAGSimple/verRctCLSAGSimple by constructing pubs
    // Since there's no CLSAG_Ver exported, we verify by re-creating the structure
    // and using verRctCLSAGSimple
    ctkeyV pubs(N);
    for (size_t i = 0; i < N; ++i)
    {
        pubs[i].dest = P[i];
        pubs[i].mask = C_nonzero[i];
    }
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, C_offset));
}

TEST(ringct, CLSAG_Gen_without_device)
{
    // Test CLSAG_Gen(message, P, p, C, z, C_nonzero, C_offset, l) overload without device
    const size_t N = 5;
    const size_t idx = 2;

    keyV P(N), C(N), C_nonzero(N);
    key p, z;

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        skpkGen(sk, P[i]);
        key csk;
        skpkGen(csk, C_nonzero[i]);
    }

    skpkGen(p, P[idx]);

    z = skGen();
    key u = skGen();
    addKeys2(C_nonzero[idx], z, u, H);

    key C_offset;
    key z2 = skGen();
    addKeys2(C_offset, z2, u, H);

    for (size_t i = 0; i < N; ++i)
        subKeys(C[i], C_nonzero[i], C_offset);

    key z_sign;
    sc_sub(z_sign.bytes, z.bytes, z2.bytes);

    key message = skGen();

    // Use the overload that does NOT take a device parameter
    clsag sig = CLSAG_Gen(message, P, p, C, z_sign, C_nonzero, C_offset, idx);

    ctkeyV pubs(N);
    for (size_t i = 0; i < N; ++i)
    {
        pubs[i].dest = P[i];
        pubs[i].mask = C_nonzero[i];
    }
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, C_offset));
}

TEST(ringct, CLSAG_Gen_ring_size_2)
{
    // Minimum meaningful ring size = 2
    const size_t N = 2;
    const size_t idx = 0;

    keyV P(N), C(N), C_nonzero(N);
    key p;

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        skpkGen(sk, P[i]);
        key csk;
        skpkGen(csk, C_nonzero[i]);
    }

    skpkGen(p, P[idx]);

    key z = skGen();
    key u = skGen();
    addKeys2(C_nonzero[idx], z, u, H);

    key C_offset;
    key z2 = skGen();
    addKeys2(C_offset, z2, u, H);

    for (size_t i = 0; i < N; ++i)
        subKeys(C[i], C_nonzero[i], C_offset);

    key z_sign;
    sc_sub(z_sign.bytes, z.bytes, z2.bytes);

    key message = skGen();
    clsag sig = CLSAG_Gen(message, P, p, C, z_sign, C_nonzero, C_offset, idx);

    ctkeyV pubs(N);
    for (size_t i = 0; i < N; ++i)
    {
        pubs[i].dest = P[i];
        pubs[i].mask = C_nonzero[i];
    }
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, C_offset));
}

TEST(ringct, CLSAG_Gen_tampered_message_fails)
{
    const size_t N = 4;
    const size_t idx = 1;

    keyV P(N), C(N), C_nonzero(N);
    key p;

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        skpkGen(sk, P[i]);
        key csk;
        skpkGen(csk, C_nonzero[i]);
    }

    skpkGen(p, P[idx]);

    key z = skGen();
    key u = skGen();
    addKeys2(C_nonzero[idx], z, u, H);

    key C_offset;
    key z2 = skGen();
    addKeys2(C_offset, z2, u, H);

    for (size_t i = 0; i < N; ++i)
        subKeys(C[i], C_nonzero[i], C_offset);

    key z_sign;
    sc_sub(z_sign.bytes, z.bytes, z2.bytes);

    key message = skGen();
    clsag sig = CLSAG_Gen(message, P, p, C, z_sign, C_nonzero, C_offset, idx);

    ctkeyV pubs(N);
    for (size_t i = 0; i < N; ++i)
    {
        pubs[i].dest = P[i];
        pubs[i].mask = C_nonzero[i];
    }
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, C_offset));

    // Verify with wrong message
    key wrong_message = skGen();
    ASSERT_FALSE(verRctCLSAGSimple(wrong_message, sig, pubs, C_offset));
}

TEST(ringct, CLSAG_Gen_last_index)
{
    // Test with signing index at the last position
    const size_t N = 6;
    const size_t idx = N - 1;

    keyV P(N), C(N), C_nonzero(N);
    key p;

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        skpkGen(sk, P[i]);
        key csk;
        skpkGen(csk, C_nonzero[i]);
    }

    skpkGen(p, P[idx]);

    key z = skGen();
    key u = skGen();
    addKeys2(C_nonzero[idx], z, u, H);

    key C_offset;
    key z2 = skGen();
    addKeys2(C_offset, z2, u, H);

    for (size_t i = 0; i < N; ++i)
        subKeys(C[i], C_nonzero[i], C_offset);

    key z_sign;
    sc_sub(z_sign.bytes, z.bytes, z2.bytes);

    key message = skGen();
    clsag sig = CLSAG_Gen(message, P, p, C, z_sign, C_nonzero, C_offset, idx);

    ctkeyV pubs(N);
    for (size_t i = 0; i < N; ++i)
    {
        pubs[i].dest = P[i];
        pubs[i].mask = C_nonzero[i];
    }
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, C_offset));
}

// ============================================================================
// MLSAG tests
// ============================================================================

TEST(ringct, MLSAG_Gen_Ver_dsRows_less_than_rows)
{
    // Test with dsRows < rows (non-linkable rows)
    const int N = 3; // cols
    const int R = 3; // rows
    const int dsRows = 1; // only 1 linkable row
    keyM P = keyMInit(R, N);
    keyV sk(R);

    int ind = 1;
    for (int j = 0; j < R; j++) {
        for (int i = 0; i < N; i++) {
            key x = skGen();
            P[i][j] = scalarmultBase(x);
            if (i == ind)
                sk[j] = x;
        }
    }

    key message = skGen();
    mgSig sig = MLSAG_Gen(message, P, sk, ind, dsRows, hw::get_device("default"));
    ASSERT_TRUE(MLSAG_Ver(message, P, sig, dsRows));

    // Bad message should fail
    key bad_message = skGen();
    ASSERT_FALSE(MLSAG_Ver(bad_message, P, sig, dsRows));
}

TEST(ringct, MLSAG_Ver_bad_signature_scalar_fails)
{
    const int N = 3;
    const int R = 2;
    keyM P = keyMInit(R, N);
    keyV sk(R);

    int ind = 0;
    for (int j = 0; j < R; j++) {
        for (int i = 0; i < N; i++) {
            key x = skGen();
            P[i][j] = scalarmultBase(x);
            if (i == ind)
                sk[j] = x;
        }
    }

    key message = skGen();
    mgSig sig = MLSAG_Gen(message, P, sk, ind, R, hw::get_device("default"));
    ASSERT_TRUE(MLSAG_Ver(message, P, sig, R));

    // Tamper with ss
    key backup = sig.ss[0][0];
    sig.ss[0][0] = skGen();
    ASSERT_FALSE(MLSAG_Ver(message, P, sig, R));
    sig.ss[0][0] = backup;

    // Tamper with cc
    key cc_backup = sig.cc;
    sig.cc = skGen();
    ASSERT_FALSE(MLSAG_Ver(message, P, sig, R));
    sig.cc = cc_backup;

    // Still valid with restored values
    ASSERT_TRUE(MLSAG_Ver(message, P, sig, R));
}

// ============================================================================
// proveRctMGSimple / verRctMGSimple tests
// ============================================================================

TEST(ringct, proveRctMGSimple_verRctMGSimple)
{
    const size_t ring_size = 4;
    const size_t idx = 2;

    ctkey inSk;
    inSk.dest = skGen();
    inSk.mask = skGen();

    ctkeyV pubs(ring_size);
    for (size_t i = 0; i < ring_size; ++i) {
        key sk;
        skpkGen(sk, pubs[i].dest);
        pubs[i].mask = scalarmultBase(skGen());
    }
    // Set the real signing key at idx
    pubs[idx].dest = scalarmultBase(inSk.dest);
    pubs[idx].mask = scalarmultBase(inSk.mask); // C[idx] = mask * G (no H component for simplicity - amounts are 0)

    key a = skGen(); // output mask
    key Cout;
    scalarmultBase(Cout, a); // Cout = a*G

    key message = skGen();
    mgSig sig = proveRctMGSimple(message, pubs, inSk, a, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctMGSimple(message, sig, pubs, Cout));

    // Wrong message fails
    ASSERT_FALSE(verRctMGSimple(skGen(), sig, pubs, Cout));
}

// ============================================================================
// proveRctCLSAGSimple / verRctCLSAGSimple comprehensive tests
// ============================================================================

TEST(ringct, proveRctCLSAGSimple_verRctCLSAGSimple_basic)
{
    const size_t N = 7;
    const size_t idx = 3;
    ctkeyV pubs;
    key p, t, t2, u;
    const key message = skGen();

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        ctkey tmp;
        skpkGen(sk, tmp.dest);
        skpkGen(sk, tmp.mask);
        pubs.push_back(tmp);
    }

    skpkGen(p, pubs[idx].dest);

    t = skGen();
    u = skGen();
    addKeys2(pubs[idx].mask, t, u, H);

    key Cout;
    t2 = skGen();
    addKeys2(Cout, t2, u, H);

    ctkey insk;
    insk.dest = p;
    insk.mask = t;

    clsag sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));
}

TEST(ringct, verRctCLSAGSimple_empty_pubs_fails)
{
    ctkeyV empty_pubs;
    clsag sig;
    sig.s.clear();
    sig.c1 = skGen();
    sig.I = scalarmultBase(skGen());
    sig.D = scalarmultBase(skGen());
    key message = skGen();
    key Cout = scalarmultBase(skGen());

    ASSERT_FALSE(verRctCLSAGSimple(message, sig, empty_pubs, Cout));
}

// ============================================================================
// genRct / verRct full RCT tests
// ============================================================================

TEST(ringct, genRct_verRct_single_input)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;

    tie(sctmp, pctmp) = ctskpkGen(5000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    vector<xmr_amount> amounts;
    keyV amount_keys;
    keyV destinations;
    key Sk, Pk;

    amounts.push_back(3000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    amounts.push_back(2000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRct(zero(), sc, pc, destinations, amounts, amount_keys, 3, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRct(s, true));   // semantics
    ASSERT_TRUE(verRct(s, false));  // non-semantics
    ASSERT_TRUE(verRct(s));         // both
}

TEST(ringct, genRct_verRct_with_fee)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;

    tie(sctmp, pctmp) = ctskpkGen(5000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    vector<xmr_amount> amounts;
    keyV amount_keys;
    keyV destinations;
    key Sk, Pk;

    // Output 1: 4000
    amounts.push_back(4000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    // Fee: 1000 (as extra amount element)
    amounts.push_back(1000);

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRct(zero(), sc, pc, destinations, amounts, amount_keys, 3, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRct(s));
    ASSERT_EQ(s.txnFee, 1000ULL);
}

TEST(ringct, verRct_wrong_type_fails)
{
    const uint64_t inputs[] = {2000};
    const uint64_t outputs[] = {1000, 1000};
    rctSig s = make_sample_rct_sig(NELTS(inputs), inputs, NELTS(outputs), outputs, true);
    ASSERT_TRUE(verRct(s));

    // Change type to simple -- should fail
    s.type = RCTTypeSimple;
    ASSERT_FALSE(verRct(s));
}

// ============================================================================
// genRctSimple with different RCT types (bulletproof, bulletproof2, CLSAG, BP+)
// ============================================================================

static rctSig make_simple_sig_with_config(const RCTConfig &rct_config)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(3000);
    tie(sctmp, pctmp) = ctskpkGen(3000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    inamounts.push_back(4000);
    tie(sctmp, pctmp) = ctskpkGen(4000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(5000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    outamounts.push_back(1500);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 500;

    return genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 3, rct_config, hw::get_device("default"));
}

TEST(ringct, genRctSimple_Borromean_full_verify)
{
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = make_simple_sig_with_config(rct_config);

    ASSERT_TRUE(verRctSemanticsSimple(s));
    ASSERT_TRUE(verRctNonSemanticsSimple(s));
    ASSERT_TRUE(verRctSimple(s));
    ASSERT_EQ(s.type, RCTTypeSimple);
}

TEST(ringct, genRctSimple_BulletproofPlus_full_verify)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 4};
    rctSig s = make_simple_sig_with_config(rct_config);

    ASSERT_EQ(s.type, RCTTypeBulletproofPlus);
    ASSERT_TRUE(verRctSemanticsSimple(s));
    ASSERT_TRUE(verRctNonSemanticsSimple(s));
    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_CLSAG_full_verify)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 3};
    rctSig s = make_simple_sig_with_config(rct_config);

    ASSERT_EQ(s.type, RCTTypeCLSAG);
    ASSERT_TRUE(verRctSemanticsSimple(s));
    ASSERT_TRUE(verRctNonSemanticsSimple(s));
    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_Bulletproof2_full_verify)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 2};
    rctSig s = make_simple_sig_with_config(rct_config);

    ASSERT_EQ(s.type, RCTTypeBulletproof2);
    ASSERT_TRUE(verRctSemanticsSimple(s));
    ASSERT_TRUE(verRctNonSemanticsSimple(s));
    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_Bulletproof1_full_verify)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 1};
    rctSig s = make_simple_sig_with_config(rct_config);

    ASSERT_EQ(s.type, RCTTypeBulletproof);
    ASSERT_TRUE(verRctSemanticsSimple(s));
    ASSERT_TRUE(verRctNonSemanticsSimple(s));
    ASSERT_TRUE(verRctSimple(s));
}

// ============================================================================
// verRctSemanticsSimple - batch verification
// ============================================================================

TEST(ringct, verRctSemanticsSimple_batch_mixed_types)
{
    const RCTConfig bp_config{RangeProofPaddedBulletproof, 4}; // BP+
    const RCTConfig borromean_config{RangeProofBorromean, 0};

    rctSig s1 = make_simple_sig_with_config(bp_config);
    rctSig s2 = make_simple_sig_with_config(borromean_config);

    // Individually verify
    ASSERT_TRUE(verRctSemanticsSimple(s1));
    ASSERT_TRUE(verRctSemanticsSimple(s2));

    // Batch verify
    std::vector<const rctSig*> sigs;
    sigs.push_back(&s1);
    sigs.push_back(&s2);
    ASSERT_TRUE(verRctSemanticsSimple(sigs));
}

TEST(ringct, verRctSemanticsSimple_batch_all_bp_plus)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 4};

    rctSig s1 = make_simple_sig_with_config(rct_config);
    rctSig s2 = make_simple_sig_with_config(rct_config);
    rctSig s3 = make_simple_sig_with_config(rct_config);

    std::vector<const rctSig*> sigs;
    sigs.push_back(&s1);
    sigs.push_back(&s2);
    sigs.push_back(&s3);
    ASSERT_TRUE(verRctSemanticsSimple(sigs));
}

TEST(ringct, verRctSemanticsSimple_batch_with_corrupted_fails)
{
    // Use bulletproof type (not Borromean) to avoid triggering a known issue
    // where verRctSemanticsSimple returns early without waiter.wait() when
    // Borromean range proofs have been submitted to the thread pool.
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 4};

    rctSig s1 = make_simple_sig_with_config(rct_config);
    rctSig s2 = make_simple_sig_with_config(rct_config);

    ASSERT_TRUE(verRctSemanticsSimple(s1));
    ASSERT_TRUE(verRctSemanticsSimple(s2));

    // Corrupt s2's pseudoOuts
    if (!s2.p.pseudoOuts.empty())
        s2.p.pseudoOuts[0] = scalarmultBase(skGen());

    std::vector<const rctSig*> sigs;
    sigs.push_back(&s1);
    sigs.push_back(&s2);
    ASSERT_FALSE(verRctSemanticsSimple(sigs));
}

// ============================================================================
// verRctNonSemanticsSimple tests
// ============================================================================

TEST(ringct, verRctNonSemanticsSimple_wrong_type_fails)
{
    rctSig s;
    s.type = RCTTypeFull;
    ASSERT_FALSE(verRctNonSemanticsSimple(s));
}

TEST(ringct, verRctSemanticsSimple_wrong_type_fails)
{
    rctSig s;
    s.type = RCTTypeFull;
    ASSERT_FALSE(verRctSemanticsSimple(s));
}

// ============================================================================
// decodeRct tests
// ============================================================================

TEST(ringct, decodeRct_full)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;

    tie(sctmp, pctmp) = ctskpkGen(7000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    vector<xmr_amount> amounts;
    keyV amount_keys;
    keyV destinations;
    key Sk, Pk;

    amounts.push_back(3000);
    key ak0 = hash_to_scalar(zero());
    amount_keys.push_back(ak0);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    amounts.push_back(4000);
    key ak1 = hash_to_scalar(identity());
    amount_keys.push_back(ak1);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRct(zero(), sc, pc, destinations, amounts, amount_keys, 3, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRct(s));

    // Decode output 0
    key mask0;
    xmr_amount decoded0 = decodeRct(s, ak0, 0, mask0, hw::get_device("default"));
    ASSERT_EQ(decoded0, 3000ULL);

    // Decode output 1
    key mask1;
    xmr_amount decoded1 = decodeRct(s, ak1, 1, mask1, hw::get_device("default"));
    ASSERT_EQ(decoded1, 4000ULL);

    // Decode without mask parameter
    xmr_amount decoded0_nomask = decodeRct(s, ak0, 0, hw::get_device("default"));
    ASSERT_EQ(decoded0_nomask, 3000ULL);
}

TEST(ringct, decodeRct_wrong_key_throws)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;

    tie(sctmp, pctmp) = ctskpkGen(1000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    vector<xmr_amount> amounts;
    keyV amount_keys;
    keyV destinations;
    key Sk, Pk;

    amounts.push_back(1000);
    key ak = hash_to_scalar(zero());
    amount_keys.push_back(ak);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRct(zero(), sc, pc, destinations, amounts, amount_keys, 3, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRct(s));

    // Using wrong key should throw
    key wrong_key = skGen();
    key mask;
    ASSERT_ANY_THROW(decodeRct(s, wrong_key, 0, mask, hw::get_device("default")));
}

// ============================================================================
// decodeRctSimple tests
// ============================================================================

TEST(ringct, decodeRctSimple_basic)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(5000);
    tie(sctmp, pctmp) = ctskpkGen(5000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(2000);
    key ak0 = hash_to_scalar(zero());
    amount_keys.push_back(ak0);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    outamounts.push_back(2500);
    key ak1 = hash_to_scalar(identity());
    amount_keys.push_back(ak1);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, 500, 3, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));

    // Decode output 0
    key mask0;
    xmr_amount decoded0 = decodeRctSimple(s, ak0, 0, mask0, hw::get_device("default"));
    ASSERT_EQ(decoded0, 2000ULL);

    // Decode output 1
    key mask1;
    xmr_amount decoded1 = decodeRctSimple(s, ak1, 1, mask1, hw::get_device("default"));
    ASSERT_EQ(decoded1, 2500ULL);

    // Decode without mask parameter
    xmr_amount decoded0_nomask = decodeRctSimple(s, ak0, 0, hw::get_device("default"));
    ASSERT_EQ(decoded0_nomask, 2000ULL);
}

TEST(ringct, decodeRctSimple_wrong_key_throws)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(1000);
    tie(sctmp, pctmp) = ctskpkGen(1000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(1000);
    key ak = hash_to_scalar(zero());
    amount_keys.push_back(ak);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, 0, 3, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));

    key wrong_key = skGen();
    key mask;
    ASSERT_ANY_THROW(decodeRctSimple(s, wrong_key, 0, mask, hw::get_device("default")));
}

TEST(ringct, decodeRctSimple_bulletproof_plus)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(10000);
    tie(sctmp, pctmp) = ctskpkGen(10000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(6000);
    key ak0 = hash_to_scalar(zero());
    amount_keys.push_back(ak0);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    outamounts.push_back(3000);
    key ak1 = hash_to_scalar(identity());
    amount_keys.push_back(ak1);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofPaddedBulletproof, 4}; // BP+
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, 1000, 3, rct_config, hw::get_device("default"));

    ASSERT_EQ(s.type, RCTTypeBulletproofPlus);
    ASSERT_TRUE(verRctSimple(s));

    key mask0;
    xmr_amount decoded0 = decodeRctSimple(s, ak0, 0, mask0, hw::get_device("default"));
    ASSERT_EQ(decoded0, 6000ULL);

    key mask1;
    xmr_amount decoded1 = decodeRctSimple(s, ak1, 1, mask1, hw::get_device("default"));
    ASSERT_EQ(decoded1, 3000ULL);
}

// ============================================================================
// get_pre_mlsag_hash tests
// ============================================================================

TEST(ringct, get_pre_mlsag_hash_borromean)
{
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = make_simple_sig_with_config(rct_config);

    // Should not throw and return a non-zero key
    key hash = get_pre_mlsag_hash(s, hw::get_device("default"));
    ASSERT_NE(hash, zero());
}

TEST(ringct, get_pre_mlsag_hash_bulletproof_plus)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 4};
    rctSig s = make_simple_sig_with_config(rct_config);

    key hash = get_pre_mlsag_hash(s, hw::get_device("default"));
    ASSERT_NE(hash, zero());
}

TEST(ringct, get_pre_mlsag_hash_clsag)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 3};
    rctSig s = make_simple_sig_with_config(rct_config);

    key hash = get_pre_mlsag_hash(s, hw::get_device("default"));
    ASSERT_NE(hash, zero());
}

TEST(ringct, get_pre_mlsag_hash_deterministic)
{
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = make_simple_sig_with_config(rct_config);

    key hash1 = get_pre_mlsag_hash(s, hw::get_device("default"));
    key hash2 = get_pre_mlsag_hash(s, hw::get_device("default"));
    ASSERT_EQ(hash1, hash2);
}

// ============================================================================
// populateFromBlockchain / populateFromBlockchainSimple tests
// ============================================================================

TEST(ringct, populateFromBlockchain_basic)
{
    ctkeyV inPk;
    ctkey pk1;
    pk1.dest = scalarmultBase(skGen());
    pk1.mask = scalarmultBase(skGen());
    inPk.push_back(pk1);

    int mixin = 3;
    ctkeyM mixRing;
    xmr_amount index;
    tie(mixRing, index) = populateFromBlockchain(inPk, mixin);

    // Should have mixin+1 columns
    ASSERT_EQ(mixRing.size(), (size_t)(mixin + 1));

    // The inPk should be at the returned index
    ASSERT_EQ(mixRing[index][0].dest, pk1.dest);
    ASSERT_EQ(mixRing[index][0].mask, pk1.mask);

    // Index should be in valid range
    ASSERT_LT(index, (xmr_amount)(mixin + 1));
}

TEST(ringct, populateFromBlockchain_multiple_inputs)
{
    ctkeyV inPk;
    ctkey pk1, pk2;
    pk1.dest = scalarmultBase(skGen());
    pk1.mask = scalarmultBase(skGen());
    pk2.dest = scalarmultBase(skGen());
    pk2.mask = scalarmultBase(skGen());
    inPk.push_back(pk1);
    inPk.push_back(pk2);

    int mixin = 2;
    ctkeyM mixRing;
    xmr_amount index;
    tie(mixRing, index) = populateFromBlockchain(inPk, mixin);

    ASSERT_EQ(mixRing.size(), (size_t)(mixin + 1));
    ASSERT_LT(index, (xmr_amount)(mixin + 1));

    // Each column at index should match inPk
    ASSERT_EQ(mixRing[index][0].dest, pk1.dest);
    ASSERT_EQ(mixRing[index][0].mask, pk1.mask);
    ASSERT_EQ(mixRing[index][1].dest, pk2.dest);
    ASSERT_EQ(mixRing[index][1].mask, pk2.mask);
}

TEST(ringct, getKeyFromBlockchain_generates_valid_keys)
{
    ctkey a;
    getKeyFromBlockchain(a, 42);

    // Should generate non-zero keys
    ASSERT_NE(a.dest, zero());
    ASSERT_NE(a.mask, zero());
}

// ============================================================================
// genRctSimple with explicit mixRing/index (the other overload)
// ============================================================================

TEST(ringct, genRctSimple_explicit_mixring)
{
    ctkeyV inSk;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    // Input 1: 3000
    inamounts.push_back(3000);
    tie(sctmp, pctmp) = ctskpkGen(3000);
    inSk.push_back(sctmp);

    // Input 2: 2000
    inamounts.push_back(2000);
    tie(sctmp, pctmp) = ctskpkGen(2000);
    inSk.push_back(sctmp);

    // Build mix ring manually
    const int mixin = 2;
    ctkeyM mixRing(2); // 2 inputs
    std::vector<unsigned int> indices(2);

    // For input 0
    {
        ctkey sk0 = inSk[0];
        ctkey pk0;
        pk0.dest = scalarmultBase(sk0.dest);
        addKeys2(pk0.mask, sk0.mask, d2h(inamounts[0]), H);

        mixRing[0].resize(mixin + 1);
        for (int i = 0; i <= mixin; i++) {
            if (i == 1) {
                mixRing[0][i] = pk0;
                indices[0] = i;
            } else {
                mixRing[0][i].dest = scalarmultBase(skGen());
                mixRing[0][i].mask = scalarmultBase(skGen());
            }
        }
    }

    // For input 1
    {
        ctkey sk1 = inSk[1];
        ctkey pk1;
        pk1.dest = scalarmultBase(sk1.dest);
        addKeys2(pk1.mask, sk1.mask, d2h(inamounts[1]), H);

        mixRing[1].resize(mixin + 1);
        for (int i = 0; i <= mixin; i++) {
            if (i == 0) {
                mixRing[1][i] = pk1;
                indices[1] = i;
            } else {
                mixRing[1][i].dest = scalarmultBase(skGen());
                mixRing[1][i].mask = scalarmultBase(skGen());
            }
        }
    }

    // Outputs
    outamounts.push_back(4000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 1000;

    ctkeyV outSk;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), inSk, destinations, inamounts, outamounts, txnfee, mixRing, amount_keys, indices, outSk, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSemanticsSimple(s));
    ASSERT_TRUE(verRctNonSemanticsSimple(s));
}

// ============================================================================
// verRctMG tests
// ============================================================================

TEST(ringct, verRctMG_basic)
{
    // Build a full RCT sig and test verRctMG directly
    ctkeyV inSk, inPk;
    ctkey sctmp, pctmp;

    tie(sctmp, pctmp) = ctskpkGen(5000);
    inSk.push_back(sctmp);
    inPk.push_back(pctmp);

    vector<xmr_amount> amounts;
    keyV amount_keys;
    keyV destinations;
    key Sk, Pk;

    amounts.push_back(3000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    amounts.push_back(2000); // fee

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRct(zero(), inSk, inPk, destinations, amounts, amount_keys, 3, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRct(s));

    // Call verRctMG directly
    key txnFeeKey = scalarmultH(d2h(s.txnFee));
    key message = get_pre_mlsag_hash(s, hw::get_device("default"));
    ASSERT_TRUE(verRctMG(s.p.MGs[0], s.mixRing, s.outPk, txnFeeKey, message));

    // Wrong message should fail
    ASSERT_FALSE(verRctMG(s.p.MGs[0], s.mixRing, s.outPk, txnFeeKey, skGen()));
}

// ============================================================================
// genRctSimple tampering tests - covering more verification code paths
// ============================================================================

TEST(ringct, genRctSimple_tampered_outPk_fails_semantics)
{
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = make_simple_sig_with_config(rct_config);

    ASSERT_TRUE(verRctSemanticsSimple(s));

    // Tamper with an outPk mask
    s.outPk[0].mask = scalarmultBase(skGen());
    ASSERT_FALSE(verRctSemanticsSimple(s));
}

TEST(ringct, genRctSimple_tampered_ecdhInfo_fails_decode)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(5000);
    tie(sctmp, pctmp) = ctskpkGen(5000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(5000);
    key ak = hash_to_scalar(zero());
    amount_keys.push_back(ak);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, 0, 3, rct_config, hw::get_device("default"));

    // Tamper with ecdhInfo
    s.ecdhInfo[0].amount = skGen();

    key mask;
    ASSERT_ANY_THROW(decodeRctSimple(s, ak, 0, mask, hw::get_device("default")));
}

TEST(ringct, genRctSimple_BPPlus_tampered_bulletproof_fails)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 4};
    rctSig s = make_simple_sig_with_config(rct_config);

    ASSERT_TRUE(verRctSemanticsSimple(s));

    // Tamper with bulletproof_plus
    if (!s.p.bulletproofs_plus.empty())
    {
        s.p.bulletproofs_plus[0].A = scalarmultBase(skGen());
        ASSERT_FALSE(verRctSemanticsSimple(s));
    }
}

TEST(ringct, genRctSimple_BP_tampered_bulletproof_fails)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 1};
    rctSig s = make_simple_sig_with_config(rct_config);

    ASSERT_TRUE(verRctSemanticsSimple(s));

    // Tamper with bulletproof
    if (!s.p.bulletproofs.empty())
    {
        s.p.bulletproofs[0].A = scalarmultBase(skGen());
        ASSERT_FALSE(verRctSemanticsSimple(s));
    }
}

// ============================================================================
// verRctSemanticsSimple structural checks
// ============================================================================

TEST(ringct, verRctSemanticsSimple_mismatched_outPk_ecdhInfo)
{
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = make_simple_sig_with_config(rct_config);
    ASSERT_TRUE(verRctSemanticsSimple(s));

    // Add extra ecdhInfo
    s.ecdhInfo.push_back(s.ecdhInfo.back());
    ASSERT_FALSE(verRctSemanticsSimple(s));
}

TEST(ringct, verRctSemanticsSimple_mismatched_pseudoOuts_MGs)
{
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = make_simple_sig_with_config(rct_config);
    ASSERT_TRUE(verRctSemanticsSimple(s));

    // Remove a pseudoOut
    if (s.pseudoOuts.size() > 1)
    {
        s.pseudoOuts.pop_back();
        ASSERT_FALSE(verRctSemanticsSimple(s));
    }
}

TEST(ringct, verRctNonSemanticsSimple_mismatched_pseudoOuts_mixRing)
{
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = make_simple_sig_with_config(rct_config);
    ASSERT_TRUE(verRctNonSemanticsSimple(s));

    // Remove a mixRing element
    if (s.mixRing.size() > 1)
    {
        s.mixRing.pop_back();
        ASSERT_FALSE(verRctNonSemanticsSimple(s));
    }
}

// ============================================================================
// CLSAG tampering tests on a full RCT sig
// ============================================================================

TEST(ringct, genRctSimple_CLSAG_tampered_CLSAG_s_fails)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 3};
    rctSig s = make_simple_sig_with_config(rct_config);

    ASSERT_TRUE(verRctNonSemanticsSimple(s));

    // Tamper with CLSAG s values
    if (!s.p.CLSAGs.empty() && !s.p.CLSAGs[0].s.empty())
    {
        s.p.CLSAGs[0].s[0] = skGen();
        ASSERT_FALSE(verRctNonSemanticsSimple(s));
    }
}

TEST(ringct, genRctSimple_CLSAG_tampered_CLSAG_c1_fails)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 3};
    rctSig s = make_simple_sig_with_config(rct_config);

    ASSERT_TRUE(verRctNonSemanticsSimple(s));

    if (!s.p.CLSAGs.empty())
    {
        s.p.CLSAGs[0].c1 = skGen();
        ASSERT_FALSE(verRctNonSemanticsSimple(s));
    }
}

TEST(ringct, genRctSimple_CLSAG_tampered_CLSAG_I_fails)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 3};
    rctSig s = make_simple_sig_with_config(rct_config);

    ASSERT_TRUE(verRctNonSemanticsSimple(s));

    if (!s.p.CLSAGs.empty())
    {
        s.p.CLSAGs[0].I = scalarmultBase(skGen());
        ASSERT_FALSE(verRctNonSemanticsSimple(s));
    }
}

TEST(ringct, genRctSimple_CLSAG_tampered_CLSAG_D_fails)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 3};
    rctSig s = make_simple_sig_with_config(rct_config);

    ASSERT_TRUE(verRctNonSemanticsSimple(s));

    if (!s.p.CLSAGs.empty())
    {
        s.p.CLSAGs[0].D = scalarmultBase(skGen());
        ASSERT_FALSE(verRctNonSemanticsSimple(s));
    }
}

// ============================================================================
// decodeRct index out of range test
// ============================================================================

TEST(ringct, decodeRct_bad_index_throws)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;

    tie(sctmp, pctmp) = ctskpkGen(1000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    vector<xmr_amount> amounts;
    keyV amount_keys;
    keyV destinations;
    key Sk, Pk;

    amounts.push_back(1000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRct(zero(), sc, pc, destinations, amounts, amount_keys, 3, rct_config, hw::get_device("default"));

    key mask;
    // Index out of range
    ASSERT_ANY_THROW(decodeRct(s, hash_to_scalar(zero()), 99, mask, hw::get_device("default")));
}

TEST(ringct, decodeRctSimple_bad_index_throws)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(1000);
    tie(sctmp, pctmp) = ctskpkGen(1000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(1000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, 0, 3, rct_config, hw::get_device("default"));

    key mask;
    ASSERT_ANY_THROW(decodeRctSimple(s, hash_to_scalar(zero()), 99, mask, hw::get_device("default")));
}

// ============================================================================
// decodeRct on wrong type
// ============================================================================

TEST(ringct, decodeRct_on_simple_type_returns_false)
{
    const uint64_t inputs[] = {1000, 1000};
    const uint64_t outputs[] = {1000};
    rctSig s = make_sample_simple_rct_sig(NELTS(inputs), inputs, NELTS(outputs), outputs, 1000);

    // decodeRct expects RCTTypeFull; passing a simple sig should return false (0)
    key mask;
    xmr_amount result = decodeRct(s, hash_to_scalar(zero()), 0, mask, hw::get_device("default"));
    ASSERT_EQ(result, 0ULL);
}

TEST(ringct, decodeRctSimple_on_full_type_returns_false)
{
    const uint64_t inputs[] = {2000};
    const uint64_t outputs[] = {1000, 1000};
    rctSig s = make_sample_rct_sig(NELTS(inputs), inputs, NELTS(outputs), outputs, true);

    // decodeRctSimple expects simple types; passing full should return false (0)
    key mask;
    xmr_amount result = decodeRctSimple(s, hash_to_scalar(zero()), 0, mask, hw::get_device("default"));
    ASSERT_EQ(result, 0ULL);
}

// ============================================================================
// verRct structural checks
// ============================================================================

TEST(ringct, verRct_semantics_mismatched_sizes)
{
    const uint64_t inputs[] = {2000};
    const uint64_t outputs[] = {1000, 1000};
    rctSig s = make_sample_rct_sig(NELTS(inputs), inputs, NELTS(outputs), outputs, true);
    ASSERT_TRUE(verRct(s));

    // Mismatched outPk/rangeSigs
    s.p.rangeSigs.pop_back();
    ASSERT_FALSE(verRct(s, true));
}

TEST(ringct, verRct_semantics_multiple_MGs_fails)
{
    const uint64_t inputs[] = {2000};
    const uint64_t outputs[] = {1000, 1000};
    rctSig s = make_sample_rct_sig(NELTS(inputs), inputs, NELTS(outputs), outputs, true);
    ASSERT_TRUE(verRct(s));

    // Add extra MG
    s.p.MGs.push_back(s.p.MGs[0]);
    ASSERT_FALSE(verRct(s, true));
}

// ============================================================================
// genRctSimple with bp_version 0 defaults to BP+
// ============================================================================

TEST(ringct, genRctSimple_bp_version_0_is_bp_plus)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 0};
    rctSig s = make_simple_sig_with_config(rct_config);
    ASSERT_EQ(s.type, RCTTypeBulletproofPlus);
    ASSERT_TRUE(verRctSimple(s));
}

// ============================================================================
// Multiple inputs and outputs with Bulletproof Plus and decode
// ============================================================================

TEST(ringct, genRctSimple_BPPlus_multiple_io_decode)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    // 3 inputs
    for (xmr_amount amt : {10000ULL, 20000ULL, 30000ULL}) {
        inamounts.push_back(amt);
        tie(sctmp, pctmp) = ctskpkGen(amt);
        sc.push_back(sctmp);
        pc.push_back(pctmp);
    }

    // 4 outputs
    keyV aks;
    for (xmr_amount amt : {15000ULL, 10000ULL, 5000ULL, 25000ULL}) {
        outamounts.push_back(amt);
        key ak = skGen();
        aks.push_back(ak);
        amount_keys.push_back(ak);
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);
    }

    xmr_amount fee = 5000; // 60000 - 55000 = 5000

    const RCTConfig rct_config{RangeProofPaddedBulletproof, 4};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, fee, 3, rct_config, hw::get_device("default"));

    ASSERT_EQ(s.type, RCTTypeBulletproofPlus);
    ASSERT_TRUE(verRctSimple(s));

    // Decode all outputs
    for (size_t i = 0; i < outamounts.size(); ++i) {
        key mask;
        xmr_amount decoded = decodeRctSimple(s, aks[i], i, mask, hw::get_device("default"));
        ASSERT_EQ(decoded, outamounts[i]);
    }
}

// ============================================================================
// CLSAG structural validation in verRctCLSAGSimple
// ============================================================================

TEST(ringct, verRctCLSAGSimple_identity_key_image_fails)
{
    const size_t N = 5;
    const size_t idx = 2;
    ctkeyV pubs;
    key p, t, t2, u;
    const key message = skGen();

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        ctkey tmp;
        skpkGen(sk, tmp.dest);
        skpkGen(sk, tmp.mask);
        pubs.push_back(tmp);
    }

    skpkGen(p, pubs[idx].dest);
    t = skGen();
    u = skGen();
    addKeys2(pubs[idx].mask, t, u, H);
    key Cout;
    t2 = skGen();
    addKeys2(Cout, t2, u, H);

    ctkey insk;
    insk.dest = p;
    insk.mask = t;

    clsag sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));

    // Set key image to identity - should fail
    sig.I = identity();
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
}

TEST(ringct, verRctCLSAGSimple_D_is_identity_after_scalarmult8_fails)
{
    const size_t N = 5;
    const size_t idx = 2;
    ctkeyV pubs;
    key p, t, t2, u;
    const key message = skGen();

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        ctkey tmp;
        skpkGen(sk, tmp.dest);
        skpkGen(sk, tmp.mask);
        pubs.push_back(tmp);
    }

    skpkGen(p, pubs[idx].dest);
    t = skGen();
    u = skGen();
    addKeys2(pubs[idx].mask, t, u, H);
    key Cout;
    t2 = skGen();
    addKeys2(Cout, t2, u, H);

    ctkey insk;
    insk.dest = p;
    insk.mask = t;

    clsag sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));

    // Set D to the zero point (identity) -- scalarmult8 of identity is identity
    sig.D = identity();
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
}

// ============================================================================
// Bulletproof with 4 outputs (padding test)
// ============================================================================

TEST(ringct, bulletproof_four_outputs)
{
    std::vector<uint64_t> amounts = {100, 200, 300, 400};
    keyV masks;
    for (size_t i = 0; i < amounts.size(); ++i)
        masks.push_back(skGen());

    Bulletproof proof = bulletproof_PROVE(amounts, masks);
    ASSERT_EQ(proof.V.size(), amounts.size());
    ASSERT_TRUE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_four_outputs)
{
    std::vector<uint64_t> amounts = {100, 200, 300, 400};
    keyV masks;
    for (size_t i = 0; i < amounts.size(); ++i)
        masks.push_back(skGen());

    BulletproofPlus proof = bulletproof_plus_PROVE(amounts, masks);
    ASSERT_EQ(proof.V.size(), amounts.size());
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
}

// ============================================================================
// genRctSimple unsupported bp_version throws
// ============================================================================

TEST(ringct, genRctSimple_unsupported_bp_version_throws)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(1000);
    tie(sctmp, pctmp) = ctskpkGen(1000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(1000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofPaddedBulletproof, 99}; // Invalid bp_version
    ASSERT_ANY_THROW(genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, 0, 3, rct_config, hw::get_device("default")));
}

// ============================================================================
// genRct with 2+ inputs should throw
// ============================================================================

TEST(ringct, genRct_multiple_inputs_throws)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;

    tie(sctmp, pctmp) = ctskpkGen(1000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    tie(sctmp, pctmp) = ctskpkGen(2000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    vector<xmr_amount> amounts = {3000};
    keyV amount_keys = {hash_to_scalar(zero())};
    keyV destinations;
    key Sk, Pk;
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofBorromean, 0};
    ASSERT_ANY_THROW(genRct(zero(), sc, pc, destinations, amounts, amount_keys, 3, rct_config, hw::get_device("default")));
}

// ============================================================================
// CLSAG full transaction with decode
// ============================================================================

TEST(ringct, full_CLSAG_transaction_with_decode)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;
    keyV aks;

    inamounts.push_back(10000);
    tie(sctmp, pctmp) = ctskpkGen(10000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    inamounts.push_back(5000);
    tie(sctmp, pctmp) = ctskpkGen(5000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    // Outputs
    for (xmr_amount amt : {7000ULL, 6000ULL}) {
        outamounts.push_back(amt);
        key ak = skGen();
        aks.push_back(ak);
        amount_keys.push_back(ak);
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);
    }

    xmr_amount fee = 2000;

    const RCTConfig rct_config{RangeProofPaddedBulletproof, 3}; // CLSAG
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, fee, 3, rct_config, hw::get_device("default"));

    ASSERT_EQ(s.type, RCTTypeCLSAG);
    ASSERT_TRUE(verRctSimple(s));
    ASSERT_EQ(s.txnFee, fee);

    // Decode
    for (size_t i = 0; i < outamounts.size(); ++i) {
        key mask;
        xmr_amount decoded = decodeRctSimple(s, aks[i], i, mask, hw::get_device("default"));
        ASSERT_EQ(decoded, outamounts[i]);
    }
}

// ============================================================================
// MLSAG additional tests
// ============================================================================

TEST(ringct, MLSAG_Gen_Ver_single_row_ring)
{
    // Ring with 2 columns and 1 row (minimal with single secret key)
    const int N = 2; // cols (ring size 2)
    const int R = 1; // rows (1 key per ring member)
    keyM P = keyMInit(R, N);
    keyV sk(R);

    int ind = 0;
    for (int i = 0; i < N; ++i) {
        key x = skGen();
        P[i][0] = scalarmultBase(x);
        if (i == ind)
            sk[0] = x;
    }

    key message = skGen();
    mgSig sig = MLSAG_Gen(message, P, sk, ind, R, hw::get_device("default"));
    ASSERT_TRUE(MLSAG_Ver(message, P, sig, R));

    // Tampered message fails
    ASSERT_FALSE(MLSAG_Ver(skGen(), P, sig, R));
}

TEST(ringct, MLSAG_Gen_single_col_throws)
{
    // Ring with only 1 column should throw
    const int N = 1;
    const int R = 1;
    keyM P = keyMInit(R, N);
    keyV sk(R);

    key x = skGen();
    P[0][0] = scalarmultBase(x);
    sk[0] = x;

    key message = skGen();
    ASSERT_ANY_THROW(MLSAG_Gen(message, P, sk, 0, R, hw::get_device("default")));
}

TEST(ringct, MLSAG_Gen_Ver_large_ring)
{
    // Ring with 16 columns and 2 rows
    const int N = 16;
    const int R = 2;
    keyM P = keyMInit(R, N);
    keyV sk(R);

    int ind = 7;
    for (int j = 0; j < R; j++) {
        for (int i = 0; i < N; i++) {
            key x = skGen();
            P[i][j] = scalarmultBase(x);
            if (i == ind)
                sk[j] = x;
        }
    }

    key message = skGen();
    mgSig sig = MLSAG_Gen(message, P, sk, ind, R, hw::get_device("default"));
    ASSERT_TRUE(MLSAG_Ver(message, P, sig, R));
}

TEST(ringct, MLSAG_Ver_wrong_public_keys_fails)
{
    const int N = 4;
    const int R = 2;
    keyM P = keyMInit(R, N);
    keyV sk(R);

    int ind = 1;
    for (int j = 0; j < R; j++) {
        for (int i = 0; i < N; i++) {
            key x = skGen();
            P[i][j] = scalarmultBase(x);
            if (i == ind)
                sk[j] = x;
        }
    }

    key message = skGen();
    mgSig sig = MLSAG_Gen(message, P, sk, ind, R, hw::get_device("default"));
    ASSERT_TRUE(MLSAG_Ver(message, P, sig, R));

    // Replace a public key in the ring (not at signing index)
    key backup = P[0][0];
    P[0][0] = scalarmultBase(skGen());
    ASSERT_FALSE(MLSAG_Ver(message, P, sig, R));
    P[0][0] = backup;

    // Replace the signing key's public key
    key backup2 = P[ind][0];
    P[ind][0] = scalarmultBase(skGen());
    ASSERT_FALSE(MLSAG_Ver(message, P, sig, R));
    P[ind][0] = backup2;

    // Still valid after restore
    ASSERT_TRUE(MLSAG_Ver(message, P, sig, R));
}

TEST(ringct, MLSAG_Ver_tampered_key_image_fails)
{
    const int N = 3;
    const int R = 2;
    keyM P = keyMInit(R, N);
    keyV sk(R);

    int ind = 2;
    for (int j = 0; j < R; j++) {
        for (int i = 0; i < N; i++) {
            key x = skGen();
            P[i][j] = scalarmultBase(x);
            if (i == ind)
                sk[j] = x;
        }
    }

    key message = skGen();
    mgSig sig = MLSAG_Gen(message, P, sk, ind, R, hw::get_device("default"));
    ASSERT_TRUE(MLSAG_Ver(message, P, sig, R));

    // Tamper with key image
    if (!sig.II.empty()) {
        key backup = sig.II[0];
        sig.II[0] = scalarmultBase(skGen());
        ASSERT_FALSE(MLSAG_Ver(message, P, sig, R));
        sig.II[0] = backup;
    }

    ASSERT_TRUE(MLSAG_Ver(message, P, sig, R));
}

TEST(ringct, MLSAG_Gen_Ver_dsRows_zero)
{
    // All non-linkable rows (dsRows = 0)
    const int N = 3;
    const int R = 2;
    const int dsRows = 0;
    keyM P = keyMInit(R, N);
    keyV sk(R);

    int ind = 1;
    for (int j = 0; j < R; j++) {
        for (int i = 0; i < N; i++) {
            key x = skGen();
            P[i][j] = scalarmultBase(x);
            if (i == ind)
                sk[j] = x;
        }
    }

    key message = skGen();
    mgSig sig = MLSAG_Gen(message, P, sk, ind, dsRows, hw::get_device("default"));
    ASSERT_TRUE(MLSAG_Ver(message, P, sig, dsRows));

    // No key images should be present for dsRows=0
    ASSERT_TRUE(sig.II.empty());

    // Wrong message fails
    ASSERT_FALSE(MLSAG_Ver(skGen(), P, sig, dsRows));
}

// ============================================================================
// CLSAG additional tests
// ============================================================================

TEST(ringct, CLSAG_Gen_tampered_pub_dest_fails)
{
    const size_t N = 6;
    const size_t idx = 2;

    keyV P(N), C(N), C_nonzero(N);
    key p;

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        skpkGen(sk, P[i]);
        key csk;
        skpkGen(csk, C_nonzero[i]);
    }

    skpkGen(p, P[idx]);

    key z = skGen();
    key u = skGen();
    addKeys2(C_nonzero[idx], z, u, H);

    key C_offset;
    key z2 = skGen();
    addKeys2(C_offset, z2, u, H);

    for (size_t i = 0; i < N; ++i)
        subKeys(C[i], C_nonzero[i], C_offset);

    key z_sign;
    sc_sub(z_sign.bytes, z.bytes, z2.bytes);

    key message = skGen();
    clsag sig = CLSAG_Gen(message, P, p, C, z_sign, C_nonzero, C_offset, idx);

    ctkeyV pubs(N);
    for (size_t i = 0; i < N; ++i)
    {
        pubs[i].dest = P[i];
        pubs[i].mask = C_nonzero[i];
    }
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, C_offset));

    // Tamper with a non-signing public key
    key backup = pubs[0].dest;
    pubs[0].dest = scalarmultBase(skGen());
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, C_offset));
    pubs[0].dest = backup;

    // Tamper with the signing key's commitment
    key mask_backup = pubs[idx].mask;
    pubs[idx].mask = scalarmultBase(skGen());
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, C_offset));
    pubs[idx].mask = mask_backup;

    // Still valid
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, C_offset));
}

TEST(ringct, CLSAG_Gen_tampered_C_offset_fails)
{
    const size_t N = 4;
    const size_t idx = 1;

    keyV P(N), C(N), C_nonzero(N);
    key p;

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        skpkGen(sk, P[i]);
        key csk;
        skpkGen(csk, C_nonzero[i]);
    }

    skpkGen(p, P[idx]);

    key z = skGen();
    key u = skGen();
    addKeys2(C_nonzero[idx], z, u, H);

    key C_offset;
    key z2 = skGen();
    addKeys2(C_offset, z2, u, H);

    for (size_t i = 0; i < N; ++i)
        subKeys(C[i], C_nonzero[i], C_offset);

    key z_sign;
    sc_sub(z_sign.bytes, z.bytes, z2.bytes);

    key message = skGen();
    clsag sig = CLSAG_Gen(message, P, p, C, z_sign, C_nonzero, C_offset, idx);

    ctkeyV pubs(N);
    for (size_t i = 0; i < N; ++i)
    {
        pubs[i].dest = P[i];
        pubs[i].mask = C_nonzero[i];
    }
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, C_offset));

    // Tamper with C_offset
    key bad_offset = scalarmultBase(skGen());
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, bad_offset));
}

TEST(ringct, CLSAG_Gen_ring_size_16)
{
    // Test with large ring size
    const size_t N = 16;
    const size_t idx = 10;

    keyV P(N), C(N), C_nonzero(N);
    key p;

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        skpkGen(sk, P[i]);
        key csk;
        skpkGen(csk, C_nonzero[i]);
    }

    skpkGen(p, P[idx]);

    key z = skGen();
    key u = skGen();
    addKeys2(C_nonzero[idx], z, u, H);

    key C_offset;
    key z2 = skGen();
    addKeys2(C_offset, z2, u, H);

    for (size_t i = 0; i < N; ++i)
        subKeys(C[i], C_nonzero[i], C_offset);

    key z_sign;
    sc_sub(z_sign.bytes, z.bytes, z2.bytes);

    key message = skGen();
    clsag sig = CLSAG_Gen(message, P, p, C, z_sign, C_nonzero, C_offset, idx);

    ctkeyV pubs(N);
    for (size_t i = 0; i < N; ++i)
    {
        pubs[i].dest = P[i];
        pubs[i].mask = C_nonzero[i];
    }
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, C_offset));
}

// ============================================================================
// proveRctMG / verRctMG tampering tests
// ============================================================================

TEST(ringct, verRctMG_tampered_sig_ss_fails)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;

    tie(sctmp, pctmp) = ctskpkGen(5000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    vector<xmr_amount> amounts;
    keyV amount_keys;
    keyV destinations;
    key Sk, Pk;

    amounts.push_back(3000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    amounts.push_back(2000);

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRct(zero(), sc, pc, destinations, amounts, amount_keys, 3, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRct(s));

    // Get the pre-mlsag hash and fee key for direct verRctMG call
    key txnFeeKey = scalarmultH(d2h(s.txnFee));
    key message = get_pre_mlsag_hash(s, hw::get_device("default"));

    // Tamper with ss values
    if (!s.p.MGs[0].ss.empty() && !s.p.MGs[0].ss[0].empty()) {
        key backup = s.p.MGs[0].ss[0][0];
        s.p.MGs[0].ss[0][0] = skGen();
        ASSERT_FALSE(verRctMG(s.p.MGs[0], s.mixRing, s.outPk, txnFeeKey, message));
        s.p.MGs[0].ss[0][0] = backup;
    }

    // Tamper with cc
    key cc_backup = s.p.MGs[0].cc;
    s.p.MGs[0].cc = skGen();
    ASSERT_FALSE(verRctMG(s.p.MGs[0], s.mixRing, s.outPk, txnFeeKey, message));
    s.p.MGs[0].cc = cc_backup;

    // Tamper with II (key image)
    if (!s.p.MGs[0].II.empty()) {
        key ii_backup = s.p.MGs[0].II[0];
        s.p.MGs[0].II[0] = scalarmultBase(skGen());
        ASSERT_FALSE(verRctMG(s.p.MGs[0], s.mixRing, s.outPk, txnFeeKey, message));
        s.p.MGs[0].II[0] = ii_backup;
    }

    // Still works
    ASSERT_TRUE(verRctMG(s.p.MGs[0], s.mixRing, s.outPk, txnFeeKey, message));
}

TEST(ringct, verRctMG_wrong_fee_fails)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;

    tie(sctmp, pctmp) = ctskpkGen(3000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    vector<xmr_amount> amounts;
    keyV amount_keys;
    keyV destinations;
    key Sk, Pk;

    amounts.push_back(2000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    amounts.push_back(1000); // fee

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRct(zero(), sc, pc, destinations, amounts, amount_keys, 3, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRct(s));

    key message = get_pre_mlsag_hash(s, hw::get_device("default"));

    // Correct fee key
    key correctFeeKey = scalarmultH(d2h(s.txnFee));
    ASSERT_TRUE(verRctMG(s.p.MGs[0], s.mixRing, s.outPk, correctFeeKey, message));

    // Wrong fee key
    key wrongFeeKey = scalarmultH(d2h(s.txnFee + 1));
    ASSERT_FALSE(verRctMG(s.p.MGs[0], s.mixRing, s.outPk, wrongFeeKey, message));
}

// ============================================================================
// proveRctMGSimple additional tests
// ============================================================================

TEST(ringct, proveRctMGSimple_tampered_Cout_fails)
{
    const size_t ring_size = 4;
    const size_t idx = 1;

    ctkey inSk;
    inSk.dest = skGen();
    inSk.mask = skGen();

    ctkeyV pubs(ring_size);
    for (size_t i = 0; i < ring_size; ++i) {
        key sk;
        skpkGen(sk, pubs[i].dest);
        pubs[i].mask = scalarmultBase(skGen());
    }
    pubs[idx].dest = scalarmultBase(inSk.dest);
    pubs[idx].mask = scalarmultBase(inSk.mask);

    key a = skGen();
    key Cout;
    scalarmultBase(Cout, a);

    key message = skGen();
    mgSig sig = proveRctMGSimple(message, pubs, inSk, a, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctMGSimple(message, sig, pubs, Cout));

    // Wrong Cout fails
    key badCout = scalarmultBase(skGen());
    ASSERT_FALSE(verRctMGSimple(message, sig, pubs, badCout));
}

TEST(ringct, proveRctMGSimple_tampered_pubs_fails)
{
    const size_t ring_size = 5;
    const size_t idx = 3;

    ctkey inSk;
    inSk.dest = skGen();
    inSk.mask = skGen();

    ctkeyV pubs(ring_size);
    for (size_t i = 0; i < ring_size; ++i) {
        key sk;
        skpkGen(sk, pubs[i].dest);
        pubs[i].mask = scalarmultBase(skGen());
    }
    pubs[idx].dest = scalarmultBase(inSk.dest);
    pubs[idx].mask = scalarmultBase(inSk.mask);

    key a = skGen();
    key Cout;
    scalarmultBase(Cout, a);

    key message = skGen();
    mgSig sig = proveRctMGSimple(message, pubs, inSk, a, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctMGSimple(message, sig, pubs, Cout));

    // Tamper with a non-signing pub key
    key backup = pubs[0].dest;
    pubs[0].dest = scalarmultBase(skGen());
    ASSERT_FALSE(verRctMGSimple(message, sig, pubs, Cout));
    pubs[0].dest = backup;

    ASSERT_TRUE(verRctMGSimple(message, sig, pubs, Cout));
}

// ============================================================================
// get_pre_mlsag_hash additional tests
// ============================================================================

TEST(ringct, get_pre_mlsag_hash_changes_with_different_fee)
{
    ctkeyV sc1, pc1, sc2, pc2;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    // Create two identical transactions except for the fee
    inamounts.push_back(5000);
    tie(sctmp, pctmp) = ctskpkGen(5000);
    sc1.push_back(sctmp);
    pc1.push_back(pctmp);
    sc2.push_back(sctmp);
    pc2.push_back(pctmp);

    outamounts.push_back(4000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofBorromean, 0};

    // Fee = 1000
    rctSig s1 = genRctSimple(zero(), sc1, pc1, destinations, inamounts, outamounts, amount_keys, 1000, 3, rct_config, hw::get_device("default"));

    // Fee = 500 (need to recreate with adjusted output)
    vector<xmr_amount> outamounts2 = {4500};
    rctSig s2 = genRctSimple(zero(), sc2, pc2, destinations, inamounts, outamounts2, amount_keys, 500, 3, rct_config, hw::get_device("default"));

    key hash1 = get_pre_mlsag_hash(s1, hw::get_device("default"));
    key hash2 = get_pre_mlsag_hash(s2, hw::get_device("default"));

    // Different fees should produce different hashes
    ASSERT_NE(hash1, hash2);
}

TEST(ringct, get_pre_mlsag_hash_different_rct_types_differ)
{
    // Borromean
    const RCTConfig config_boro{RangeProofBorromean, 0};
    rctSig s_boro = make_simple_sig_with_config(config_boro);

    // Bulletproof Plus
    const RCTConfig config_bp{RangeProofPaddedBulletproof, 4};
    rctSig s_bp = make_simple_sig_with_config(config_bp);

    key hash_boro = get_pre_mlsag_hash(s_boro, hw::get_device("default"));
    key hash_bp = get_pre_mlsag_hash(s_bp, hw::get_device("default"));

    // Different RCT types produce different range proofs => different hashes
    ASSERT_NE(hash_boro, hash_bp);
}

TEST(ringct, get_pre_mlsag_hash_different_messages_differ)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(1000);
    tie(sctmp, pctmp) = ctskpkGen(1000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(1000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofBorromean, 0};

    // Two sigs with different messages
    key msg1 = skGen();
    key msg2 = skGen();

    rctSig s1 = genRctSimple(msg1, sc, pc, destinations, inamounts, outamounts, amount_keys, 0, 3, rct_config, hw::get_device("default"));
    rctSig s2 = genRctSimple(msg2, sc, pc, destinations, inamounts, outamounts, amount_keys, 0, 3, rct_config, hw::get_device("default"));

    key hash1 = get_pre_mlsag_hash(s1, hw::get_device("default"));
    key hash2 = get_pre_mlsag_hash(s2, hw::get_device("default"));

    ASSERT_NE(hash1, hash2);
}

// ============================================================================
// genRct with explicit mixRing (first overload)
// ============================================================================

TEST(ringct, genRct_explicit_mixring)
{
    ctkeyV inSk;
    ctkey sctmp, pctmp;

    tie(sctmp, pctmp) = ctskpkGen(5000);
    inSk.push_back(sctmp);

    // Build mix ring manually
    const int mixin = 3;
    ctkeyM mixRing(mixin + 1);
    unsigned int index = 2;

    for (int i = 0; i <= mixin; ++i) {
        ctkey pk;
        if ((unsigned int)i == index) {
            pk.dest = scalarmultBase(sctmp.dest);
            addKeys2(pk.mask, sctmp.mask, d2h(5000ULL), H);
        } else {
            pk.dest = scalarmultBase(skGen());
            pk.mask = scalarmultBase(skGen());
        }
        mixRing[i].push_back(pk);
    }

    vector<xmr_amount> amounts;
    keyV amount_keys;
    keyV destinations;
    key Sk, Pk;

    amounts.push_back(3000);
    key ak = hash_to_scalar(zero());
    amount_keys.push_back(ak);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    amounts.push_back(2000); // fee

    ctkeyV outSk;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRct(zero(), inSk, destinations, amounts, mixRing, amount_keys, index, outSk, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRct(s));

    // Decode amount
    key mask;
    xmr_amount decoded = decodeRct(s, ak, 0, mask, hw::get_device("default"));
    ASSERT_EQ(decoded, 3000ULL);
}

// ============================================================================
// Borromean additional tests
// ============================================================================

TEST(ringct, borromean_alternating_indices)
{
    // Alternating bit pattern: 0, 1, 0, 1, ...
    key64 xv, P1v, P2v;
    bits indi;

    for (int j = 0; j < 64; j++) {
        indi[j] = j % 2;
        xv[j] = skGen();
        if ((int)indi[j] == 0) {
            scalarmultBase(P1v[j], xv[j]);
        } else {
            addKeys1(P1v[j], xv[j], H2[j]);
        }
        subKeys(P2v[j], P1v[j], H2[j]);
    }

    boroSig bb = genBorromean(xv, P1v, P2v, indi);
    ASSERT_TRUE(verifyBorromean(bb, P1v, P2v));
}

TEST(ringct, borromean_tampered_P1v_fails)
{
    key64 xv, P1v, P2v;
    bits indi;

    for (int j = 0; j < 64; j++) {
        indi[j] = (int)randXmrAmount(2);
        xv[j] = skGen();
        if ((int)indi[j] == 0) {
            scalarmultBase(P1v[j], xv[j]);
        } else {
            addKeys1(P1v[j], xv[j], H2[j]);
        }
        subKeys(P2v[j], P1v[j], H2[j]);
    }

    boroSig bb = genBorromean(xv, P1v, P2v, indi);
    ASSERT_TRUE(verifyBorromean(bb, P1v, P2v));

    // Tamper with a P1v element
    P1v[32] = scalarmultBase(skGen());
    ASSERT_FALSE(verifyBorromean(bb, P1v, P2v));
}

TEST(ringct, borromean_tampered_P2v_fails)
{
    key64 xv, P1v, P2v;
    bits indi;

    for (int j = 0; j < 64; j++) {
        indi[j] = (int)randXmrAmount(2);
        xv[j] = skGen();
        if ((int)indi[j] == 0) {
            scalarmultBase(P1v[j], xv[j]);
        } else {
            addKeys1(P1v[j], xv[j], H2[j]);
        }
        subKeys(P2v[j], P1v[j], H2[j]);
    }

    boroSig bb = genBorromean(xv, P1v, P2v, indi);
    ASSERT_TRUE(verifyBorromean(bb, P1v, P2v));

    // Tamper with a P2v element
    P2v[0] = scalarmultBase(skGen());
    ASSERT_FALSE(verifyBorromean(bb, P1v, P2v));
}

// ============================================================================
// Range proof additional tests
// ============================================================================

TEST(ringct, proveRange_verRange_various_amounts)
{
    // Test with several interesting amounts
    for (xmr_amount amount : {1ULL, 2ULL, 255ULL, 256ULL, 65535ULL,
                               1000000000ULL, 0x7FFFFFFFFFFFFFFFULL})
    {
        key C, mask;
        rangeSig rs = proveRange(C, mask, amount);
        ASSERT_TRUE(verRange(C, rs));

        // Commitment matches
        key c_check = commit(amount, mask);
        ASSERT_EQ(C, c_check);
    }
}

TEST(ringct, proveRange_verRange_tampered_asig_fails)
{
    key C, mask;
    rangeSig rs = proveRange(C, mask, 42);
    ASSERT_TRUE(verRange(C, rs));

    // Tamper with a Borromean s0 value inside the range proof
    rs.asig.s0[0] = skGen();
    ASSERT_FALSE(verRange(C, rs));
}

TEST(ringct, proveRange_verRange_tampered_multiple_Ci_fails)
{
    key C, mask;
    rangeSig rs = proveRange(C, mask, 999);
    ASSERT_TRUE(verRange(C, rs));

    // Tamper with multiple Ci elements
    rs.Ci[0] = scalarmultBase(skGen());
    rs.Ci[63] = scalarmultBase(skGen());
    ASSERT_FALSE(verRange(C, rs));
}

// ============================================================================
// verRctSemanticsSimple additional structural tests
// ============================================================================

TEST(ringct, verRctSemanticsSimple_empty_outPk_with_bp_plus)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 4};
    rctSig s = make_simple_sig_with_config(rct_config);
    ASSERT_TRUE(verRctSemanticsSimple(s));

    // Clear outPk - should fail
    s.outPk.clear();
    ASSERT_FALSE(verRctSemanticsSimple(s));
}

TEST(ringct, verRctNonSemanticsSimple_tampered_mixRing_member)
{
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = make_simple_sig_with_config(rct_config);
    ASSERT_TRUE(verRctNonSemanticsSimple(s));

    // Tamper with a mixRing member
    if (!s.mixRing.empty() && !s.mixRing[0].empty()) {
        s.mixRing[0][0].dest = scalarmultBase(skGen());
        ASSERT_FALSE(verRctNonSemanticsSimple(s));
    }
}

// ============================================================================
// genRctSimple with Bulletproof2 and decode
// ============================================================================

TEST(ringct, genRctSimple_Bulletproof2_decode)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(8000);
    tie(sctmp, pctmp) = ctskpkGen(8000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(5000);
    key ak0 = skGen();
    amount_keys.push_back(ak0);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    outamounts.push_back(2000);
    key ak1 = skGen();
    amount_keys.push_back(ak1);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    const RCTConfig rct_config{RangeProofPaddedBulletproof, 2};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, 1000, 3, rct_config, hw::get_device("default"));

    ASSERT_EQ(s.type, RCTTypeBulletproof2);
    ASSERT_TRUE(verRctSimple(s));

    key mask0;
    ASSERT_EQ(decodeRctSimple(s, ak0, 0, mask0, hw::get_device("default")), 5000ULL);

    key mask1;
    ASSERT_EQ(decodeRctSimple(s, ak1, 1, mask1, hw::get_device("default")), 2000ULL);
}

// ============================================================================
// genRctSimple with CLSAG and multiple inputs
// ============================================================================

TEST(ringct, genRctSimple_CLSAG_multiple_inputs)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;
    keyV aks;

    // 3 inputs
    for (xmr_amount amt : {5000ULL, 3000ULL, 2000ULL}) {
        inamounts.push_back(amt);
        tie(sctmp, pctmp) = ctskpkGen(amt);
        sc.push_back(sctmp);
        pc.push_back(pctmp);
    }

    // 2 outputs
    for (xmr_amount amt : {4000ULL, 5000ULL}) {
        outamounts.push_back(amt);
        key ak = skGen();
        aks.push_back(ak);
        amount_keys.push_back(ak);
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);
    }

    xmr_amount fee = 1000;

    const RCTConfig rct_config{RangeProofPaddedBulletproof, 3}; // CLSAG
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, fee, 3, rct_config, hw::get_device("default"));

    ASSERT_EQ(s.type, RCTTypeCLSAG);
    ASSERT_TRUE(verRctSemanticsSimple(s));
    ASSERT_TRUE(verRctNonSemanticsSimple(s));
    ASSERT_TRUE(verRctSimple(s));

    // Decode all outputs
    for (size_t i = 0; i < outamounts.size(); ++i) {
        key mask;
        xmr_amount decoded = decodeRctSimple(s, aks[i], i, mask, hw::get_device("default"));
        ASSERT_EQ(decoded, outamounts[i]);
    }

    // CLSAGs should have correct count
    ASSERT_EQ(s.p.CLSAGs.size(), inamounts.size());
}

// ============================================================================
// verRct with Borromean tampering specific to semantic check
// ============================================================================

TEST(ringct, verRct_semantics_tampered_outPk_fails)
{
    const uint64_t inputs[] = {2000};
    const uint64_t outputs[] = {1000, 1000};
    rctSig s = make_sample_rct_sig(NELTS(inputs), inputs, NELTS(outputs), outputs, true);
    ASSERT_TRUE(verRct(s, true));

    // Tamper with outPk mask
    s.outPk[0].mask = scalarmultBase(skGen());
    ASSERT_FALSE(verRct(s, true));
}

TEST(ringct, verRct_nonsemantic_tampered_mixRing_fails)
{
    const uint64_t inputs[] = {2000};
    const uint64_t outputs[] = {1000, 1000};
    rctSig s = make_sample_rct_sig(NELTS(inputs), inputs, NELTS(outputs), outputs, true);
    ASSERT_TRUE(verRct(s, false));

    // Tamper with mixRing
    if (!s.mixRing.empty() && !s.mixRing[0].empty()) {
        s.mixRing[0][0].dest = scalarmultBase(skGen());
        ASSERT_FALSE(verRct(s, false));
    }
}

// ============================================================================
// Bulletproof commitment consistency
// ============================================================================

TEST(ringct, bulletproof_commitment_matches)
{
    // Verify that the V commitment in a bulletproof matches (1/8) * commit(amount, gamma)
    uint64_t amount = 42;
    key gamma = skGen();

    Bulletproof proof = bulletproof_PROVE(amount, gamma);
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    // V[i] = INV_EIGHT * commit(amount, gamma)
    key full_commit = commit(amount, gamma);
    key expected = scalarmultKey(full_commit, INV_EIGHT);
    ASSERT_EQ(proof.V[0], expected);
}

TEST(ringct, bulletproof_plus_commitment_matches)
{
    // Verify that the V commitment in a bulletproof+ matches (1/8) * commit(amount, gamma)
    uint64_t amount = 42;
    key gamma = skGen();

    BulletproofPlus proof = bulletproof_plus_PROVE(amount, gamma);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));

    // V[i] = INV_EIGHT * commit(amount, gamma)
    key full_commit = commit(amount, gamma);
    key expected = scalarmultKey(full_commit, INV_EIGHT);
    ASSERT_EQ(proof.V[0], expected);
}

TEST(ringct, bulletproof_key_overload)
{
    // Test the overload that takes rct::key amount and gamma
    key amount_key = d2h(12345);
    key gamma = skGen();

    Bulletproof proof = bulletproof_PROVE(amount_key, gamma);
    ASSERT_TRUE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_key_overload)
{
    // Test the overload that takes rct::key amount and gamma
    key amount_key = d2h(67890);
    key gamma = skGen();

    BulletproofPlus proof = bulletproof_plus_PROVE(amount_key, gamma);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_keyV_overload)
{
    // Test the overload that takes rct::keyV amounts and gammas
    keyV amounts, gammas;
    amounts.push_back(d2h(100));
    amounts.push_back(d2h(200));
    gammas.push_back(skGen());
    gammas.push_back(skGen());

    Bulletproof proof = bulletproof_PROVE(amounts, gammas);
    ASSERT_TRUE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_keyV_overload)
{
    // Test the overload that takes rct::keyV amounts and gammas
    keyV amounts, gammas;
    amounts.push_back(d2h(300));
    amounts.push_back(d2h(400));
    gammas.push_back(skGen());
    gammas.push_back(skGen());

    BulletproofPlus proof = bulletproof_plus_PROVE(amounts, gammas);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_batch_verify_vector_overload)
{
    // Test the overload that takes vector<Bulletproof> (non-pointer)
    std::vector<Bulletproof> proofs;
    proofs.push_back(bulletproof_PROVE(100, skGen()));
    proofs.push_back(bulletproof_PROVE(200, skGen()));
    proofs.push_back(bulletproof_PROVE(300, skGen()));
    ASSERT_TRUE(bulletproof_VERIFY(proofs));
}

TEST(ringct, bulletproof_plus_batch_verify_vector_overload)
{
    // Test the overload that takes vector<BulletproofPlus> (non-pointer)
    std::vector<BulletproofPlus> proofs;
    proofs.push_back(bulletproof_plus_PROVE(100, skGen()));
    proofs.push_back(bulletproof_plus_PROVE(200, skGen()));
    proofs.push_back(bulletproof_plus_PROVE(300, skGen()));
    ASSERT_TRUE(bulletproof_plus_VERIFY(proofs));
}

TEST(ringct, bulletproof_tampered_S_fails)
{
    Bulletproof proof = bulletproof_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    proof.S = scalarmultBase(skGen());
    ASSERT_FALSE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_tampered_T1_fails)
{
    Bulletproof proof = bulletproof_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    proof.T1 = scalarmultBase(skGen());
    ASSERT_FALSE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_tampered_T2_fails)
{
    Bulletproof proof = bulletproof_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    proof.T2 = scalarmultBase(skGen());
    ASSERT_FALSE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_tampered_taux_fails)
{
    Bulletproof proof = bulletproof_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    proof.taux = skGen();
    ASSERT_FALSE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_tampered_mu_fails)
{
    Bulletproof proof = bulletproof_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    proof.mu = skGen();
    ASSERT_FALSE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_tampered_a_fails)
{
    Bulletproof proof = bulletproof_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    proof.a = skGen();
    ASSERT_FALSE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_tampered_b_fails)
{
    Bulletproof proof = bulletproof_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    proof.b = skGen();
    ASSERT_FALSE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_tampered_t_fails)
{
    Bulletproof proof = bulletproof_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    proof.t = skGen();
    ASSERT_FALSE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_tampered_L_fails)
{
    Bulletproof proof = bulletproof_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    if (!proof.L.empty()) {
        proof.L[0] = scalarmultBase(skGen());
        ASSERT_FALSE(bulletproof_VERIFY(proof));
    }
}

TEST(ringct, bulletproof_tampered_R_fails)
{
    Bulletproof proof = bulletproof_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    if (!proof.R.empty()) {
        proof.R[0] = scalarmultBase(skGen());
        ASSERT_FALSE(bulletproof_VERIFY(proof));
    }
}

TEST(ringct, bulletproof_tampered_V_fails)
{
    Bulletproof proof = bulletproof_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    if (!proof.V.empty()) {
        proof.V[0] = scalarmultBase(skGen());
        ASSERT_FALSE(bulletproof_VERIFY(proof));
    }
}

TEST(ringct, bulletproof_plus_tampered_A1_fails)
{
    BulletproofPlus proof = bulletproof_plus_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));

    proof.A1 = scalarmultBase(skGen());
    ASSERT_FALSE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_tampered_B_field_fails)
{
    BulletproofPlus proof = bulletproof_plus_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));

    proof.B = scalarmultBase(skGen());
    ASSERT_FALSE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_tampered_r1_fails)
{
    BulletproofPlus proof = bulletproof_plus_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));

    proof.r1 = skGen();
    ASSERT_FALSE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_tampered_s1_fails)
{
    BulletproofPlus proof = bulletproof_plus_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));

    proof.s1 = skGen();
    ASSERT_FALSE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_tampered_d1_fails)
{
    BulletproofPlus proof = bulletproof_plus_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));

    proof.d1 = skGen();
    ASSERT_FALSE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_tampered_L_fails)
{
    BulletproofPlus proof = bulletproof_plus_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));

    if (!proof.L.empty()) {
        proof.L[0] = scalarmultBase(skGen());
        ASSERT_FALSE(bulletproof_plus_VERIFY(proof));
    }
}

TEST(ringct, bulletproof_plus_tampered_R_fails)
{
    BulletproofPlus proof = bulletproof_plus_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));

    if (!proof.R.empty()) {
        proof.R[0] = scalarmultBase(skGen());
        ASSERT_FALSE(bulletproof_plus_VERIFY(proof));
    }
}

TEST(ringct, bulletproof_plus_tampered_V_fails)
{
    BulletproofPlus proof = bulletproof_plus_PROVE(1000, skGen());
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));

    if (!proof.V.empty()) {
        proof.V[0] = scalarmultBase(skGen());
        ASSERT_FALSE(bulletproof_plus_VERIFY(proof));
    }
}

TEST(ringct, bulletproof_max_amount)
{
    Bulletproof proof = bulletproof_PROVE(0xFFFFFFFFFFFFFFFFULL, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_max_amount)
{
    BulletproofPlus proof = bulletproof_plus_PROVE(0xFFFFFFFFFFFFFFFFULL, skGen());
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_power_of_2_outputs)
{
    // Test 8 outputs (power of 2) to test padding behavior
    std::vector<uint64_t> amounts = {10, 20, 30, 40, 50, 60, 70, 80};
    keyV masks;
    for (size_t i = 0; i < amounts.size(); ++i)
        masks.push_back(skGen());

    Bulletproof proof = bulletproof_PROVE(amounts, masks);
    ASSERT_EQ(proof.V.size(), amounts.size());
    ASSERT_TRUE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_power_of_2_outputs)
{
    // Test 8 outputs (power of 2) to test padding behavior
    std::vector<uint64_t> amounts = {10, 20, 30, 40, 50, 60, 70, 80};
    keyV masks;
    for (size_t i = 0; i < amounts.size(); ++i)
        masks.push_back(skGen());

    BulletproofPlus proof = bulletproof_plus_PROVE(amounts, masks);
    ASSERT_EQ(proof.V.size(), amounts.size());
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_single_output_amount_one)
{
    Bulletproof proof = bulletproof_PROVE(1, skGen());
    ASSERT_TRUE(bulletproof_VERIFY(proof));
    ASSERT_EQ(proof.V.size(), 1u);
}

TEST(ringct, bulletproof_plus_single_output_amount_one)
{
    BulletproofPlus proof = bulletproof_plus_PROVE(1, skGen());
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
    ASSERT_EQ(proof.V.size(), 1u);
}

// ============================================================================
// genRctSimple explicit mixring with BP Plus
// ============================================================================

TEST(ringct, genRctSimple_explicit_mixring_bp_plus)
{
    ctkeyV inSk;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(5000);
    tie(sctmp, pctmp) = ctskpkGen(5000);
    inSk.push_back(sctmp);

    // Build mix ring manually
    const int mixin = 3;
    ctkeyM mixRing(1);
    std::vector<unsigned int> indices(1);

    ctkey pk0;
    pk0.dest = scalarmultBase(inSk[0].dest);
    addKeys2(pk0.mask, inSk[0].mask, d2h(inamounts[0]), H);

    mixRing[0].resize(mixin + 1);
    for (int i = 0; i <= mixin; i++) {
        if (i == 2) {
            mixRing[0][i] = pk0;
            indices[0] = i;
        } else {
            mixRing[0][i].dest = scalarmultBase(skGen());
            mixRing[0][i].mask = scalarmultBase(skGen());
        }
    }

    outamounts.push_back(4000);
    key ak = skGen();
    amount_keys.push_back(ak);
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount fee = 1000;

    ctkeyV outSk;
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 4}; // BP+
    rctSig s = genRctSimple(zero(), inSk, destinations, inamounts, outamounts, fee, mixRing, amount_keys, indices, outSk, rct_config, hw::get_device("default"));

    ASSERT_EQ(s.type, RCTTypeBulletproofPlus);
    ASSERT_TRUE(verRctSemanticsSimple(s));
    ASSERT_TRUE(verRctNonSemanticsSimple(s));

    // Decode
    key mask;
    xmr_amount decoded = decodeRctSimple(s, ak, 0, mask, hw::get_device("default"));
    ASSERT_EQ(decoded, 4000ULL);
}

// ============================================================================
// Batch bulletproof verify with mix of tampered and valid
// ============================================================================

TEST(ringct, bulletproof_batch_one_tampered_one_valid)
{
    Bulletproof p1 = bulletproof_PROVE(500, skGen());
    Bulletproof p2 = bulletproof_PROVE(600, skGen());

    ASSERT_TRUE(bulletproof_VERIFY(p1));
    ASSERT_TRUE(bulletproof_VERIFY(p2));

    // Tamper only p1
    p1.A = scalarmultBase(skGen());

    std::vector<const Bulletproof*> proofs;
    proofs.push_back(&p1);
    proofs.push_back(&p2);
    ASSERT_FALSE(bulletproof_VERIFY(proofs));
}

TEST(ringct, bulletproof_plus_batch_one_tampered_one_valid)
{
    BulletproofPlus p1 = bulletproof_plus_PROVE(500, skGen());
    BulletproofPlus p2 = bulletproof_plus_PROVE(600, skGen());

    ASSERT_TRUE(bulletproof_plus_VERIFY(p1));
    ASSERT_TRUE(bulletproof_plus_VERIFY(p2));

    // Tamper only p2
    p2.A = scalarmultBase(skGen());

    std::vector<const BulletproofPlus*> proofs;
    proofs.push_back(&p1);
    proofs.push_back(&p2);
    ASSERT_FALSE(bulletproof_plus_VERIFY(proofs));
}

// ============================================================================
// decodeRct with multiple outputs (full RCT)
// ============================================================================

TEST(ringct, decodeRct_all_outputs)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;

    tie(sctmp, pctmp) = ctskpkGen(10000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    vector<xmr_amount> amounts;
    keyV amount_keys;
    keyV destinations;
    key Sk, Pk;

    keyV aks;
    for (xmr_amount amt : {3000ULL, 2000ULL, 4000ULL}) {
        amounts.push_back(amt);
        key ak = skGen();
        aks.push_back(ak);
        amount_keys.push_back(ak);
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);
    }

    amounts.push_back(1000); // fee

    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRct(zero(), sc, pc, destinations, amounts, amount_keys, 3, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRct(s));
    ASSERT_EQ(s.txnFee, 1000ULL);

    // Decode all three outputs
    for (size_t i = 0; i < 3; ++i) {
        key mask;
        xmr_amount decoded = decodeRct(s, aks[i], i, mask, hw::get_device("default"));
        ASSERT_EQ(decoded, amounts[i]);
    }
}

// ============================================================================
// verRctSemanticsSimple batch with single element
// ============================================================================

TEST(ringct, verRctSemanticsSimple_batch_single)
{
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 4};
    rctSig s = make_simple_sig_with_config(rct_config);

    std::vector<const rctSig*> sigs;
    sigs.push_back(&s);
    ASSERT_TRUE(verRctSemanticsSimple(sigs));
}

// ============================================================================
// verRctSemanticsSimple / verRctNonSemanticsSimple with null type
// ============================================================================

TEST(ringct, verRctSemanticsSimple_null_type_fails)
{
    rctSig s;
    s.type = RCTTypeNull;
    // RCTTypeNull is not a simple type
    ASSERT_FALSE(verRctSemanticsSimple(s));
}

TEST(ringct, verRctNonSemanticsSimple_null_type_fails)
{
    rctSig s;
    s.type = RCTTypeNull;
    ASSERT_FALSE(verRctNonSemanticsSimple(s));
}

// ============================================================================
// CLSAG: proveRctCLSAGSimple with ring size 2
// ============================================================================

TEST(ringct, proveRctCLSAGSimple_ring_size_2)
{
    const size_t N = 2;
    const size_t idx = 1;
    ctkeyV pubs;
    key p, t, t2, u;
    const key message = skGen();

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        ctkey tmp;
        skpkGen(sk, tmp.dest);
        skpkGen(sk, tmp.mask);
        pubs.push_back(tmp);
    }

    skpkGen(p, pubs[idx].dest);
    t = skGen();
    u = skGen();
    addKeys2(pubs[idx].mask, t, u, H);

    key Cout;
    t2 = skGen();
    addKeys2(Cout, t2, u, H);

    ctkey insk;
    insk.dest = p;
    insk.mask = t;

    clsag sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));

    // Wrong message fails
    ASSERT_FALSE(verRctCLSAGSimple(skGen(), sig, pubs, Cout));
}

// ============================================================================
// Additional CLSAG ring signature tests
// ============================================================================

TEST(ringct, CLSAG_ring_size_11_first_index)
{
    const size_t N = 11;
    const size_t idx = 0;
    ctkeyV pubs;
    key p, t, t2, u;
    const key message = skGen();

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        ctkey tmp;
        skpkGen(sk, tmp.dest);
        skpkGen(sk, tmp.mask);
        pubs.push_back(tmp);
    }

    skpkGen(p, pubs[idx].dest);
    t = skGen();
    u = skGen();
    addKeys2(pubs[idx].mask, t, u, H);

    key Cout;
    t2 = skGen();
    addKeys2(Cout, t2, u, H);

    ctkey insk;
    insk.dest = p;
    insk.mask = t;

    clsag sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));
}

TEST(ringct, CLSAG_ring_size_11_last_index)
{
    const size_t N = 11;
    const size_t idx = N - 1;
    ctkeyV pubs;
    key p, t, t2, u;
    const key message = skGen();

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        ctkey tmp;
        skpkGen(sk, tmp.dest);
        skpkGen(sk, tmp.mask);
        pubs.push_back(tmp);
    }

    skpkGen(p, pubs[idx].dest);
    t = skGen();
    u = skGen();
    addKeys2(pubs[idx].mask, t, u, H);

    key Cout;
    t2 = skGen();
    addKeys2(Cout, t2, u, H);

    ctkey insk;
    insk.dest = p;
    insk.mask = t;

    clsag sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));
}

TEST(ringct, CLSAG_ring_size_16_middle_index)
{
    const size_t N = 16;
    const size_t idx = 8;
    ctkeyV pubs;
    key p, t, t2, u;
    const key message = skGen();

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        ctkey tmp;
        skpkGen(sk, tmp.dest);
        skpkGen(sk, tmp.mask);
        pubs.push_back(tmp);
    }

    skpkGen(p, pubs[idx].dest);
    t = skGen();
    u = skGen();
    addKeys2(pubs[idx].mask, t, u, H);

    key Cout;
    t2 = skGen();
    addKeys2(Cout, t2, u, H);

    ctkey insk;
    insk.dest = p;
    insk.mask = t;

    clsag sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));
}

TEST(ringct, CLSAG_verify_wrong_key_fails)
{
    const size_t N = 11;
    const size_t idx = 5;
    ctkeyV pubs;
    key p, t, t2, u;
    const key message = skGen();

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        ctkey tmp;
        skpkGen(sk, tmp.dest);
        skpkGen(sk, tmp.mask);
        pubs.push_back(tmp);
    }

    skpkGen(p, pubs[idx].dest);
    t = skGen();
    u = skGen();
    addKeys2(pubs[idx].mask, t, u, H);

    key Cout;
    t2 = skGen();
    addKeys2(Cout, t2, u, H);

    ctkey insk;
    insk.dest = p;
    insk.mask = t;

    clsag sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));

    // Tamper with a public key in the ring
    key dummy_sk;
    skpkGen(dummy_sk, pubs[0].dest);
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
}

TEST(ringct, CLSAG_single_member_ring)
{
    const size_t N = 1;
    const size_t idx = 0;
    ctkeyV pubs;
    key p, t, t2, u;
    const key message = skGen();

    {
        key sk;
        ctkey tmp;
        skpkGen(sk, tmp.dest);
        skpkGen(sk, tmp.mask);
        pubs.push_back(tmp);
    }

    skpkGen(p, pubs[idx].dest);
    t = skGen();
    u = skGen();
    addKeys2(pubs[idx].mask, t, u, H);

    key Cout;
    t2 = skGen();
    addKeys2(Cout, t2, u, H);

    ctkey insk;
    insk.dest = p;
    insk.mask = t;

    clsag sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));
}

TEST(ringct, CLSAG_wrong_Cout_fails)
{
    const size_t N = 11;
    const size_t idx = 3;
    ctkeyV pubs;
    key p, t, t2, u;
    const key message = skGen();

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        ctkey tmp;
        skpkGen(sk, tmp.dest);
        skpkGen(sk, tmp.mask);
        pubs.push_back(tmp);
    }

    skpkGen(p, pubs[idx].dest);
    t = skGen();
    u = skGen();
    addKeys2(pubs[idx].mask, t, u, H);

    key Cout;
    t2 = skGen();
    addKeys2(Cout, t2, u, H);

    ctkey insk;
    insk.dest = p;
    insk.mask = t;

    clsag sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));

    // Wrong commitment offset should fail
    key wrong_Cout = scalarmultBase(skGen());
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, wrong_Cout));
}

TEST(ringct, CLSAG_empty_ring_fails)
{
    ctkeyV pubs;
    const key message = skGen();
    key Cout = scalarmultBase(skGen());

    clsag sig;
    sig.s = keyV(0);
    sig.c1 = zero();
    sig.I = identity();
    sig.D = identity();

    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
}

TEST(ringct, CLSAG_Gen_ring_size_2_index_0)
{
    const size_t N = 2;
    const size_t idx = 0;

    keyV P(N), C(N), C_nonzero(N);
    key p;

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        skpkGen(sk, P[i]);
        key csk;
        skpkGen(csk, C_nonzero[i]);
    }

    skpkGen(p, P[idx]);

    key z = skGen();
    key u = skGen();
    addKeys2(C_nonzero[idx], z, u, H);

    key C_offset;
    key z2 = skGen();
    addKeys2(C_offset, z2, u, H);

    for (size_t i = 0; i < N; ++i)
        subKeys(C[i], C_nonzero[i], C_offset);

    key z_sign;
    sc_sub(z_sign.bytes, z.bytes, z2.bytes);

    key message = skGen();
    clsag sig = CLSAG_Gen(message, P, p, C, z_sign, C_nonzero, C_offset, idx);

    ctkeyV pub_keys(N);
    for (size_t i = 0; i < N; ++i)
    {
        pub_keys[i].dest = P[i];
        pub_keys[i].mask = C_nonzero[i];
    }
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pub_keys, C_offset));
}

TEST(ringct, CLSAG_deterministic_key_image)
{
    const size_t N = 4;
    const size_t idx = 2;
    ctkeyV pubs;
    key p, t, t2, u;
    const key message = skGen();

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        ctkey tmp;
        skpkGen(sk, tmp.dest);
        skpkGen(sk, tmp.mask);
        pubs.push_back(tmp);
    }

    skpkGen(p, pubs[idx].dest);
    t = skGen();
    u = skGen();
    addKeys2(pubs[idx].mask, t, u, H);

    key Cout;
    t2 = skGen();
    addKeys2(Cout, t2, u, H);

    ctkey insk;
    insk.dest = p;
    insk.mask = t;

    // Generate two signatures with same secret keys
    clsag sig1 = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));
    clsag sig2 = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));

    // Key images must be identical (deterministic)
    ASSERT_EQ(sig1.I, sig2.I);
    // Both signatures should verify
    ASSERT_TRUE(verRctCLSAGSimple(message, sig1, pubs, Cout));
    ASSERT_TRUE(verRctCLSAGSimple(message, sig2, pubs, Cout));
}

TEST(ringct, CLSAG_tampered_I_fails)
{
    const size_t N = 8;
    const size_t idx = 4;
    ctkeyV pubs;
    key p, t, t2, u;
    const key message = skGen();

    for (size_t i = 0; i < N; ++i)
    {
        key sk;
        ctkey tmp;
        skpkGen(sk, tmp.dest);
        skpkGen(sk, tmp.mask);
        pubs.push_back(tmp);
    }

    skpkGen(p, pubs[idx].dest);
    t = skGen();
    u = skGen();
    addKeys2(pubs[idx].mask, t, u, H);

    key Cout;
    t2 = skGen();
    addKeys2(Cout, t2, u, H);

    ctkey insk;
    insk.dest = p;
    insk.mask = t;

    clsag sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, idx, hw::get_device("default"));
    ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));

    // Tamper with key image
    sig.I = scalarmultBase(skGen());
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
}

// ============================================================================
// Additional Bulletproof / BulletproofPlus tests
// ============================================================================

TEST(ringct, bulletproof_prove_verify_four_amounts)
{
    std::vector<uint64_t> amounts = {100, 200, 300, 400};
    keyV masks;
    for (size_t i = 0; i < amounts.size(); ++i)
        masks.push_back(skGen());

    Bulletproof proof = bulletproof_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_prove_verify_eight_amounts)
{
    std::vector<uint64_t> amounts = {10, 20, 30, 40, 50, 60, 70, 80};
    keyV masks;
    for (size_t i = 0; i < amounts.size(); ++i)
        masks.push_back(skGen());

    Bulletproof proof = bulletproof_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_wrong_commitment_fails)
{
    std::vector<uint64_t> amounts = {5000};
    keyV masks;
    masks.push_back(skGen());

    Bulletproof proof = bulletproof_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_VERIFY(proof));

    // Replace V with a wrong commitment
    proof.V[0] = scalarmultBase(skGen());
    ASSERT_FALSE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_roundtrip_various_amounts)
{
    const std::vector<uint64_t> test_amounts = {0, 1, 1000, 1000000, UINT64_MAX};
    for (uint64_t amt : test_amounts)
    {
        std::vector<uint64_t> amounts = {amt};
        keyV masks;
        masks.push_back(skGen());

        Bulletproof proof = bulletproof_PROVE(amounts, masks);
        ASSERT_TRUE(bulletproof_VERIFY(proof));
    }
}

TEST(ringct, bulletproof_plus_prove_verify_four_amounts)
{
    std::vector<uint64_t> amounts = {100, 200, 300, 400};
    keyV masks;
    for (size_t i = 0; i < amounts.size(); ++i)
        masks.push_back(skGen());

    BulletproofPlus proof = bulletproof_plus_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_prove_verify_eight_amounts)
{
    std::vector<uint64_t> amounts = {10, 20, 30, 40, 50, 60, 70, 80};
    keyV masks;
    for (size_t i = 0; i < amounts.size(); ++i)
        masks.push_back(skGen());

    BulletproofPlus proof = bulletproof_plus_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_wrong_commitment_fails)
{
    std::vector<uint64_t> amounts = {9999};
    keyV masks;
    masks.push_back(skGen());

    BulletproofPlus proof = bulletproof_plus_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));

    // Replace V with a wrong commitment
    proof.V[0] = scalarmultBase(skGen());
    ASSERT_FALSE(bulletproof_plus_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_roundtrip_various_amounts)
{
    const std::vector<uint64_t> test_amounts = {0, 1, 999, 1000000, UINT64_MAX};
    for (uint64_t amt : test_amounts)
    {
        std::vector<uint64_t> amounts = {amt};
        keyV masks;
        masks.push_back(skGen());

        BulletproofPlus proof = bulletproof_plus_PROVE(amounts, masks);
        ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
    }
}

TEST(ringct, bulletproof_batch_verify_four_proofs)
{
    std::vector<Bulletproof> proofs_store;
    for (int i = 0; i < 4; ++i)
    {
        std::vector<uint64_t> amounts = {(uint64_t)(100 * (i + 1))};
        keyV masks;
        masks.push_back(skGen());
        proofs_store.push_back(bulletproof_PROVE(amounts, masks));
    }

    std::vector<const Bulletproof*> proofs;
    for (auto &p : proofs_store)
        proofs.push_back(&p);
    ASSERT_TRUE(bulletproof_VERIFY(proofs));
}

TEST(ringct, bulletproof_plus_batch_verify_four_proofs)
{
    std::vector<BulletproofPlus> proofs_store;
    for (int i = 0; i < 4; ++i)
    {
        std::vector<uint64_t> amounts = {(uint64_t)(200 * (i + 1))};
        keyV masks;
        masks.push_back(skGen());
        proofs_store.push_back(bulletproof_plus_PROVE(amounts, masks));
    }

    std::vector<const BulletproofPlus*> proofs;
    for (auto &p : proofs_store)
        proofs.push_back(&p);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proofs));
}

TEST(ringct, bulletproof_batch_verify_eight_proofs)
{
    std::vector<Bulletproof> proofs_store;
    for (int i = 0; i < 8; ++i)
    {
        std::vector<uint64_t> amounts = {(uint64_t)(50 * (i + 1))};
        keyV masks;
        masks.push_back(skGen());
        proofs_store.push_back(bulletproof_PROVE(amounts, masks));
    }

    std::vector<const Bulletproof*> proofs;
    for (auto &p : proofs_store)
        proofs.push_back(&p);
    ASSERT_TRUE(bulletproof_VERIFY(proofs));
}

TEST(ringct, bulletproof_plus_batch_verify_eight_proofs)
{
    std::vector<BulletproofPlus> proofs_store;
    for (int i = 0; i < 8; ++i)
    {
        std::vector<uint64_t> amounts = {(uint64_t)(50 * (i + 1))};
        keyV masks;
        masks.push_back(skGen());
        proofs_store.push_back(bulletproof_plus_PROVE(amounts, masks));
    }

    std::vector<const BulletproofPlus*> proofs;
    for (auto &p : proofs_store)
        proofs.push_back(&p);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proofs));
}

TEST(ringct, bulletproof_and_bulletproof_plus_differ)
{
    // Same amount and mask should produce different proof structures
    std::vector<uint64_t> amounts = {12345};
    keyV masks;
    masks.push_back(skGen());

    Bulletproof bp = bulletproof_PROVE(amounts, masks);
    // Re-generate masks for BP+ (different proof system)
    keyV masks2;
    masks2.push_back(skGen());
    BulletproofPlus bpp = bulletproof_plus_PROVE(amounts, masks2);

    // Both should verify independently
    ASSERT_TRUE(bulletproof_VERIFY(bp));
    ASSERT_TRUE(bulletproof_plus_VERIFY(bpp));

    // BP has S, T1, T2 fields; BPP has A1, B fields
    // They are different proof structures
    ASSERT_NE(bp.V.size(), 0u);
    ASSERT_NE(bpp.V.size(), 0u);
}

TEST(ringct, bulletproof_prove_verify_two_amounts)
{
    std::vector<uint64_t> amounts = {500, 1500};
    keyV masks;
    masks.push_back(skGen());
    masks.push_back(skGen());

    Bulletproof proof = bulletproof_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_VERIFY(proof));
}

TEST(ringct, bulletproof_plus_prove_verify_two_amounts)
{
    std::vector<uint64_t> amounts = {500, 1500};
    keyV masks;
    masks.push_back(skGen());
    masks.push_back(skGen());

    BulletproofPlus proof = bulletproof_plus_PROVE(amounts, masks);
    ASSERT_TRUE(bulletproof_plus_VERIFY(proof));
}

// ============================================================================
// Additional RCT simple tests
// ============================================================================

TEST(ringct, genRctSimple_single_input_single_output)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(1000);
    tie(sctmp, pctmp) = ctskpkGen(1000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(900);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 100;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_two_inputs_one_output)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(2000);
    tie(sctmp, pctmp) = ctskpkGen(2000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    inamounts.push_back(3000);
    tie(sctmp, pctmp) = ctskpkGen(3000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(4500);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 500;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_one_input_two_outputs)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(10000);
    tie(sctmp, pctmp) = ctskpkGen(10000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(6000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    outamounts.push_back(3000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 1000;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_four_inputs_two_outputs)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    const xmr_amount per_input = 1000;
    for (int i = 0; i < 4; ++i)
    {
        inamounts.push_back(per_input);
        tie(sctmp, pctmp) = ctskpkGen(per_input);
        sc.push_back(sctmp);
        pc.push_back(pctmp);
    }

    outamounts.push_back(2000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    outamounts.push_back(1500);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 500;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_two_inputs_four_outputs)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(5000);
    tie(sctmp, pctmp) = ctskpkGen(5000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    inamounts.push_back(5000);
    tie(sctmp, pctmp) = ctskpkGen(5000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    for (int i = 0; i < 4; ++i)
    {
        outamounts.push_back(2000);
        amount_keys.push_back(hash_to_scalar(zero()));
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);
    }

    xmr_amount txnfee = 2000;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_four_inputs_four_outputs)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    for (int i = 0; i < 4; ++i)
    {
        inamounts.push_back(3000);
        tie(sctmp, pctmp) = ctskpkGen(3000);
        sc.push_back(sctmp);
        pc.push_back(pctmp);
    }

    for (int i = 0; i < 4; ++i)
    {
        outamounts.push_back(2500);
        amount_keys.push_back(hash_to_scalar(zero()));
        skpkGen(Sk, Pk);
        destinations.push_back(Pk);
    }

    xmr_amount txnfee = 2000;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_fee_zero)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(5000);
    tie(sctmp, pctmp) = ctskpkGen(5000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(5000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 0;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_fee_large)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(10000);
    tie(sctmp, pctmp) = ctskpkGen(10000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(1000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 9000;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_verRctSemanticsSimple_and_NonSemantics)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(8000);
    tie(sctmp, pctmp) = ctskpkGen(8000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    inamounts.push_back(2000);
    tie(sctmp, pctmp) = ctskpkGen(2000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(7000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    outamounts.push_back(2500);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 500;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    // Full verification chain
    ASSERT_TRUE(verRctSemanticsSimple(s));
    ASSERT_TRUE(verRctNonSemanticsSimple(s));
    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_BPPlus_single_input_single_output)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(5000);
    tie(sctmp, pctmp) = ctskpkGen(5000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(4500);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 500;
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 4};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_EQ(s.type, RCTTypeBulletproofPlus);
    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_BPPlus_four_inputs_two_outputs)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    for (int i = 0; i < 4; ++i)
    {
        inamounts.push_back(2000);
        tie(sctmp, pctmp) = ctskpkGen(2000);
        sc.push_back(sctmp);
        pc.push_back(pctmp);
    }

    outamounts.push_back(5000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    outamounts.push_back(2000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 1000;
    const RCTConfig rct_config{RangeProofPaddedBulletproof, 4};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_EQ(s.type, RCTTypeBulletproofPlus);
    ASSERT_TRUE(verRctSemanticsSimple(s));
    ASSERT_TRUE(verRctNonSemanticsSimple(s));
}

TEST(ringct, genRctSimple_decode_all_outputs)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk, mask;

    inamounts.push_back(6000);
    tie(sctmp, pctmp) = ctskpkGen(6000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(2000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    outamounts.push_back(3000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 1000;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));

    // Decode each output
    xmr_amount decoded0 = decodeRctSimple(s, amount_keys[0], 0, mask, hw::get_device("default"));
    ASSERT_EQ(decoded0, 2000u);

    xmr_amount decoded1 = decodeRctSimple(s, amount_keys[1], 1, mask, hw::get_device("default"));
    ASSERT_EQ(decoded1, 3000u);
}

TEST(ringct, genRctSimple_fee_equals_total_input)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;

    inamounts.push_back(5000);
    tie(sctmp, pctmp) = ctskpkGen(5000);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    // No outputs, entire input goes to fee
    xmr_amount txnfee = 5000;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));
}

TEST(ringct, genRctSimple_fee_one_unit)
{
    ctkeyV sc, pc;
    ctkey sctmp, pctmp;
    vector<xmr_amount> inamounts, outamounts;
    keyV destinations, amount_keys;
    key Sk, Pk;

    inamounts.push_back(1001);
    tie(sctmp, pctmp) = ctskpkGen(1001);
    sc.push_back(sctmp);
    pc.push_back(pctmp);

    outamounts.push_back(1000);
    amount_keys.push_back(hash_to_scalar(zero()));
    skpkGen(Sk, Pk);
    destinations.push_back(Pk);

    xmr_amount txnfee = 1;
    const RCTConfig rct_config{RangeProofBorromean, 0};
    rctSig s = genRctSimple(zero(), sc, pc, destinations, inamounts, outamounts, amount_keys, txnfee, 2, rct_config, hw::get_device("default"));

    ASSERT_TRUE(verRctSimple(s));
}
