/*
 * Copyright (C) 2021 The Android Open Source Project
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
#include "DeviceFileReader.h"
#include <termios.h>

namespace android {
namespace hardware {
namespace gnss {
namespace common {

void DeviceFileReader::getDataFromDeviceFile(const std::string& command, int mMinIntervalMs) {
    char inputBuffer[INPUT_BUFFER_SIZE];
    int gnss_fd, epoll_fd;

    if (gnss_fix_fd_ == -1 && initializeGnssDevice() < 0) {
        ALOGV("Failed to initialize the gnss device");
        return;
    }

    if (command == CMD_GET_LOCATION) {
        gnss_fd = gnss_fix_fd_;
    } else if (command == CMD_GET_RAWMEASUREMENT) {
        /*Todo: may set to raw device fd*/
        gnss_fd = gnss_fix_fd_;
    } else {
        // Invalid command
        return;
    }

    // Create an epoll instance.
    if ((epoll_fd = epoll_create1(EPOLL_CLOEXEC)) < 0) {
        return;
    }

    // Add file descriptor to epoll instance.
    struct epoll_event ev, events[1];
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = gnss_fd;
    ev.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, gnss_fd, &ev) == -1) {
        ALOGE("gnss epoll_ctl failed");
        close(epoll_fd);
        close(gnss_fd);
        gnss_fix_fd_ = -1;
        return;
    }

    // Wait for device file event.
    if (epoll_wait(epoll_fd, events, 1, mMinIntervalMs) == -1) {
        ALOGE("gnss epoll_wait failed");
        close(epoll_fd);
        close(gnss_fd);
        gnss_fix_fd_ = -1;
        return;
    }

    // Handle event and write data to string buffer.
    int bytes_read = -1;
    int total_read = 0;
    std::string inputStr = "";
    while (total_read < BYTES_PER_SES) {
        memset(inputBuffer, 0, INPUT_BUFFER_SIZE);
        bytes_read = read(gnss_fd, &inputBuffer, INPUT_BUFFER_SIZE);
        if (bytes_read <= 0) {
            ALOGE("Read gnss data failed");
            gnss_fix_fd_ = -1;
            break;
        }
        total_read += bytes_read;
        inputStr += std::string(inputBuffer, bytes_read);
    }
    close(epoll_fd);

    // Cache the injected data.
    if (command == CMD_GET_LOCATION) {
        // TODO validate data
        data_[CMD_GET_LOCATION] = inputStr;
    } else if (command == CMD_GET_RAWMEASUREMENT) {
        if (ReplayUtils::isGnssRawMeasurement(inputStr)) {
            data_[CMD_GET_RAWMEASUREMENT] = inputStr;
        }
    }
}

std::string DeviceFileReader::getLocationData() {
    std::unique_lock<std::mutex> lock(mMutex);
    getDataFromDeviceFile(CMD_GET_LOCATION, 20);
    return data_[CMD_GET_LOCATION];
}

std::string DeviceFileReader::getGnssRawMeasurementData() {
    std::unique_lock<std::mutex> lock(mMutex);
    getDataFromDeviceFile(CMD_GET_RAWMEASUREMENT, 20);
    return data_[CMD_GET_RAWMEASUREMENT];
}

bool DeviceFileReader::findGnssDevice() {
    string gnssDevBase = "/dev/ttyUSB";
    string gnssDev;
    struct termios tty;
    char buf[128];
    int bytes_read = -1;
    for (int i = 0; i < DEVICE_COUNT; i++) {
        gnssDev = gnssDevBase + to_string(i);
        if ((gnss_fix_fd_ = open(gnssDev.c_str(),
                  O_RDWR | O_NOCTTY)) == -1) {
            ALOGV("GNSS open %s failed", gnssDev.c_str());
            continue;
        }
        memset(buf, 0, sizeof(buf));

        if (tcgetattr(gnss_fix_fd_, &tty) != 0) {
            ALOGE("Error getting GNSS terminal attributes");
            close(gnss_fix_fd_);
            gnss_fix_fd_ = -1;
            continue;
        }

        tty.c_cc[VMIN] = 128;
        tty.c_cc[VTIME] = 10;

        if (tcsetattr(gnss_fix_fd_, TCSANOW, &tty) != 0) {
            ALOGE("Error setting GNSS terminal attributes");
            close(gnss_fix_fd_);
            gnss_fix_fd_ = -1;
            continue;
        }

        bytes_read = read(gnss_fix_fd_, buf, sizeof(buf));
        if (bytes_read <= 0) {
            ALOGE("Read gnss data failed, invalid gnss device?");
            close(gnss_fix_fd_);
            gnss_fix_fd_ = -1;
            continue;
        }

        if (strstr(buf, "$GP") != nullptr ||
                strstr(buf, "$GN") != nullptr ||
                strstr(buf, "$BD") != nullptr) {
            ALOGD("Find the GNSS device: %s", gnssDev.c_str());
            close(gnss_fix_fd_);
            if ((gnss_fix_fd_ = open(gnssDev.c_str(),
                      O_RDWR | O_NOCTTY)) == -1) {
                ALOGE("Reopen the GNSS device with blocking mode failed");
                return false;
            }
            return true;
        }

        close(gnss_fix_fd_);
        gnss_fix_fd_ = -1;
    }

    return false;
}

int DeviceFileReader::initializeGnssDevice() {
    if (!findGnssDevice()) {
        ALOGV("Find gnss device failed");
        return -1;
    }

    //TODO: Need add support of customizing GNSS device
    /*
    if (write(gnss_fix_fd_, CMD_SET_CC_GGARMC, strlen(CMD_SET_CC_GGARMC)) <= 0) {
        ALOGE("CMD_SET_CC_GGARMC failed");
        close(gnss_fix_fd_);
        gnss_fix_fd_ = -1;
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (write(gnss_fix_fd_, CMD_SET_CC_2HZ, strlen(CMD_SET_CC_2HZ)) <= 0) {
        ALOGE("CMD_SET_CC_2HZ failed");
        close(gnss_fix_fd_);
        gnss_fix_fd_ = -1;
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (write(gnss_fix_fd_, CMD_SET_CC_BR115200, strlen(CMD_SET_CC_BR115200)) <= 0) {
        ALOGE("CMD_SET_CC_BR115200 failed");
        close(gnss_fix_fd_);
        gnss_fix_fd_ = -1;
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    close(gnss_fix_fd_);
    if ((gnss_fix_fd_ = open(deviceFilePath.c_str(), O_RDWR | O_NOCTTY)) == -1) {
        ALOGE("Reopen gnss serial device failed");
        return -1;
    }*/

    return 0;
}

DeviceFileReader::DeviceFileReader() {
    initializeGnssDevice();
}

DeviceFileReader::~DeviceFileReader() {
    if (gnss_fix_fd_ > 0)
        close(gnss_fix_fd_);
}

}  // namespace common
}  // namespace gnss
}  // namespace hardware
}  // namespace android
