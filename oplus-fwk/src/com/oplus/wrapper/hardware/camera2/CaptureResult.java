package com.oplus.wrapper.hardware.camera2;

import com.oplus.wrapper.hardware.camera2.impl.CameraMetadataNative;

public class CaptureResult {
    private final android.hardware.camera2.CaptureResult mCaptureResult;

    public CaptureResult(android.hardware.camera2.CaptureResult captureResult) {
        mCaptureResult = captureResult;
    }

    public CameraMetadataNative getNativeMetadata() {
        if (mCaptureResult == null) {
            return null;
        }
        return new CameraMetadataNative(mCaptureResult.getNativeMetadata());
    }
}
