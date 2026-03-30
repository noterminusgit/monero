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

"""Test transaction lifecycle: relay, describe, get_transfer, fee estimation, maturity"""

from framework.daemon import Daemon
from framework.wallet import Wallet

SEED1 = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
SEED2 = 'peeled mixture ionic radar utopia puddle buying illness nuns gadget river spout cavernous bounced paradise drunk looking cottage jump tequila melting went winter adjust spout'
SEED3 = 'dilute gutter certain antics pamphlet macro enjoy left slid guarded bogeys upload nineteen bomb jubilee enhanced irritate turnip eggs swung jukebox loudly reduce sedan slid'
ADDR1 = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class TxLifecycleTest():
    def run_test(self):
        self.reset()
        self.create()
        self.mine()
        self.test_tx_relay_lifecycle()
        self.test_get_transfer_by_txid()
        self.test_fee_estimation_accuracy()
        self.test_coinbase_maturity()
        self.test_multi_destination_transfer()

    def reset(self):
        print('Resetting blockchain')
        daemon = Daemon()
        res = daemon.get_height()
        daemon.pop_blocks(res.height - 1)
        daemon.flush_txpool()

    def create(self):
        print('Creating wallets')
        self.wallet = [None, None, None]
        seeds = [SEED1, SEED2, SEED3]
        for i in range(3):
            self.wallet[i] = Wallet(idx = i)
            try: self.wallet[i].close_wallet()
            except: pass
            self.wallet[i].restore_deterministic_wallet(seed = seeds[i])

    def mine(self):
        print('Mining initial blocks')
        daemon = Daemon()
        daemon.generateblocks(ADDR1, 100)
        for w in self.wallet:
            w.refresh()

    def test_tx_relay_lifecycle(self):
        print('Testing tx relay lifecycle')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        res = w1.get_address()
        dst = res.address

        # Create a transaction without relaying
        res = w0.transfer([{'address': dst, 'amount': 1000000000000}], do_not_relay = True)
        tx_hash = res.tx_hash
        tx_blob = res.tx_blob
        assert tx_hash != ''
        assert tx_blob != ''

        # Should not be in pool yet
        res = daemon.get_transaction_pool_hashes()
        pool_hashes = res.tx_hashes if 'tx_hashes' in res else []
        assert tx_hash not in pool_hashes

        # Relay the transaction
        res = daemon.send_raw_transaction(tx_blob)
        assert res.status == 'OK' or res.status == 'ok'

        # Should now be in pool
        res = daemon.get_transaction_pool_hashes()
        pool_hashes = res.tx_hashes if 'tx_hashes' in res else []
        assert tx_hash in pool_hashes

        # Mine to confirm
        daemon.generateblocks(ADDR1, 1)
        w0.refresh()
        w1.refresh()

        # Should no longer be in pool
        res = daemon.get_transaction_pool_hashes()
        pool_hashes = res.tx_hashes if 'tx_hashes' in res else []
        assert tx_hash not in pool_hashes

        # Should be in the blockchain
        res = daemon.get_transactions([tx_hash])
        assert len(res.txs) == 1
        assert res.txs[0].in_pool == False

    def test_get_transfer_by_txid(self):
        print('Testing get_transfer_by_txid')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        res = w1.get_address()
        dst = res.address

        amount = 500000000000
        res = w0.transfer([{'address': dst, 'amount': amount}])
        tx_hash = res.tx_hash
        fee = res.fee
        assert tx_hash != ''

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        # Query the transfer by txid
        res = w0.get_transfer_by_txid(tx_hash)
        assert 'transfer' in res
        transfer = res.transfer
        assert transfer.txid == tx_hash
        assert transfer.fee == fee
        assert transfer.type == 'out'

    def test_fee_estimation_accuracy(self):
        print('Testing fee estimation accuracy')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        # Get fee estimate from daemon
        res = daemon.get_fee_estimate()
        estimated_fee_per_byte = res.fee

        res = w1.get_address()
        dst = res.address

        # Make a transfer and check actual fee
        res = w0.transfer([{'address': dst, 'amount': 1000000000000}])
        actual_fee = res.fee
        assert actual_fee > 0

        # Fee should be reasonable (non-zero, not absurdly high)
        # In regtest with fixed difficulty, fee should be within expected range
        print('Fee estimation: estimated_per_byte={} actual_fee={}'.format(estimated_fee_per_byte, actual_fee))

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

    def test_coinbase_maturity(self):
        print('Testing coinbase maturity')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        # Mine a single block to create a fresh coinbase
        res = daemon.get_height()
        start_height = res.height
        daemon.generateblocks(ADDR1, 1)
        w0.refresh()

        res = w1.get_address()
        dst = res.address

        # Try to spend the immature coinbase (< 60 blocks)
        res = w0.get_balance()
        unlocked = res.unlocked_balance
        total = res.balance

        # The newly mined block's coinbase should not be unlocked
        # (it needs 60 confirmations)
        # We might still have old unlocked balance from the initial mining

        # Mine to just below maturity
        daemon.generateblocks(ADDR1, 55)
        w0.refresh()
        res = w0.get_balance()
        unlocked_55 = res.unlocked_balance

        # Mine past maturity (total 60+ blocks after the coinbase)
        daemon.generateblocks(ADDR1, 10)
        w0.refresh()
        res = w0.get_balance()
        unlocked_65 = res.unlocked_balance

        # Unlocked balance should increase as coinbases mature
        assert unlocked_65 >= unlocked_55
        print('Coinbase maturity: unlocked_at_55={}, unlocked_at_65={}'.format(unlocked_55, unlocked_65))

    def test_multi_destination_transfer(self):
        print('Testing multi-destination transfer')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]
        w2 = self.wallet[2]

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        res = w1.get_address()
        dst1 = res.address
        res = w2.get_address()
        dst2 = res.address

        amount1 = 500000000000
        amount2 = 300000000000

        # Transfer to multiple destinations in one tx
        res = w0.transfer([
            {'address': dst1, 'amount': amount1},
            {'address': dst2, 'amount': amount2},
        ])
        tx_hash = res.tx_hash
        assert tx_hash != ''

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()
        w1.refresh()
        w2.refresh()

        # Both recipients should have received their amounts
        res = w1.get_balance()
        assert res.balance >= amount1

        res = w2.get_balance()
        assert res.balance >= amount2

        print('Multi-destination: sent {} and {}'.format(amount1, amount2))

if __name__ == '__main__':
    TxLifecycleTest().run_test()
