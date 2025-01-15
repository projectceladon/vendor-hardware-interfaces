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
#include "SockFileReader.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>

namespace android {
namespace hardware {
namespace gnss {
namespace common {

#define VSOCK_PORT 1065
#define HOST_CID 2 // Typically, the host CID is 2
#define BUFFER_SIZE 256
#define TIMEOUT_SECONDS 2

void SockFileReader::getDataFromSockFile(const std::string& command, int mMinIntervalMs) {
    char inputBuffer[INPUT_BUFFER_SIZE];
    int gnss_fd, epoll_fd;

    if (gnss_fix_fd_ < 0 && initializeGnssSocket() < 0) {
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
        bytes_read = recv(gnss_fd, &inputBuffer, INPUT_BUFFER_SIZE, 0);
        if (bytes_read <= 0) {
            ALOGE("Read gnss data failed");
            close(gnss_fd);
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

std::string SockFileReader::getLocationData() {
    std::unique_lock<std::mutex> lock(mMutex);
    getDataFromSockFile(CMD_GET_LOCATION, 20);
    return data_[CMD_GET_LOCATION];
}

std::string SockFileReader::getGnssRawMeasurementData() {
    std::unique_lock<std::mutex> lock(mMutex);
    getDataFromSockFile(CMD_GET_RAWMEASUREMENT, 20);
    return data_[CMD_GET_RAWMEASUREMENT];
}

int SockFileReader::initializeGnssSocket() {
    struct sockaddr_vm server_addr;
    struct timeval timeout;
    // Create the vsock socket
    gnss_fix_fd_ = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (gnss_fix_fd_ < 0) {
        ALOGE("Failed to create the gnss vsock");
        return -1;
    }

    // Connect to the host
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.svm_family = AF_VSOCK;
    server_addr.svm_cid = HOST_CID; // Host CID
    server_addr.svm_port = VSOCK_PORT;

    if (connect(gnss_fix_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ALOGE("connect gnss vsock failed");
        close(gnss_fix_fd_);
        gnss_fix_fd_ = -1;
        return -1;
    }

    ALOGV("Connected to gnss vsock!");

    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;
    if (setsockopt(gnss_fix_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        ALOGE("setsockopt for gnss vsock failed");
    }

    return 0;
}

SockFileReader::SockFileReader() {
    initializeGnssSocket();
}

SockFileReader::~SockFileReader() {
    if (gnss_fix_fd_ > 0)
        close(gnss_fix_fd_);
}

}  // namespace common
}  // namespace gnss
}  // namespace hardware
}  // namespace android
