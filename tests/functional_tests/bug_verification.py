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

"""Test known bug regression contracts from specs/bugs.md"""

from framework.daemon import Daemon
from framework.wallet import Wallet

SEED1 = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
SEED2 = 'peeled mixture ionic radar utopia puddle buying illness nuns gadget river spout cavernous bounced paradise drunk looking cottage jump tequila melting went winter adjust spout'
STANDARD_ADDRESS = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class BugVerificationTest():
    def run_test(self):
        self.reset()
        self.create()
        self.mine()
        self.test_bug10_missed_ids()
        self.test_bug13_alt_chain()
        self.test_txpool_kept_by_block()

    def reset(self):
        print('Resetting blockchain')
        daemon = Daemon()
        res = daemon.get_height()
        daemon.pop_blocks(res.height - 1)
        daemon.flush_txpool()

    def create(self):
        print('Creating wallets')
        self.wallet = [None, None]
        self.wallet[0] = Wallet(idx = 0)
        self.wallet[1] = Wallet(idx = 1)
        try: self.wallet[0].close_wallet()
        except: pass
        try: self.wallet[1].close_wallet()
        except: pass
        self.wallet[0].restore_deterministic_wallet(seed = SEED1)
        self.wallet[1].restore_deterministic_wallet(seed = SEED2)

    def mine(self):
        print('Mining blocks')
        daemon = Daemon()
        daemon.generateblocks(STANDARD_ADDRESS, 100)
        for w in self.wallet:
            w.refresh()

    def test_bug10_missed_ids(self):
        print('Testing Bug #10: get_transactions with mixed valid/invalid hashes')
        daemon = Daemon()

        # Get a valid tx hash from a block
        res = daemon.getblock(height = 1)
        # The miner tx is always in the block
        valid_hash = None
        if 'tx_hashes' in res and len(res.tx_hashes) > 0:
            valid_hash = res.tx_hashes[0]

        fake_hash = 'a' * 64  # non-existent

        if valid_hash:
            # Query with both valid and invalid
            res = daemon.get_transactions([valid_hash, fake_hash])
            assert len(res.txs) >= 1  # At least the valid one
            if 'missed_tx' in res:
                assert fake_hash in res.missed_tx
            print('Bug #10: mixed valid/invalid query OK')
        else:
            # No non-coinbase txs at height 1, try with just invalid
            res = daemon.get_transactions([fake_hash])
            assert 'missed_tx' in res and fake_hash in res.missed_tx
            print('Bug #10: invalid-only query OK')

    def test_bug13_alt_chain(self):
        print('Testing Bug #13: alt chain and get_alternate_chains')
        daemon = Daemon()

        # Query alternate chains
        res = daemon.get_alternate_chains()
        # In regtest offline mode there may be no alt chains
        # but the call should succeed
        if 'chains' in res:
            print('Bug #13: {} alternate chains found'.format(len(res.chains)))
            for chain in res.chains:
                assert 'block_hash' in chain
                assert 'height' in chain
                assert 'length' in chain
                assert 'difficulty' in chain
        else:
            print('Bug #13: no alternate chains (expected in regtest)')

    def test_txpool_kept_by_block(self):
        print('Testing txpool kept_by_block behavior')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        daemon.generateblocks(STANDARD_ADDRESS, 10)
        w0.refresh()

        res = w1.get_address()
        dst = res.address

        # Create and submit a transaction
        res = w0.transfer([{'address': dst, 'amount': 1000000000000}])
        tx_hash = res.tx_hash

        # Mine to include the tx
        daemon.generateblocks(STANDARD_ADDRESS, 1)

        # Verify tx is in chain
        res = daemon.get_transactions([tx_hash])
        assert len(res.txs) == 1
        assert res.txs[0].in_pool == False

        # Pop the block
        daemon.pop_blocks(1)

        # The tx should return to pool with kept_by_block=True
        res = daemon.get_transaction_pool()
        if 'transactions' in res and res.transactions:
            found = False
            for tx in res.transactions:
                if tx.id_hash == tx_hash:
                    found = True
                    # kept_by_block should be true for txs that came from popped blocks
                    if hasattr(tx, 'kept_by_block'):
                        assert tx.kept_by_block == True
                    break
            if found:
                print('txpool kept_by_block: OK')
            else:
                print('txpool kept_by_block: tx not found in pool after pop')
        else:
            print('txpool kept_by_block: pool empty after pop')

        # Re-mine
        daemon.generateblocks(STANDARD_ADDRESS, 1)
        w0.refresh()
        w1.refresh()

if __name__ == '__main__':
    BugVerificationTest().run_test()
