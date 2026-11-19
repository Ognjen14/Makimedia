package com.topicdev.makimedia.org;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class MakimediaPlayerClient {
    private static final String TAG = "MakimediaPlayerClient";
    private static final String[] KIND_NAME = {"playback", "preview"};
    private static final long CONNECT_WAIT_MS = 10000;
    private static final int CONNECT_TRIES = 3;

    private static final Object s_lock = new Object();
    private static final ServiceConnection[] s_connections = new ServiceConnection[2];
    private static final IBinder[] s_binders = new IBinder[2];
    private static final int[] s_pids = {-1, -1};

    private static Class<?> hostClass(int kind) {
        return kind == MakimediaVideoSurface.PREVIEW ? MakimediaPreviewHost.class
                                                     : MakimediaPlaybackHost.class;
    }

    public static int connect(Context context, int kind, int fd) {
        for (int attempt = 1; attempt <= CONNECT_TRIES; ++attempt) {
            final int pid = connectOnce(context, kind, fd);
            if (pid > 0) {
                synchronized (s_lock) {
                    s_pids[kind] = pid;
                }
                Log.i(TAG, KIND_NAME[kind] + " player process " + pid
                           + " connected on attempt " + attempt);
                return pid;
            }
            disconnect(context, kind, -1);
            try {
                Thread.sleep(250);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return -1;
            }
        }
        Log.e(TAG, "no " + KIND_NAME[kind] + " player process could be started");
        return -1;
    }

    private static int connectOnce(Context context, int kind, int fd) {
        final CountDownLatch connected = new CountDownLatch(1);
        final IBinder[] bound = new IBinder[1];
        final ServiceConnection connection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                bound[0] = service;
                connected.countDown();
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
                Log.w(TAG, KIND_NAME[kind] + " player process went away");
                synchronized (s_lock) {
                    if (s_connections[kind] == this) {
                        s_binders[kind] = null;
                    }
                }
            }
        };

        final Intent intent = new Intent(context, hostClass(kind));
        final boolean binding;
        try {
            binding = context.bindService(intent, connection,
                Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT);
        } catch (RuntimeException e) {
            Log.e(TAG, "binding the " + KIND_NAME[kind] + " player failed", e);
            return -1;
        }
        synchronized (s_lock) {
            s_connections[kind] = connection;
            s_binders[kind] = null;
            s_pids[kind] = -1;
        }
        if (!binding) {
            Log.e(TAG, "Android refused to start the " + KIND_NAME[kind] + " player");
            return -1;
        }

        try {
            if (!connected.await(CONNECT_WAIT_MS, TimeUnit.MILLISECONDS)) {
                Log.e(TAG, KIND_NAME[kind] + " player process did not start in "
                           + CONNECT_WAIT_MS + " ms");
                return -1;
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return -1;
        }

        final IBinder binder = bound[0];
        synchronized (s_lock) {
            s_binders[kind] = binder;
        }

        ParcelFileDescriptor socket = null;
        final Parcel data = Parcel.obtain();
        final Parcel reply = Parcel.obtain();
        try {
            socket = ParcelFileDescriptor.fromFd(fd);
            data.writeFileDescriptor(socket.getFileDescriptor());
            binder.transact(MakimediaPlayerHost.START, data, reply, 0);
            return reply.readInt();
        } catch (Exception e) {
            Log.w(TAG, "handing the socket to the " + KIND_NAME[kind] + " player failed", e);
            return -1;
        } finally {
            data.recycle();
            reply.recycle();
            if (socket != null) {
                try {
                    socket.close();
                } catch (java.io.IOException ignored) {
                }
            }
        }
    }

    public static boolean sendSurface(int kind) {
        final IBinder binder;
        synchronized (s_lock) {
            binder = s_binders[kind];
        }
        if (binder == null) {
            Log.w(TAG, "no " + KIND_NAME[kind] + " player process to hand a surface to");
            return false;
        }
        final Surface surface = MakimediaVideoSurface.surface(kind);
        final Parcel data = Parcel.obtain();
        final Parcel reply = Parcel.obtain();
        try {
            if (surface != null && surface.isValid()) {
                data.writeInt(1);
                surface.writeToParcel(data, 0);
            } else {
                data.writeInt(0);
            }
            binder.transact(MakimediaPlayerHost.SURFACE, data, reply, 0);
            return surface != null;
        } catch (RemoteException e) {
            Log.w(TAG, "handing a surface to the " + KIND_NAME[kind] + " player failed", e);
            return false;
        } finally {
            data.recycle();
            reply.recycle();
        }
    }

    public static void disconnect(Context context, int kind, int pid) {
        final ServiceConnection connection;
        synchronized (s_lock) {
            if (pid > 0 && s_pids[kind] != pid) {
                return;
            }
            connection = s_connections[kind];
            s_connections[kind] = null;
            s_binders[kind] = null;
            s_pids[kind] = -1;
        }
        if (connection == null) {
            return;
        }
        try {
            context.unbindService(connection);
        } catch (RuntimeException e) {
            Log.w(TAG, "unbinding the " + KIND_NAME[kind] + " player failed", e);
        }
    }
}
