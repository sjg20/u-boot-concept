# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

import cgi
import os.path
import shutil
import subprocess

mod_dir = os.path.dirname(os.path.abspath(__file__))

class LogfileStream(object):
    def __init__(self, logfile, name, chained_file):
        self.logfile = logfile
        self.name = name
        self.chained_file = chained_file

    def close(self):
        pass

    def write(self, data, implicit=False):
        self.logfile.write(self, data, implicit)
        if self.chained_file:
            self.chained_file.write(data)

    def flush(self):
        self.logfile.flush()
        if self.chained_file:
            self.chained_file.flush()

class RunAndLog(object):
    def __init__(self, logfile, name, chained_file):
        self.logfile = logfile
        self.name = name
        self.chained_file = chained_file

    def close(self):
        pass

    def run(self, cmd, cwd=None):
        msg = "+" + " ".join(cmd) + "\n"
        if self.chained_file:
            self.chained_file.write(msg)
        self.logfile.write(self, msg)

        try:
            p = subprocess.Popen(cmd, cwd=cwd,
                stdin=None, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (output, stderr) = p.communicate()
            status = p.returncode
        except subprocess.CalledProcessError as cpe:
            output = cpe.output
            status = cpe.returncode
        self.logfile.write(self, output)
        if status:
            if self.chained_file:
                self.chained_file.write(output)
            raise Exception("command failed; exit code " + str(status))

class SectionCtxMgr(object):
    def __init__(self, log, marker):
        self.log = log
        self.marker = marker

    def __enter__(self):
        self.log.start_section(self.marker)

    def __exit__(self, extype, value, traceback):
        self.log.end_section(self.marker)

class Logfile(object):
    def __init__(self, fn):
        self.f = open(fn, "wt")
        self.last_stream = None
        self.blocks = []
        self.cur_evt = 1
        shutil.copy(mod_dir + "/multiplexed_log.css", os.path.dirname(fn))
        self.f.write("""\
<html>
<head>
<link rel="stylesheet" type="text/css" href="multiplexed_log.css">
</head>
<body>
<tt>
""")

    def close(self):
        self.f.write("""\
</tt>
</body>
</html>
""")
        self.f.close()

    def _escape(self, data):
        data = data.replace(chr(13), "")
        data = "".join((c in self._nonprint) and ("%%%02x" % ord(c)) or
                       c for c in data)
        data = cgi.escape(data)
        return data

    def _terminate_stream(self):
        self.cur_evt += 1
        if not self.last_stream:
            return
        self.f.write("</pre>\n")
        self.f.write("<div class=\"stream-trailer\" id=\"" +
                     self.last_stream.name + "\">End stream: " +
                     self.last_stream.name + "</div>\n")
        self.f.write("</div>\n")
        self.last_stream = None

    def _note(self, note_type, msg):
        self._terminate_stream()
        self.f.write("<div class=\"" + note_type + "\">\n")
        self.f.write(self._escape(msg))
        self.f.write("\n")
        self.f.write("</div>\n")

    def start_section(self, marker):
        self._terminate_stream()
        self.blocks.append(marker)
        blk_path = "/".join(self.blocks)
        self.f.write("<div class=\"section\" id=\"" + blk_path + "\">\n")
        self.f.write("<div class=\"section-header\" id=\"" + blk_path +
                     "\">Section: " + blk_path + "</div>\n")

    def end_section(self, marker):
        if (not self.blocks) or (marker != self.blocks[-1]):
            raise Exception("Block nesting mismatch: \"%s\" \"%s\"" %
                            (marker, "/".join(self.blocks)))
        self._terminate_stream()
        blk_path = "/".join(self.blocks)
        self.f.write("<div class=\"section-trailer\" id=\"section-trailer-" +
                     blk_path + "\">End section: " + blk_path + "</div>\n")
        self.f.write("</div>\n")
        self.blocks.pop()

    def section(self, marker):
        return SectionCtxMgr(self, marker)

    def error(self, msg):
        self._note("error", msg)

    def warning(self, msg):
        self._note("warning", msg)

    def info(self, msg):
        self._note("info", msg)

    def action(self, msg):
        self._note("action", msg)

    def status_pass(self, msg):
        self._note("status-pass", msg)

    def status_skipped(self, msg):
        self._note("status-skipped", msg)

    def status_fail(self, msg):
        self._note("status-fail", msg)

    def get_stream(self, name, chained_file=None):
        return LogfileStream(self, name, chained_file)

    def get_runner(self, name, chained_file=None):
        return RunAndLog(self, name, chained_file)

    _nonprint = ("^%" + "".join(chr(c) for c in range(0, 32) if c not in (9, 10)) +
                 "".join(chr(c) for c in range(127, 256)))

    def write(self, stream, data, implicit=False):
        if stream != self.last_stream:
            self._terminate_stream()
            self.f.write("<div class=\"stream\" id=\"%s\">\n" % stream.name)
            self.f.write("<div class=\"stream-header\" id=\"" + stream.name +
                         "\">Stream: " + stream.name + "</div>\n")
            self.f.write("<pre>")
        if implicit:
            self.f.write("<span class=\"implicit\">")
        self.f.write(self._escape(data))
        if implicit:
            self.f.write("</span>")
        self.last_stream = stream

    def flush(self):
        self.f.flush()
