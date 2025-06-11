
import socket
import time

import cmdsock_pb2
from google.protobuf.internal.decoder import _DecodeVarint32  # For decoding varints

sock_name = '/tmp/xxx'
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(sock_name)

# desc = cmdsock_pb2.Example.DESCRIPTOR
'''
msg = cmdsock_pb2.Message()
msg.str = 'start'
data = msg.SerializeToString()
sock.send(data)
'''
msg = cmdsock_pb2.Message()
print('msg', msg)
msg.start_req.name = 'pytest'
data = msg.SerializeToString()
sock.send(data)


buf = b''

while True:
    data = sock.recv(20)
    # if data:
        # print('data', data)

    buf += data
    while True:
        try:
            size, new_pos = _DecodeVarint32(buf, 0)
        except IndexError:
            # Need more data for the length prefix
            # print('need more1')
            break

        if len(buf) < new_pos + size:
            # Need more data for message
            # print('need more2')
            break

        mdata = buf[new_pos:new_pos + size]

        msg = cmdsock_pb2.Message()
        val = msg.ParseFromString(mdata)
        print('val', msg)
        buf = buf[new_pos + size:]


time.sleep(1)
