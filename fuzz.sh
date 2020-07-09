#!/bin/bash

./AFL/afl-fuzz -m 128 -i testcase_dir -o findings_dir $@
