# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved.

"""
Logic to spawn a sub-process and interact with its stdio.

This is used by console_board and console_sandbox

- console_board (for real hardware): Spawns 'u-boot-test-console' and provides
    access to the console input/output
- console_sandbox (for sandbox): Spawns 'u-boot' and provides access to the
    console input/output

In both cases, Spawn provides a way to send console commands and receive the
response from U-Boot. An expect() function helps to simplify things for the
higher levels.

The code in this file should be generic, i.e. not specific to sandbox or real
hardware.

Within the console_*py files, self.p is used to refer to the Spawn() object,
perhaps short for 'process'.
"""

import io
import os
import pty
import signal
import select
import sys
import termios
import time
import traceback

# Character to send (twice) to exit the terminal
EXIT_CHAR = 0x1d    # FS (Ctrl + ])


class Spawn:
    """Represents the stdio of a freshly created sub-process. Commands may be
    sent to the process, and responses waited for.
    """

    def __init__(self, args, cwd=None, decode_signal=False):
        """Spawn (fork/exec) the sub-process.

        Args:
            args (list of str): processs arguments. argv[0] is the command to
                execute.
            cwd (str or None): the directory to run the process in, or None for
                no change.
            decode_signal (bool): True to indicate the exception number when
                something goes wrong

        Returns:
            Nothing.
        """
        self.decode_signal = decode_signal
        self.waited = False
        self.exit_code = 0
        self.exit_info = ''

        (self.pid, self.fd) = pty.fork()
        if self.pid == 0:
            try:
                # For some reason, SIGHUP is set to SIG_IGN at this point when
                # run under "go" (www.go.cd). Perhaps this happens under any
                # background (non-interactive) system?
                signal.signal(signal.SIGHUP, signal.SIG_DFL)
                if cwd:
                    os.chdir(cwd)
                os.execvp(args[0], args)
            except:
                print('CHILD EXECEPTION:')
                traceback.print_exc()
            finally:
                os._exit(255)

        old = None
        try:
            isatty = False
            try:
                isatty = os.isatty(sys.stdout.fileno())

            # with --capture=tee-sys we cannot call fileno()
            except io.UnsupportedOperation:
                pass
            if isatty:
                new = termios.tcgetattr(self.fd)
                old = new
                new[3] = new[3] & ~(termios.ICANON | termios.ISIG)
                new[3] = new[3] & ~termios.ECHO
                new[6][termios.VMIN] = 0
                new[6][termios.VTIME] = 0
                termios.tcsetattr(self.fd, termios.TCSANOW, new)

            self.poll = select.poll()
            self.poll.register(self.fd, select.POLLIN | select.POLLPRI | select.POLLERR |
                               select.POLLHUP | select.POLLNVAL)
        except:
            if old:
                termios.tcsetattr(self.fd, termios.TCSANOW, old)
            self.close()
            raise

    def kill(self, sig):
        """Send unix signal "sig" to the child process.

        Args:
            sig (int): The signal number to send
        """
        os.kill(self.pid, sig)

    def checkalive(self):
        """Determine whether the child process is still running.

        Returns:
            tuple:
                True if process is alive, else False
                0 if process is alive, else exit code of process
                string describing what happened ('' or 'status/signal n')
        """
        if self.waited:
            return False, self.exit_code, self.exit_info

        w = os.waitpid(self.pid, os.WNOHANG)
        if w[0] == 0:
            return True, 0, 'running'
        status = w[1]

        if os.WIFEXITED(status):
            self.exit_code = os.WEXITSTATUS(status)
            self.exit_info = f'status {self.exit_code}'
        elif os.WIFSIGNALED(status):
            signum = os.WTERMSIG(status)
            self.exit_code = -signum
            self.exit_info = f'signal {signum} ({signal.Signals(signum).name})'
        self.waited = True
        return False, self.exit_code, self.exit_info

    def isalive(self):
        """Determine whether the child process is still running.

        Returns:
            bool: indicating whether process is alive
        """
        return self.checkalive()[0]

    def send(self, data):
        """Send data to the sub-process's stdin.

        Args:
            data (str): The data to send to the process.
        """
        os.write(self.fd, data.encode(errors='replace'))

    def receive(self, num_bytes):
        """Receive data from the sub-process's stdin.

        Args:
            num_bytes (int): Maximum number of bytes to read

        Returns:
            str: The data received

        Raises:
            ValueError if U-Boot died
        """
        try:
            c = os.read(self.fd, num_bytes).decode(errors='replace')
        except OSError as err:
            # With sandbox, try to detect when U-Boot exits when it
            # shouldn't and explain why. This is much more friendly than
            # just dying with an I/O error
            if self.decode_signal and err.errno == 5:  # I/O error
                alive, _, info = self.checkalive()
                if alive:
                    raise err
                raise ValueError(f'U-Boot exited with {info}') from err
            raise
        return c

    def close(self):
        """Close the stdio connection to the sub-process.

        This also waits a reasonable time for the sub-process to stop running.

        Args:
            None.

        Returns:
            str: Type of closure completed
        """
        # For Labgrid-sjg, ask it is exit gracefully, so it can transition the
        # board to the final state (like 'off') before exiting.
        if os.environ.get('USE_LABGRID_SJG'):
            self.send(chr(EXIT_CHAR) * 2)

            # Wait about 10 seconds for Labgrid to close and power off the board
            for _ in range(100):
                if not self.isalive():
                    return 'normal'
                time.sleep(0.1)

        # That didn't work, so try closing the PTY
        os.close(self.fd)
        for _ in range(100):
            if not self.isalive():
                return 'break'
            time.sleep(0.1)

        return 'timeout'
