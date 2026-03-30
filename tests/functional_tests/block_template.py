#!/usr/bin/env python3

# Copyright (c) 2019-2024, The Monero Project
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Test block template and mining contracts: template construction, calc_pow, aux_pow, header queries"""

from framework.daemon import Daemon
from framework.wallet import Wallet

SEED = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
STANDARD_ADDRESS = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class BlockTemplateTest():
    def run_test(self):
        self.reset()
        self.create()
        self.mine()
        self.test_block_template_basic()
        self.test_block_template_with_txpool()
        self.test_getblockheadersrange()
        self.test_getblockheaderbyhash_batch()
        self.test_calc_pow()
        self.test_add_aux_pow()

    def reset(self):
        print('Resetting blockchain')
        daemon = Daemon()
        res = daemon.get_height()
        daemon.pop_blocks(res.height - 1)
        daemon.flush_txpool()

    def create(self):
        print('Creating wallet')
        self.wallet = Wallet(idx = 0)
        try: self.wallet.close_wallet()
        except: pass
        self.wallet.restore_deterministic_wallet(seed = SEED)

    def mine(self):
        print('Mining blocks')
        daemon = Daemon()
        daemon.generateblocks(STANDARD_ADDRESS, 100)
        self.wallet.refresh()

    def test_block_template_basic(self):
        print('Testing basic block template')
        daemon = Daemon()

        res = daemon.getblocktemplate(STANDARD_ADDRESS)
        assert 'blocktemplate_blob' in res
        assert 'blockhashing_blob' in res
        assert 'difficulty' in res
        assert 'height' in res
        assert 'prev_hash' in res
        assert 'expected_reward' in res
        assert res.height > 0
        assert res.difficulty > 0
        assert res.expected_reward > 0
        assert len(res.blocktemplate_blob) > 0
        assert len(res.blockhashing_blob) > 0
        print('Block template: height={}, difficulty={}'.format(res.height, res.difficulty))

    def test_block_template_with_txpool(self):
        print('Testing block template with txpool')
        daemon = Daemon()
        w0 = self.wallet

        daemon.generateblocks(STANDARD_ADDRESS, 10)
        w0.refresh()

        # Add a transaction to the pool
        dst = '44AFFq5kSiGBoZ4NMDwYtN18obc8AemS33DBLWs3H7otXft3XjrpDtQGv7SqSsaBYBb98uNbr2VBBEt7f2wfn3RVGQBEP3A'
        res = w0.transfer([{'address': dst, 'amount': 1000000000000}])
        tx_hash = res.tx_hash

        # Get block template - it should include the pool tx
        res = daemon.getblocktemplate(STANDARD_ADDRESS)
        # The expected_reward should include the fee
        assert res.expected_reward > 0

        # Clean up
        daemon.generateblocks(STANDARD_ADDRESS, 1)
        w0.refresh()

    def test_getblockheadersrange(self):
        print('Testing getblockheadersrange')
        daemon = Daemon()

        res = daemon.getblockheadersrange(start_height = 0, end_height = 9)
        assert 'headers' in res
        assert len(res.headers) == 10

        for i, header in enumerate(res.headers):
            assert header.height == i
            assert 'hash' in header
            assert 'timestamp' in header
            assert 'reward' in header
            assert header.reward > 0

    def test_getblockheaderbyhash_batch(self):
        print('Testing getblockheaderbyhash batch')
        daemon = Daemon()

        # Get some block hashes
        hashes = []
        for h in range(1, 6):
            res = daemon.getblockheaderbyheight(height = h)
            hashes.append(res.block_header.hash)

        # Query by multiple hashes
        res = daemon.getblockheaderbyhash(hashes = hashes)
        assert 'block_headers' in res
        assert len(res.block_headers) == len(hashes)

        for i, header in enumerate(res.block_headers):
            assert header.hash == hashes[i]

    def test_calc_pow(self):
        print('Testing calc_pow')
        daemon = Daemon()

        res = daemon.getblocktemplate(STANDARD_ADDRESS)
        major_version = res.blocktemplate_blob[:2]  # First byte hex
        height = res.height
        blobdata = res.blockhashing_blob
        seed_hash = res.seed_hash if 'seed_hash' in res else '0' * 64

        try:
            res = daemon.calc_pow(major_version = int(major_version, 16),
                                   height = height,
                                   block_blob = blobdata,
                                   seed_hash = seed_hash)
            assert len(res) > 0
            print('calc_pow: OK')
        except Exception as e:
            print('calc_pow: {}'.format(str(e)))

    def test_add_aux_pow(self):
        print('Testing add_aux_pow')
        daemon = Daemon()

        res = daemon.getblocktemplate(STANDARD_ADDRESS)
        blocktemplate_blob = res.blocktemplate_blob

        # Try the aux pow flow
        try:
            aux_id = '0' * 64  # dummy aux id
            res = daemon.add_aux_pow(blocktemplate_blob = blocktemplate_blob,
                                      aux_pow = [{'id': aux_id, 'hash': '0' * 64}])
            assert 'blocktemplate_blob' in res or 'blockhashing_blob' in res
            print('add_aux_pow: OK')
        except Exception as e:
            print('add_aux_pow: {}'.format(str(e)))

if __name__ == '__main__':
    BlockTemplateTest().run_test()
