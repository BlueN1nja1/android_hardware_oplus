package com.oplus.wrapper.view;

import android.graphics.Region;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class ViewTreeObserver {
    private final android.view.ViewTreeObserver mObserver;
    private final Map<OnComputeInternalInsetsListener,
            android.view.ViewTreeObserver.OnComputeInternalInsetsListener> mListeners =
            new ConcurrentHashMap<>();

    public ViewTreeObserver(android.view.ViewTreeObserver observer) {
        mObserver = observer;
    }

    public static final class InternalInsetsInfo {
        public static final int TOUCHABLE_INSETS_REGION =
                android.view.ViewTreeObserver.InternalInsetsInfo.TOUCHABLE_INSETS_REGION;

        private final android.view.ViewTreeObserver.InternalInsetsInfo mInfo;

        public InternalInsetsInfo(android.view.ViewTreeObserver.InternalInsetsInfo info) {
            mInfo = info;
        }

        public Region getTouchableRegion() {
            return mInfo.touchableRegion;
        }

        public void setTouchableInsets(int val) {
            mInfo.setTouchableInsets(val);
        }
    }

    public interface OnComputeInternalInsetsListener {
        void onComputeInternalInsets(InternalInsetsInfo inoutInfo);
    }

    public void addOnComputeInternalInsetsListener(OnComputeInternalInsetsListener listener) {
        if (mObserver == null || listener == null) {
            return;
        }
        android.view.ViewTreeObserver.OnComputeInternalInsetsListener platformListener =
                info -> listener.onComputeInternalInsets(new InternalInsetsInfo(info));
        mListeners.put(listener, platformListener);
        mObserver.addOnComputeInternalInsetsListener(platformListener);
    }

    public void removeOnComputeInternalInsetsListener(OnComputeInternalInsetsListener listener) {
        if (mObserver == null || listener == null) {
            return;
        }
        android.view.ViewTreeObserver.OnComputeInternalInsetsListener platformListener =
                mListeners.remove(listener);
        if (platformListener != null) {
            mObserver.removeOnComputeInternalInsetsListener(platformListener);
        }
    }
}
