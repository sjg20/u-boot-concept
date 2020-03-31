# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#
# Main control for labman
#

from tools.labman.lab import Lab

def Labman(args):
    lab = None
    if args.lab:
        lab = Lab()
        lab.read(args.lab)

    if args.cmd == 'ls':
        lab.show_list()
    elif args.cmd == 'emit':
        lab.emit(args.dut, args.ftype)
    elif args.cmd == 'prov':
        lab.provision(args.component, args.name, args.serial)
