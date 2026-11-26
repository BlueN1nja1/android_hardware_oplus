/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Vibrator.h"

#include <cutils/properties.h>
#include <inttypes.h>
#include <log/log.h>

#include <algorithm>
#include <chrono>
#include <cmath>

#include "aac_vibra_function.h"

#define RICHTAP_LIGHT_STRENGTH 75
#define RICHTAP_MEDIUM_STRENGTH 90
#define RICHTAP_STRONG_STRENGTH 100
#define RICHTAP_CLICK_MEDIUM_STRENGTH 52

namespace {

    constexpr int32_t kComposeDelayMaxMs = 10000;
    constexpr int32_t kComposeSizeMax = 128;

    constexpr uint32_t kRichTapPrebakedTick = 4096;
    constexpr uint32_t kRichTapPrebakedClick = 4097;
    constexpr uint32_t kRichTapPrebakedHeavy = 4098;

}  // namespace

namespace aidl {
    namespace android {
        namespace hardware {
            namespace vibrator {

                Vibrator::Vibrator() {
                    uint32_t deviceType = 0;

                    int32_t ret = aac_vibra_init(&deviceType);
                    if (ret) {
                        ALOGE("AAC init failed: %d", ret);
                    } else {
                        aac_vibra_looper_start();
                        ALOGI("AAC init success: %u", deviceType);
                    }

                    mWorkerThread = std::thread(&Vibrator::workerLoop, this);
                }

                Vibrator::~Vibrator() {
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        mShutdown = true;
                        ++mActiveJobId;
                        mPendingJob.reset();
                    }
                    mCv.notify_all();
                    if (mWorkerThread.joinable()) {
                        mWorkerThread.join();
                    }
                }

                bool Vibrator::mapEffect(Effect effect, uint32_t* effectId, int32_t* fallbackDurationMs) {
                    switch (effect) {
                        case Effect::TICK:
                        case Effect::TEXTURE_TICK:
                            *effectId = kRichTapPrebakedTick;
                            *fallbackDurationMs = 12;
                            return true;
                        case Effect::CLICK:
                        case Effect::POP:
                            *effectId = kRichTapPrebakedClick;
                            *fallbackDurationMs = 18;
                            return true;
                        case Effect::DOUBLE_CLICK:
                        case Effect::THUD:
                        case Effect::HEAVY_CLICK:
                            *effectId = kRichTapPrebakedHeavy;
                            *fallbackDurationMs = 30;
                            return true;
                        default:
                            return false;
                    }
                }

                bool Vibrator::mapPrimitive(CompositePrimitive primitive, PrimitiveMapping* out) {
                    switch (primitive) {
                        case CompositePrimitive::LIGHT_TICK:
                            *out = {kRichTapPrebakedTick, 12, 1.50f};
                            return true;
                        case CompositePrimitive::LOW_TICK:
                            *out = {kRichTapPrebakedTick, 16, 0.35f};
                            return true;
                        case CompositePrimitive::CLICK:
                            *out = {kRichTapPrebakedTick, 14, 1.00f};
                            return true;
                        case CompositePrimitive::QUICK_RISE:
                            *out = {kRichTapPrebakedClick, 25, 0.88f};
                            return true;
                        case CompositePrimitive::QUICK_FALL:
                            *out = {kRichTapPrebakedHeavy, 24, 0.80f};
                            return true;
                        case CompositePrimitive::SLOW_RISE:
                            *out = {kRichTapPrebakedHeavy, 45, 0.85f};
                            return true;
                        case CompositePrimitive::SPIN:
                            *out = {kRichTapPrebakedHeavy, 36, 0.88f};
                            return true;
                        case CompositePrimitive::THUD:
                            *out = {kRichTapPrebakedHeavy, 28, 0.95f};
                            return true;
                        default:
                            return false;
                    }
                }

                void Vibrator::workerLoop() {
                    std::unique_lock<std::mutex> lock(mMutex);
                    while (!mShutdown) {
                        mCv.wait(lock, [this] { return mShutdown || mPendingJob.has_value(); });
                        if (mShutdown) {
                            break;
                        }

                        WorkItem item = std::move(*mPendingJob);
                        mPendingJob.reset();

                        if (item.jobId != mActiveJobId) {
                            continue;
                        }

                        bool completed = true;

                        if (item.type == WorkItem::Type::WAIT_CALLBACK) {
                            if (item.durationMs > 0) {
                                bool cancelled = mCv.wait_for(
                                    lock, std::chrono::milliseconds(item.durationMs),
                                                              [this, &item] { return mShutdown || mActiveJobId != item.jobId; });
                                if (cancelled) {
                                    completed = false;
                                }
                            }
                        } else if (item.type == WorkItem::Type::COMPOSE) {
                            bool isSliderBurst = (item.composite.size() >= 2 &&
                            item.composite[0].primitive == CompositePrimitive::LOW_TICK &&
                            item.composite[1].primitive == CompositePrimitive::LOW_TICK &&
                            item.composite[1].delayMs == 0 &&
                            std::fabs(item.composite[0].scale - item.composite[1].scale) < 0.001f);
                            bool isSingleLowTick = (item.composite.size() == 1 &&
                            item.composite[0].primitive == CompositePrimitive::LOW_TICK);
                            CompositePrimitive prevPrimitive = CompositePrimitive::NOOP;
                            float prevScale = -1.0f;
                            for (const auto& effect : item.composite) {
                                if (effect.delayMs == 0 &&
                                    effect.primitive == CompositePrimitive::LOW_TICK &&
                                    prevPrimitive == CompositePrimitive::LOW_TICK &&
                                    std::fabs(effect.scale - prevScale) < 0.001f) {
                                    continue;
                                    }
                                    prevPrimitive = effect.primitive;
                                prevScale = effect.scale;

                                if (effect.delayMs > 0) {
                                    bool cancelled = mCv.wait_for(
                                        lock, std::chrono::milliseconds(effect.delayMs),
                                                                  [this, &item] { return mShutdown || mActiveJobId != item.jobId; });
                                    if (cancelled) {
                                        completed = false;
                                        break;
                                    }
                                }

                                if (effect.primitive == CompositePrimitive::NOOP || effect.scale <= 0.0f) {
                                    continue;
                                }

                                PrimitiveMapping mapping{};
                                if (!mapPrimitive(effect.primitive, &mapping)) {
                                    completed = false;
                                    break;
                                }

                                int32_t strength = std::clamp<int32_t>(
                                    static_cast<int32_t>(std::lround(
                                        effect.scale * mapping.strengthFactor * RICHTAP_STRONG_STRENGTH)),
                                        1, RICHTAP_STRONG_STRENGTH);

                                if (isSliderBurst && effect.primitive == CompositePrimitive::LOW_TICK &&
                                    effect.scale >= 0.20f) {
                                    strength = RICHTAP_LIGHT_STRENGTH;  // Level 1 (smooth, never Level 2)
                                    } else if (isSingleLowTick && effect.scale >= 0.50f) {
                                        strength = RICHTAP_LIGHT_STRENGTH;  // Recents scroll snap -> Level 1
                                    } else if (item.composite.size() == 1 &&
                                        effect.primitive == CompositePrimitive::CLICK &&
                                        effect.scale < 0.35f) {
                                        strength = RICHTAP_LIGHT_STRENGTH;  // Volume slider endpoint -> Level 1
                                        }

                                // Strictly preserve lock order: release mMutex before taking mHwMutex.
                                lock.unlock();
                                int32_t playedMs = -1;
                                {
                                    std::lock_guard<std::mutex> hwLock(mHwMutex);
                                    bool stillActive = false;
                                    {
                                        std::lock_guard<std::mutex> stateLock(mMutex);
                                        stillActive = (!mShutdown && !mInExternalControl &&
                                        mActiveJobId == item.jobId);
                                    }
                                    if (stillActive) {
                                        playedMs = aac_vibra_looper_prebaked_effect(mapping.effectId, strength);
                                    }
                                }
                                lock.lock();

                                if (mShutdown || mActiveJobId != item.jobId) {
                                    completed = false;
                                    break;
                                }

                                if (playedMs < 0) {
                                    ALOGE("AAC compose primitive %d failed: %d",
                                          static_cast<int32_t>(effect.primitive), playedMs);
                                    completed = false;
                                    break;
                                }

                                int32_t waitMs = (playedMs > 0) ? playedMs : mapping.durationMs;
                                if (waitMs > 0) {
                                    bool cancelled = mCv.wait_for(
                                        lock, std::chrono::milliseconds(waitMs),
                                                                  [this, &item] { return mShutdown || mActiveJobId != item.jobId; });
                                    if (cancelled) {
                                        completed = false;
                                        break;
                                    }
                                }
                            }
                        }

                        if (completed && !mShutdown && mActiveJobId == item.jobId && item.callback != nullptr) {
                            auto cb = item.callback;
                            lock.unlock();
                            cb->onComplete();
                            lock.lock();
                        }
                    }
                }

                ndk::ScopedAStatus Vibrator::getCapabilities(int32_t* _aidl_return) {
                    *_aidl_return = IVibrator::CAP_ON_CALLBACK | IVibrator::CAP_PERFORM_CALLBACK |
                    IVibrator::CAP_AMPLITUDE_CONTROL | IVibrator::CAP_EXTERNAL_CONTROL |
                    IVibrator::CAP_COMPOSE_EFFECTS;

                    return ndk::ScopedAStatus::ok();
                }

                ndk::ScopedAStatus Vibrator::off() {
                    std::lock_guard<std::mutex> hwLock(mHwMutex);
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        ++mActiveJobId;
                        mPendingJob.reset();
                    }
                    mCv.notify_all();

                    int32_t ret = aac_vibra_off();
                    if (ret) {
                        ALOGE("AAC off failed: %d", ret);
                        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_SERVICE_SPECIFIC));
                    }

                    return ndk::ScopedAStatus::ok();
                }

                ndk::ScopedAStatus Vibrator::on(int32_t timeoutMs,
                                                const std::shared_ptr<IVibratorCallback>& callback) {
                    if (timeoutMs <= 0) {
                        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
                    }

                    std::lock_guard<std::mutex> hwLock(mHwMutex);
                    uint64_t jobId = 0;
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        if (mInExternalControl) {
                            return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                        }
                        jobId = ++mActiveJobId;
                        mPendingJob.reset();
                    }
                    mCv.notify_all();

                    int32_t ret = aac_vibra_looper_on(static_cast<uint32_t>(timeoutMs));
                    if (ret < 0) {
                        ALOGE("AAC on failed: %d", ret);
                        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_SERVICE_SPECIFIC));
                    }

                    if (callback != nullptr) {
                        {
                            std::lock_guard<std::mutex> lock(mMutex);
                            mPendingJob = WorkItem{
                                WorkItem::Type::WAIT_CALLBACK,
                                jobId,
                                (ret > 0) ? ret : timeoutMs,
                                {},
                                callback,
                            };
                        }
                        mCv.notify_all();
                    }

                    return ndk::ScopedAStatus::ok();
                                                }

                                                ndk::ScopedAStatus Vibrator::perform(Effect effect, EffectStrength es,
                                                                                     const std::shared_ptr<IVibratorCallback>& callback,
                                                                                     int32_t* _aidl_return) {
                                                    uint32_t effectId = 0;
                                                    int32_t fallbackDurationMs = 0;
                                                    if (!mapEffect(effect, &effectId, &fallbackDurationMs)) {
                                                        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                    }

                                                    int32_t strength;
                                                    switch (es) {
                                                        case EffectStrength::LIGHT:
                                                            strength = RICHTAP_LIGHT_STRENGTH;
                                                            break;
                                                        case EffectStrength::MEDIUM:
                                                            strength = RICHTAP_MEDIUM_STRENGTH;
                                                            break;
                                                        case EffectStrength::STRONG:
                                                            strength = RICHTAP_STRONG_STRENGTH;
                                                            break;
                                                        default:
                                                            return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                    }

                                                    if (effect == Effect::CLICK && es == EffectStrength::MEDIUM) {
                                                        strength = RICHTAP_CLICK_MEDIUM_STRENGTH;
                                                    }

                                                    if (effect == Effect::TEXTURE_TICK) {
                                                        strength = RICHTAP_CLICK_MEDIUM_STRENGTH;
                                                        bool throttled = false;
                                                        {
                                                            std::lock_guard<std::mutex> lock(mMutex);
                                                            if (mInExternalControl) {
                                                                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                            }
                                                            static auto lastTextureTick = std::chrono::steady_clock::time_point{};
                                                            auto now = std::chrono::steady_clock::now();
                                                            if (now - lastTextureTick < std::chrono::milliseconds(40)) {
                                                                throttled = true;
                                                            } else {
                                                                lastTextureTick = now;
                                                            }
                                                        }
                                                        if (throttled) {
                                                            *_aidl_return = 12;
                                                            if (callback != nullptr) {
                                                                callback->onComplete();
                                                            }
                                                            return ndk::ScopedAStatus::ok();
                                                        }
                                                    }

                                                    std::lock_guard<std::mutex> hwLock(mHwMutex);
                                                    uint64_t jobId = 0;
                                                    {
                                                        std::lock_guard<std::mutex> lock(mMutex);
                                                        if (mInExternalControl) {
                                                            return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                        }
                                                        jobId = ++mActiveJobId;
                                                        mPendingJob.reset();
                                                    }
                                                    mCv.notify_all();

                                                    int32_t ret = aac_vibra_looper_prebaked_effect(effectId, strength);
                                                    if (ret < 0) {
                                                        ALOGE("AAC perform failed: %d", ret);
                                                        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_SERVICE_SPECIFIC));
                                                    }

                                                    int32_t durationMs = (ret > 0) ? ret : fallbackDurationMs;
                                                    if (callback != nullptr) {
                                                        {
                                                            std::lock_guard<std::mutex> lock(mMutex);
                                                            mPendingJob = WorkItem{
                                                                WorkItem::Type::WAIT_CALLBACK,
                                                                jobId,
                                                                durationMs,
                                                                {},
                                                                callback,
                                                            };
                                                        }
                                                        mCv.notify_all();
                                                    }

                                                    *_aidl_return = durationMs;

                                                    return ndk::ScopedAStatus::ok();
                                                                                     }

                                                                                     ndk::ScopedAStatus Vibrator::getSupportedEffects(std::vector<Effect>* _aidl_return) {
                                                                                         *_aidl_return = {
                                                                                             Effect::CLICK,
                                                                                             Effect::DOUBLE_CLICK,
                                                                                             Effect::TICK,
                                                                                             Effect::THUD,
                                                                                             Effect::POP,
                                                                                             Effect::HEAVY_CLICK,
                                                                                             Effect::TEXTURE_TICK,
                                                                                         };

                                                                                         return ndk::ScopedAStatus::ok();
                                                                                     }

                                                                                     ndk::ScopedAStatus Vibrator::setAmplitude(float amplitude) {
                                                                                         if (amplitude <= 0.0f || amplitude > 1.0f || std::isnan(amplitude)) {
                                                                                             return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
                                                                                         }

                                                                                         std::lock_guard<std::mutex> hwLock(mHwMutex);
                                                                                         {
                                                                                             std::lock_guard<std::mutex> lock(mMutex);
                                                                                             if (mInExternalControl) {
                                                                                                 return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                             }
                                                                                         }

                                                                                         uint8_t tmp = static_cast<uint8_t>(
                                                                                             std::clamp<int32_t>(static_cast<int32_t>(std::lround(amplitude * 255.0f)), 1, 255));

                                                                                         int32_t ret = aac_vibra_setAmplitude(tmp);
                                                                                         if (ret) {
                                                                                             ALOGE("AAC set amplitude failed: %d", ret);
                                                                                             return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_SERVICE_SPECIFIC));
                                                                                         }

                                                                                         return ndk::ScopedAStatus::ok();
                                                                                     }

                                                                                     ndk::ScopedAStatus Vibrator::setExternalControl(bool enabled) {
                                                                                         std::lock_guard<std::mutex> hwLock(mHwMutex);
                                                                                         {
                                                                                             std::lock_guard<std::mutex> lock(mMutex);
                                                                                             mInExternalControl = enabled;
                                                                                             if (enabled) {
                                                                                                 ++mActiveJobId;
                                                                                                 mPendingJob.reset();
                                                                                             }
                                                                                         }
                                                                                         if (enabled) {
                                                                                             mCv.notify_all();
                                                                                             aac_vibra_off();
                                                                                         }

                                                                                         return ndk::ScopedAStatus::ok();
                                                                                     }

                                                                                     ndk::ScopedAStatus Vibrator::getCompositionDelayMax(int32_t* maxDelayMs) {
                                                                                         *maxDelayMs = kComposeDelayMaxMs;
                                                                                         return ndk::ScopedAStatus::ok();
                                                                                     }

                                                                                     ndk::ScopedAStatus Vibrator::getCompositionSizeMax(int32_t* maxSize) {
                                                                                         *maxSize = kComposeSizeMax;
                                                                                         return ndk::ScopedAStatus::ok();
                                                                                     }

                                                                                     ndk::ScopedAStatus Vibrator::getSupportedPrimitives(std::vector<CompositePrimitive>* supported) {
                                                                                         *supported = {
                                                                                             CompositePrimitive::CLICK,
                                                                                             CompositePrimitive::THUD,
                                                                                             CompositePrimitive::SPIN,
                                                                                             CompositePrimitive::QUICK_RISE,
                                                                                             CompositePrimitive::SLOW_RISE,
                                                                                             CompositePrimitive::QUICK_FALL,
                                                                                             CompositePrimitive::LIGHT_TICK,
                                                                                             CompositePrimitive::LOW_TICK,
                                                                                         };

                                                                                         return ndk::ScopedAStatus::ok();
                                                                                     }

                                                                                     ndk::ScopedAStatus Vibrator::getPrimitiveDuration(CompositePrimitive primitive,
                                                                                                                                       int32_t* durationMs) {
                                                                                         if (primitive == CompositePrimitive::NOOP) {
                                                                                             *durationMs = 0;
                                                                                             return ndk::ScopedAStatus::ok();
                                                                                         }

                                                                                         PrimitiveMapping mapping{};
                                                                                         if (!mapPrimitive(primitive, &mapping)) {
                                                                                             return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                         }

                                                                                         *durationMs = mapping.durationMs;
                                                                                         return ndk::ScopedAStatus::ok();
                                                                                                                                       }

                                                                                                                                       ndk::ScopedAStatus Vibrator::compose(const std::vector<CompositeEffect>& composite,
                                                                                                                                                                            const std::shared_ptr<IVibratorCallback>& callback) {
                                                                                                                                           if (composite.empty() || composite.size() > static_cast<size_t>(kComposeSizeMax)) {
                                                                                                                                               return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
                                                                                                                                           }

                                                                                                                                           for (const auto& effect : composite) {
                                                                                                                                               if (effect.delayMs < 0 || effect.delayMs > kComposeDelayMaxMs) {
                                                                                                                                                   return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
                                                                                                                                               }
                                                                                                                                               if (effect.scale < 0.0f || effect.scale > 1.0f || std::isnan(effect.scale)) {
                                                                                                                                                   return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
                                                                                                                                               }
                                                                                                                                               if (effect.primitive != CompositePrimitive::NOOP) {
                                                                                                                                                   PrimitiveMapping mapping{};
                                                                                                                                                   if (!mapPrimitive(effect.primitive, &mapping)) {
                                                                                                                                                       return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                   }
                                                                                                                                               }
                                                                                                                                           }

                                                                                                                                           {
                                                                                                                                               std::lock_guard<std::mutex> lock(mMutex);
                                                                                                                                               if (mInExternalControl) {
                                                                                                                                                   return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                               }
                                                                                                                                               uint64_t jobId = ++mActiveJobId;
                                                                                                                                               mPendingJob = WorkItem{
                                                                                                                                                   WorkItem::Type::COMPOSE,
                                                                                                                                                   jobId,
                                                                                                                                                   0,
                                                                                                                                                   composite,
                                                                                                                                                   callback,
                                                                                                                                               };
                                                                                                                                           }
                                                                                                                                           mCv.notify_all();

                                                                                                                                           return ndk::ScopedAStatus::ok();
                                                                                                                                                                            }

                                                                                                                                                                            ndk::ScopedAStatus Vibrator::getSupportedAlwaysOnEffects(
                                                                                                                                                                                std::vector<Effect>* _aidl_return __unused) {
                                                                                                                                                                                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                                                }

                                                                                                                                                                                ndk::ScopedAStatus Vibrator::alwaysOnEnable(int32_t id __unused, Effect effect __unused,
                                                                                                                                                                                                                            EffectStrength strength __unused) {
                                                                                                                                                                                    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                                                                                            }

                                                                                                                                                                                                                            ndk::ScopedAStatus Vibrator::alwaysOnDisable(int32_t id __unused) {
                                                                                                                                                                                                                                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                                                                                            }

                                                                                                                                                                                                                            ndk::ScopedAStatus Vibrator::getResonantFrequency(float* resonantFreqHz __unused) {
                                                                                                                                                                                                                                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                                                                                            }

                                                                                                                                                                                                                            ndk::ScopedAStatus Vibrator::getQFactor(float* qFactor __unused) {
                                                                                                                                                                                                                                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                                                                                            }

                                                                                                                                                                                                                            ndk::ScopedAStatus Vibrator::getFrequencyResolution(float* freqResolutionHz __unused) {
                                                                                                                                                                                                                                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                                                                                            }

                                                                                                                                                                                                                            ndk::ScopedAStatus Vibrator::getFrequencyMinimum(float* freqMinimumHz __unused) {
                                                                                                                                                                                                                                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                                                                                            }

                                                                                                                                                                                                                            ndk::ScopedAStatus Vibrator::getBandwidthAmplitudeMap(std::vector<float>* _aidl_return __unused) {
                                                                                                                                                                                                                                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                                                                                            }

                                                                                                                                                                                                                            ndk::ScopedAStatus Vibrator::getPwlePrimitiveDurationMax(int32_t* durationMs __unused) {
                                                                                                                                                                                                                                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                                                                                            }

                                                                                                                                                                                                                            ndk::ScopedAStatus Vibrator::getPwleCompositionSizeMax(int32_t* maxSize __unused) {
                                                                                                                                                                                                                                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                                                                                            }

                                                                                                                                                                                                                            ndk::ScopedAStatus Vibrator::getSupportedBraking(std::vector<Braking>* supported __unused) {
                                                                                                                                                                                                                                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                                                                                            }

                                                                                                                                                                                                                            ndk::ScopedAStatus Vibrator::composePwle(const std::vector<PrimitivePwle>& composite __unused,
                                                                                                                                                                                                                                                                const std::shared_ptr<IVibratorCallback>& callback
                                                                                                                                                                                                                                                                __unused) {
                                                                                                                                                                                                                                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
                                                                                                                                                                                                                                                                }

            }  // namespace vibrator
        }  // namespace hardware
    }  // namespace android
}  // namespace aidl
