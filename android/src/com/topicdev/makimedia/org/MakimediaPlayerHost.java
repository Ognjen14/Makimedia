package com.topicdev.makimedia.org;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.util.Log;
import android.view.Surface;

public class MakimediaPlayerHost extends Service {
    private static final String TAG = "MakimediaPlayerHost";

    static final int START = IBinder.FIRST_CALL_TRANSACTION;
    static final int SURFACE = IBinder.FIRST_CALL_TRANSACTION + 1;

    static {
        System.loadLibrary("mmplayerhost");
    }

    private static native boolean nativeStart(int fd);
    private static native void nativeSetSurface(Surface surface);

    private final Binder m_binder = new Binder() {
        @Override
        protected boolean onTransact(int code, Parcel data, Parcel reply, int flags)
                throws android.os.RemoteException {
            if (Binder.getCallingUid() != Process.myUid()) {
                return false;
            }
            switch (code) {
            case START: {
                final ParcelFileDescriptor socket = data.readFileDescriptor();
                if (socket == null) {
                    reply.writeInt(-1);
                    return true;
                }
                final boolean started = nativeStart(socket.detachFd());
                reply.writeInt(started ? Process.myPid() : -1);
                return true;
            }
            case SURFACE: {
                final Surface surface = data.readInt() != 0
                    ? Surface.CREATOR.createFromParcel(data) : null;
                nativeSetSurface(surface);
                reply.writeInt(1);
                return true;
            }
            default:
                return super.onTransact(code, data, reply, flags);
            }
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, getClass().getSimpleName() + " started in process " + Process.myPid());
    }

    @Override
    public IBinder onBind(Intent intent) {
        return m_binder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.i(TAG, getClass().getSimpleName() + " unbound, ending process " + Process.myPid());
        Process.killProcess(Process.myPid());
        return false;
    }
}
