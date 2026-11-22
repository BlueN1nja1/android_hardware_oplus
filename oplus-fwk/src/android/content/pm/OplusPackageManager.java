package android.content.pm;

import android.content.Context;

public class OplusPackageManager {
    private static final OplusPackageManager sInstance = new OplusPackageManager();

    public static OplusPackageManager getOplusPackageManager(Context context) {
        return sInstance;
    }
}
