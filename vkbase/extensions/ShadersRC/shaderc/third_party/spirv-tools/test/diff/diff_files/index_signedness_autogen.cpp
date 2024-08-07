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

// Test where signedness of indices are different between src and dst.
constexpr char kSrc[] = R"(               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %4 "main"
               OpExecutionMode %4 LocalSize 1 1 1
               OpSource ESSL 310
               OpName %4 "main"
               OpName %13 "BufferOut"
               OpMemberName %13 0 "o1"
               OpMemberName %13 1 "o2"
               OpMemberName %13 2 "o3"
               OpName %15 ""
               OpName %22 "BufferIn"
               OpMemberName %22 0 "i1"
               OpMemberName %22 1 "i2"
               OpName %24 ""
               OpDecorate %8 ArrayStride 4
               OpDecorate %9 ArrayStride 4
               OpDecorate %11 ArrayStride 4
               OpDecorate %12 ArrayStride 8
               OpMemberDecorate %13 0 Offset 0
               OpMemberDecorate %13 1 Offset 12
               OpMemberDecorate %13 2 Offset 24
               OpDecorate %13 BufferBlock
               OpDecorate %15 DescriptorSet 0
               OpDecorate %15 Binding 1
               OpDecorate %18 ArrayStride 16
               OpDecorate %19 ArrayStride 48
               OpDecorate %21 ArrayStride 16
               OpMemberDecorate %22 0 Offset 0
               OpMemberDecorate %22 1 Offset 96
               OpDecorate %22 Block
               OpDecorate %24 DescriptorSet 0
               OpDecorate %24 Binding 0
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeInt 32 0
          %7 = OpConstant %6 3
          %8 = OpTypeArray %6 %7
          %9 = OpTypeArray %6 %7
         %10 = OpConstant %6 2
         %11 = OpTypeArray %6 %10
         %12 = OpTypeArray %11 %10
         %13 = OpTypeStruct %8 %9 %12
         %14 = OpTypePointer Uniform %13
         %15 = OpVariable %14 Uniform
         %16 = OpTypeInt 32 1
         %17 = OpConstant %16 0
         %18 = OpTypeArray %6 %7
         %19 = OpTypeArray %18 %10
         %20 = OpConstant %6 4
         %21 = OpTypeArray %6 %20
         %22 = OpTypeStruct %19 %21
         %23 = OpTypePointer Uniform %22
         %24 = OpVariable %23 Uniform
         %25 = OpTypePointer Uniform %6
         %28 = OpConstant %6 1
         %31 = OpConstant %16 1
         %34 = OpConstant %6 0
         %37 = OpConstant %16 2
         %61 = OpConstant %16 3
          %4 = OpFunction %2 None %3
          %5 = OpLabel
         %26 = OpAccessChain %25 %24 %17 %17 %17
         %27 = OpLoad %6 %26
         %29 = OpIAdd %6 %27 %28
         %30 = OpAccessChain %25 %15 %17 %17
               OpStore %30 %29
         %32 = OpAccessChain %25 %24 %17 %31 %17
         %33 = OpLoad %6 %32
         %35 = OpIAdd %6 %33 %34
         %36 = OpAccessChain %25 %15 %17 %31
               OpStore %36 %35
         %38 = OpAccessChain %25 %24 %17 %31 %31
         %39 = OpLoad %6 %38
         %40 = OpIAdd %6 %39 %10
         %41 = OpAccessChain %25 %15 %17 %37
               OpStore %41 %40
         %42 = OpAccessChain %25 %24 %17 %17 %37
         %43 = OpLoad %6 %42
         %44 = OpAccessChain %25 %15 %31 %17
               OpStore %44 %43
         %45 = OpAccessChain %25 %24 %17 %17 %31
         %46 = OpLoad %6 %45
         %47 = OpIMul %6 %46 %7
         %48 = OpAccessChain %25 %15 %31 %31
               OpStore %48 %47
         %49 = OpAccessChain %25 %24 %17 %31 %37
         %50 = OpLoad %6 %49
         %51 = OpAccessChain %25 %15 %31 %37
               OpStore %51 %50
         %52 = OpAccessChain %25 %24 %31 %17
         %53 = OpLoad %6 %52
         %54 = OpAccessChain %25 %15 %37 %17 %17
               OpStore %54 %53
         %55 = OpAccessChain %25 %24 %31 %31
         %56 = OpLoad %6 %55
         %57 = OpAccessChain %25 %15 %37 %17 %31
               OpStore %57 %56
         %58 = OpAccessChain %25 %24 %31 %37
         %59 = OpLoad %6 %58
         %60 = OpAccessChain %25 %15 %37 %31 %17
               OpStore %60 %59
         %62 = OpAccessChain %25 %24 %31 %61
         %63 = OpLoad %6 %62
         %64 = OpAccessChain %25 %15 %37 %31 %31
               OpStore %64 %63
               OpReturn
               OpFunctionEnd
)";
constexpr char kDst[] = R"(               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %4 "main"
               OpExecutionMode %4 LocalSize 1 1 1
               OpSource ESSL 310
               OpName %4 "main"
               OpName %13 "BufferOut"
               OpMemberName %13 0 "o1"
               OpMemberName %13 1 "o2"
               OpMemberName %13 2 "o3"
               OpName %15 ""
               OpName %22 "BufferIn"
               OpMemberName %22 0 "i1"
               OpMemberName %22 1 "i2"
               OpName %24 ""
               OpDecorate %8 ArrayStride 4
               OpDecorate %9 ArrayStride 4
               OpDecorate %11 ArrayStride 4
               OpDecorate %12 ArrayStride 8
               OpMemberDecorate %13 0 Offset 0
               OpMemberDecorate %13 1 Offset 12
               OpMemberDecorate %13 2 Offset 24
               OpDecorate %13 BufferBlock
               OpDecorate %15 DescriptorSet 0
               OpDecorate %15 Binding 1
               OpDecorate %18 ArrayStride 16
               OpDecorate %19 ArrayStride 48
               OpDecorate %21 ArrayStride 16
               OpMemberDecorate %22 0 Offset 0
               OpMemberDecorate %22 1 Offset 96
               OpDecorate %22 Block
               OpDecorate %24 DescriptorSet 0
               OpDecorate %24 Binding 0
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeInt 32 0
         %16 = OpTypeInt 32 1
          %7 = OpConstant %16 3
          %8 = OpTypeArray %6 %7
          %9 = OpTypeArray %6 %7
         %10 = OpConstant %16 2
         %11 = OpTypeArray %6 %10
         %12 = OpTypeArray %11 %10
         %13 = OpTypeStruct %8 %9 %12
         %14 = OpTypePointer Uniform %13
         %15 = OpVariable %14 Uniform
         %18 = OpTypeArray %6 %7
         %19 = OpTypeArray %18 %10
         %20 = OpConstant %16 4
         %21 = OpTypeArray %6 %20
         %22 = OpTypeStruct %19 %21
         %23 = OpTypePointer Uniform %22
         %24 = OpVariable %23 Uniform
         %25 = OpTypePointer Uniform %6
         %17 = OpConstant %16 0
         %28 = OpConstant %16 1
         %31 = OpConstant %6 1
         %34 = OpConstant %6 0
         %37 = OpConstant %6 2
         %61 = OpConstant %6 3
          %4 = OpFunction %2 None %3
          %5 = OpLabel
         %26 = OpAccessChain %25 %24 %17 %17 %17
         %27 = OpLoad %6 %26
         %29 = OpIAdd %6 %27 %28
         %30 = OpAccessChain %25 %15 %17 %17
               OpStore %30 %29
         %32 = OpAccessChain %25 %24 %17 %31 %17
         %33 = OpLoad %6 %32
         %35 = OpIAdd %6 %33 %34
         %36 = OpAccessChain %25 %15 %17 %31
               OpStore %36 %35
         %38 = OpAccessChain %25 %24 %17 %31 %31
         %39 = OpLoad %6 %38
         %40 = OpIAdd %6 %39 %37
         %41 = OpAccessChain %25 %15 %17 %10
               OpStore %41 %40
         %42 = OpAccessChain %25 %24 %17 %17 %10
         %43 = OpLoad %6 %42
         %44 = OpAccessChain %25 %15 %31 %17
               OpStore %44 %43
         %45 = OpAccessChain %25 %24 %17 %17 %31
         %46 = OpLoad %6 %45
         %47 = OpIMul %6 %46 %7
         %48 = OpAccessChain %25 %15 %31 %31
               OpStore %48 %47
         %49 = OpAccessChain %25 %24 %17 %31 %10
         %50 = OpLoad %6 %49
         %51 = OpAccessChain %25 %15 %31 %10
               OpStore %51 %50
         %52 = OpAccessChain %25 %24 %31 %17
         %53 = OpLoad %6 %52
         %54 = OpAccessChain %25 %15 %37 %17 %17
               OpStore %54 %53
         %55 = OpAccessChain %25 %24 %31 %31
         %56 = OpLoad %6 %55
         %57 = OpAccessChain %25 %15 %37 %17 %31
               OpStore %57 %56
         %58 = OpAccessChain %25 %24 %31 %37
         %59 = OpLoad %6 %58
         %60 = OpAccessChain %25 %15 %37 %31 %17
               OpStore %60 %59
         %62 = OpAccessChain %25 %24 %31 %61
         %63 = OpLoad %6 %62
         %64 = OpAccessChain %25 %15 %37 %31 %31
               OpStore %64 %63
               OpReturn
               OpFunctionEnd

)";

TEST(DiffTest, IndexSignedness) {
  constexpr char kDiff[] = R"( ; SPIR-V
 ; Version: 1.6
 ; Generator: Khronos SPIR-V Tools Assembler; 0
 ; Bound: 65
 ; Schema: 0
 OpCapability Shader
 %1 = OpExtInstImport "GLSL.std.450"
 OpMemoryModel Logical GLSL450
 OpEntryPoint GLCompute %4 "main"
 OpExecutionMode %4 LocalSize 1 1 1
 OpSource ESSL 310
 OpName %4 "main"
 OpName %13 "BufferOut"
 OpMemberName %13 0 "o1"
 OpMemberName %13 1 "o2"
 OpMemberName %13 2 "o3"
 OpName %15 ""
 OpName %22 "BufferIn"
 OpMemberName %22 0 "i1"
 OpMemberName %22 1 "i2"
 OpName %24 ""
 OpDecorate %8 ArrayStride 4
 OpDecorate %9 ArrayStride 4
 OpDecorate %11 ArrayStride 4
 OpDecorate %12 ArrayStride 8
 OpMemberDecorate %13 0 Offset 0
 OpMemberDecorate %13 1 Offset 12
 OpMemberDecorate %13 2 Offset 24
 OpDecorate %13 BufferBlock
 OpDecorate %15 DescriptorSet 0
 OpDecorate %15 Binding 1
 OpDecorate %18 ArrayStride 16
 OpDecorate %19 ArrayStride 48
 OpDecorate %21 ArrayStride 16
 OpMemberDecorate %22 0 Offset 0
 OpMemberDecorate %22 1 Offset 96
 OpDecorate %22 Block
 OpDecorate %24 DescriptorSet 0
 OpDecorate %24 Binding 0
 %2 = OpTypeVoid
 %3 = OpTypeFunction %2
 %6 = OpTypeInt 32 0
 %7 = OpConstant %6 3
-%8 = OpTypeArray %6 %7
+%8 = OpTypeArray %6 %61
-%9 = OpTypeArray %6 %7
+%9 = OpTypeArray %6 %61
 %10 = OpConstant %6 2
-%11 = OpTypeArray %6 %10
+%11 = OpTypeArray %6 %37
-%12 = OpTypeArray %11 %10
+%12 = OpTypeArray %11 %37
 %13 = OpTypeStruct %8 %9 %12
 %14 = OpTypePointer Uniform %13
 %15 = OpVariable %14 Uniform
 %16 = OpTypeInt 32 1
 %17 = OpConstant %16 0
-%18 = OpTypeArray %6 %7
+%18 = OpTypeArray %6 %61
-%19 = OpTypeArray %18 %10
+%19 = OpTypeArray %18 %37
-%20 = OpConstant %6 4
+%20 = OpConstant %16 4
 %21 = OpTypeArray %6 %20
 %22 = OpTypeStruct %19 %21
 %23 = OpTypePointer Uniform %22
 %24 = OpVariable %23 Uniform
 %25 = OpTypePointer Uniform %6
 %28 = OpConstant %6 1
 %31 = OpConstant %16 1
 %34 = OpConstant %6 0
 %37 = OpConstant %16 2
 %61 = OpConstant %16 3
 %4 = OpFunction %2 None %3
 %5 = OpLabel
 %26 = OpAccessChain %25 %24 %17 %17 %17
 %27 = OpLoad %6 %26
-%29 = OpIAdd %6 %27 %28
+%29 = OpIAdd %6 %27 %31
 %30 = OpAccessChain %25 %15 %17 %17
 OpStore %30 %29
-%32 = OpAccessChain %25 %24 %17 %31 %17
+%32 = OpAccessChain %25 %24 %17 %28 %17
 %33 = OpLoad %6 %32
 %35 = OpIAdd %6 %33 %34
-%36 = OpAccessChain %25 %15 %17 %31
+%36 = OpAccessChain %25 %15 %17 %28
 OpStore %36 %35
-%38 = OpAccessChain %25 %24 %17 %31 %31
+%38 = OpAccessChain %25 %24 %17 %28 %28
 %39 = OpLoad %6 %38
 %40 = OpIAdd %6 %39 %10
 %41 = OpAccessChain %25 %15 %17 %37
 OpStore %41 %40
 %42 = OpAccessChain %25 %24 %17 %17 %37
 %43 = OpLoad %6 %42
-%44 = OpAccessChain %25 %15 %31 %17
+%44 = OpAccessChain %25 %15 %28 %17
 OpStore %44 %43
-%45 = OpAccessChain %25 %24 %17 %17 %31
+%45 = OpAccessChain %25 %24 %17 %17 %28
 %46 = OpLoad %6 %45
-%47 = OpIMul %6 %46 %7
+%47 = OpIMul %6 %46 %61
-%48 = OpAccessChain %25 %15 %31 %31
+%48 = OpAccessChain %25 %15 %28 %28
 OpStore %48 %47
-%49 = OpAccessChain %25 %24 %17 %31 %37
+%49 = OpAccessChain %25 %24 %17 %28 %37
 %50 = OpLoad %6 %49
-%51 = OpAccessChain %25 %15 %31 %37
+%51 = OpAccessChain %25 %15 %28 %37
 OpStore %51 %50
-%52 = OpAccessChain %25 %24 %31 %17
+%52 = OpAccessChain %25 %24 %28 %17
 %53 = OpLoad %6 %52
-%54 = OpAccessChain %25 %15 %37 %17 %17
+%54 = OpAccessChain %25 %15 %10 %17 %17
 OpStore %54 %53
-%55 = OpAccessChain %25 %24 %31 %31
+%55 = OpAccessChain %25 %24 %28 %28
 %56 = OpLoad %6 %55
-%57 = OpAccessChain %25 %15 %37 %17 %31
+%57 = OpAccessChain %25 %15 %10 %17 %28
 OpStore %57 %56
-%58 = OpAccessChain %25 %24 %31 %37
+%58 = OpAccessChain %25 %24 %28 %10
 %59 = OpLoad %6 %58
-%60 = OpAccessChain %25 %15 %37 %31 %17
+%60 = OpAccessChain %25 %15 %10 %28 %17
 OpStore %60 %59
-%62 = OpAccessChain %25 %24 %31 %61
+%62 = OpAccessChain %25 %24 %28 %7
 %63 = OpLoad %6 %62
-%64 = OpAccessChain %25 %15 %37 %31 %31
+%64 = OpAccessChain %25 %15 %10 %28 %28
 OpStore %64 %63
 OpReturn
 OpFunctionEnd
)";
  Options options;
  DoStringDiffTest(kSrc, kDst, kDiff, options);
}

TEST(DiffTest, IndexSignednessNoDebug) {
  constexpr char kSrcNoDebug[] = R"(               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %4 "main"
               OpExecutionMode %4 LocalSize 1 1 1
               OpSource ESSL 310
               OpDecorate %8 ArrayStride 4
               OpDecorate %9 ArrayStride 4
               OpDecorate %11 ArrayStride 4
               OpDecorate %12 ArrayStride 8
               OpMemberDecorate %13 0 Offset 0
               OpMemberDecorate %13 1 Offset 12
               OpMemberDecorate %13 2 Offset 24
               OpDecorate %13 BufferBlock
               OpDecorate %15 DescriptorSet 0
               OpDecorate %15 Binding 1
               OpDecorate %18 ArrayStride 16
               OpDecorate %19 ArrayStride 48
               OpDecorate %21 ArrayStride 16
               OpMemberDecorate %22 0 Offset 0
               OpMemberDecorate %22 1 Offset 96
               OpDecorate %22 Block
               OpDecorate %24 DescriptorSet 0
               OpDecorate %24 Binding 0
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeInt 32 0
          %7 = OpConstant %6 3
          %8 = OpTypeArray %6 %7
          %9 = OpTypeArray %6 %7
         %10 = OpConstant %6 2
         %11 = OpTypeArray %6 %10
         %12 = OpTypeArray %11 %10
         %13 = OpTypeStruct %8 %9 %12
         %14 = OpTypePointer Uniform %13
         %15 = OpVariable %14 Uniform
         %16 = OpTypeInt 32 1
         %17 = OpConstant %16 0
         %18 = OpTypeArray %6 %7
         %19 = OpTypeArray %18 %10
         %20 = OpConstant %6 4
         %21 = OpTypeArray %6 %20
         %22 = OpTypeStruct %19 %21
         %23 = OpTypePointer Uniform %22
         %24 = OpVariable %23 Uniform
         %25 = OpTypePointer Uniform %6
         %28 = OpConstant %6 1
         %31 = OpConstant %16 1
         %34 = OpConstant %6 0
         %37 = OpConstant %16 2
         %61 = OpConstant %16 3
          %4 = OpFunction %2 None %3
          %5 = OpLabel
         %26 = OpAccessChain %25 %24 %17 %17 %17
         %27 = OpLoad %6 %26
         %29 = OpIAdd %6 %27 %28
         %30 = OpAccessChain %25 %15 %17 %17
               OpStore %30 %29
         %32 = OpAccessChain %25 %24 %17 %31 %17
         %33 = OpLoad %6 %32
         %35 = OpIAdd %6 %33 %34
         %36 = OpAccessChain %25 %15 %17 %31
               OpStore %36 %35
         %38 = OpAccessChain %25 %24 %17 %31 %31
         %39 = OpLoad %6 %38
         %40 = OpIAdd %6 %39 %10
         %41 = OpAccessChain %25 %15 %17 %37
               OpStore %41 %40
         %42 = OpAccessChain %25 %24 %17 %17 %37
         %43 = OpLoad %6 %42
         %44 = OpAccessChain %25 %15 %31 %17
               OpStore %44 %43
         %45 = OpAccessChain %25 %24 %17 %17 %31
         %46 = OpLoad %6 %45
         %47 = OpIMul %6 %46 %7
         %48 = OpAccessChain %25 %15 %31 %31
               OpStore %48 %47
         %49 = OpAccessChain %25 %24 %17 %31 %37
         %50 = OpLoad %6 %49
         %51 = OpAccessChain %25 %15 %31 %37
               OpStore %51 %50
         %52 = OpAccessChain %25 %24 %31 %17
         %53 = OpLoad %6 %52
         %54 = OpAccessChain %25 %15 %37 %17 %17
               OpStore %54 %53
         %55 = OpAccessChain %25 %24 %31 %31
         %56 = OpLoad %6 %55
         %57 = OpAccessChain %25 %15 %37 %17 %31
               OpStore %57 %56
         %58 = OpAccessChain %25 %24 %31 %37
         %59 = OpLoad %6 %58
         %60 = OpAccessChain %25 %15 %37 %31 %17
               OpStore %60 %59
         %62 = OpAccessChain %25 %24 %31 %61
         %63 = OpLoad %6 %62
         %64 = OpAccessChain %25 %15 %37 %31 %31
               OpStore %64 %63
               OpReturn
               OpFunctionEnd

)";
  constexpr char kDstNoDebug[] = R"(               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %4 "main"
               OpExecutionMode %4 LocalSize 1 1 1
               OpSource ESSL 310
               OpDecorate %8 ArrayStride 4
               OpDecorate %9 ArrayStride 4
               OpDecorate %11 ArrayStride 4
               OpDecorate %12 ArrayStride 8
               OpMemberDecorate %13 0 Offset 0
               OpMemberDecorate %13 1 Offset 12
               OpMemberDecorate %13 2 Offset 24
               OpDecorate %13 BufferBlock
               OpDecorate %15 DescriptorSet 0
               OpDecorate %15 Binding 1
               OpDecorate %18 ArrayStride 16
               OpDecorate %19 ArrayStride 48
               OpDecorate %21 ArrayStride 16
               OpMemberDecorate %22 0 Offset 0
               OpMemberDecorate %22 1 Offset 96
               OpDecorate %22 Block
               OpDecorate %24 DescriptorSet 0
               OpDecorate %24 Binding 0
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeInt 32 0
         %16 = OpTypeInt 32 1
          %7 = OpConstant %16 3
          %8 = OpTypeArray %6 %7
          %9 = OpTypeArray %6 %7
         %10 = OpConstant %16 2
         %11 = OpTypeArray %6 %10
         %12 = OpTypeArray %11 %10
         %13 = OpTypeStruct %8 %9 %12
         %14 = OpTypePointer Uniform %13
         %15 = OpVariable %14 Uniform
         %18 = OpTypeArray %6 %7
         %19 = OpTypeArray %18 %10
         %20 = OpConstant %16 4
         %21 = OpTypeArray %6 %20
         %22 = OpTypeStruct %19 %21
         %23 = OpTypePointer Uniform %22
         %24 = OpVariable %23 Uniform
         %25 = OpTypePointer Uniform %6
         %17 = OpConstant %16 0
         %28 = OpConstant %16 1
         %31 = OpConstant %6 1
         %34 = OpConstant %6 0
         %37 = OpConstant %6 2
         %61 = OpConstant %6 3
          %4 = OpFunction %2 None %3
          %5 = OpLabel
         %26 = OpAccessChain %25 %24 %17 %17 %17
         %27 = OpLoad %6 %26
         %29 = OpIAdd %6 %27 %28
         %30 = OpAccessChain %25 %15 %17 %17
               OpStore %30 %29
         %32 = OpAccessChain %25 %24 %17 %31 %17
         %33 = OpLoad %6 %32
         %35 = OpIAdd %6 %33 %34
         %36 = OpAccessChain %25 %15 %17 %31
               OpStore %36 %35
         %38 = OpAccessChain %25 %24 %17 %31 %31
         %39 = OpLoad %6 %38
         %40 = OpIAdd %6 %39 %37
         %41 = OpAccessChain %25 %15 %17 %10
               OpStore %41 %40
         %42 = OpAccessChain %25 %24 %17 %17 %10
         %43 = OpLoad %6 %42
         %44 = OpAccessChain %25 %15 %31 %17
               OpStore %44 %43
         %45 = OpAccessChain %25 %24 %17 %17 %31
         %46 = OpLoad %6 %45
         %47 = OpIMul %6 %46 %7
         %48 = OpAccessChain %25 %15 %31 %31
               OpStore %48 %47
         %49 = OpAccessChain %25 %24 %17 %31 %10
         %50 = OpLoad %6 %49
         %51 = OpAccessChain %25 %15 %31 %10
               OpStore %51 %50
         %52 = OpAccessChain %25 %24 %31 %17
         %53 = OpLoad %6 %52
         %54 = OpAccessChain %25 %15 %37 %17 %17
               OpStore %54 %53
         %55 = OpAccessChain %25 %24 %31 %31
         %56 = OpLoad %6 %55
         %57 = OpAccessChain %25 %15 %37 %17 %31
               OpStore %57 %56
         %58 = OpAccessChain %25 %24 %31 %37
         %59 = OpLoad %6 %58
         %60 = OpAccessChain %25 %15 %37 %31 %17
               OpStore %60 %59
         %62 = OpAccessChain %25 %24 %31 %61
         %63 = OpLoad %6 %62
         %64 = OpAccessChain %25 %15 %37 %31 %31
               OpStore %64 %63
               OpReturn
               OpFunctionEnd

)";
  constexpr char kDiff[] = R"( ; SPIR-V
 ; Version: 1.6
 ; Generator: Khronos SPIR-V Tools Assembler; 0
 ; Bound: 65
 ; Schema: 0
 OpCapability Shader
 %1 = OpExtInstImport "GLSL.std.450"
 OpMemoryModel Logical GLSL450
 OpEntryPoint GLCompute %4 "main"
 OpExecutionMode %4 LocalSize 1 1 1
 OpSource ESSL 310
 OpDecorate %8 ArrayStride 4
 OpDecorate %9 ArrayStride 4
 OpDecorate %11 ArrayStride 4
 OpDecorate %12 ArrayStride 8
 OpMemberDecorate %13 0 Offset 0
 OpMemberDecorate %13 1 Offset 12
 OpMemberDecorate %13 2 Offset 24
 OpDecorate %13 BufferBlock
 OpDecorate %15 DescriptorSet 0
 OpDecorate %15 Binding 1
 OpDecorate %18 ArrayStride 16
 OpDecorate %19 ArrayStride 48
 OpDecorate %21 ArrayStride 16
 OpMemberDecorate %22 0 Offset 0
 OpMemberDecorate %22 1 Offset 96
 OpDecorate %22 Block
 OpDecorate %24 DescriptorSet 0
 OpDecorate %24 Binding 0
 %2 = OpTypeVoid
 %3 = OpTypeFunction %2
 %6 = OpTypeInt 32 0
 %7 = OpConstant %6 3
-%8 = OpTypeArray %6 %7
+%8 = OpTypeArray %6 %61
-%9 = OpTypeArray %6 %7
+%9 = OpTypeArray %6 %61
 %10 = OpConstant %6 2
-%11 = OpTypeArray %6 %10
+%11 = OpTypeArray %6 %37
-%12 = OpTypeArray %11 %10
+%12 = OpTypeArray %11 %37
 %13 = OpTypeStruct %8 %9 %12
 %14 = OpTypePointer Uniform %13
 %15 = OpVariable %14 Uniform
 %16 = OpTypeInt 32 1
 %17 = OpConstant %16 0
-%18 = OpTypeArray %6 %7
+%18 = OpTypeArray %6 %61
-%19 = OpTypeArray %18 %10
+%19 = OpTypeArray %18 %37
-%20 = OpConstant %6 4
+%20 = OpConstant %16 4
 %21 = OpTypeArray %6 %20
 %22 = OpTypeStruct %19 %21
 %23 = OpTypePointer Uniform %22
 %24 = OpVariable %23 Uniform
 %25 = OpTypePointer Uniform %6
 %28 = OpConstant %6 1
 %31 = OpConstant %16 1
 %34 = OpConstant %6 0
 %37 = OpConstant %16 2
 %61 = OpConstant %16 3
 %4 = OpFunction %2 None %3
 %5 = OpLabel
 %26 = OpAccessChain %25 %24 %17 %17 %17
 %27 = OpLoad %6 %26
-%29 = OpIAdd %6 %27 %28
+%29 = OpIAdd %6 %27 %31
 %30 = OpAccessChain %25 %15 %17 %17
 OpStore %30 %29
-%32 = OpAccessChain %25 %24 %17 %31 %17
+%32 = OpAccessChain %25 %24 %17 %28 %17
 %33 = OpLoad %6 %32
 %35 = OpIAdd %6 %33 %34
-%36 = OpAccessChain %25 %15 %17 %31
+%36 = OpAccessChain %25 %15 %17 %28
 OpStore %36 %35
-%38 = OpAccessChain %25 %24 %17 %31 %31
+%38 = OpAccessChain %25 %24 %17 %28 %28
 %39 = OpLoad %6 %38
 %40 = OpIAdd %6 %39 %10
 %41 = OpAccessChain %25 %15 %17 %37
 OpStore %41 %40
 %42 = OpAccessChain %25 %24 %17 %17 %37
 %43 = OpLoad %6 %42
-%44 = OpAccessChain %25 %15 %31 %17
+%44 = OpAccessChain %25 %15 %28 %17
 OpStore %44 %43
-%45 = OpAccessChain %25 %24 %17 %17 %31
+%45 = OpAccessChain %25 %24 %17 %17 %28
 %46 = OpLoad %6 %45
-%47 = OpIMul %6 %46 %7
+%47 = OpIMul %6 %46 %61
-%48 = OpAccessChain %25 %15 %31 %31
+%48 = OpAccessChain %25 %15 %28 %28
 OpStore %48 %47
-%49 = OpAccessChain %25 %24 %17 %31 %37
+%49 = OpAccessChain %25 %24 %17 %28 %37
 %50 = OpLoad %6 %49
-%51 = OpAccessChain %25 %15 %31 %37
+%51 = OpAccessChain %25 %15 %28 %37
 OpStore %51 %50
-%52 = OpAccessChain %25 %24 %31 %17
+%52 = OpAccessChain %25 %24 %28 %17
 %53 = OpLoad %6 %52
-%54 = OpAccessChain %25 %15 %37 %17 %17
+%54 = OpAccessChain %25 %15 %10 %17 %17
 OpStore %54 %53
-%55 = OpAccessChain %25 %24 %31 %31
+%55 = OpAccessChain %25 %24 %28 %28
 %56 = OpLoad %6 %55
-%57 = OpAccessChain %25 %15 %37 %17 %31
+%57 = OpAccessChain %25 %15 %10 %17 %28
 OpStore %57 %56
-%58 = OpAccessChain %25 %24 %31 %37
+%58 = OpAccessChain %25 %24 %28 %10
 %59 = OpLoad %6 %58
-%60 = OpAccessChain %25 %15 %37 %31 %17
+%60 = OpAccessChain %25 %15 %10 %28 %17
 OpStore %60 %59
-%62 = OpAccessChain %25 %24 %31 %61
+%62 = OpAccessChain %25 %24 %28 %7
 %63 = OpLoad %6 %62
-%64 = OpAccessChain %25 %15 %37 %31 %31
+%64 = OpAccessChain %25 %15 %10 %28 %28
 OpStore %64 %63
 OpReturn
 OpFunctionEnd
)";
  Options options;
  DoStringDiffTest(kSrcNoDebug, kDstNoDebug, kDiff, options);
}

}  // namespace
}  // namespace diff
}  // namespace spvtools
