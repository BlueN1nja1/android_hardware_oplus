package com.oplus.wrapper.hardware.devicestate;

public class DeviceState {
    private final android.hardware.devicestate.DeviceState mDeviceState;

    public DeviceState(android.hardware.devicestate.DeviceState deviceState) {
        mDeviceState = deviceState;
    }

    public int getIdentifier() {
        return mDeviceState != null ? mDeviceState.getIdentifier() : 0;
    }
}
