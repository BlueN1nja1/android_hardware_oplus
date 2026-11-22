package com.oplus.view;

import android.content.Context;

public class JankManager {
    private static final JankManager sInstance = new JankManager();

    public static JankManager getInstance() {
        return sInstance;
    }

    public void gfxSceneBegin(Context context, int scene, String name, long flag) {
    }

    public void gfxSceneEnd(Context context, int scene) {
    }
}
