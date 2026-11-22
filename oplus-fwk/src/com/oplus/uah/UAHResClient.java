package com.oplus.uah;

import com.oplus.uah.info.UAHEventRequest;

public class UAHResClient {
    private static final UAHResClient sInstance = new UAHResClient();

    public static UAHResClient get(Class<?> cls) {
        return sInstance;
    }

    public int acquireEvent(UAHEventRequest request) {
        return 0;
    }

    public void release(int handle) {
    }
}
