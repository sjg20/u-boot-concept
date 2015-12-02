# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

import pytest

# FIXME: This might be useful for other tests;
# perhaps refactor it into ConsoleBase or some other state object?
class StateTestEnv(object):
    def __init__(self, uboot_console):
        self.uboot_console = uboot_console
        self.get_env()
        self.set_var = self.get_non_existent_var()

    def get_env(self):
        response = self.uboot_console.run_command("printenv")
        self.env = {}
        for l in response.splitlines():
            if not "=" in l:
                continue
            (var, value) = l.strip().split("=", 1)
            self.env[var] = value

    def get_existent_var(self):
        for var in self.env:
            return var

    def get_non_existent_var(self):
        n = 0
        while True:
            var = "test_env_" + str(n)
            if var not in self.env:
                return var
            n += 1

@pytest.fixture(scope="module")
def state_test_env(uboot_console):
    return StateTestEnv(uboot_console)

def unset_var(state_test_env, var):
    state_test_env.uboot_console.run_command("setenv " + var)
    if var in state_test_env.env:
        del state_test_env.env[var]

def set_var(state_test_env, var, value):
    state_test_env.uboot_console.run_command("setenv " + var + " \"" + value + "\"")
    state_test_env.env[var] = value

def validate_empty(state_test_env, var):
    response = state_test_env.uboot_console.run_command("echo $" + var)
    assert response == ""

def validate_set(state_test_env, var, value):
    # echo does not preserve leading, internal, or trailing whitespace in the
    # value. printenv does, and hence allows more complete testing.
    response = state_test_env.uboot_console.run_command("printenv " + var)
    assert response == (var + "=" + value)

def test_env_echo_exists(state_test_env):
    """Echo a variable that exists"""
    var = state_test_env.get_existent_var()
    value = state_test_env.env[var]
    validate_set(state_test_env, var, value)

def test_env_echo_non_existent(state_test_env):
    """Echo a variable that doesn't exist"""
    var = state_test_env.set_var
    validate_empty(state_test_env, var)

def test_env_printenv_non_existent(state_test_env):
    """Check printenv error message"""
    var = state_test_env.set_var
    c = state_test_env.uboot_console
    with c.disable_check("error_notification"):
        response = c.run_command("printenv " + var)
    assert(response == "## Error: \"" + var + "\" not defined")

def test_env_unset_non_existent(state_test_env):
    """Unset a nonexistent variable"""
    var = state_test_env.get_non_existent_var()
    unset_var(state_test_env, var)
    validate_empty(state_test_env, var)

def test_env_set_non_existent(state_test_env):
    """Set a new variable"""
    var = state_test_env.set_var
    value = "foo"
    set_var(state_test_env, var, value)
    validate_set(state_test_env, var, value)

def test_env_set_existing(state_test_env):
    """Set an existing variable"""
    var = state_test_env.set_var
    value = "bar"
    set_var(state_test_env, var, value)
    validate_set(state_test_env, var, value)

def test_env_unset_existing(state_test_env):
    """Unset a variable"""
    var = state_test_env.set_var
    unset_var(state_test_env, var)
    validate_empty(state_test_env, var)

def test_env_expansion_spaces(state_test_env):
    var_space = None
    var_test = None
    try:
        var_space = state_test_env.get_non_existent_var()
        set_var(state_test_env, var_space, " ")

        var_test = state_test_env.get_non_existent_var()
        value = " 1${%(var_space)s}${%(var_space)s} 2 " % locals()
        set_var(state_test_env, var_test, value)
        value = " 1   2 "
        validate_set(state_test_env, var_test, value)
    finally:
        if var_space:
            unset_var(state_test_env, var_space)
        if var_test:
            unset_var(state_test_env, var_test)
