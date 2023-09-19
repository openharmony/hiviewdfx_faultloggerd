#!/bin/bash
# Copyright (c) 2022 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
set -e

gcc ./elf_test_source_code.cpp -std=c++17 -lstdc++ -o ../elf_test -Wl,--gc-sections
strip ../elf_test -o ../elf_test_stripped
objcopy  -R .note.gnu.build-id -R .note.gnu.property -R .note.ABI-tag ../elf_test_stripped ../elf_test_stripped_nobuildid
objcopy  -R .eh_frame_hdr ../elf_test_stripped ../elf_test_stripped_noehframehdr


gcc -m32 ./elf_test_source_code.cpp -std=c++17 -lstdc++ -o ../elf32_test -Wl,--gc-sections
strip ../elf32_test -o ../elf32_test_stripped
objcopy  -R .note.gnu.build-id -R .note.gnu.property -R .note.ABI-tag ../elf32_test_stripped ../elf32_test_stripped_nobuildid
objcopy  -R .eh_frame_hdr ../elf32_test_stripped ../elf32_test_stripped_noehframehdr
