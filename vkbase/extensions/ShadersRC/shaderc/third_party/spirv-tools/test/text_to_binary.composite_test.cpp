// Copyright (c) 2018 Google LLC
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

// Assembler tests for instructions in the "Group Instrucions" section of the
// SPIR-V spec.

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "test/test_fixture.h"
#include "test/unit_spirv.h"

using ::testing::Eq;
using ::testing::HasSubstr;

namespace spvtools {
namespace {

using spvtest::Concatenate;

using CompositeRoundTripTest = RoundTripTest;

TEST_F(CompositeRoundTripTest, Good) {
  std::string spirv = "%2 = OpCopyLogical %1 %3\n";
  std::string disassembly = EncodeAndDecodeSuccessfully(
      spirv, SPV_BINARY_TO_TEXT_OPTION_NONE, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_THAT(disassembly, Eq(spirv));
}

TEST_F(CompositeRoundTripTest, V13Bad) {
  std::string spirv = "%2 = OpCopyLogical %1 %3\n";
  std::string err = CompileFailure(spirv, SPV_ENV_UNIVERSAL_1_3);
  EXPECT_THAT(err, HasSubstr("Invalid Opcode name 'OpCopyLogical'"));
}

}  // namespace
}  // namespace spvtools
