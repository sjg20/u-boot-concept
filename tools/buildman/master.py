# Copyright (c) 2017 Google, Inc
#
# SPDX-License-Identifier:	GPL-2.0+
#
# Master controller for buildman. It sends build requests to workers, receives
# the results and records them.

import errno
import socket
import sys
import time

import bsettings
import net_cmd


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
            print 'master exception', e
            self.timeout('connect')
            self.sock = None
        self.sock.setblocking(0)

    def close(self):
        if self.sock:
            self.sock.close()

    def timeout(self, msg):
        #print 'master timeout', msg
        self.timed_out = True
        self.close()

    def send(self, cmd):
        data = bytearray(cmd + '\n')
        #print "master sending data '%s'" % data
        tosend = len(data)
        stop_time = time.time() + self.send_timeout_ms / 1000.0
        upto = 0
        while upto < tosend:
            if time.time() > stop_time:
                self.timeout('send')
                return False
            done = self.sock.send(data[upto:])
            upto += done
        #print 'master sent %s' % upto
        return True

    def recv(self):
        done = False
        data = bytearray()
        stop_time = time.time() + self.recv_timeout_ms / 1000.0
        #print 'start recv'
        while not done:
            #print time.time(), stop_time
            if time.time() > stop_time:
                self.timeout('recv')
                return None
            try:
                recv = self.sock.recv(4096)
            except IOError as e:
                if e.errno == errno.EAGAIN:
                    recv = ''
                else:
                    raise

            #if recv:
            #print 'master recv', recv
            data += recv
            if data and data[-1] == 10:
                done = True
                #print 'done'
            time.sleep(.1)
        #print 'out of loop'
        return data


def Run():
    master = Master()
    return master.open('localhost')
