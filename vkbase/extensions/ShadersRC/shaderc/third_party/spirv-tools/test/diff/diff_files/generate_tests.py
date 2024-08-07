#! /usr/bin/python3
#
# Copyright (c) 2022 Google LLC
#
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

import glob
import os
import subprocess
import sys

# A handful of relevant tests are hand-picked to generate extra unit tests with
# specific options of spirv-diff.
IGNORE_SET_BINDING_TESTS = ['different_decorations_vertex']
IGNORE_LOCATION_TESTS = ['different_decorations_fragment']
IGNORE_DECORATIONS_TESTS = ['different_decorations_vertex', 'different_decorations_fragment']
DUMP_IDS_TESTS = ['basic', 'int_vs_uint_constants', 'multiple_same_entry_points', 'small_functions_small_diffs']

LICENSE = u"""Copyright (c) 2022 Google LLC.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

TEMPLATE_TEST_FILE = u"""// GENERATED FILE - DO NOT EDIT.
// Generated by {script_name}
//
{license}

#include "../diff_test_utils.h"

#include "gtest/gtest.h"

namespace spvtools {{
namespace diff {{
namespace {{

{test_comment}
constexpr char kSrc[] = R"({src_spirv})";
constexpr char kDst[] = R"({dst_spirv})";

TEST(DiffTest, {test_name}) {{
  constexpr char kDiff[] = R"({diff_spirv})";
  Options options;
  DoStringDiffTest(kSrc, kDst, kDiff, options);
}}

TEST(DiffTest, {test_name}NoDebug) {{
  constexpr char kSrcNoDebug[] = R"({src_spirv_no_debug})";
  constexpr char kDstNoDebug[] = R"({dst_spirv_no_debug})";
  constexpr char kDiff[] = R"({diff_spirv_no_debug})";
  Options options;
  DoStringDiffTest(kSrcNoDebug, kDstNoDebug, kDiff, options);
}}
{extra_tests}
}}  // namespace
}}  // namespace diff
}}  // namespace spvtools
"""

TEMPLATE_TEST_FUNC = u"""
TEST(DiffTest, {test_name}{test_tag}) {{
  constexpr char kDiff[] = R"({diff_spirv})";
  Options options;
  {test_options}
  DoStringDiffTest(kSrc, kDst, kDiff, options);
}}
"""

TEMPLATE_TEST_FILES_CMAKE = u"""# GENERATED FILE - DO NOT EDIT.
# Generated by {script_name}
#
{license}

list(APPEND DIFF_TEST_FILES
{test_files}
)
"""

VARIANT_NONE = 0
VARIANT_IGNORE_SET_BINDING = 1
VARIANT_IGNORE_LOCATION = 2
VARIANT_IGNORE_DECORATIONS = 3
VARIANT_DUMP_IDS = 4

def print_usage():
    print("Usage: {} <path-to-spirv-diff>".format(sys.argv[0]))

def remove_debug_info(in_path):
    tmp_dir = '.no_dbg'

    if not os.path.exists(tmp_dir):
        os.makedirs(tmp_dir)

    (in_basename, in_ext) = os.path.splitext(in_path)
    out_name = in_basename + '_no_dbg' + in_ext
    out_path = os.path.join(tmp_dir, out_name)

    with open(in_path, 'r') as fin:
        with open(out_path, 'w') as fout:
            for line in fin:
                ops = line.strip().split()
                op = ops[0] if len(ops) > 0 else ''
                if (op != ';;' and op != 'OpName' and op != 'OpMemberName' and op != 'OpString' and
                    op != 'OpLine' and op != 'OpNoLine' and op != 'OpModuleProcessed'):
                    fout.write(line)

    return out_path

def make_src_file(test_name):
    return '{}_src.spvasm'.format(test_name)

def make_dst_file(test_name):
    return '{}_dst.spvasm'.format(test_name)

def make_cpp_file(test_name):
    return '{}_autogen.cpp'.format(test_name)

def make_camel_case(test_name):
    return test_name.replace('_', ' ').title().replace(' ', '')

def make_comment(text, comment_prefix):
    return '\n'.join([comment_prefix + (' ' if line.strip() else '') + line for line in text.splitlines()])

def read_file(file_name):
    with open(file_name, 'r') as f:
        content = f.read()

    # Use unix line endings.
    content = content.replace('\r\n', '\n')

    return content

def parse_test_comment(src_spirv_file_name, src_spirv):
    src_spirv_lines = src_spirv.splitlines()
    comment_line_count = 0
    while comment_line_count < len(src_spirv_lines):
        if not src_spirv_lines[comment_line_count].strip().startswith(';;'):
            break
        comment_line_count += 1

    if comment_line_count == 0:
        print("Expected comment on test file '{}'.  See README.md next to this file.".format(src_spirv_file_name))
        sys.exit(1)

    comment_block = src_spirv_lines[:comment_line_count]
    spirv_block = src_spirv_lines[comment_line_count:]

    comment_block = ['// ' + line.replace(';;', '').strip() for line in comment_block]

    return '\n'.join(spirv_block), '\n'.join(comment_block)

def run_diff_tool(diff_tool, src_file, dst_file, variant):
    args = [diff_tool]

    if variant == VARIANT_IGNORE_SET_BINDING or variant == VARIANT_IGNORE_DECORATIONS:
        args.append('--ignore-set-binding')

    if variant == VARIANT_IGNORE_LOCATION or variant == VARIANT_IGNORE_DECORATIONS:
        args.append('--ignore-location')

    if variant == VARIANT_DUMP_IDS:
        args.append('--with-id-map')

    args.append('--no-color')
    args.append('--no-indent')

    args.append(src_file)
    args.append(dst_file)

    success = True
    print(' '.join(args))
    process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    out, err = process.communicate()

    if process.returncode != 0:
        print(err)
        sys.exit(process.returncode)

    # Use unix line endings.
    out = out.replace('\r\n', '\n')

    return out

def generate_extra_test(diff_tool, src_file, dst_file, variant, test_name_camel_case, test_tag, test_options):
    diff = run_diff_tool(diff_tool, src_file, dst_file, variant)
    return TEMPLATE_TEST_FUNC.format(
        test_name = test_name_camel_case,
        test_tag = test_tag,
        test_options = test_options,
        diff_spirv = diff)

def generate_test(diff_tool, test_name):
    src_file = make_src_file(test_name)
    dst_file = make_dst_file(test_name)
    src_file_no_debug = remove_debug_info(src_file)
    dst_file_no_debug = remove_debug_info(dst_file)

    src_spirv = read_file(src_file)
    dst_spirv = read_file(dst_file)
    src_spirv_no_debug = read_file(src_file_no_debug)
    dst_spirv_no_debug = read_file(dst_file_no_debug)

    test_name_camel_case = make_camel_case(test_name)

    diff_spirv = run_diff_tool(diff_tool, src_file, dst_file, VARIANT_NONE)
    diff_spirv_no_debug = run_diff_tool(diff_tool, src_file_no_debug, dst_file_no_debug, VARIANT_NONE)

    extra_tests = []

    if test_name in IGNORE_SET_BINDING_TESTS:
        extra_tests.append(generate_extra_test(diff_tool, src_file, dst_file, VARIANT_IGNORE_SET_BINDING,
            test_name_camel_case, 'IgnoreSetBinding', 'options.ignore_set_binding = true;'))

    if test_name in IGNORE_LOCATION_TESTS:
        extra_tests.append(generate_extra_test(diff_tool, src_file, dst_file, VARIANT_IGNORE_LOCATION,
            test_name_camel_case, 'IgnoreLocation', 'options.ignore_location = true;'))

    if test_name in IGNORE_DECORATIONS_TESTS:
        extra_tests.append(generate_extra_test(diff_tool, src_file, dst_file, VARIANT_IGNORE_DECORATIONS,
            test_name_camel_case, 'IgnoreSetBindingLocation',
            '\n  '.join(['options.ignore_set_binding = true;', 'options.ignore_location = true;'])))

    if test_name in DUMP_IDS_TESTS:
        extra_tests.append(generate_extra_test(diff_tool, src_file, dst_file, VARIANT_DUMP_IDS,
            test_name_camel_case, 'DumpIds', 'options.dump_id_map = true;'))

    src_spirv, test_comment = parse_test_comment(src_file, src_spirv)

    test_file = TEMPLATE_TEST_FILE.format(
            script_name = os.path.basename(__file__),
            license = make_comment(LICENSE, '//'),
            test_comment = test_comment,
            test_name = test_name_camel_case,
            src_spirv = src_spirv,
            dst_spirv = dst_spirv,
            diff_spirv = diff_spirv,
            src_spirv_no_debug = src_spirv_no_debug,
            dst_spirv_no_debug = dst_spirv_no_debug,
            diff_spirv_no_debug = diff_spirv_no_debug,
            extra_tests = ''.join(extra_tests))

    test_file_name = make_cpp_file(test_name)
    with open(test_file_name, 'wb') as fout:
        fout.write(str.encode(test_file))

    return test_file_name

def generate_tests(diff_tool, test_names):
    return [generate_test(diff_tool, test_name) for test_name in test_names]

def generate_cmake(test_files):
    cmake = TEMPLATE_TEST_FILES_CMAKE.format(
            script_name = os.path.basename(__file__),
            license = make_comment(LICENSE, '#'),
            test_files = '\n'.join(['"diff_files/{}"'.format(f) for f in test_files]))

    with open('diff_test_files_autogen.cmake', 'wb') as fout:
        fout.write(str.encode(cmake))

def main():

    if len(sys.argv) != 2:
        print_usage()
        return 1

    diff_tool = sys.argv[1]
    if not os.path.exists(diff_tool):
        print("No such file: {}".format(diff_tool))
        print_usage()
        return 1

    diff_tool = os.path.realpath(diff_tool)
    os.chdir(os.path.dirname(__file__))

    test_names = sorted([f[:-11] for f in glob.glob("*_src.spvasm")])

    test_files = generate_tests(diff_tool, test_names)

    generate_cmake(test_files)

    return 0

if __name__ == '__main__':
    sys.exit(main())
