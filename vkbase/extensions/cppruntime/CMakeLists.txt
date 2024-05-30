cmake_minimum_required(VERSION 3.16)
project(vkbase)
set(CMAKE_CXX_STANDARD 20)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)


# Note: used llvm-config.exe --libs to retrieve the list of libraries below:
SET(LLVM_LIBS LLVMXRay LLVMWindowsManifest LLVMTableGen LLVMSymbolize LLVMDebugInfoPDB LLVMOrcJIT  LLVMJITLink LLVMObjectYAML LLVMMCA LLVMLTO LLVMPasses LLVMCoroutines LLVMObjCARCOpts LLVMLineEditor LLVMLibDriver LLVMInterpreter LLVMFuzzMutate LLVMMCJIT LLVMExecutionEngine LLVMRuntimeDyld LLVMDWARFLinker LLVMDlltoolDriver LLVMOption LLVMDebugInfoGSYM LLVMCoverage LLVMX86Disassembler LLVMX86AsmParser LLVMX86CodeGen LLVMX86Desc  LLVMX86Info   LLVMipo LLVMInstrumentation LLVMVectorize LLVMLinker LLVMIRReader LLVMAsmParser LLVMFrontendOpenMP LLVMMCDisassembler  LLVMCFGuard LLVMGlobalISel LLVMSelectionDAG LLVMAsmPrinter LLVMDebugInfoDWARF LLVMCodeGen LLVMTarget LLVMScalarOpts LLVMInstCombine LLVMAggressiveInstCombine LLVMTransformUtils LLVMBitWriter LLVMAnalysis LLVMProfileData LLVMObject LLVMTextAPI LLVMBitReader LLVMCore LLVMRemarks LLVMBitstreamReader LLVMMCParser LLVMMC LLVMDebugInfoCodeView LLVMDebugInfoMSF LLVMBinaryFormat LLVMSupport LLVMDemangle)


SET(CLANG_LIBS clangAST clangBasic clangLex clangCodeGen clangFrontend clangEdit
		clangSerialization clangSema clangDriver clangParse clangAnalysis)

ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
ADD_DEFINITIONS(-DNOMINMAX)

add_library(cppruntime cppruntime.cpp cppruntime.hpp)

#shared library
add_library(cppruntime_shared SHARED shared.c)

target_link_libraries(cppruntime PRIVATE ${CLANG_LIBS} ${LLVM_LIBS})

add_executable(cppruntime_test test.cpp)
target_link_libraries(cppruntime_test PRIVATE cppruntime cppruntime_shared)
