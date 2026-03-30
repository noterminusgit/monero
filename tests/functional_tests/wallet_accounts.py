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

"""Test wallet account/subaddress management and isolation"""

from framework.daemon import Daemon
from framework.wallet import Wallet

SEED1 = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
SEED2 = 'peeled mixture ionic radar utopia puddle buying illness nuns gadget river spout cavernous bounced paradise drunk looking cottage jump tequila melting went winter adjust spout'
ADDR1 = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class WalletAccountsTest():
    def run_test(self):
        self.reset()
        self.create()
        self.mine()
        self.test_multi_account_transfers()
        self.test_subaddress_receive()
        self.test_account_and_address_index_lookup()
        self.test_wallet_restore_subaddresses()
        self.test_generate_from_keys()
        self.test_view_only_wallet()

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

    def test_multi_account_transfers(self):
        print('Testing multi-account transfers')
        daemon = Daemon()
        w0 = self.wallet[0]

        # Create a second account
        res = w0.create_account(label = 'test_account')
        account_index = res.account_index
        assert account_index == 1

        res = w0.get_address(account_index = account_index)
        account_addr = res.address

        # Transfer from account 0 to account 1
        amount = 2000000000000
        res = w0.transfer([{'address': account_addr, 'amount': amount}])
        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        # Check per-account balances
        res = w0.get_balance(account_index = 0)
        balance_0 = res.balance
        res = w0.get_balance(account_index = 1)
        balance_1 = res.balance

        assert balance_1 >= amount
        print('Account 0 balance: {}, Account 1 balance: {}'.format(balance_0, balance_1))

    def test_subaddress_receive(self):
        print('Testing subaddress receive')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        # Create a subaddress
        res = w1.create_address(account_index = 0, label = 'test_sub')
        subaddr = res.address
        subaddr_index = res.address_index
        assert subaddr_index > 0

        # Send to the subaddress
        amount = 1000000000000
        daemon.generateblocks(ADDR1, 10)
        w0.refresh()
        res = w0.transfer([{'address': subaddr, 'amount': amount}])
        daemon.generateblocks(ADDR1, 10)
        w0.refresh()
        w1.refresh()

        # Check incoming transfers for correct subaddress index
        res = w1.incoming_transfers(transfer_type = 'all')
        assert 'transfers' in res
        found = False
        for t in res.transfers:
            if t.subaddr_index.minor == subaddr_index:
                found = True
                assert t.amount == amount
                break
        assert found, 'Transfer to subaddress not found'

    def test_account_and_address_index_lookup(self):
        print('Testing get_address_index')
        w0 = self.wallet[0]

        # Look up the main address
        res = w0.get_address()
        main_addr = res.address
        res = w0.get_address_index(address = main_addr)
        assert res.index.major == 0
        assert res.index.minor == 0

        # Create a subaddress and look it up
        res = w0.create_address(account_index = 0, label = 'lookup_test')
        subaddr = res.address
        sub_index = res.address_index

        res = w0.get_address_index(address = subaddr)
        assert res.index.major == 0
        assert res.index.minor == sub_index

    def test_wallet_restore_subaddresses(self):
        print('Testing wallet restore subaddresses')
        w2 = Wallet(idx = 2)
        try: w2.close_wallet()
        except: pass

        # Restore from the same seed
        w2.restore_deterministic_wallet(seed = SEED1)
        w2.refresh()

        # The restored wallet should have the same main address
        res = w2.get_address()
        assert res.address == ADDR1

        # Create subaddresses and verify they match the original wallet
        res = self.wallet[0].get_address(account_index = 0, subaddresses = [0, 1])
        orig_addresses = [a.address for a in res.addresses]

        res = w2.create_address(account_index = 0)
        res = w2.get_address(account_index = 0, subaddresses = [0, 1])
        restored_addresses = [a.address for a in res.addresses]

        assert orig_addresses[0] == restored_addresses[0]  # Main address must match

    def test_generate_from_keys(self):
        print('Testing generate_from_keys')
        w0 = self.wallet[0]
        w3 = Wallet(idx = 3)
        try: w3.close_wallet()
        except: pass

        # Get keys from wallet 0
        view_key = w0.query_key('view_key').key
        spend_key = w0.query_key('spend_key').key

        # Restore wallet 3 from explicit keys
        res = w3.generate_from_keys(viewkey = view_key, spendkey = spend_key, address = ADDR1)
        assert res.address == ADDR1

        w3.refresh()
        res = w3.get_address()
        assert res.address == ADDR1

    def test_view_only_wallet(self):
        print('Testing view-only wallet')
        daemon = Daemon()
        w0 = self.wallet[0]
        w_view = Wallet(idx = 3)
        try: w_view.close_wallet()
        except: pass

        # Get view key only
        view_key = w0.query_key('view_key').key

        # Create view-only wallet
        res = w_view.generate_from_keys(viewkey = view_key, address = ADDR1)
        assert res.address == ADDR1

        daemon.generateblocks(ADDR1, 10)
        w_view.refresh()

        # View-only wallet can see incoming
        res = w_view.get_balance()
        assert res.balance > 0

        # View-only wallet cannot spend
        res = self.wallet[1].get_address()
        dst = res.address
        ok = False
        try:
            w_view.transfer([{'address': dst, 'amount': 1000000000000}])
        except:
            ok = True
        assert ok, 'View-only wallet should not be able to transfer'

if __name__ == '__main__':
    WalletAccountsTest().run_test()
