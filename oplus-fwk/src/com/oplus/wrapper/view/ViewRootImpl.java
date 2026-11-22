package com.oplus.wrapper.view;

import android.view.SurfaceControl;

import com.oplus.wrapper.graphics.HardwareRenderer;

public class ViewRootImpl {
    private final android.view.ViewRootImpl mViewRootImpl;

    public ViewRootImpl(android.view.ViewRootImpl viewRootImpl) {
        mViewRootImpl = viewRootImpl;
    }

    public SurfaceControl getSurfaceControl() {
        return mViewRootImpl != null ? mViewRootImpl.getSurfaceControl() : null;
    }

    public void registerRtFrameCallback(HardwareRenderer.FrameDrawingCallback callback) {
        if (mViewRootImpl != null && callback != null) {
            mViewRootImpl.registerRtFrameCallback(callback::onFrameDraw);
        }
    }
}
