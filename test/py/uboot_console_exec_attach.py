# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

from ubspawn import Spawn
from uboot_console_base import ConsoleBase

def cmdline(app, args):
    return app + ' "' + '" "'.join(args) + '"'

class ConsoleExecAttach(ConsoleBase):
    def __init__(self, log, config):
        # The max_fifo_fill value might need tweaking per-board/-SoC?
        # 1 would be safe anywhere, but is very slow (a pexpect issue?).
        # 16 is a common FIFO size.
        # HW flow control would mean this could be infinite.
        super(ConsoleExecAttach, self).__init__(log, config, max_fifo_fill=16)

        self.log.action("Flashing U-Boot")
        cmd = ["uboot-test-flash", config.board_type, config.board_identity]
        runner = self.log.get_runner(cmd[0])
        runner.run(cmd)
        runner.close()

    def get_spawn(self):
        args = [self.config.board_type, self.config.board_identity]
        s = Spawn(["uboot-test-console"] + args)

        self.log.action("Resetting board")
        cmd = ["uboot-test-reset"] + args
        runner = self.log.get_runner(cmd[0])
        runner.run(cmd)
        runner.close()

        return s
