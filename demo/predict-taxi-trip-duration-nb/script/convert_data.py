#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2021 4Paradigm
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import sys
import time, datetime
i = 0
for line in sys.stdin:
    if i == 0:
        i+=1
        print(line.strip())
        continue
    arr = line.strip().split(",")
    arr[2] = str(int(time.mktime(time.strptime(arr[2], "%Y-%m-%d %H:%M:%S"))) * 1000)
    arr[3] = str(int(time.mktime(time.strptime(arr[3], "%Y-%m-%d %H:%M:%S"))) * 1000)
    print(",".join(arr))

