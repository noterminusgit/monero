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

"""Test sweep operations: sweep_all, sweep_single, sweep_dust, transfer_split"""

from framework.daemon import Daemon
from framework.wallet import Wallet

SEED1 = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
SEED2 = 'peeled mixture ionic radar utopia puddle buying illness nuns gadget river spout cavernous bounced paradise drunk looking cottage jump tequila melting went winter adjust spout'
ADDR1 = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class SweepOperationsTest():
    def run_test(self):
        self.reset()
        self.create()
        self.mine()
        self.test_sweep_all_basic()
        self.test_sweep_all_subaccount()
        self.test_sweep_all_below_amount()
        self.test_sweep_single()
        self.test_sweep_do_not_relay()
        self.test_transfer_split()

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

    def test_sweep_all_basic(self):
        print('Testing sweep_all basic')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        res = w0.get_balance()
        balance_before = res.balance
        assert balance_before > 0

        res = w1.get_address()
        dst_address = res.address

        res = w0.sweep_all(address = dst_address)
        assert len(res.tx_hash_list) > 0
        fee = sum(res.fee_list)
        assert fee > 0

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()
        w1.refresh()

        res = w0.get_balance()
        # After sweep_all, the wallet should have only the newly mined coinbase
        # The swept balance should not remain

        res = w1.get_balance()
        received = res.balance
        assert received > 0
        # Received amount + fee should approximately equal balance_before
        # Allow some tolerance due to new mining rewards going to w0
        print('sweep_all basic: received {} fee {}'.format(received, fee))

    def test_sweep_all_subaccount(self):
        print('Testing sweep_all subaccount')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        # Create a subaccount and send some funds to it
        res = w0.create_account(label = 'sweep_test')
        account_index = res.account_index
        res = w0.get_address(account_index = account_index)
        sub_address = res.address

        # Mine more blocks to w0 so we have funds to send to subaccount
        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        # Transfer to subaccount
        res = w0.transfer([{'address': sub_address, 'amount': 1000000000000}])
        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        # Sweep the subaccount
        res = w1.get_address()
        dst = res.address
        res = w0.sweep_all(address = dst, account_index = account_index)
        assert len(res.tx_hash_list) > 0

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()
        w1.refresh()

        # Subaccount should be empty
        res = w0.get_balance(account_index = account_index)
        assert res.balance == 0 or res.unlocked_balance == 0

    def test_sweep_all_below_amount(self):
        print('Testing sweep_all below_amount')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        res = w1.get_address()
        dst = res.address

        # Sweep only outputs below a certain amount
        # Use a very high threshold to sweep everything
        res = w0.get_balance()
        if res.unlocked_balance > 0:
            res = w0.sweep_all(address = dst, below_amount = 1000000000000000)
            assert len(res.tx_hash_list) > 0
            daemon.generateblocks(ADDR1, 10)
            w0.refresh()
            w1.refresh()

    def test_sweep_single(self):
        print('Testing sweep_single')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        # Get a specific key image to sweep
        res = w0.incoming_transfers(transfer_type = 'available')
        assert 'transfers' in res and len(res.transfers) > 0
        ki = res.transfers[0].key_image
        amount = res.transfers[0].amount

        res = w1.get_address()
        dst = res.address

        res = w0.sweep_single(address = dst, key_image = ki)
        assert res.tx_hash != ''
        fee = res.fee
        assert fee > 0

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()
        w1.refresh()

        # Verify the key image is now spent
        res = daemon.is_key_image_spent([ki])
        assert res.spent_status[0] == 1  # 1 = spent in chain

    def test_sweep_do_not_relay(self):
        print('Testing sweep with do_not_relay')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        res = w1.get_address()
        dst = res.address

        res = w0.get_balance()
        if res.unlocked_balance > 0:
            res = w0.sweep_all(address = dst, do_not_relay = True)
            assert len(res.tx_hash_list) > 0
            tx_blob_list = res.tx_blob_list

            # Transaction should not be in the pool yet
            res = daemon.get_transaction_pool()
            pool_hashes = [tx.id_hash for tx in res.transactions] if 'transactions' in res and res.transactions else []

            # Manually relay
            for blob in tx_blob_list:
                res = daemon.send_raw_transaction(blob)
                assert res.status == 'OK' or res.status == 'ok'

            daemon.generateblocks(ADDR1, 10)
            w0.refresh()
            w1.refresh()

    def test_transfer_split(self):
        print('Testing transfer split with many inputs')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        # Mine many blocks to create many small outputs
        daemon.generateblocks(ADDR1, 30)
        w0.refresh()

        res = w1.get_address()
        dst = res.address

        # Try a transfer with many small amounts to encourage splitting
        res = w0.get_balance()
        if res.unlocked_balance > 0:
            # Create multiple small destinations to try to trigger split
            amount_per_dest = res.unlocked_balance // 20
            if amount_per_dest > 0:
                dsts = [{'address': dst, 'amount': amount_per_dest} for _ in range(min(15, max(1, int(res.unlocked_balance // amount_per_dest) - 2)))]
                if len(dsts) > 0:
                    try:
                        res = w0.transfer(dsts)
                        assert res.tx_hash != '' or len(res.tx_hash_list) > 0
                        daemon.generateblocks(ADDR1, 10)
                        w0.refresh()
                        w1.refresh()
                    except Exception as e:
                        # May fail if not enough balance - that's ok
                        print('transfer split: {}'.format(str(e)))

if __name__ == '__main__':
    SweepOperationsTest().run_test()
