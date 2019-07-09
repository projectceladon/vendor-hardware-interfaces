#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := \
       async_fd_watcher.cc \
       bluetooth_hci.cc \
       bluetooth_address.cc \
       vendor_interface.cc \
       hci_packetizer.cc \
       hci_protocol.cc \
       h4_protocol.cc \
       mct_protocol.cc \

LOCAL_C_INCLUDES := \
        $(TOP_DIR)system/bt/device/include \
        $(TOP_DIR)system/bt/stack/include \

LOCAL_SHARED_LIBRARIES := \
       android.hardware.bluetooth@1.0 \
       libbase \
       libcutils \
       libhardware \
       libhwbinder \
       libhidlbase \
       libhidltransport \
       liblog \
       libutils \
       libusb \

ifeq ($(BOARD_HAVE_BLUETOOTH_INTEL_ICNV), true)
LOCAL_CFLAGS       := -DBOARD_HAVE_INTEL_CNV
endif #BOARD_HAVE_BLUETOOTH_INTEL_ICNV

LOCAL_MODULE := android.hardware.bluetooth@1.0-impl.vbt
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
       service.cpp \

LOCAL_C_INCLUDES := \
        $(TOP_DIR)system/bt/device/include \
        $(TOP_DIR)system/bt/stack/include \

LOCAL_SHARED_LIBRARIES := \
       android.hardware.bluetooth@1.0-impl.vbt \
       android.hardware.bluetooth@1.0 \
       libbase \
       libcutils \
       libhardware \
       libhwbinder \
       libhidlbase \
       libhidltransport \
       liblog \
       libutils \
       libusb \

LOCAL_MODULE := android.hardware.bluetooth@1.0-service.vbt
LOCAL_INIT_RC := android.hardware.bluetooth@1.0-service.vbt.rc
LOCAL_MODULE_STEM := android.hardware.bluetooth@1.0-service.vbt
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_EXECUTABLE)
