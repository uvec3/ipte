LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cc .cpp .cxx
LOCAL_SRC_FILES:=test.cpp
LOCAL_MODULE:=spirvtools_test
LOCAL_LDLIBS:=-landroid
LOCAL_CXXFLAGS:=-std=c++11 -fno-exceptions -fno-rtti -Werror
LOCAL_STATIC_LIBRARIES=SPIRV-Tools SPIRV-Tools-opt
include $(BUILD_SHARED_LIBRARY)

include $(LOCAL_PATH)/../Android.mk
