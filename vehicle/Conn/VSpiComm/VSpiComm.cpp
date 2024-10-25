/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "VSpiComm"

#include <android/log.h>
#include <log/log.h>
#include <poll.h>
#include <iostream>
#include <cstring>

#include "VSpiComm.h"

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace V2_0 {

namespace impl {

VSpiComm::VSpiComm(MessageProcessor* messageProcessor)
    : CommConn(messageProcessor), mSpiDevice("/dev/spidev0.0"), mMode(SPI_MODE_0), mBitsPerWord(8), mSpeed(100000) {
    // Constructor now only initializes member variables
}

void VSpiComm::start() {
    ALOGI("Opening SPI device: %s", mSpiDevice.c_str()); // Print mSpiDevice

    mSpiFd = open(mSpiDevice.c_str(), O_RDWR);
    if (mSpiFd < 0) {
        ALOGE("Failed to open SPI device %s, mSpiFd: %d", mSpiDevice.c_str(), mSpiFd);
        return;
    }

    if (ioctl(mSpiFd, SPI_IOC_WR_MODE32, &mMode) < 0) {
        ALOGE("Failed to set SPI mode");
        close(mSpiFd);
        mSpiFd = -1;
        return;
    }

    if (ioctl(mSpiFd, SPI_IOC_WR_BITS_PER_WORD, &mBitsPerWord) < 0) {
        ALOGE("Failed to set bits per word");
        close(mSpiFd);
        mSpiFd = -1;
        return;
    }

    if (ioctl(mSpiFd, SPI_IOC_WR_MAX_SPEED_HZ, &mSpeed) < 0) {
        ALOGE("Failed to set max speed");
        close(mSpiFd);
        mSpiFd = -1;
        return;
    }

    // Start the read thread
    CommConn::start();
}

void VSpiComm::stop() {
    // Stop the read thread
    CommConn::stop();

    // Close the SPI device
    if (mSpiFd > 0) {
        ALOGI("Closing SPI device: %s", mSpiDevice.c_str());
        close(mSpiFd);
        mSpiFd = -1;
    }
}

VSpiComm::~VSpiComm() {
    if (mSpiFd > 0) {
        close(mSpiFd);
    }
}

std::vector<uint8_t> VSpiComm::read() {
    uint8_t rxBuffer[128] = {0}; // Buffer for rx_buf with max size 128
    uint8_t dummyTxBuffer[128] = {0}; // Dummy buffer for tx_buf with max size 128

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)dummyTxBuffer,
        .rx_buf = (unsigned long)rxBuffer,
        .len = sizeof(rxBuffer),
        .speed_hz = mSpeed,
        .bits_per_word = mBitsPerWord,
    };

    // Check if mSpiFd is valid before calling ioctl
    if (mSpiFd < 0) {
        ALOGE("Invalid SPI file descriptor: %d", mSpiFd);
        return {};
    }

    // Poll the file descriptor to check if it is ready for reading
    struct pollfd fds;
    fds.fd = mSpiFd;
    fds.events = POLLIN | POLLERR;
    fds.revents = 0;
    int ret = poll(&fds, 1, -1); // Wait indefinitely for the file descriptor to be ready
    if (ret <= 0) {
        ALOGE("Polling SPI file descriptor failed or timed out");
        return {};
    }

    // Lock the mutex after polling
    std::lock_guard<std::mutex> lock(mSpiMutex);

    if (ioctl(mSpiFd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        ALOGE("Failed to read from SPI device");
        return {};
    }

    // Extract the actual data size from the first four bytes of rxBuffer
    uint32_t dataSize;
    memcpy(&dataSize, rxBuffer, sizeof(dataSize));

    // Ensure the dataSize does not exceed the buffer size
    if (dataSize > sizeof(rxBuffer) - sizeof(dataSize)) {
        ALOGE("Invalid data size received: %u", dataSize);
        return {};
    }

    // Extract the actual data from rxBuffer based on the extracted data size
    std::vector<uint8_t> data(rxBuffer + sizeof(dataSize), rxBuffer + sizeof(dataSize) + dataSize);

    return data;
}

int VSpiComm::write(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mSpiMutex);

    uint8_t dummyRxBuffer[128] = {0}; // Dummy buffer for rx_buf
    uint8_t txBuffer[128] = {0}; // Buffer to store size and data

    // Store the size of the data in the first four bytes
    uint32_t dataSize = static_cast<uint32_t>(data.size());
    std::memcpy(txBuffer, &dataSize, sizeof(dataSize));

    // Store the actual data after the size
    std::memcpy(txBuffer + sizeof(dataSize), data.data(), data.size());

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)txBuffer,
        .rx_buf = (unsigned long)dummyRxBuffer,
        .len = 128,
        .speed_hz = mSpeed,
        .bits_per_word = mBitsPerWord,
    };

    // Check if mSpiFd is valid before calling ioctl
    if (mSpiFd < 0) {
        ALOGE("Invalid SPI file descriptor: %d", mSpiFd);
        return -1;
    }

    if (ioctl(mSpiFd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        ALOGE("Failed to write to SPI device");
        return -1;
    }

    return data.size();
}


}  // impl

}  // namespace V2_0
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

