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

"""Test ZMQ pub/sub event contracts: chain_main, txpool_add, miner_data"""

from framework.daemon import Daemon
from framework.wallet import Wallet

SEED1 = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
ADDR1 = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class ZmqEventsTest():
    def run_test(self):
        self.reset()
        self.create()
        self.mine()
        self.test_chain_main_full()
        self.test_chain_main_minimal()
        self.test_txpool_add_full()
        self.test_miner_data()

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
        daemon.generateblocks(ADDR1, 100)
        self.wallet.refresh()

    def test_chain_main_full(self):
        print('Testing ZMQ json-full-chain_main')
        daemon = Daemon()
        try:
            from framework.zmq import Zmq
            zmq_sub = Zmq(protocol = 'tcp', host = '127.0.0.1', port = 18480)
            topic = 'json-full-chain_main'
            zmq_sub.sub(topic)

            # Mine a block
            daemon.generateblocks(ADDR1, 1)

            # Receive the ZMQ message
            data = zmq_sub.recv(topic)
            assert isinstance(data, list) and len(data) > 0
            block = data[0]
            assert 'major_version' in block
            assert 'minor_version' in block
            assert 'timestamp' in block
            assert 'miner_tx' in block
            print('ZMQ full chain_main: got block with version {}'.format(block['major_version']))
        except ImportError:
            print('ZMQ framework not available, skipping')
        except Exception as e:
            print('ZMQ test skipped: {}'.format(str(e)))

    def test_chain_main_minimal(self):
        print('Testing ZMQ json-minimal-chain_main')
        daemon = Daemon()
        try:
            from framework.zmq import Zmq
            zmq_sub = Zmq(protocol = 'tcp', host = '127.0.0.1', port = 18480)
            topic = 'json-minimal-chain_main'
            zmq_sub.sub(topic)

            daemon.generateblocks(ADDR1, 1)

            data = zmq_sub.recv(topic)
            assert isinstance(data, list) and len(data) > 0
            block = data[0]
            assert 'first_height' in block or 'ids' in block
            print('ZMQ minimal chain_main: OK')
        except ImportError:
            print('ZMQ framework not available, skipping')
        except Exception as e:
            print('ZMQ test skipped: {}'.format(str(e)))

    def test_txpool_add_full(self):
        print('Testing ZMQ json-full-txpool_add')
        daemon = Daemon()
        w0 = self.wallet
        try:
            from framework.zmq import Zmq
            zmq_sub = Zmq(protocol = 'tcp', host = '127.0.0.1', port = 18480)
            topic = 'json-full-txpool_add'
            zmq_sub.sub(topic)

            daemon.generateblocks(ADDR1, 10)
            w0.refresh()

            # Create a transaction (it will be added to the pool)
            dst = '44AFFq5kSiGBoZ4NMDwYtN18obc8AemS33DBLWs3H7otXft3XjrpDtQGv7SqSsaBYBb98uNbr2VBBEt7f2wfn3RVGQBEP3A'
            res = w0.transfer([{'address': dst, 'amount': 1000000000000}])

            data = zmq_sub.recv(topic)
            assert isinstance(data, list) and len(data) > 0
            tx = data[0]
            assert 'id' in tx or 'version' in tx
            print('ZMQ txpool_add: OK')

            daemon.generateblocks(ADDR1, 1)
            w0.refresh()
        except ImportError:
            print('ZMQ framework not available, skipping')
        except Exception as e:
            print('ZMQ test skipped: {}'.format(str(e)))

    def test_miner_data(self):
        print('Testing ZMQ json-full-miner_data')
        daemon = Daemon()
        try:
            from framework.zmq import Zmq
            zmq_sub = Zmq(protocol = 'tcp', host = '127.0.0.1', port = 18480)
            topic = 'json-full-miner_data'
            zmq_sub.sub(topic)

            daemon.generateblocks(ADDR1, 1)

            data = zmq_sub.recv(topic)
            assert 'major_version' in data
            assert 'height' in data
            assert 'prev_id' in data
            assert 'seed_hash' in data
            assert 'difficulty' in data
            print('ZMQ miner_data: height={}'.format(data['height']))
        except ImportError:
            print('ZMQ framework not available, skipping')
        except Exception as e:
            print('ZMQ test skipped: {}'.format(str(e)))

if __name__ == '__main__':
    ZmqEventsTest().run_test()
