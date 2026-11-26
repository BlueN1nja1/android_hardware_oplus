/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/vibrator/BnVibrator.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace aidl {
  namespace android {
    namespace hardware {
      namespace vibrator {

        class Vibrator : public BnVibrator {
        public:
          Vibrator();
          ~Vibrator() override;

          ndk::ScopedAStatus getCapabilities(int32_t* _aidl_return) override;
          ndk::ScopedAStatus off() override;
          ndk::ScopedAStatus on(int32_t timeoutMs,
                                const std::shared_ptr<IVibratorCallback>& callback) override;
                                ndk::ScopedAStatus perform(Effect effect, EffectStrength strength,
                                                           const std::shared_ptr<IVibratorCallback>& callback,
                                                           int32_t* _aidl_return) override;
                                                           ndk::ScopedAStatus getSupportedEffects(std::vector<Effect>* _aidl_return) override;
                                                           ndk::ScopedAStatus setAmplitude(float amplitude) override;
                                                           ndk::ScopedAStatus setExternalControl(bool enabled) override;
                                                           ndk::ScopedAStatus getCompositionDelayMax(int32_t* maxDelayMs) override;
                                                           ndk::ScopedAStatus getCompositionSizeMax(int32_t* maxSize) override;
                                                           ndk::ScopedAStatus getSupportedPrimitives(std::vector<CompositePrimitive>* supported) override;
                                                           ndk::ScopedAStatus getPrimitiveDuration(CompositePrimitive primitive,
                                                                                                   int32_t* durationMs) override;
                                                                                                   ndk::ScopedAStatus compose(const std::vector<CompositeEffect>& composite,
                                                                                                                              const std::shared_ptr<IVibratorCallback>& callback) override;
                                                                                                                              ndk::ScopedAStatus getSupportedAlwaysOnEffects(std::vector<Effect>* _aidl_return) override;
                                                                                                                              ndk::ScopedAStatus alwaysOnEnable(int32_t id, Effect effect, EffectStrength strength) override;
                                                                                                                              ndk::ScopedAStatus alwaysOnDisable(int32_t id) override;
                                                                                                                              ndk::ScopedAStatus getResonantFrequency(float* resonantFreqHz) override;
                                                                                                                              ndk::ScopedAStatus getQFactor(float* qFactor) override;
                                                                                                                              ndk::ScopedAStatus getFrequencyResolution(float* freqResolutionHz) override;
                                                                                                                              ndk::ScopedAStatus getFrequencyMinimum(float* freqMinimumHz) override;
                                                                                                                              ndk::ScopedAStatus getBandwidthAmplitudeMap(std::vector<float>* _aidl_return) override;
                                                                                                                              ndk::ScopedAStatus getPwlePrimitiveDurationMax(int32_t* durationMs) override;
                                                                                                                              ndk::ScopedAStatus getPwleCompositionSizeMax(int32_t* maxSize) override;
                                                                                                                              ndk::ScopedAStatus getSupportedBraking(std::vector<Braking>* supported) override;
                                                                                                                              ndk::ScopedAStatus composePwle(const std::vector<PrimitivePwle>& composite,
                                                                                                                                                             const std::shared_ptr<IVibratorCallback>& callback) override;

        private:
          struct WorkItem {
            enum class Type {
              WAIT_CALLBACK,
              COMPOSE,
            };
            Type type;
            uint64_t jobId;
            int32_t durationMs;
            std::vector<CompositeEffect> composite;
            std::shared_ptr<IVibratorCallback> callback;
          };

          struct PrimitiveMapping {
            uint32_t effectId;
            int32_t durationMs;
            float strengthFactor;
          };

          static bool mapEffect(Effect effect, uint32_t* effectId, int32_t* fallbackDurationMs);
          static bool mapPrimitive(CompositePrimitive primitive, PrimitiveMapping* out);
          void workerLoop();

          std::mutex mHwMutex;
          std::mutex mMutex;
          std::condition_variable mCv;
          std::thread mWorkerThread;
          bool mShutdown{false};
          bool mInExternalControl{false};
          uint64_t mActiveJobId{0};
          std::optional<WorkItem> mPendingJob;
        };

      }  // namespace vibrator
    }  // namespace hardware
  }  // namespace android
}  // namespace aidl
