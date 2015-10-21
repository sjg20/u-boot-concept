# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

import os
import re
import pty
import select
import time

class Timeout(Exception):
    pass

class Spawn(object):
    def __init__(self, args):
        self.waited = False
        self.buf = ""
        self.logfile_read = None
        self.before = ""
        self.after = ""
        self.timeout = None

        (self.pid, self.fd) = pty.fork()
        if self.pid == 0:
            try:
                os.execvp(args[0], args)
            except:
                print "CHILD EXECEPTION:"
                import traceback
                traceback.print_exc()
            finally:
                os._exit(255)

        self.poll = select.poll()
        self.poll.register(self.fd, select.POLLIN | select.POLLPRI | select.POLLERR | select.POLLHUP | select.POLLNVAL)

    def kill(self, sig):
        os.kill(self.pid, sig)

    def isalive(self):
        if self.waited:
            return False

        w = os.waitpid(self.pid, os.WNOHANG)
        if w[0] == 0:
            return True

        self.waited = True
        return False

    def send(self, data):
        os.write(self.fd, data)

    def expect(self, patterns):
        for pi in xrange(len(patterns)):
            if type(patterns[pi]) == type(""):
                patterns[pi] = re.compile(patterns[pi])

        try:
            while True:
                earliest_m = None
                earliest_pi = None
                for pi in xrange(len(patterns)):
                    pattern = patterns[pi]
                    m = pattern.search(self.buf)
                    if not m:
                        continue
                    if earliest_m and m.start() > earliest_m.start():
                        continue
                    earliest_m = m
                    earliest_pi = pi
                if earliest_m:
                    pos = earliest_m.start()
                    posafter = earliest_m.end() + 1
                    self.before = self.buf[:pos]
                    self.after = self.buf[pos:posafter]
                    self.buf = self.buf[posafter:]
                    return earliest_pi
                events = self.poll.poll(self.timeout)
                if not events:
                    raise Timeout()
                c = os.read(self.fd, 1024)
                if not c:
                    raise EOFError()
                if self.logfile_read:
                    self.logfile_read.write(c)
                self.buf += c
        finally:
            if self.logfile_read:
                self.logfile_read.flush()

    def close(self):
        os.close(self.fd)
        for i in xrange(100):
            if not self.isalive():
                break
            time.sleep(0.1)
