package com.oplus.view;

import android.view.SurfaceControl;
import android.view.SurfaceView;

public class OplusEdrUtils {
    public static SurfaceControl getBlastSurfaceControl(SurfaceView view) {
        return view != null ? view.getSurfaceControl() : null;
    }

    public static boolean setEdrFlags(
            SurfaceControl sc, SurfaceControl.Transaction tx, int flags) {
        return false;
    }
}
