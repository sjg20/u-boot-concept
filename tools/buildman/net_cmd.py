#
# Copyright (c) 2017 Google, Inc
#
# SPDX-License-Identifier:      GPL-2.0+
#
# Network commands

class Cmd(object):
    def __init__(self, sender):
        self.sender = sender

    def parse(self):
        pass

    @classmethod
    def parse_to_cmd(self, sender, data):
        #print 'parse %s' % data
        line = str(data.strip())
        parts = line.split(' ', 1)
        cmd_name = parts[0]
        self.rest = parts[1] if len(parts) > 1 else ''
        for try_cmd in commands:
            if cmd_name == try_cmd.get_name():
                cmd = try_cmd(sender)
                cmd.parse()
                return cmd


class CmdPing(Cmd):
    def __init__(self, sender):
        super(self.__class__, self).__init__(sender)

    @staticmethod
    def get_name():
        return 'ping'

    def send(self):
        self.sender.send('ping')

    def run(self):
        self.sender.send('pong')


class CmdSetBoards(Cmd):
    def __init__(self, sender, boards=None):
        super(self.__class__, self).__init__(sender)
        self.boards = boards

    @staticmethod
    def get_name():
        return 'set_boards'

    def send(self):
        self.sender.send('set_boards %s' % ' '.join(self.boards))

    def parse(self):
        self.boards = self.rest.split()

    def run(self):
        self.sender.send('ok')


commands = [CmdPing, CmdSetBoards]
