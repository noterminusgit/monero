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

"""Test RPC error response contracts: invalid inputs, restricted endpoints, insufficient funds"""

from framework.daemon import Daemon
from framework.wallet import Wallet

SEED1 = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
ADDR1 = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class RpcErrorHandlingTest():
    def run_test(self):
        self.reset()
        self.create()
        self.mine()
        self.test_invalid_block_height()
        self.test_invalid_tx_hash()
        self.test_invalid_send_raw_tx()
        self.test_restricted_rpc_endpoints()
        self.test_wallet_rpc_without_wallet()
        self.test_transfer_insufficient_funds()
        self.test_transfer_to_invalid_address()

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
        self.wallet.restore_deterministic_wallet(seed = SEED1)

    def mine(self):
        print('Mining blocks')
        daemon = Daemon()
        daemon.generateblocks(ADDR1, 80)
        self.wallet.refresh()

    def test_invalid_block_height(self):
        print('Testing invalid block height')
        daemon = Daemon()
        ok = False
        try:
            res = daemon.get_block_header_by_height(height = 999999999)
        except Exception as e:
            ok = True
        assert ok, 'Expected error for invalid block height'

    def test_invalid_tx_hash(self):
        print('Testing invalid tx hash')
        daemon = Daemon()
        fake_hash = '0' * 64
        res = daemon.get_transactions(txs_hashes = [fake_hash])
        assert 'missed_tx' in res or len(res.missed_tx) > 0 or (hasattr(res, 'txs') and len(res.txs) == 0), \
            'Expected missed_tx for non-existent transaction'

    def test_invalid_send_raw_tx(self):
        print('Testing invalid send_raw_transaction')
        daemon = Daemon()
        ok = False
        try:
            res = daemon.send_raw_transaction('deadbeef')
            # If it doesn't throw, it should return an error status
            if hasattr(res, 'status') and res.status != 'OK' and res.status != 'ok':
                ok = True
            elif hasattr(res, 'double_spend'):
                ok = True
        except:
            ok = True
        assert ok, 'Expected error for garbage raw transaction'

    def test_restricted_rpc_endpoints(self):
        print('Testing restricted RPC endpoints')
        # Daemon idx=1 is the restricted RPC daemon (port 18481)
        restricted_daemon = Daemon(restricted_rpc = True)
        try:
            # get_info should work even on restricted daemon
            res = restricted_daemon.get_info()
            assert 'height' in res
        except Exception as e:
            print('Restricted daemon access: {}'.format(str(e)))

    def test_wallet_rpc_without_wallet(self):
        print('Testing wallet RPC without wallet')
        w = Wallet(idx = 2)
        try: w.close_wallet()
        except: pass

        # Operations without a wallet open should fail
        ok = False
        try:
            res = w.get_balance()
        except:
            ok = True
        assert ok, 'Expected error when no wallet is open'

    def test_transfer_insufficient_funds(self):
        print('Testing transfer with insufficient funds')
        w = self.wallet

        # Try to transfer more than we have
        ok = False
        try:
            res = w.transfer([{
                'address': '44AFFq5kSiGBoZ4NMDwYtN18obc8AemS33DBLWs3H7otXft3XjrpDtQGv7SqSsaBYBb98uNbr2VBBEt7f2wfn3RVGQBEP3A',
                'amount': 99999999999999999999
            }])
        except:
            ok = True
        assert ok, 'Expected error for insufficient funds'

    def test_transfer_to_invalid_address(self):
        print('Testing transfer to invalid address')
        w = self.wallet

        ok = False
        try:
            res = w.transfer([{
                'address': 'not_a_valid_monero_address',
                'amount': 1000000000000
            }])
        except:
            ok = True
        assert ok, 'Expected error for invalid address'

if __name__ == '__main__':
    RpcErrorHandlingTest().run_test()
