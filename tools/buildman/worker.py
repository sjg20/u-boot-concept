# Copyright (c) 2017 Google, Inc
#
# SPDX-License-Identifier:	GPL-2.0+
#
# Worker server for buildman. It accepts build requests, does a build and
# returns the results

import SocketServer
import threading
import time

import bsettings


class WorkerRequestHandler(SocketServer.BaseRequestHandler):
    def __init__(self, request, client_address, server):
        self.data = bytearray()
        SocketServer.BaseRequestHandler.__init__(self, request, client_address,
                                                 server)

    def send(self, data):
        self.request.sendall(data + '\n')

    def process(self):
        cmd = self.data.strip()
        #print 'got cmd', cmd
        if cmd == 'ping':
            self.send('pong')
        else:
            self.send('unknown command')

    """
    The request handler class for our server.

    It is instantiated once per connection to the server, and must
    override the handle() method to implement communication to the
    client.
    """
    def handle(self):
        # self.request is the TCP socket connected to the client
        cur_thread = threading.current_thread()
        #print 'new', cur_thread
        while True:
            data = self.request.recv(1024)
            if not data:
                break
            self.data += data
            #print 'worker data', self.data, self.data[-1]
            if self.data[-1] == 10:
                self.process()
            #print '{} wrote:'.format(self.client_address[0])
            #print cur_thread, self.data
            # just send back the same data, but upper-cased
            #self.request.sendall(self.data.upper())
        #print 'die', threading.active_count()


class WorkerServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
    pass


class Worker:
    def start(self):
        # Create the server, binding to localhost on port 9999
        SocketServer.TCPServer.allow_reuse_address = True
        self.server = WorkerServer(
                (bsettings.worker_host, bsettings.worker_port),
                WorkerRequestHandler)

        self.server_thread = threading.Thread(target=self.server.serve_forever)
        self.server_thread.daemon = True
        self.server_thread.start()

    def stop(self):
        self.server.shutdown()
        self.server.server_close()


def Run():
    worker = Worker()
    worker.start()
    while True:
        time.sleep(1)
