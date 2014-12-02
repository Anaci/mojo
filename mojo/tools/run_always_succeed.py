#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs its command line and returns 0 (success)."""

import subprocess
import sys


def main():
  subprocess.call(sys.argv[1:])
  sys.exit(0)


if __name__ == '__main__':
  main()
