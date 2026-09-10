## @ conftest.py
#
# Shared pytest fixtures for the Slim Bootloader Python tool tests.
#
# Copyright (c) 2026, Intel Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

import os
import sys

import pytest

TEST_DIR = os.path.dirname (os.path.abspath (__file__))
TOOLS_DIR = os.path.dirname (TEST_DIR)
WORKSPACE = os.path.normpath (os.path.join (TOOLS_DIR, '..', '..'))

# The tools are scripts rather than an installed package, so make them
# importable exactly as BuildLoader.py does.
if TOOLS_DIR not in sys.path:
    sys.path.insert (0, TOOLS_DIR)
if WORKSPACE not in sys.path:
    sys.path.insert (0, WORKSPACE)

os.environ.setdefault ('SBL_SOURCE', WORKSPACE)
os.environ.setdefault ('WORKSPACE', WORKSPACE)
os.environ.setdefault ('PLT_SOURCE', WORKSPACE)


@pytest.fixture (scope='session')
def workspace ():
    """Absolute path to the repository root."""
    return WORKSPACE


@pytest.fixture (scope='session')
def tools_dir ():
    """Absolute path to BootloaderCorePkg/Tools."""
    return TOOLS_DIR
