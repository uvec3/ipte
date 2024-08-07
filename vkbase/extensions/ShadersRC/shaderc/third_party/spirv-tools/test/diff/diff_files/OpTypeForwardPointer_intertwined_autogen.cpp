// GENERATED FILE - DO NOT EDIT.
// Generated by generate_tests.py
//
// Copyright (c) 2022 Google LLC.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "../diff_test_utils.h"

#include "gtest/gtest.h"

namespace spvtools {
namespace diff {
namespace {

// Tests that two forwarded types whose declarations are intertwined match
// correctly
constexpr char kSrc[] = R"(               OpCapability Kernel
               OpCapability Addresses
               OpCapability Linkage
               OpMemoryModel Logical OpenCL
               OpName %Aptr "Aptr"
               OpName %Bptr "Bptr"
               OpTypeForwardPointer %Aptr UniformConstant
               OpTypeForwardPointer %Bptr UniformConstant
       %uint = OpTypeInt 32 0
          %A = OpTypeStruct %Aptr %uint %Bptr
          %B = OpTypeStruct %uint %Aptr %Bptr
  %Aptr = OpTypePointer UniformConstant %A
  %Bptr = OpTypePointer UniformConstant %B)";
constexpr char kDst[] = R"(               OpCapability Kernel
               OpCapability Addresses
               OpCapability Linkage
               OpMemoryModel Logical OpenCL
               OpName %Aptr "Aptr"
               OpName %Bptr "Bptr"
               OpTypeForwardPointer %Bptr UniformConstant
               OpTypeForwardPointer %Aptr UniformConstant
       %uint = OpTypeInt 32 0
          %B = OpTypeStruct %uint %Aptr %Bptr %uint
          %A = OpTypeStruct %Aptr %uint %Bptr
  %Aptr = OpTypePointer UniformConstant %A
  %Bptr = OpTypePointer UniformConstant %B
)";

TEST(DiffTest, OptypeforwardpointerIntertwined) {
  constexpr char kDiff[] = R"( ; SPIR-V
 ; Version: 1.6
 ; Generator: Khronos SPIR-V Tools Assembler; 0
-; Bound: 6
+; Bound: 7
 ; Schema: 0
 OpCapability Kernel
 OpCapability Addresses
 OpCapability Linkage
 OpMemoryModel Logical OpenCL
 OpName %1 "Aptr"
 OpName %2 "Bptr"
 OpTypeForwardPointer %1 UniformConstant
 OpTypeForwardPointer %2 UniformConstant
 %3 = OpTypeInt 32 0
+%6 = OpTypeStruct %3 %1 %2 %3
 %4 = OpTypeStruct %1 %3 %2
-%5 = OpTypeStruct %3 %1 %2
 %1 = OpTypePointer UniformConstant %4
-%2 = OpTypePointer UniformConstant %5
+%2 = OpTypePointer UniformConstant %6
)";
  Options options;
  DoStringDiffTest(kSrc, kDst, kDiff, options);
}

TEST(DiffTest, OptypeforwardpointerIntertwinedNoDebug) {
  constexpr char kSrcNoDebug[] = R"(               OpCapability Kernel
               OpCapability Addresses
               OpCapability Linkage
               OpMemoryModel Logical OpenCL
               OpTypeForwardPointer %Aptr UniformConstant
               OpTypeForwardPointer %Bptr UniformConstant
       %uint = OpTypeInt 32 0
          %A = OpTypeStruct %Aptr %uint %Bptr
          %B = OpTypeStruct %uint %Aptr %Bptr
  %Aptr = OpTypePointer UniformConstant %A
  %Bptr = OpTypePointer UniformConstant %B
)";
  constexpr char kDstNoDebug[] = R"(               OpCapability Kernel
               OpCapability Addresses
               OpCapability Linkage
               OpMemoryModel Logical OpenCL
               OpTypeForwardPointer %Bptr UniformConstant
               OpTypeForwardPointer %Aptr UniformConstant
       %uint = OpTypeInt 32 0
          %B = OpTypeStruct %uint %Aptr %Bptr %uint
          %A = OpTypeStruct %Aptr %uint %Bptr
  %Aptr = OpTypePointer UniformConstant %A
  %Bptr = OpTypePointer UniformConstant %B
)";
  constexpr char kDiff[] = R"( ; SPIR-V
 ; Version: 1.6
 ; Generator: Khronos SPIR-V Tools Assembler; 0
-; Bound: 6
+; Bound: 10
 ; Schema: 0
 OpCapability Kernel
 OpCapability Addresses
 OpCapability Linkage
 OpMemoryModel Logical OpenCL
-OpTypeForwardPointer %1 UniformConstant
-OpTypeForwardPointer %2 UniformConstant
+OpTypeForwardPointer %6 UniformConstant
+OpTypeForwardPointer %7 UniformConstant
 %3 = OpTypeInt 32 0
-%4 = OpTypeStruct %1 %3 %2
-%5 = OpTypeStruct %3 %1 %2
-%1 = OpTypePointer UniformConstant %4
-%2 = OpTypePointer UniformConstant %5
+%8 = OpTypeStruct %3 %7 %6 %3
+%9 = OpTypeStruct %7 %3 %6
+%7 = OpTypePointer UniformConstant %9
+%6 = OpTypePointer UniformConstant %8
)";
  Options options;
  DoStringDiffTest(kSrcNoDebug, kDstNoDebug, kDiff, options);
}

}  // namespace
}  // namespace diff
}  // namespace spvtools
