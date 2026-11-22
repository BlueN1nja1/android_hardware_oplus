package com.oplus.wrapper.hardware.devicestate;

import android.content.Context;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;

public class DeviceStateManager {
    private final android.hardware.devicestate.DeviceStateManager mManager;
    private final Map<DeviceStateCallback,
            android.hardware.devicestate.DeviceStateManager.DeviceStateCallback> mCallbacks =
            new ConcurrentHashMap<>();

    public DeviceStateManager(Context context) {
        mManager = context.getSystemService(android.hardware.devicestate.DeviceStateManager.class);
    }

    public interface DeviceStateCallback {
        default void onDeviceStateChanged(DeviceState state) {}
        default void onStateChanged(int state) {}
    }

    public void registerCallback(Executor executor, DeviceStateCallback callback) {
        if (mManager == null || callback == null) {
            return;
        }
        android.hardware.devicestate.DeviceStateManager.DeviceStateCallback platformCb =
                state -> {
                    callback.onDeviceStateChanged(new DeviceState(state));
                    callback.onStateChanged(state.getIdentifier());
                };
        mCallbacks.put(callback, platformCb);
        mManager.registerCallback(executor, platformCb);
    }

    public void unregisterCallback(DeviceStateCallback callback) {
        if (mManager == null || callback == null) {
            return;
        }
        android.hardware.devicestate.DeviceStateManager.DeviceStateCallback platformCb =
                mCallbacks.remove(callback);
        if (platformCb != null) {
            mManager.unregisterCallback(platformCb);
        }
    }
}
