#
# Copyright (c) 2017 Google, Inc
#
# SPDX-License-Identifier:      GPL-2.0+
#
# Network commands

class Cmd(object):
    def __init__(self, sender):
        self.sender = sender


class CmdPing(Cmd):
    def send(self):
        self.sender.send('ping')

class CmdSetBoards(Cmd):
    def __init__(self, sender, boards):
        super(self.__class__, self).__init__(sender)
        self.boards = boards

    def send(self):
        self.sender.send('set_boards %s' % ' '.join(self.boards))
