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

"""Test pruning behavioral contracts"""

from framework.daemon import Daemon
from framework.wallet import Wallet

ADDR1 = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class PruningTest():
    def run_test(self):
        self.reset()
        self.mine()
        self.test_prune_blockchain()
        self.test_pruned_block_queries()
        self.test_get_transactions_pruned()

    def reset(self):
        print('Resetting blockchain')
        daemon = Daemon()
        res = daemon.get_height()
        daemon.pop_blocks(res.height - 1)
        daemon.flush_txpool()

    def mine(self):
        print('Mining blocks')
        daemon = Daemon()
        daemon.generateblocks(ADDR1, 100)

    def test_prune_blockchain(self):
        print('Testing prune_blockchain')
        daemon = Daemon()

        res = daemon.get_info()
        pruning_seed_before = res.pruning_seed if 'pruning_seed' in res else 0

        # Prune the blockchain
        res = daemon.prune_blockchain()
        assert res.status == 'OK' or res.status == 'ok'
        pruning_seed = res.pruning_seed
        assert pruning_seed > 0

        # Verify pruning seed in get_info
        res = daemon.get_info()
        assert res.pruning_seed == pruning_seed

    def test_pruned_block_queries(self):
        print('Testing pruned block queries')
        daemon = Daemon()

        # A pruned node should still serve block headers
        res = daemon.get_last_block_header()
        assert 'block_header' in res
        header = res.block_header
        assert header.height > 0
        assert header.hash != ''

        # Get block header by height
        res = daemon.get_block_header_by_height(height = 1)
        assert res.block_header.height == 1

        # Get block headers range
        res = daemon.get_block_headers_range(start_height = 0, end_height = 10)
        assert len(res.headers) == 11

    def test_get_transactions_pruned(self):
        print('Testing get_transactions pruned')
        daemon = Daemon()

        # Get a block to find transaction hashes
        res = daemon.get_block(height = 1)
        block = res.block_header

        # Get the coinbase tx hash from the block
        res = daemon.get_block(height = 1)
        tx_hashes = res.tx_hashes if 'tx_hashes' in res else []

        # Get the miner tx from any block with transactions
        if len(tx_hashes) > 0:
            # Get transactions with prune=True
            res = daemon.get_transactions(tx_hashes, prune = True)
            assert len(res.txs) == len(tx_hashes)
            for tx in res.txs:
                assert 'as_hex' in tx or 'pruned_as_hex' in tx
                # Pruned transactions should have smaller blobs
                if 'pruned_as_hex' in tx:
                    print('Got pruned tx blob')
        else:
            print('No non-coinbase transactions at height 1, skipping pruned tx test')

if __name__ == '__main__':
    PruningTest().run_test()
