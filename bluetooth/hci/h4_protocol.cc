/*
 * Copyright 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "h4_protocol.h"

#define LOG_TAG "android.hardware.bluetooth.hci-h4"

#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#include <asm/byteorder.h>
#include <linux/usb/ch9.h>
#include <libusb/libusb.h>

#define HCI_COMMAND_COMPLETE_EVT 0x0E
#define HCI_COMMAND_STATUS_EVT 0x0F
#define HCI_ESCO_CONNECTION_COMP_EVT 0x2C
#define HCI_RESET_SUPPORTED(x) ((x)[5] & 0x80)
#define HCI_GRP_INFORMATIONAL_PARAMS (0x04 << 10)    /* 0x1000 */
#define HCI_READ_LOCAL_SUPPORTED_CMDS (0x0002 | HCI_GRP_INFORMATIONAL_PARAMS)
#define HCI_GRP_HOST_CONT_BASEBAND_CMDS (0x03 << 10) /* 0x0C00 */
#define HCI_RESET (0x0003 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)

#define INTEL_VID 0x8087
#define INTEL_PID_8265 0x0a2b // Windstorm peak
#define INTEL_PID_3168 0x0aa7 //SandyPeak (SdP)
#define INTEL_PID_9260 0x0025 // 9160/9260 (also known as ThunderPeak)
#define INTEL_PID_9560 0x0aaa // 9460/9560 also know as Jefferson Peak (JfP)
#define INTEL_PID_AX201 0x0026 // AX201 also know as Harrison Peak (HrP)
#define INTEL_PID_AX211 0x0033 // AX211 also know as GarfieldPeak (Gfp)
#define INTEL_PID_AX210 0x0032 // AX210 also know as TyphoonPeak (TyP2)

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/uio.h>
#include <string.h>

#include "log/log.h"

namespace android::hardware::bluetooth::hci {

H4Protocol::H4Protocol(int fd, PacketReadCallback cmd_cb,
                       PacketReadCallback acl_cb, PacketReadCallback sco_cb,
                       PacketReadCallback event_cb, PacketReadCallback iso_cb,
                       DisconnectCallback disconnect_cb)
    : uart_fd_(fd),
      cmd_cb_(std::move(cmd_cb)),
      acl_cb_(std::move(acl_cb)),
      sco_cb_(std::move(sco_cb)),
      event_cb_(std::move(event_cb)),
      iso_cb_(std::move(iso_cb)),
      disconnect_cb_(std::move(disconnect_cb)) {}

size_t H4Protocol::Send(PacketType type, const std::vector<uint8_t>& vector) {
  return Send(type, vector.data(), vector.size());
}

size_t H4Protocol::Send(PacketType type, const uint8_t* data, size_t length) {
  /* For HCI communication over USB dongle, multiple write results in
   * response timeout as driver expect type + data at once to process
   * the command, so using "writev"(for atomicity) here.
   */
  struct iovec iov[2];
  ssize_t ret = 0;
  iov[0].iov_base = &type;
  iov[0].iov_len = sizeof(type);
  iov[1].iov_base = (void*)data;
  iov[1].iov_len = length;
  while (1) {
    ret = TEMP_FAILURE_RETRY(writev(uart_fd_, iov, 2));
    if (ret == -1) {
      LOG_ALWAYS_FATAL("%s error writing to UART (%s)", __func__,
                       strerror(errno));
    } else if (ret == 0) {
      // Nothing written :(
      ALOGE("%s zero bytes written - something went wrong...", __func__);
      break;
    }
    break;
  }
  return ret;
}

bool H4Protocol::IsIntelController(uint16_t vid, uint16_t pid) {
    if ((vid == INTEL_VID) && ((pid == INTEL_PID_8265) ||
                                (pid == INTEL_PID_3168)||
                                (pid == INTEL_PID_9260)||
                                (pid == INTEL_PID_9560)||
                                (pid == INTEL_PID_AX201)||
                                (pid == INTEL_PID_AX211)||
                                (pid == INTEL_PID_AX210)))
        return true;
    else
	return false;
}

int H4Protocol::GetUsbpath(void) {
    size_t count, i;
    int ret = 0, busnum, devnum;
    struct libusb_device **dev_list = NULL;
    struct libusb_context *ctx;
    uint16_t vid = 0, pid = 0;
    ALOGD(" Initializing GenericUSB (libusb-1.0)...\n");
    ret = libusb_init(&ctx);
    if (ret < 0) {
        ALOGE("libusb failed to initialize: %d\n", ret);
        return ret;
    }
    count = libusb_get_device_list(ctx, &dev_list);
    if (count <= 0) {
        ALOGE("Error getting USB device list: %s\n", strerror(count));
        goto exit;
    }
    for (i = 0; i < count; ++i) {
        struct libusb_device* dev = dev_list[i];
        busnum = libusb_get_bus_number(dev);
        devnum = libusb_get_device_address(dev);
        struct libusb_device_descriptor descriptor;
        ret = libusb_get_device_descriptor(dev, &descriptor);
        if (ret < 0)  {
            ALOGE("Error getting device descriptor %d ", ret);
            goto exit;
        }
        vid = descriptor.idVendor;
        pid = descriptor.idProduct;
        if (H4Protocol::IsIntelController(vid, pid)) {
            snprintf(dev_address, sizeof(dev_address), "/dev/bus/usb/%03d/%03d",
                                                       busnum, devnum);
            ALOGD("Value of BT device address = %s", dev_address);
            goto exit;
        }
    }
exit:
    libusb_free_device_list(dev_list, count);
    libusb_exit(ctx);
    return ret;
}

int H4Protocol::SendHandle(void) {
    int fd, ret = 0;
    fd = open(dev_address,O_WRONLY|O_NONBLOCK);
    if (fd < 0) {
        ALOGE("Fail to open USB device %s, value of fd= %d", dev_address, fd);
        return -1;
    } else {
        struct usbdevfs_ioctl   wrapper;
        wrapper.ifno = 1;
        wrapper.ioctl_code = USBDEVFS_IOCTL;
        wrapper.data = sco_handle;
        ret = ioctl(fd, USBDEVFS_IOCTL, &wrapper);
        if (ret < 0)
            ALOGE("Failed to send SCO handle err = %d", ret);
        close(fd);
        return ret;
    }
}

size_t H4Protocol::OnPacketReady(const std::vector<uint8_t>& packet) {
  int ret = 0;
  switch (hci_packet_type_) {
    case PacketType::COMMAND:
      cmd_cb_(packet);
      break;
    case PacketType::ACL_DATA:
      acl_cb_(packet);
      break;
    case PacketType::SCO_DATA:
      sco_cb_(packet);
      break;
    case PacketType::EVENT:
      if (!packet.empty()) {
          if (packet[0] == HCI_COMMAND_COMPLETE_EVT) {
              unsigned int cmd, lsb, msb;
              msb = packet[4];
              lsb = packet[3];
              cmd = msb << 8 | lsb ;
              if (cmd == HCI_RESET) {
                  event_cb_(packet);
                  hci_packet_type_ = PacketType::UNKNOWN;
                  ret = H4Protocol::GetUsbpath();
                  if (ret < 0)
                      ALOGE("Failed to get the USB path for btusb-sound-card");
                  break;
              }
          } else if (packet[0] == HCI_ESCO_CONNECTION_COMP_EVT) {
              const unsigned char *handle = packet.data() + 3;
              memcpy(sco_handle, handle, 2);
              ALOGD("Value of SCO handle = %x, %x", handle[0], handle[1]);
              ret = H4Protocol::SendHandle();
              if (ret < 0)
                  ALOGE("Failed to send SCO handle to btusb-sound-card driver");
          }
      }
      event_cb_(packet);
      break;
    case PacketType::ISO_DATA:
      iso_cb_(packet);
      break;
    default: {
      LOG_ALWAYS_FATAL("Bad packet type 0x%x",
                       static_cast<int>(hci_packet_type_));
    }
  }
  return packet.size();
}

void H4Protocol::SendDataToPacketizer(uint8_t* buffer, size_t length) {
  std::vector<uint8_t> input_buffer{buffer, buffer + length};
  size_t buffer_offset = 0;
  while (buffer_offset < input_buffer.size()) {
    if (hci_packet_type_ == PacketType::UNKNOWN) {
      hci_packet_type_ =
          static_cast<PacketType>(input_buffer.data()[buffer_offset]);
      buffer_offset += 1;
    } else {
      bool packet_ready = hci_packetizer_.OnDataReady(
          hci_packet_type_, input_buffer, &buffer_offset);
      if (packet_ready) {
        // Call packet callback.
        OnPacketReady(hci_packetizer_.GetPacket());
        // Get ready for the next type byte.
        hci_packet_type_ = PacketType::UNKNOWN;
      }
    }
  }
}

void H4Protocol::OnDataReady() {
  if (disconnected_) {
    return;
  }
  uint8_t buffer[kMaxPacketLength];
  ssize_t bytes_read =
      TEMP_FAILURE_RETRY(read(uart_fd_, buffer, kMaxPacketLength));
  if (bytes_read == 0) {
    ALOGI("No bytes read, calling the disconnect callback");
    disconnected_ = true;
    disconnect_cb_();
    return;
  }
  if (bytes_read < 0) {
    ALOGW("error reading from UART (%s)", strerror(errno));
    return;
  }
  SendDataToPacketizer(buffer, bytes_read);
}

}  // namespace android::hardware::bluetooth::hci
