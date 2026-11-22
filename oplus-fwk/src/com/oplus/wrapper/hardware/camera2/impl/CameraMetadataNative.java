package com.oplus.wrapper.hardware.camera2.impl;

public class CameraMetadataNative {
    private final android.hardware.camera2.impl.CameraMetadataNative mMetadataNative;

    public CameraMetadataNative(android.hardware.camera2.impl.CameraMetadataNative metadataNative) {
        mMetadataNative = metadataNative;
    }

    public long getMetadataPtr() {
        return mMetadataNative != null ? mMetadataNative.getMetadataPtr() : 0L;
    }
}
