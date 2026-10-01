package com.topicdev.makimedia;

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

// A television's decoder returns a layout only the display pipeline reads, so
// the picture must be a composited surface rather than a texture: a
// SurfaceView under Qt's, shown through a translucent window.
public class MakimediaVideoSurface implements SurfaceHolder.Callback {
    private static final String TAG = "MakimediaVideoSurface";

    private static volatile MakimediaVideoSurface m_instance;

    private SurfaceView m_view;
    private ViewGroup m_parent;
    private volatile Surface m_surface;

    private static volatile boolean s_surfaceHandedOver;

    // Static and set off the UI thread: chosen at startup, before the view.
    private static int s_fillColour = Color.TRANSPARENT;

    private static int s_videoWidth;
    private static int s_videoHeight;

    // A forced display aspect, or zero for the picture's own. mpv never sees
    // the frames, so an aspect setting has nowhere else to take effect.
    private static double s_aspect;
    private static boolean s_fill;

    private boolean m_awaitingLayout;

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
                           + (bottom - top) + ", reshaping the video surface");
                view.post(new Runnable() {
                    @Override
                    public void run() {
                        applyVideoSize();
                    }
                });
            }
        };

    private static native void nativeVideoSurfaceReady();
    private static native void nativeVideoSurfaceLost();

    public static void create() {
        final Activity activity = MakimediaActivity.instance();
        if (activity == null) {
            Log.w(TAG, "no activity yet, video surface not created");
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
    public static Surface surface() {
        final MakimediaVideoSurface instance = m_instance;
        if (instance == null) {
            return null;
        }
        final Surface surface = instance.m_surface;
        if (surface != null) {
            s_surfaceHandedOver = true;
        }
        return surface;
    }

    // The decoder fills whatever surface it is handed and cannot letterbox,
    // so the black bars have to be the shape of the surface itself.
    public static void setVideoSize(final int width, final int height) {
        s_videoWidth = width;
        s_videoHeight = height;

        final Activity activity = MakimediaActivity.instance();
        if (activity == null) {
            return;
        }

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (m_instance != null) {
                    m_instance.applyVideoSize();
                }
            }
        });
    }

    // A flat colour, for checking the surface really shows through the Qt
    // window before any video points at it. Playback does not need it.
    public static void setFillColour(final int colour) {
        s_fillColour = colour;

        final Activity activity = MakimediaActivity.instance();
        if (activity == null) {
            return;
        }

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (m_instance != null) {
                    m_instance.paintFill();
                }
            }
        });
    }

    private void attach(Activity activity) {
        if (m_view != null) {
            return;
        }

        ViewGroup content = activity.findViewById(android.R.id.content);
        if (content == null) {
            Log.e(TAG, "no content view, video surface not created");
            return;
        }

        // Never hidden: a SurfaceView only holds a surface while visible, so
        // hiding it between videos leaves the next play nothing to draw into.
        m_view = new SurfaceView(activity);
        m_view.getHolder().addCallback(this);
        m_view.getHolder().setFormat(PixelFormat.OPAQUE);

        // Index zero and no z-order override, so it stays behind the window.
        content.addView(m_view, 0, new FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT));

        m_parent = content;
        m_parent.addOnLayoutChangeListener(m_parentResized);

        Log.i(TAG, "video surface view added beneath Qt, children now "
                   + content.getChildCount());
    }

    private void detach() {
        if (m_view == null) {
            return;
        }

        if (m_surface != null) {
            notifySurfaceLost();
        }

        m_view.getHolder().removeCallback(this);

        if (m_parent != null) {
            m_parent.removeOnLayoutChangeListener(m_parentResized);
            m_parent = null;
        }

        ViewGroup parent = (ViewGroup) m_view.getParent();
        if (parent != null) {
            parent.removeView(m_view);
        }

        m_view = null;

        Log.i(TAG, "video surface view removed");
    }

    public static void setAspect(final double aspect, final boolean fill) {
        s_aspect = aspect;
        s_fill = fill;

        final Activity activity = MakimediaActivity.instance();
        if (activity == null) {
            return;
        }

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (m_instance != null) {
                    m_instance.applyVideoSize();
                }
            }
        });
    }

    private void applyVideoSize() {
        if (m_view == null || s_videoWidth <= 0 || s_videoHeight <= 0) {
            return;
        }

        ViewGroup parent = (ViewGroup) m_view.getParent();
        if (parent == null) {
            return;
        }

        final int availableWidth = parent.getWidth();
        final int availableHeight = parent.getHeight();

        if (availableWidth <= 0 || availableHeight <= 0) {
            // Asked before the parent was measured. One retry, behind the
            // layout pass about to happen.
            if (!m_awaitingLayout) {
                m_awaitingLayout = true;
                m_view.post(new Runnable() {
                    @Override
                    public void run() {
                        m_awaitingLayout = false;
                        applyVideoSize();
                    }
                });
            }
            return;
        }

        final double aspect = s_aspect > 0
            ? s_aspect
            : (double) s_videoWidth / (double) s_videoHeight;

        // Fit leaves bars; fill grows until it covers and the parent clips the
        // overhang, the only way to crop when the decoder fills the surface.
        final double byWidth = availableWidth;
        final double byHeight = availableHeight * aspect;
        final double targetWidth = s_fill ? Math.max(byWidth, byHeight)
                                          : Math.min(byWidth, byHeight);

        final int width = (int) Math.round(targetWidth);
        final int height = (int) Math.round(targetWidth / aspect);

        FrameLayout.LayoutParams params =
            new FrameLayout.LayoutParams(width, height);
        params.leftMargin = (availableWidth - width) / 2;
        params.topMargin = (availableHeight - height) / 2;
        m_view.setLayoutParams(params);

        Log.i(TAG, "video surface " + width + "x" + height + " in "
                   + availableWidth + "x" + availableHeight
                   + (s_fill ? " filling" : " fitted")
                   + " at aspect " + String.format("%.3f", aspect));
    }

    // Only before the first video. Locking the canvas takes the buffer queue
    // for the CPU and never gives it back, so MediaCodec is refused with
    // "already connected" for the rest of the process.
    private void paintFill() {
        if (m_surface == null || s_fillColour == Color.TRANSPARENT) {
            return;
        }
        if (s_surfaceHandedOver) {
            Log.w(TAG, "fill colour ignored, playback has used this surface");
            return;
        }

        Canvas canvas = m_surface.lockCanvas(null);
        if (canvas == null) {
            return;
        }
        canvas.drawColor(s_fillColour);
        m_surface.unlockCanvasAndPost(canvas);
    }

    private void notifySurfaceLost() {
        m_surface = null;
        Log.i(TAG, "surface destroyed, waiting for playback to let go");
        try {
            nativeVideoSurfaceLost();
        } catch (UnsatisfiedLinkError e) {
        }
        Log.i(TAG, "surface released to the system");
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        m_surface = holder.getSurface();
        Log.i(TAG, "surface created");
        paintFill();
        try {
            nativeVideoSurfaceReady();
        } catch (UnsatisfiedLinkError e) {
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format,
                               int width, int height) {
        m_surface = holder.getSurface();
        Log.i(TAG, "surface changed to " + width + "x" + height);
        paintFill();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        notifySurfaceLost();
    }
}
