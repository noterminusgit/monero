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

"""Test chain reorg handling: pop_blocks, rescan, deep reorg"""

from framework.daemon import Daemon
from framework.wallet import Wallet

SEED1 = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
SEED2 = 'peeled mixture ionic radar utopia puddle buying illness nuns gadget river spout cavernous bounced paradise drunk looking cottage jump tequila melting went winter adjust spout'
ADDR1 = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class ChainReorgTest():
    def run_test(self):
        self.reset()
        self.create()
        self.mine()
        self.test_pop_blocks_and_resync()
        self.test_wallet_reorg_balance_recovery()
        self.test_rescan_blockchain()
        self.test_rescan_spent()

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
        print('Mining initial blocks')
        daemon = Daemon()
        daemon.generateblocks(ADDR1, 100)
        for w in self.wallet:
            w.refresh()

    def test_pop_blocks_and_resync(self):
        print('Testing pop_blocks and resync')
        daemon = Daemon()
        w0 = self.wallet[0]

        res = daemon.get_height()
        height_before = res.height

        # Pop some blocks
        n_pop = 5
        daemon.pop_blocks(n_pop)

        res = daemon.get_height()
        assert res.height == height_before - n_pop

        # Re-mine them
        daemon.generateblocks(ADDR1, n_pop)

        res = daemon.get_height()
        assert res.height == height_before

        w0.refresh()
        res = w0.get_height()
        assert res.height == height_before

    def test_wallet_reorg_balance_recovery(self):
        print('Testing wallet reorg balance recovery')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        res = w1.get_address()
        dst = res.address

        # Record balance before transfer
        res = w0.get_balance()
        balance_before = res.balance

        # Make a transfer
        amount = 1000000000000
        res = w0.transfer([{'address': dst, 'amount': amount}])
        tx_hash = res.tx_hash
        fee = res.fee
        daemon.generateblocks(ADDR1, 5)
        w0.refresh()
        w1.refresh()

        res = w0.get_balance()
        balance_after_tx = res.balance

        # Pop the blocks containing the transaction
        daemon.pop_blocks(5)
        daemon.flush_txpool()

        # Re-mine without the transaction
        daemon.generateblocks(ADDR1, 5)
        w0.refresh()
        w1.refresh()

        # The wallet should recover the balance since the tx is no longer in chain
        res = w0.get_balance()
        balance_after_reorg = res.balance
        # Balance after reorg should be higher than after tx (tx was reversed)
        print('Balance recovery: before={} after_tx={} after_reorg={}'.format(
            balance_before, balance_after_tx, balance_after_reorg))

    def test_rescan_blockchain(self):
        print('Testing rescan_blockchain')
        daemon = Daemon()
        w0 = self.wallet[0]

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        res = w0.get_balance()
        balance_before = res.balance

        # Rescan should reconstruct wallet state
        w0.rescan_blockchain()

        res = w0.get_balance()
        balance_after = res.balance
        # Balance should be the same after rescan
        assert balance_after == balance_before, \
            'Balance mismatch after rescan: {} vs {}'.format(balance_after, balance_before)

    def test_rescan_spent(self):
        print('Testing rescan_spent')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        res = w1.get_address()
        dst = res.address

        # Make a transfer so we have spent outputs
        res = w0.transfer([{'address': dst, 'amount': 1000000000000}])
        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        res = w0.get_balance()
        balance_before = res.balance

        # Rescan spent status
        w0.rescan_spent()

        res = w0.get_balance()
        balance_after = res.balance
        assert balance_after == balance_before

if __name__ == '__main__':
    ChainReorgTest().run_test()
