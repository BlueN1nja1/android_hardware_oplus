/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <thread>

#include <aidl/vendor/oplus/hardware/radio/IOplusRadio.h>

#include "OplusRadioResponse.h"

using aidl::vendor::oplus::hardware::radio::IOplusRadio;
using aidl::vendor::oplus::hardware::radio::NrModeState;
using aidl::vendor::oplus::hardware::radio::OplusRadioResponse;
using aidl::vendor::oplus::hardware::radio::IOplusRadio;
using aidl::vendor::oplus::hardware::radio::OplusRadioResponse;
using android::base::GetIntProperty;
using android::base::GetProperty;
using std::chrono_literals::operator""s;

namespace {

    enum OplusNrMode {
        NSA_PRE,
        NSA_ONLY,
        SA_ONLY,
        SA_PRE,
        NR_MODE_MAX,
    };
    constexpr auto kDefaultAutoNrMode = SA_PRE;
    constexpr auto kPropertyDefaultAutoNrMode = "ro.vendor.radio.auto_nr_mode";

    constexpr auto kIRadioAidlServiceName = "vendor.oplus.hardware.radio.IRadioStable/OplusRadio";
    constexpr auto kMaxSimSlot = 2;
    constexpr auto kOplusRilSerial = 1001;

    OplusNrMode GetDefaultAutoNrMode() {
        auto mode = GetIntProperty(kPropertyDefaultAutoNrMode, static_cast<int>(kDefaultAutoNrMode));
        if (mode < NSA_PRE || mode >= NR_MODE_MAX) {
            LOG(WARNING) << "Invalid NR mode: " << mode << ", using default: " << kDefaultAutoNrMode;
            return kDefaultAutoNrMode;
        }
        return static_cast<OplusNrMode>(mode);
    }

    int GetMaxSimSlot() {
        auto multisim = GetProperty("persist.radio.multisim.config",
                                    GetProperty("ro.boot.radio.multisim.config", ""));
        if (multisim == "ssss" || multisim == "SSSS") {
            return 1;
        }
        return kMaxSimSlot;
    }

};  // anonymous namespace

int main() {
    int rc = EXIT_SUCCESS;

    ABinderProcess_setThreadPoolMaxThreadCount(1);
    ABinderProcess_startThreadPool();

    auto mode = GetDefaultAutoNrMode();
    auto max_sim_slot = GetMaxSimSlot();
    for (auto i = 0; i < max_sim_slot; ++i) {
        auto instance = kIRadioAidlServiceName + std::to_string(i);
        if (!AServiceManager_isDeclared(instance.c_str())) {
            // Instance not declared in manifest, skip.
            continue;
        }
        auto radio = IOplusRadio::fromBinder(
            ndk::SpAIBinder(AServiceManager_waitForService(instance.c_str())));
        if (!radio) {
            LOG(DEBUG) << "Failed to get service " << instance;
            continue;
        }

        auto state = std::make_shared<NrModeState>();
        auto resp_cb = ndk::SharedRefBase::make<OplusRadioResponse>(state);
        auto cb_status = radio->setCallback(resp_cb, nullptr);
        if (!cb_status.isOk()) {
            LOG(ERROR) << "setCallback failed for SIM" << i << ": "
            << cb_status.getDescription();
            rc = EXIT_FAILURE;
            continue;
        }

        bool succeed = false;
        int32_t serial = kOplusRilSerial;
        for (auto retry = 100; retry > 0; --retry, ++serial) {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->expected_serial = serial;
                state->done = false;
                state->error = -1;
                state->result = -1;
            }

            auto status = radio->setNrMode(serial, mode);
            if (!status.isOk()) {
                LOG(ERROR) << "setNrMode binder call failed for SIM" << i
                << " (" << status.getDescription()
                << "), remaining retries: " << retry - 1;
                std::this_thread::sleep_for(1s);
                continue;
            }

            std::unique_lock<std::mutex> lock(state->mutex);
            bool got_resp = state->cv.wait_for(lock, 2s, [&] { return state->done; });
            if (!got_resp || state->error != 0 || state->result != 0) {
                LOG(ERROR) << "setNrMode failed for SIM" << i
                << " (timeout=" << !got_resp
                << ", error=" << state->error
                << ", result=" << state->result
                << "), remaining retries: " << retry - 1;
                lock.unlock();
                std::this_thread::sleep_for(1s);
                continue;
            }
            succeed = true;
            break;
        }
        if (!succeed) rc = EXIT_FAILURE;
    }

    return rc;
}
