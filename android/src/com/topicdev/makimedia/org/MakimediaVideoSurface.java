package com.topicdev.makimedia.org;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import java.util.ArrayList;

// A television's decoder returns a layout only the display pipeline reads, so
// the picture must be a composited surface rather than a texture: a
// SurfaceView under Qt's, shown through a translucent window.
public class MakimediaVideoSurface {
    private static final String TAG = "MakimediaVideoSurface";

    public static final int MAIN = 0;
    public static final int PREVIEW = 1;
    private static final int[] POOL_SIZE = {4, 2};
    private static final String[] KIND_NAME = {"playback", "preview"};

    private static volatile MakimediaVideoSurface m_instance;
    private static final Object s_lock = new Object();

    @SuppressWarnings("unchecked")
    private final ArrayList<Slot>[] m_slots = new ArrayList[] {
        new ArrayList<Slot>(), new ArrayList<Slot>()
    };
    private ViewGroup m_parent;

    private static volatile boolean s_surfaceHandedOver;
    private static int s_fillColour = Color.TRANSPARENT;

    private static final int[] s_videoWidth = new int[2];
    private static final int[] s_videoHeight = new int[2];
    private static final int[] s_pixelWidth = new int[2];
    private static final int[] s_pixelHeight = new int[2];

    private static final int MAX_PIXEL_WIDTH = 4096;
    private static final int MAX_PIXEL_HEIGHT = 2304;

    private static double s_aspect;
    private static boolean s_fill;

    private static int s_frameX;
    private static int s_frameY;
    private static int s_frameWidth;
    private static int s_frameHeight;

    private final View.OnLayoutChangeListener m_parentResized =
        new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View view, int left, int top, int right,
                                       int bottom, int oldLeft, int oldTop,
                                       int oldRight, int oldBottom) {
                if (right - left == oldRight - oldLeft
                    && bottom - top == oldBottom - oldTop) {
                    return;
                }
                Log.i(TAG, "window resized to " + (right - left) + "x"
                           + (bottom - top) + ", reshaping the video surfaces");
                view.post(new Runnable() {
                    @Override
                    public void run() {
                        applyLayout(MAIN);
                        applyLayout(PREVIEW);
                    }
                });
            }
        };

    private static native void nativeVideoSurfaceReady(int kind);
    private static native void nativeVideoSurfaceLost(int kind);

    private final class Slot implements SurfaceHolder.Callback {
        final int kind;
        final int number;
        SurfaceView view;
        volatile Surface surface;
        int fixedWidth;
        int fixedHeight;
        boolean awaitingLayout;

        Slot(int kind, int number) {
            this.kind = kind;
            this.number = number;
        }

        boolean active() {
            synchronized (s_lock) {
                return !m_slots[kind].isEmpty() && m_slots[kind].get(0) == this;
            }
        }

        String name() {
            return KIND_NAME[kind] + " surface " + number;
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            surface = holder.getSurface();
            final boolean active = active();
            Log.i(TAG, name() + " created" + (active ? ", in use" : ", spare"));
            if (!active) {
                return;
            }
            if (kind == MAIN) {
                paintFill(this);
            }
            notifyReady(kind);
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            surface = holder.getSurface();
            if (active()) {
                Log.i(TAG, name() + " changed to " + width + "x" + height);
            }
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            final boolean active = active();
            surface = null;
            if (active) {
                notifyLost(kind, name());
            }
        }
    }

    public static void create() {
        final Activity activity = MakimediaActivity.instance();
        if (activity == null) {
            Log.w(TAG, "no activity yet, video surfaces not created");
            return;
        }

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (m_instance == null) {
                    m_instance = new MakimediaVideoSurface();
                }
                m_instance.attach(activity);
            }
        });
    }

    public static void destroy() {
        final Activity activity = MakimediaActivity.instance();
        if (activity == null || m_instance == null) {
            return;
        }

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (m_instance != null) {
                    m_instance.detach();
                }
            }
        });
    }

    // Null until the system hands over a surface, a frame or two after the
    // view is added.
    public static Surface surface(int kind) {
        final MakimediaVideoSurface instance = m_instance;
        if (instance == null) {
            return null;
        }
        final Slot slot = instance.activeSlot(kind);
        final Surface surface = slot == null ? null : slot.surface;
        if (surface != null && kind == MAIN) {
            s_surfaceHandedOver = true;
        }
        return surface;
    }

    public static boolean retire(final int kind) {
        final MakimediaVideoSurface instance = m_instance;
        if (instance == null) {
            return false;
        }
        synchronized (s_lock) {
            if (instance.m_slots[kind].size() < 2) {
                Log.w(TAG, "no spare " + KIND_NAME[kind] + " surface left");
                return false;
            }
        }

        final Activity activity = MakimediaActivity.instance();
        if (activity == null) {
            return false;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                instance.retireActive(kind);
            }
        });
        return true;
    }

    // The decoder fills whatever surface it is handed and cannot letterbox,
    // so the black bars have to be the shape of the surface itself.
    public static void setVideoSize(final int kind, final int width, final int height,
                                    final int pixelWidth, final int pixelHeight) {
        s_videoWidth[kind] = width;
        s_videoHeight[kind] = height;
        s_pixelWidth[kind] = pixelWidth;
        s_pixelHeight[kind] = pixelHeight;
        relayout(kind);
    }

    public static void setAspect(final int kind, final double aspect, final boolean fill) {
        if (kind != MAIN) {
            return;
        }
        s_aspect = aspect;
        s_fill = fill;
        relayout(kind);
    }

    public static void show(final int kind, final int x, final int y,
                            final int width, final int height) {
        if (kind != PREVIEW) {
            return;
        }
        s_frameX = x;
        s_frameY = y;
        s_frameWidth = width;
        s_frameHeight = height;
        relayout(kind);
    }

    public static void hide(final int kind) {
        if (kind != PREVIEW) {
            return;
        }
        s_frameWidth = 0;
        s_frameHeight = 0;
        relayout(kind);
    }

    public static void setFillColour(final int kind, final int colour) {
        if (kind != MAIN) {
            return;
        }
        s_fillColour = colour;

        final Activity activity = MakimediaActivity.instance();
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (m_instance != null) {
                    final Slot slot = m_instance.activeSlot(MAIN);
                    if (slot != null) {
                        paintFill(slot);
                    }
                }
            }
        });
    }

    private static void relayout(final int kind) {
        final Activity activity = MakimediaActivity.instance();
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (m_instance != null) {
                    m_instance.applyLayout(kind);
                }
            }
        });
    }

    private Slot activeSlot(int kind) {
        synchronized (s_lock) {
            return m_slots[kind].isEmpty() ? null : m_slots[kind].get(0);
        }
    }

    private void attach(Activity activity) {
        if (m_parent != null) {
            return;
        }

        ViewGroup content = activity.findViewById(android.R.id.content);
        if (content == null) {
            Log.e(TAG, "no content view, video surfaces not created");
            return;
        }

        // Never hidden: a SurfaceView only holds a surface while visible, so
        // hiding it between videos leaves the next play nothing to draw into.
        int index = 0;
        for (int kind = MAIN; kind <= PREVIEW; ++kind) {
            for (int number = 0; number < POOL_SIZE[kind]; ++number) {
                final Slot slot = new Slot(kind, number);
                slot.view = new SurfaceView(activity);
                slot.view.getHolder().addCallback(slot);
                slot.view.getHolder().setFormat(PixelFormat.OPAQUE);
                synchronized (s_lock) {
                    m_slots[kind].add(slot);
                }
                content.addView(slot.view, index++, tucked());
            }
        }

        m_parent = content;
        m_parent.addOnLayoutChangeListener(m_parentResized);
        applyLayout(MAIN);
        applyLayout(PREVIEW);

        Log.i(TAG, index + " video surfaces added beneath Qt, children now "
                   + content.getChildCount());
    }

    private void detach() {
        if (m_parent == null) {
            return;
        }
        for (int kind = MAIN; kind <= PREVIEW; ++kind) {
            final ArrayList<Slot> slots;
            synchronized (s_lock) {
                slots = new ArrayList<Slot>(m_slots[kind]);
            }
            for (final Slot slot : slots) {
                removeSlot(slot);
            }
            synchronized (s_lock) {
                m_slots[kind].clear();
            }
        }
        m_parent.removeOnLayoutChangeListener(m_parentResized);
        m_parent = null;
        Log.i(TAG, "video surfaces removed");
    }

    private void retireActive(int kind) {
        final Slot old;
        final Slot next;
        synchronized (s_lock) {
            if (m_slots[kind].size() < 2) {
                return;
            }
            old = m_slots[kind].remove(0);
            next = m_slots[kind].get(0);
        }

        Log.w(TAG, "throwing away " + old.name() + ", " + next.name() + " takes over, "
                   + (m_slots[kind].size() - 1) + " spare left");
        if (old.surface != null) {
            notifyLost(kind, old.name());
        }
        removeSlot(old);
        applyLayout(kind);

        if (next.surface != null) {
            notifyReady(kind);
        }
    }

    private void removeSlot(Slot slot) {
        if (slot.view == null) {
            return;
        }
        slot.view.getHolder().removeCallback(slot);
        ViewGroup parent = (ViewGroup) slot.view.getParent();
        if (parent != null) {
            parent.removeView(slot.view);
        }
        slot.view = null;
        slot.surface = null;
    }

    private static FrameLayout.LayoutParams tucked() {
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(2, 2);
        params.leftMargin = -4;
        params.topMargin = -4;
        return params;
    }

    private void applyLayout(int kind) {
        if (m_parent == null) {
            return;
        }
        final ArrayList<Slot> slots;
        synchronized (s_lock) {
            slots = new ArrayList<Slot>(m_slots[kind]);
        }
        for (int i = 1; i < slots.size(); ++i) {
            final Slot spare = slots.get(i);
            if (spare.view != null) {
                spare.view.setLayoutParams(tucked());
            }
        }
        if (slots.isEmpty() || slots.get(0).view == null) {
            return;
        }
        final Slot slot = slots.get(0);

        final int parentWidth = m_parent.getWidth();
        final int parentHeight = m_parent.getHeight();
        if (parentWidth <= 0 || parentHeight <= 0) {
            // Asked before the parent was measured. One retry, behind the
            // layout pass about to happen.
            if (!slot.awaitingLayout) {
                slot.awaitingLayout = true;
                slot.view.post(new Runnable() {
                    @Override
                    public void run() {
                        slot.awaitingLayout = false;
                        applyLayout(slot.kind);
                    }
                });
            }
            return;
        }

        if (kind == PREVIEW && (s_frameWidth <= 0 || s_frameHeight <= 0)) {
            slot.view.setLayoutParams(tucked());
            return;
        }

        final boolean framed = kind == PREVIEW;
        final int areaX = framed ? s_frameX : 0;
        final int areaY = framed ? s_frameY : 0;
        final int areaWidth = framed ? s_frameWidth : parentWidth;
        final int areaHeight = framed ? s_frameHeight : parentHeight;

        if (s_videoWidth[kind] <= 0 || s_videoHeight[kind] <= 0) {
            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(areaWidth, areaHeight);
            params.leftMargin = areaX;
            params.topMargin = areaY;
            slot.view.setLayoutParams(params);
            return;
        }

        final double aspect = (!framed && s_aspect > 0)
            ? s_aspect
            : (double) s_videoWidth[kind] / (double) s_videoHeight[kind];

        final boolean fill = framed || s_fill;
        final double byWidth = areaWidth;
        final double byHeight = areaHeight * aspect;
        final double targetWidth = fill ? Math.max(byWidth, byHeight)
                                        : Math.min(byWidth, byHeight);

        final int width = (int) Math.round(targetWidth);
        final int height = (int) Math.round(targetWidth / aspect);

        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(width, height);
        params.leftMargin = areaX + (areaWidth - width) / 2;
        params.topMargin = areaY + (areaHeight - height) / 2;
        slot.view.setLayoutParams(params);

        final int pixelWidth = s_pixelWidth[kind];
        final int pixelHeight = s_pixelHeight[kind];
        final boolean lockable = pixelWidth > 0 && pixelHeight > 0
                                 && pixelWidth <= MAX_PIXEL_WIDTH
                                 && pixelHeight <= MAX_PIXEL_HEIGHT;
        if (lockable && (pixelWidth != slot.fixedWidth || pixelHeight != slot.fixedHeight)) {
            slot.fixedWidth = pixelWidth;
            slot.fixedHeight = pixelHeight;
            slot.view.getHolder().setFixedSize(pixelWidth, pixelHeight);
        }

        Log.i(TAG, slot.name() + " " + width + "x" + height + " in "
                   + areaWidth + "x" + areaHeight
                   + (framed ? " at " + areaX + "," + areaY : "")
                   + (fill ? " filling" : " fitted")
                   + " at aspect " + String.format("%.3f", aspect)
                   + (slot.fixedWidth > 0
                          ? ", holding " + slot.fixedWidth + "x" + slot.fixedHeight + " pixels"
                          : ", pixels as the view"));
    }

    // Only before the first video. Locking the canvas takes the buffer queue
    // for the CPU and never gives it back, so MediaCodec is refused with
    // "already connected" for the rest of the process.
    private static void paintFill(Slot slot) {
        if (slot.surface == null || s_fillColour == Color.TRANSPARENT) {
            return;
        }
        if (s_surfaceHandedOver) {
            Log.w(TAG, "fill colour ignored, playback has used this surface");
            return;
        }

        Canvas canvas = slot.surface.lockCanvas(null);
        if (canvas == null) {
            return;
        }
        canvas.drawColor(s_fillColour);
        slot.surface.unlockCanvasAndPost(canvas);
    }

    private static void notifyReady(int kind) {
        try {
            nativeVideoSurfaceReady(kind);
        } catch (UnsatisfiedLinkError e) {
        }
    }

    private static void notifyLost(int kind, String name) {
        Log.i(TAG, name + " going, waiting for playback to let go");
        try {
            nativeVideoSurfaceLost(kind);
        } catch (UnsatisfiedLinkError e) {
        }
        Log.i(TAG, name + " released to the system");
    }
}
