# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

import multiplexed_log
import os
import pytest
import re
import sys

pattern_uboot_spl_signon = re.compile("(U-Boot SPL \\d{4}\\.\\d{2}-[^\r\n]*)")
pattern_uboot_main_signon = re.compile("(U-Boot \\d{4}\\.\\d{2}-[^\r\n]*)")
pattern_stop_autoboot_prompt = re.compile("Hit any key to stop autoboot: ")
pattern_unknown_command = re.compile("Unknown command '.*' - try 'help'")
pattern_error_notification = re.compile("## Error: ")

class ConsoleDisableCheck(object):
    def __init__(self, console, check_type):
        self.console = console
        self.check_type = check_type

    def __enter__(self):
        self.console.disable_check_count[self.check_type] += 1

    def __exit__(self, extype, value, traceback):
        self.console.disable_check_count[self.check_type] -= 1

class ConsoleBase(object):
    def __init__(self, log, config, max_fifo_fill):
        self.log = log
        self.config = config
        self.max_fifo_fill = max_fifo_fill

        self.logstream = self.log.get_stream("console", sys.stdout)

        # Array slice removes leading/trailing quotes
        self.prompt = self.config.buildconfig["config_sys_prompt"][1:-1]
        self.prompt_escaped = re.escape(self.prompt)
        self.p = None
        self.disable_check_count = {
            "spl_signon": 0,
            "main_signon": 0,
            "unknown_command": 0,
            "error_notification": 0,
        }

        self.at_prompt = False
        self.at_prompt_logevt = None
        self.ram_base = None

    def close(self):
        if self.p:
            self.p.close()
        self.logstream.close()

    def run_command(self, cmd, wait_for_echo=True, send_nl=True, wait_for_prompt=True):
        self.ensure_spawned()

        if self.at_prompt and \
                self.at_prompt_logevt != self.logstream.logfile.cur_evt:
            self.logstream.write(self.prompt, implicit=True)

        bad_patterns = []
        bad_pattern_ids = []
        if (self.disable_check_count["spl_signon"] == 0 and
                self.uboot_spl_signon):
            bad_patterns.append(self.uboot_spl_signon_escaped)
            bad_pattern_ids.append("SPL signon")
        if self.disable_check_count["main_signon"] == 0:
            bad_patterns.append(self.uboot_main_signon_escaped)
            bad_pattern_ids.append("U-Boot main signon")
        if self.disable_check_count["unknown_command"] == 0:
            bad_patterns.append(pattern_unknown_command)
            bad_pattern_ids.append("Unknown command")
        if self.disable_check_count["error_notification"] == 0:
            bad_patterns.append(pattern_error_notification)
            bad_pattern_ids.append("Error notification")
        try:
            self.at_prompt = False
            if send_nl:
                cmd += "\n"
            while cmd:
                # Limit max outstanding data, so UART FIFOs don't overflow
                chunk = cmd[:self.max_fifo_fill]
                cmd = cmd[self.max_fifo_fill:]
                self.p.send(chunk)
                if not wait_for_echo:
                    continue
                chunk = re.escape(chunk)
                chunk = chunk.replace("\\\n", "[\r\n]")
                m = self.p.expect([chunk] + bad_patterns)
                if m != 0:
                    self.at_prompt = False
                    raise Exception("Bad pattern found on console: " +
                                    bad_pattern_ids[m - 1])
            if not wait_for_prompt:
                return
            m = self.p.expect([self.prompt_escaped] + bad_patterns)
            if m != 0:
                self.at_prompt = False
                raise Exception("Bad pattern found on console: " +
                                bad_pattern_ids[m - 1])
            self.at_prompt = True
            self.at_prompt_logevt = self.logstream.logfile.cur_evt
            # Only strip \r\n; space/TAB might be significant if testing
            # indentation.
            return self.p.before.strip("\r\n")
        except Exception as ex:
            self.log.error(str(ex))
            self.cleanup_spawn()
            raise

    def ctrlc(self):
        self.run_command(chr(3), wait_for_echo=False, send_nl=False)

    def ensure_spawned(self):
        if self.p:
            return
        try:
            self.at_prompt = False
            self.log.action("Starting U-Boot")
            self.p = self.get_spawn()
            # Real targets can take a long time to scroll large amounts of
            # text if LCD is enabled. This value may need tweaking in the
            # future, possibly per-test to be optimal. This works for "help"
            # on board "seaboard".
            self.p.timeout = 30000
            self.p.logfile_read = self.logstream
            if self.config.buildconfig.get("CONFIG_SPL", False) == "y":
                self.p.expect([pattern_uboot_spl_signon])
                self.uboot_spl_signon = self.p.after
                self.uboot_spl_signon_escaped = re.escape(self.p.after)
            else:
                self.uboot_spl_signon = None
            self.p.expect([pattern_uboot_main_signon])
            self.uboot_main_signon = self.p.after
            self.uboot_main_signon_escaped = re.escape(self.p.after)
            while True:
                match = self.p.expect([self.prompt_escaped,
                                       pattern_stop_autoboot_prompt])
                if match == 1:
                    self.p.send(chr(3)) # CTRL-C
                    continue
                break
            self.at_prompt = True
            self.at_prompt_logevt = self.logstream.logfile.cur_evt
        except Exception as ex:
            self.log.error(str(ex))
            self.cleanup_spawn()
            raise

    def cleanup_spawn(self):
        try:
            if self.p:
                self.p.close()
        except:
            pass
        self.p = None

    def validate_main_signon_in_text(self, text):
        assert(self.uboot_main_signon in text)

    def disable_check(self, check_type):
        return ConsoleDisableCheck(self, check_type)

    def find_ram_base(self):
        if self.config.buildconfig.get("config_cmd_bdi", "n") != "y":
            pytest.skip("bdinfo command not supported")
        if self.ram_base == -1:
            pytest.skip("Previously failed to find RAM bank start")
        if self.ram_base is not None:
            return self.ram_base

        with self.log.section("find_ram_base"):
            response = self.run_command("bdinfo")
            for l in response.split("\n"):
                if "-> start" in l:
                    self.ram_base = int(l.split("=")[1].strip(), 16)
                    break
            if self.ram_base is None:
                self.ram_base = -1
                raise Exception("Failed to find RAM bank start in `bdinfo`")

        return self.ram_base
