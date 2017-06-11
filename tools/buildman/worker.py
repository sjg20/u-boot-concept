# Copyright (c) 2017 Google, Inc
#
# SPDX-License-Identifier:	GPL-2.0+
#
# Worker server for buildman. It accepts build requests, does a build and
# returns the results

import threading
import SocketServer


HOST, PORT = '', 1968

class WorkerRequestHandler(SocketServer.BaseRequestHandler):
    """
    The request handler class for our server.

    It is instantiated once per connection to the server, and must
    override the handle() method to implement communication to the
    client.
    """
    def handle(self):
        # self.request is the TCP socket connected to the client
        cur_thread = threading.current_thread()
        while True:
            self.data = self.request.recv(1024).strip()
            if not self.data:
                break
            print '{} wrote:'.format(self.client_address[0])
            print cur_thread, self.data
            # just send back the same data, but upper-cased
            self.request.sendall(self.data.upper())
        print 'die', threading.active_count()


class WorkerServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
    pass


class Worker:
    def start(self):
        # Create the server, binding to localhost on port 9999
        server = WorkerServer((HOST, PORT), WorkerRequestHandler)

        # Activate the server; this will keep running until you
        # interrupt the program with Ctrl-C
        server.serve_forever()


def Run():
    worker = Worker()
    return worker.start()
