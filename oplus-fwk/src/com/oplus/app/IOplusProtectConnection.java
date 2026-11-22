package com.oplus.app;

import android.os.Binder;
import android.os.IBinder;
import android.os.IInterface;

public interface IOplusProtectConnection extends IInterface {
    abstract class Stub extends Binder implements IOplusProtectConnection {
        public Stub() {
        }

        @Override
        public IBinder asBinder() {
            return this;
        }
    }
}
