# SPDX-License-Identifier: GPL-2.0
# Copyright 2025 Simon Glass <sjg@chromum.org>

"""Implementation of a command-socket client

The command-socket, or cmdsock, can be used to control sandbox remotely without
relying on the cmdline interface.

When active, sandbox does not write to the console, unless it crashes. All
commands and outcome are passed over the cmdsock.
"""

import grpc
from io import BytesIO
import select
import socket
import time
from google.protobuf.internal.decoder import _DecodeVarint32  # For decoding varints

import cmdsock_pb2
#import cmdsock_pb2_grpc
# import pb_encode
# import pb_decode
import io

BUF_SIZE = 4096

SOCK = 0

# https://stackoverflow.com/questions/10917581/efficient-fifo-queue-for-arbitrarily-sized-chunks-of-bytes-in-python/10917767#10917767
class FifoFileBuffer():
    """Fast implementation of a bytewise FIFO"""

    def __init__(self, name):
        self.buf = BytesIO()
        self.available = 0    # Bytes available for reading
        self.size = 0
        self.write_fp = 0
        self.name = name

    def read(self, size = None):
        """Reads size bytes from buffer"""
        if size is None or size > self.available:
            size = self.available
        size = max(size, 0)

        result = self.buf.read(size)
        self.available -= size

        if len(result) < size:
            self.buf.seek(0)
            result += self.buf.read(size - len(result))

        return result

    def readline(self):
        """Reads a line buffer"""
        cur_pos = self.buf.tell()
        line = self.buf.readline()
        self.buf.seek(cur_pos)

        if not line.endswith(b'\n'):
            return b''

        # Drop \n from the string
        return self.read(len(line))[:-1]

    def look(self, size):
        """Looks ahead in the buffer"""
        cur_pos = self.buf.tell()
        avail = self.available

        result = self.read(size)
        # size = min(avail, size)
        # result = self.buf.read(size)
        self.buf.seek(cur_pos)
        self.available = avail
        # print(f'look {self.name} {len(result)}')

        return result

    def write(self, data):
        """Appends data to buffer"""
        # print(f'append {self.name} {len(data)}')
        orig_avail = self.available
        if self.size < self.available + len(data):
            # Expand buffer
            new_buf = BytesIO()
            new_buf.write(self.read())
            self.write_fp = self.available = new_buf.tell()
            read_fp = 0
            while self.size <= self.available + len(data):
                self.size = max(self.size, 1024) * 2
            new_buf.write(b'0' * (self.size - self.write_fp))
            self.buf = new_buf
        else:
            read_fp = self.buf.tell()

        self.buf.seek(self.write_fp)
        written = self.size - self.write_fp
        self.buf.write(data[:written])
        self.write_fp += len(data)
        self.available += len(data)
        if written < len(data):
            self.write_fp -= self.size
            self.buf.seek(0)
            self.buf.write(data[written:])
        self.buf.seek(read_fp)
        assert self.available == orig_avail + len(data)
        # print(f'available {self.name} {self.available}')


class Cmdsock:
    """Manage the cmdsock with a running U-Boot"""

    def __init__(self, sock_name):
        """Set up a connection to sandbox via a command-socket

        Args:
            name (str): Filename of socket

        Properties:
            sock_name (str): Socket name
            sock (socket.socket): Socket object
        """
        self.sock_name = sock_name
        self.sock = None
        self.inq = FifoFileBuffer('inq')
        self.outq = FifoFileBuffer('outq')
        self.chan = None
        self.stub = None
        global SOCK
        assert SOCK == 0
        SOCK + 1

    def connect_to_sandbox(self, poll, no_timeout=False):
        """Connect to sandbox over the cmdsock"""
        # self.chan = grpc.insecure_channel(f'unix://{self.sock_name}')
        # self.stub = cmdsock_pb2_grpc.CmdsockStub(chan)

        max_retries = 20
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        while max_retries:
            time.sleep(.2)
            try:
                self.sock.connect(self.sock_name)
                # print('tries left', max_retries)
                break
            except socket.error as exc:
                pass
            if not no_timeout:
                max_retries -= 1
                if not max_retries:
                    raise ValueError(
                        f'Error connecting to U-Boot sandbox at {self.sock_name}')
        # print('connected fd', self.sock)
        poll.register(self.sock, select.POLLIN |
                               select.POLLOUT | select.POLLPRI |
                               select.POLLERR | select.POLLHUP |
                               select.POLLNVAL)

    def read_banner(self):
        data = self.inq.look(BUF_SIZE)
        if not data:
            return None
        # print('data1', data)

        p = data.index(b'\n')
        if not p:
            return None
        to_read = p + 1
        data = self.inq.read(to_read)
        # print('data2', data, data[p])
        if data[p] != 0xa:  # newline
            raise ValueError("Internal error with 'hello' from server")
        return data[:p]

    def fail(self, msg):
        """Report a socket failure

        Args:
            msg (str): Message received
        """
        raise ValueError(f'Failed {msg}')

    def get_eventmask_str(self, mask):
        bits = {
            select.POLLIN: 'in',
            select.POLLPRI: 'pri',
            select.POLLOUT: 'out',
            select.POLLERR: 'err',
            select.POLLHUP: 'hup',
            select.POLLNVAL: 'inval',
            }

        out = []
        for bit, name in bits.items():
            if mask & bit:
                out.append(name)
        return ' '.join(out)

    def xfer_data(self, event_mask):
        # if event_mask != select.POLLOUT:
            # print(f'event_mask: {event_mask:x} {self.get_eventmask_str(event_mask)}')
        if event_mask & select.POLLHUP:
            self.fail('socket closed')
        if event_mask & select.POLLIN:
            data = self.sock.recv(BUF_SIZE)
            # print('  can recv', data)
            # print(f'wrote {len(data)} bytes into inq')
            self.inq.write(data)
            # print('  xfer recv', data)
        if event_mask & select.POLLOUT and self.outq.available:
            # print('  can send')
            data = self.outq.read(BUF_SIZE)
            if data:
                print(f'event_mask: {event_mask:x} {self.get_eventmask_str(event_mask)}')
                self.sock.send(data)

    def get_msgs(self):
        while True:
            # print('  call recv')
            msg = self.get_next_msg()
            if not msg:
                break
            yield msg

    def get_next_msg(self):
        # data = self.inq.read(BUF_SIZE)
        # print('data', data)
        '''
        line = self.inq.readline()
        if line:
            cmd, rest = line.split(maxsplit=1)
            if cmd == b'puts':
                size_str, rest = rest.split(maxsplit=1)
                size = int(size_str, 16)
                # print('puts', rest, len(rest), size)
                return rest
        '''
        data = self.inq.look(BUF_SIZE)
        data2 = self.inq.look(BUF_SIZE)
        assert data == data2

        # print('   get_next_msg', data)
        # time.sleep(.05)
        if not data:
            # print('no data', data)
            return None
        # print('recv data', len(data))
        try:
            size, pos = _DecodeVarint32(data, 0)
        except IndexError:
            # Need more data for the length prefix
            # print('need more', len(data), data)
            return None
        # print('pos', pos, size)
        to_read = pos + size
        if len(data) < to_read:
            # Need more data for message
            # print('need more2')
            # print('need more2', len(data), data)
            return None

        data = self.inq.read(to_read)
        data2 = self.inq.look(BUF_SIZE)
        # print(f'to_read {to_read} data: {data}         remaining {data2}')
        msg = cmdsock_pb2.Message()
        msg.ParseFromString(data[pos:])
        # google.protobuf.message.DecodeError
        # print(f"msgx '{msg}' data '{data}'")
        return msg

    def send(self, msg):
        """Send some data to the cmdsock

        Args:
            msg (cmdsock_pb2 object): Data to send
        """
        data = msg.SerializeToString()
        # print('send', len(data), data)
        self.outq.write(data)

    def start(self):
        """Tell U-Boot to start, which shows a banner and other messages"""
        # buffer = bytearray(100)  # Create a buffer to hold the encoded message
        # stream = pb_encode.pb_ostream_from_buffer(buffer, len(buffer))
        # pb_encode.pb_encode(stream, cmdsock_pb2.MyMessage.DESCRIPTOR, message)
        # encoded_message = bytes(buffer[:stream.bytes_written])
        # desc = cmdsock_pb2.Example.DESCRIPTOR
        msg = cmdsock_pb2.Message()
        msg.start_req.name = 'pytest'
        # print('send msg', msg)
        self.send(msg)

    def run_command(self, cmd):
        msg = cmdsock_pb2.Message()
        msg.run_cmd_req.cmd = cmd
        msg.run_cmd_req.flag = 0
        # print('send msg', msg)
        self.send(msg)
