# Copyright (c) 2017 Google, Inc
#
# SPDX-License-Identifier:	GPL-2.0+
#
# Master controller for buildman. It sends build requests to workers, receives
# the results and records them.


import socket
import sys
import time

import bsettings


class Master:
    def open(self, host):
        self.send_timeout_ms = 100
        self.recv_timeout_ms = 1000

        self.timed_out = False

        # Create a socket (SOCK_STREAM means a TCP socket)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        try:
            # Connect to server and send data
            #print 'connect', host, bsettings.worker_port
            self.sock.connect((host, bsettings.worker_port))
            '''
            for i in range(2):
                data = sys.argv[1 + i]
                sock.sendall(data + "\n")

                # Receive data from the server and shut down
                received = sock.recv(1024)

                print "Sent:     {}".format(data)
                print "Received: {}".format(received)
            '''
        except Exception as e:
            print e
            self.timeout('connect')
            self.sock = None

    def close(self):
        self.sock.close()

    def timeout(self, msg):
        print 'timeout', msg
        self.timed_out = True
        self.close()

    def send(self, cmd):
        data = bytearray(cmd + '\n')
        #print "sending data '%s'" % data
        tosend = len(data)
        stop_time = time.clock() + self.send_timeout_ms / 1000.0
        upto = 0
        while upto < tosend:
            if time.clock() > stop_time:
                self.timeout('send')
                return False
            done = self.sock.send(data[upto:])
            upto += done
        return True

    def recv(self):
        done = False
        data = bytearray()
        stop_time = time.clock() + self.recv_timeout_ms / 1000
        #print 'start recv'
        while not done:
            if time.clock() > stop_time:
                self.timeout('recv')
                return None
            recv = self.sock.recv(4096)
            #print 'master recv', recv
            data += recv
            if data[-1] == 10:
                done = True
                #print 'done'
            time.sleep(.1)
        #print 'out of loop'
        return data

    def cmd_ping(self):
        self.send('ping')
        return self.recv()

    def cmd_set_boards(self, boards):
        self.send('set_boards %s' % ' '.join(boards))
        return self.recv()


def Run():
    master = Master()
    return master.open('localhost')
