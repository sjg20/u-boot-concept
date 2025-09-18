# U-Boot Build Instructions for Claude Code

This file contains information about building U-Boot for use with Claude Code.

## Building U-Boot

### Using crosfw (Recommended)

To build U-Boot for sandbox testing, use the `crosfw` command:

```bash
# Build for sandbox
crosfw sandbox -L

# The -L flag disables LTO (equivalent to NO_LTO=1)
# The build is silent unless there are warnings or errors
# The build is done in /tmp/b/<board_name>, so /tmp/b/sandbox in this case
```

### Using make directly

If you prefer to use make directly, please use O= to avoid adding build files to
the source tree:

```bash
# Clean the build (recommended before first build)
make mrproper

# Configure for sandbox
make sandbox_defconfig O=/tmp/<build_dir>

# Build
make -j$(nproc) O=/tmp/<build_dir>
```
## Testing

There are aliases in ~/bin/git-alias which you can use.

To run a sandbox test:

```bash
rtv <test_name>
# For example: rtv dm_test_video_box
# which translates to: /tmp/b/sandbox/u-boot -v -Tf -c "ut dm video_box"
# test output is silenced unless -v is given
```

To run using the Python suite:

```bash
pyt <test_name>
# alias for: test/py/test.py -B sandbox --build-dir /tmp/b/sandbox -k <test_name>
```

## Notes

- The `crosfw` tool is the preferred build method for this codebase
- Always run `make mrproper` if you encounter build issues
- The sandbox build creates a test environment for U-Boot that runs on the host system
- When using `git diff`, add `--no-ext-diff` to avoid external diff tools that may not work in this environment

## Coding Conventions

- For function parameters that return values (output parameters), add 'p' suffix to indicate pointer
  - Example: `uint *sizep` instead of `uint *size`
  - Example: `bool *visiblep` instead of `bool *visible`
- This follows U-Boot's established naming convention for output parameters
- Keep commit messages concise - focus on the key change and essential details only
- Code should be formatted to 80 columns and not have trailing spaces
