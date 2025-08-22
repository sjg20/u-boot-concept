.. SPDX-License-Identifier: GPL-2.0+
.. sectionauthor:: Simon Glass <sjg@chromium.org>

=======
Console
=======


Paging
------

See the user documentation at :doc:`/usage/console`. For the API, see
:doc:`/api/pager`.

The pager is implemented in `common/pager.c` and integrated into the console
system via `console_puts()` in `common/console.c`. When output is sent to
stdout:

1. Text is passed to `pager_post()` which buffers it
2. `pager_next()` returns portions of text up to the page limit
3. When the page limit is reached, a prompt is displayed
4. The system waits for user input (SPACE key) via `getchar()`
5. After SPACE is pressed, the prompt is cleared and output continues

The pager maintains state through the `struct pager` which tracks:

- Current line count within the page
- Page length setting
- Text buffer and overflow handling
- Current pager state (normal, waiting for user, clearing prompt)

Bypass Mode
~~~~~~~~~~~

The pager can be put into bypass mode using `pager_set_bypass()`, which is
useful for automated testing. In bypass mode, all text flows through without
any paging interruptions.

The pager automatically enters bypass mode when output is not connected to a
terminal, preventing paging prompts from appearing in non-interactive
environments like scripts or automated builds.
