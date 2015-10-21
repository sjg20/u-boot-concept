# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

import time
from ubspawn import Spawn
from uboot_console_base import ConsoleBase

class ConsoleSandbox(ConsoleBase):
    def __init__(self, log, config):
        super(ConsoleSandbox, self).__init__(log, config, max_fifo_fill=1024)

    def get_spawn(self):
        return Spawn([self.config.build_dir + "/u-boot"])

    def kill(self, sig):
        self.ensure_spawned()
        self.log.action("kill %d" % sig)
        self.p.kill(sig)

    def validate_exited(self):
        p = self.p
        self.p = None
        for i in xrange(100):
            ret = not p.isalive()
            if ret:
                break
            time.sleep(0.1)
        p.close()
        return ret
