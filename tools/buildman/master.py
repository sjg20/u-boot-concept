# Copyright (c) 2017 Google, Inc
#
# SPDX-License-Identifier:	GPL-2.0+
#
# Master controller for buildman. It sends build requests to workers, receives
# the results and records them.


import socket
import sys


HOST, PORT = '', 1968

class Master:
    def start(self):
        # Create a socket (SOCK_STREAM means a TCP socket)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        try:
            # Connect to server and send data
            sock.connect((HOST, PORT))
            for i in range(2):
                data = sys.argv[1 + i]
                sock.sendall(data + "\n")

                # Receive data from the server and shut down
                received = sock.recv(1024)

                print "Sent:     {}".format(data)
                print "Received: {}".format(received)
        finally:
            sock.close()


def Run():
    master = Master()
    return master.start()
