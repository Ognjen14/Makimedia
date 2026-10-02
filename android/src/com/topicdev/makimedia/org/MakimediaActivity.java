package com.topicdev.makimedia.org;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.UriPermission;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.database.ContentObserver;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;

import java.util.ArrayList;
import java.util.List;

import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import org.qtproject.qt.android.bindings.QtActivity;

public class MakimediaActivity extends QtActivity {
    private static MakimediaActivity m_instance;
    private static volatile Context s_applicationContext;
    private OnBackInvokedCallback m_backCallback;

    @SuppressWarnings("deprecation")
    @Override
    public void onCreate(Bundle savedInstanceState) {
        m_instance = this;
        s_applicationContext = getApplicationContext();

        Window window = getWindow();
        WindowCompat.setDecorFitsSystemWindows(window, false);
        if (Build.VERSION.SDK_INT < 35) {
            window.setStatusBarColor(Color.TRANSPARENT);
            window.setNavigationBarColor(Color.TRANSPARENT);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            window.setNavigationBarContrastEnforced(false);
            window.setStatusBarContrastEnforced(false);
        }

        super.onCreate(savedInstanceState);

        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_USER);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            m_backCallback = () -> {
                try { nativeOnBack(); } catch (UnsatisfiedLinkError e) { finish(); }
            };
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                OnBackInvokedDispatcher.PRIORITY_DEFAULT, m_backCallback);
        }

        watchStorageIndex();
        watchKeyboard();
    }

    private static native void nativeKeyboardHeight(int pixels);

    // Edge to edge means the keyboard never resizes the window, so Qt cannot
    // see it. The inset height is passed on for a bottom sheet to lift by.
    private void watchKeyboard() {
        final View decor = getWindow().getDecorView();
        ViewCompat.setOnApplyWindowInsetsListener(decor, (view, insets) -> {
            final int keyboard = insets.getInsets(WindowInsetsCompat.Type.ime()).bottom;
            try {
                nativeKeyboardHeight(keyboard);
            } catch (UnsatisfiedLinkError e) {
                Log.e("Makimedia", "could not report the keyboard height", e);
            }

            final WindowInsets platform = insets.toWindowInsets();
            if (platform == null) {
                return insets;
            }
            return WindowInsetsCompat.toWindowInsetsCompat(
                view.onApplyWindowInsets(platform), view);
        });
    }

    private static final long INDEX_SETTLE_MS = 3000;
    private static final long INDEX_MAX_WAIT_MS = 15000;

    private final Handler m_indexHandler = new Handler(Looper.getMainLooper());
    private final java.util.Set<String> m_changedVolumes = new java.util.HashSet<>();
    private long m_firstIndexChangeMs = 0;
    private ContentObserver m_indexObserver;
    private BroadcastReceiver m_indexingReceiver;
    private final Runnable m_reportIndexChange = this::reportIndexChange;

    private static native void nativeStorageIndexChanged(String volumeName);
    private static native void nativeStorageReading(String volumeName, boolean reading);

    @SuppressWarnings("deprecation")
    private void watchStorageIndex() {
        m_indexObserver = new ContentObserver(m_indexHandler) {
            @Override
            public void onChange(boolean selfChange, Uri uri) {
                onStorageIndexChange(uri);
            }
        };
        try {
            getContentResolver().registerContentObserver(
                Uri.parse("content://" + android.provider.MediaStore.AUTHORITY),
                true, m_indexObserver);
        } catch (Exception e) {
            Log.e("Makimedia", "could not watch the media index", e);
            m_indexObserver = null;
        }

        m_indexingReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                onIndexingBroadcast(intent);
            }
        };
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_MEDIA_SCANNER_STARTED);
        filter.addAction(Intent.ACTION_MEDIA_SCANNER_FINISHED);
        filter.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
        filter.addDataScheme("file");
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                registerReceiver(m_indexingReceiver, filter, Context.RECEIVER_EXPORTED);
            } else {
                registerReceiver(m_indexingReceiver, filter);
            }
        } catch (Exception e) {
            Log.e("Makimedia", "could not listen for media scanner broadcasts", e);
            m_indexingReceiver = null;
        }
    }

    private void unwatchStorageIndex() {
        m_indexHandler.removeCallbacks(m_reportIndexChange);
        if (m_indexObserver != null) {
            getContentResolver().unregisterContentObserver(m_indexObserver);
            m_indexObserver = null;
        }
        if (m_indexingReceiver != null) {
            try {
                unregisterReceiver(m_indexingReceiver);
            } catch (IllegalArgumentException e) {
            }
            m_indexingReceiver = null;
        }
    }

    private void onStorageIndexChange(Uri uri) {
        String volume = "";
        if (uri != null) {
            List<String> segments = uri.getPathSegments();
            if (!segments.isEmpty()) {
                volume = segments.get(0);
            }
            if (segments.size() > 1
                && (segments.get(1).equals("images") || segments.get(1).equals("audio"))) {
                return;
            }
        }
        if (volume.equals("internal")) {
            return;
        }
        if (volume.equals(android.provider.MediaStore.VOLUME_EXTERNAL)) {
            volume = "";
        }

        long now = SystemClock.uptimeMillis();
        if (m_changedVolumes.isEmpty()) {
            m_firstIndexChangeMs = now;
        }
        m_changedVolumes.add(volume);

        long delay = Math.min(INDEX_SETTLE_MS,
                              Math.max(0, m_firstIndexChangeMs + INDEX_MAX_WAIT_MS - now));
        m_indexHandler.removeCallbacks(m_reportIndexChange);
        m_indexHandler.postDelayed(m_reportIndexChange, delay);
    }

    private void reportIndexChange() {
        List<String> volumes = new ArrayList<>(m_changedVolumes);
        m_changedVolumes.clear();
        m_firstIndexChangeMs = 0;
        if (volumes.contains("")) {
            volumes.clear();
            volumes.add("");
        }
        for (String volume : volumes) {
            try {
                nativeStorageIndexChanged(volume);
            } catch (UnsatisfiedLinkError e) {
                Log.w("Makimedia", "media index changed before the app finished loading");
                return;
            }
        }
    }

    private void onIndexingBroadcast(Intent intent) {
        Uri data = intent.getData();
        String path = data != null ? data.getPath() : null;
        if (path == null || path.startsWith("/storage/emulated")) {
            return;
        }
        String volume = Uri.parse("file://" + path).getLastPathSegment();
        if (volume == null || volume.isEmpty()) {
            return;
        }
        boolean reading = Intent.ACTION_MEDIA_SCANNER_STARTED.equals(intent.getAction());
        Log.i("Makimedia", intent.getAction() + " for " + path);
        try {
            nativeStorageReading(volume.toLowerCase(java.util.Locale.ROOT), reading);
        } catch (UnsatisfiedLinkError e) {
            Log.w("Makimedia", "media scanner broadcast before the app finished loading");
        }
    }

    @SuppressWarnings("deprecation")
    @Override
    public void onBackPressed() {
        try {
            nativeOnBack();
        } catch (UnsatisfiedLinkError e) {
            Log.w("Makimedia", "back pressed before the app finished loading");
            super.onBackPressed();
        }
    }

    private static native void nativeOnBack();

    private static native void nativeVideoPermissionResult(boolean granted);

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
                                           int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        if (requestCode != 1001) return;

        boolean granted = grantResults.length > 0
                          && grantResults[0] == PackageManager.PERMISSION_GRANTED;
        try {
            nativeVideoPermissionResult(granted);
        } catch (UnsatisfiedLinkError e) {
        }
    }

    static MakimediaActivity instance() {
        return m_instance;
    }

    static Context applicationContext() {
        return s_applicationContext;
    }

    @Override
    protected void onDestroy() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && m_backCallback != null) {
            getOnBackInvokedDispatcher().unregisterOnBackInvokedCallback(m_backCallback);
            m_backCallback = null;
        }
        unwatchStorageIndex();
        MakimediaMediaSession.releaseForDestroyedActivity();
        m_instance = null;

        if (!isChangingConfigurations()) {
            Log.i("Makimedia", "activity destroyed"
                  + (isFinishing() ? "" : " by the system")
                  + ", ending the process instead of waiting for Qt to shut down");
            android.os.Process.killProcess(android.os.Process.myPid());
        }
        super.onDestroy();
    }

    public static void minimizeApp() {
        if (m_instance == null) return;
        m_instance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                m_instance.moveTaskToBack(true);
            }
        });
    }

    public static void setLightSystemBars(final boolean light) {
        if (m_instance == null) return;

        m_instance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Window window = m_instance.getWindow();
                WindowInsetsControllerCompat controller =
                    new WindowInsetsControllerCompat(window, window.getDecorView());
                controller.setAppearanceLightStatusBars(light);
                controller.setAppearanceLightNavigationBars(light);
            }
        });
    }

    public static void setKeepScreenOn(final boolean keepOn) {
        if (m_instance == null) return;

        m_instance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Window window = m_instance.getWindow();
                if (keepOn) {
                    window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                } else {
                    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                }
            }
        });
    }

    public static float systemBrightness() {
        if (m_instance == null) return 0.5f;

        try {
            int value = Settings.System.getInt(
                m_instance.getContentResolver(),
                Settings.System.SCREEN_BRIGHTNESS);
            return Math.max(0.01f, Math.min(1.0f, value / 255.0f));
        } catch (Exception e) {
            return 0.5f;
        }
    }

    public static void setScreenBrightness(final float value) {
        if (m_instance == null) return;

        m_instance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Window window = m_instance.getWindow();
                WindowManager.LayoutParams params = window.getAttributes();
                params.screenBrightness = value;
                window.setAttributes(params);
            }
        });
    }

    public static void setImmersiveMode(final boolean immersive) {
        if (m_instance == null) return;

        m_instance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Window window = m_instance.getWindow();
                WindowInsetsControllerCompat controller =
                    WindowCompat.getInsetsController(window, window.getDecorView());

                if (immersive) {
                    controller.setSystemBarsBehavior(
                        WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                    controller.hide(WindowInsetsCompat.Type.systemBars());
                } else {
                    controller.show(WindowInsetsCompat.Type.systemBars());
                }
            }
        });
    }

    private static String videoPermissionName() {
        if (Build.VERSION.SDK_INT >= 33) {
            return "android.permission.READ_MEDIA_VIDEO";
        }
        return "android.permission.READ_EXTERNAL_STORAGE";
    }

    public static boolean hasVideoPermission() {
        if (m_instance == null) return false;

        return m_instance.checkSelfPermission(videoPermissionName())
               == PackageManager.PERMISSION_GRANTED;
    }

    private static final String PARTIAL_MEDIA_PERMISSION =
        "android.permission.READ_MEDIA_VISUAL_USER_SELECTED";

    public static boolean hasPartialVideoAccess() {
        if (m_instance == null || Build.VERSION.SDK_INT < 34) return false;
        if (hasVideoPermission()) return false;

        return m_instance.checkSelfPermission(PARTIAL_MEDIA_PERMISSION)
               == PackageManager.PERMISSION_GRANTED;
    }

    private static final int REQUEST_PICK_SUBTITLE = 2001;

    private static native void nativeSubtitlePicked(String uri);

    public static void pickSubtitleFile() {
        if (m_instance == null) return;

        m_instance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("*/*");
                    intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    m_instance.startActivityForResult(intent, REQUEST_PICK_SUBTITLE);
                } catch (Exception e) {
                    Log.e("Makimedia", "no document picker available", e);
                    try { nativeSubtitlePicked(""); } catch (UnsatisfiedLinkError l) {}
                }
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == REQUEST_SUBTITLE_FOLDER) {
            onSubtitleFolderPicked(resultCode, data);
            return;
        }

        if (requestCode == REQUEST_SAVE_LOG) {
            onLogDestinationPicked(resultCode, data);
            return;
        }

        if (requestCode != REQUEST_PICK_SUBTITLE) return;

        String uri = "";
        if (resultCode == RESULT_OK && data != null && data.getData() != null) {
            uri = data.getData().toString();
        }

        Log.i("Makimedia", "subtitle picker returned " + (uri.isEmpty() ? "nothing" : uri));
        try {
            nativeSubtitlePicked(uri);
        } catch (UnsatisfiedLinkError e) {
            Log.e("Makimedia", "nativeSubtitlePicked is not bound", e);
        }
    }

    public static int openContentFd(String uriString) {
        if (m_instance == null) return -1;

        try {
            Uri uri = Uri.parse(uriString);
            android.os.ParcelFileDescriptor descriptor =
                m_instance.getContentResolver().openFileDescriptor(uri, "r");
            if (descriptor == null) return -1;
            return descriptor.detachFd();
        } catch (Exception e) {
            Log.e("Makimedia", "could not open content uri " + uriString, e);
            return -1;
        }
    }

    private static final int REQUEST_SUBTITLE_FOLDER = 2002;
    private static final String EXTERNAL_STORAGE_DOCUMENTS =
        "com.android.externalstorage.documents";

    private static native void nativeSubtitleFolderResult(boolean granted);

    private static String documentIdForPath(String path) {
        if (path == null) return null;

        String primary = "/storage/emulated/0";
        if (path.equals(primary)) return "primary:";
        if (path.startsWith(primary + "/")) {
            return "primary:" + path.substring(primary.length() + 1);
        }

        String storage = "/storage/";
        if (!path.startsWith(storage)) return null;

        String rest = path.substring(storage.length());
        int slash = rest.indexOf('/');
        String volume = slash < 0 ? rest : rest.substring(0, slash);
        if (volume.isEmpty() || volume.equals("emulated") || volume.equals("self")) {
            return null;
        }
        return volume + ":" + (slash < 0 ? "" : rest.substring(slash + 1));
    }

    private static List<Uri> grantedTrees() {
        List<Uri> trees = new ArrayList<>();
        Context context = s_applicationContext;
        if (context == null) return trees;

        for (UriPermission permission : context.getContentResolver().getPersistedUriPermissions()) {
            Uri tree = permission.getUri();
            if (permission.isReadPermission()
                && EXTERNAL_STORAGE_DOCUMENTS.equals(tree.getAuthority())) {
                trees.add(tree);
            }
        }
        return trees;
    }

    private static String treeIdOf(Uri tree) {
        try {
            return DocumentsContract.getTreeDocumentId(tree);
        } catch (IllegalArgumentException e) {
            return null;
        }
    }

    private static String childPrefix(String documentId) {
        return documentId.endsWith(":") ? documentId : documentId + "/";
    }

    private static boolean isSubtitleFolderName(String name) {
        return name.equalsIgnoreCase("subs") || name.equalsIgnoreCase("subtitles")
               || name.equalsIgnoreCase("sub");
    }

    private static Uri grantedTreeFor(String documentId) {
        if (documentId == null) return null;

        for (Uri tree : grantedTrees()) {
            String treeId = treeIdOf(tree);
            if (treeId == null) continue;
            if (documentId.equals(treeId) || documentId.startsWith(childPrefix(treeId))) {
                return tree;
            }
        }
        return null;
    }

    private static List<Uri> grantedSubtitleFolderTrees(String folderId) {
        List<Uri> trees = new ArrayList<>();
        String prefix = childPrefix(folderId);
        for (Uri tree : grantedTrees()) {
            String treeId = treeIdOf(tree);
            if (treeId == null || !treeId.startsWith(prefix)) continue;
            String rest = treeId.substring(prefix.length());
            if (rest.indexOf('/') < 0 && isSubtitleFolderName(rest)) {
                trees.add(tree);
            }
        }
        return trees;
    }

    public static boolean hasSubtitleFolderAccess(String videoFolderPath) {
        String folderId = documentIdForPath(videoFolderPath);
        if (folderId == null) return false;
        return grantedTreeFor(folderId) != null
               || !grantedSubtitleFolderTrees(folderId).isEmpty();
    }

    private static void appendChildren(Context context, Uri tree, String parentId,
                                       String place, StringBuilder listing,
                                       List<String> folderIds, List<String> folderNames) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, parentId);
        String[] columns = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };

        try (Cursor cursor = context.getContentResolver().query(children, columns, null, null, null)) {
            if (cursor == null) return;
            while (cursor.moveToNext()) {
                String childId = cursor.getString(0);
                String name = cursor.getString(1);
                String mime = cursor.getString(2);
                if (childId == null || name == null
                    || name.indexOf('\n') >= 0 || name.indexOf('\t') >= 0) {
                    continue;
                }
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                    if (folderIds != null) {
                        folderIds.add(childId);
                        folderNames.add(name);
                    }
                    continue;
                }
                listing.append(place).append('\t').append(name).append('\t')
                       .append(DocumentsContract.buildDocumentUriUsingTree(tree, childId))
                       .append('\n');
            }
        } catch (Exception e) {
            Log.e("Makimedia", "could not list a granted folder " + parentId, e);
        }
    }

    private static void appendSubtitleFolder(Context context, Uri tree, String folderId,
                                             String videoBaseName, StringBuilder listing) {
        List<String> ids = new ArrayList<>();
        List<String> names = new ArrayList<>();
        appendChildren(context, tree, folderId, "subs", listing, ids, names);
        for (int i = 0; i < ids.size(); i++) {
            if (names.get(i).equalsIgnoreCase(videoBaseName)) {
                appendChildren(context, tree, ids.get(i), "named", listing, null, null);
            }
        }
    }

    public static String listSubtitleCandidates(String videoFolderPath, String videoBaseName) {
        Context context = s_applicationContext;
        String folderId = documentIdForPath(videoFolderPath);
        if (context == null || folderId == null) return "";

        StringBuilder listing = new StringBuilder();
        Uri folderTree = grantedTreeFor(folderId);
        if (folderTree != null) {
            List<String> ids = new ArrayList<>();
            List<String> names = new ArrayList<>();
            appendChildren(context, folderTree, folderId, "beside", listing, ids, names);
            for (int i = 0; i < ids.size(); i++) {
                if (isSubtitleFolderName(names.get(i))) {
                    appendSubtitleFolder(context, folderTree, ids.get(i), videoBaseName, listing);
                }
            }
            return listing.toString();
        }

        for (Uri tree : grantedSubtitleFolderTrees(folderId)) {
            appendSubtitleFolder(context, tree, treeIdOf(tree), videoBaseName, listing);
        }
        return listing.toString();
    }

    // Lower case and ending in a slash, except the volume root, which
    // MediaStore gives as an empty path and has to stay one.
    private static String volumeFolder(String relativePath) {
        String folder = relativePath.toLowerCase();
        if (folder.equals("/")) return "";
        if (!folder.isEmpty() && !folder.endsWith("/")) folder = folder + "/";
        return folder;
    }

    // Subtitles for a video on a drive, which has no path to read and no
    // folder to grant. MediaStore indexes the whole volume, so the rows come
    // from there in the same place\tname\turi form as a granted folder.
    public static String listVolumeSubtitleCandidates(String videoUriString) {
        Context context = s_applicationContext;
        if (context == null || videoUriString == null) return "";
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return "";

        String volume;
        String relativePath;
        String videoName;
        String[] videoColumns = {
            android.provider.MediaStore.MediaColumns.VOLUME_NAME,
            android.provider.MediaStore.MediaColumns.RELATIVE_PATH,
            android.provider.MediaStore.MediaColumns.DISPLAY_NAME
        };
        try (Cursor cursor = context.getContentResolver().query(
                 Uri.parse(videoUriString), videoColumns, null, null, null)) {
            if (cursor == null || !cursor.moveToFirst()) return "";
            volume = cursor.getString(0);
            relativePath = cursor.getString(1);
            videoName = cursor.getString(2);
        } catch (Exception e) {
            Log.e("Makimedia", "could not read where " + videoUriString + " is", e);
            return "";
        }
        if (volume == null || relativePath == null || videoName == null) return "";

        String folder = volumeFolder(relativePath);
        int dot = videoName.lastIndexOf('.');
        String base = (dot > 0 ? videoName.substring(0, dot) : videoName).toLowerCase();

        String[] subsNames = { "subs/", "subtitles/", "sub/" };
        String[] folders = new String[1 + 2 * subsNames.length];
        folders[0] = folder;
        for (int i = 0; i < subsNames.length; i++) {
            folders[1 + i] = folder + subsNames[i];
            folders[1 + subsNames.length + i] = folder + subsNames[i] + base + "/";
        }

        StringBuilder where = new StringBuilder("LOWER(")
            .append(android.provider.MediaStore.MediaColumns.RELATIVE_PATH)
            .append(") IN (");
        for (int i = 0; i < folders.length; i++) {
            where.append(i == 0 ? "?" : ",?");
        }
        where.append(')');

        Uri files = android.provider.MediaStore.Files.getContentUri(volume);
        String[] fileColumns = {
            android.provider.MediaStore.MediaColumns._ID,
            android.provider.MediaStore.MediaColumns.DISPLAY_NAME,
            android.provider.MediaStore.MediaColumns.RELATIVE_PATH
        };

        StringBuilder listing = new StringBuilder();
        listing.append("video\t").append(videoName).append('\n');
        try (Cursor cursor = context.getContentResolver().query(
                 files, fileColumns, where.toString(), folders, null)) {
            if (cursor == null) return listing.toString();
            while (cursor.moveToNext()) {
                String name = cursor.getString(1);
                String path = cursor.getString(2);
                if (name == null || path == null
                    || name.indexOf('\n') >= 0 || name.indexOf('\t') >= 0) {
                    continue;
                }
                String at = volumeFolder(path);
                String place = at.equals(folder) ? "beside"
                             : at.endsWith("/" + base + "/") ? "named" : "subs";
                Uri row = android.content.ContentUris.withAppendedId(files, cursor.getLong(0));
                listing.append(place).append('\t').append(name).append('\t')
                       .append(row.toString()).append('\n');
            }
        } catch (Exception e) {
            Log.e("Makimedia", "could not list the files beside " + videoUriString, e);
        }
        return listing.toString();
    }

    public static void requestSubtitleFolderAccess(final String folderPath) {
        if (m_instance == null) return;

        m_instance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                    intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                                    | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                    String documentId = documentIdForPath(folderPath);
                    if (documentId != null) {
                        intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI,
                            DocumentsContract.buildDocumentUri(EXTERNAL_STORAGE_DOCUMENTS,
                                                               documentId));
                    }
                    m_instance.startActivityForResult(intent, REQUEST_SUBTITLE_FOLDER);
                } catch (Exception e) {
                    Log.e("Makimedia", "no folder picker available", e);
                    try { nativeSubtitleFolderResult(false); } catch (UnsatisfiedLinkError l) {}
                }
            }
        });
    }

    private void onSubtitleFolderPicked(int resultCode, Intent data) {
        boolean granted = false;
        if (resultCode == RESULT_OK && data != null && data.getData() != null) {
            try {
                getContentResolver().takePersistableUriPermission(
                    data.getData(), Intent.FLAG_GRANT_READ_URI_PERMISSION);
                granted = true;
            } catch (SecurityException e) {
                Log.e("Makimedia", "folder access could not be kept", e);
            }
        }

        Log.i("Makimedia", "subtitle folder picker returned "
              + (granted ? data.getData().toString() : "nothing"));
        try {
            nativeSubtitleFolderResult(granted);
        } catch (UnsatisfiedLinkError e) {
            Log.e("Makimedia", "nativeSubtitleFolderResult is not bound", e);
        }
    }

    // Every mounted volume but the primary one. A drive is not readable under
    // /storage on Android 11 and later, but MediaStore indexes it anyway.
    public static String removableVolumeNames() {
        if (m_instance == null) return "";

        // getExternalVolumeNames is API 29, and a missing method throws
        // NoSuchMethodError, which the catch below would not hold.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return "";
        }

        StringBuilder out = new StringBuilder();
        try {
            java.util.Set<String> names =
                android.provider.MediaStore.getExternalVolumeNames(m_instance);
            for (String name : names) {
                if (name.equals(android.provider.MediaStore.VOLUME_EXTERNAL)
                    || name.equals(
                        android.provider.MediaStore.VOLUME_EXTERNAL_PRIMARY)) {
                    continue;
                }
                if (out.length() > 0) out.append('\n');
                out.append(name);
            }
        } catch (Exception e) {
            Log.e("Makimedia", "could not list external volumes", e);
        }
        return out.toString();
    }

    // The label the system shows for a drive, found by its MediaStore volume
    // name - which is only a filesystem id, so two drives differ by hex
    // alone. Empty when nothing matches, and the caller keeps the id.
    public static String volumeDescription(String mediaStoreVolumeName) {
        Context context = s_applicationContext;
        if (context == null || mediaStoreVolumeName == null) return "";

        String wanted = mediaStoreVolumeName.replace("-", "").toLowerCase();
        try {
            android.os.storage.StorageManager storage =
                (android.os.storage.StorageManager)
                    context.getSystemService(Context.STORAGE_SERVICE);
            if (storage == null) return "";

            for (android.os.storage.StorageVolume volume : storage.getStorageVolumes()) {
                boolean matches = false;
                // getMediaStoreVolumeName is API 30 and exact; below that the
                // filesystem id is what the name is made of.
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    String name = volume.getMediaStoreVolumeName();
                    matches = name != null
                              && name.replace("-", "").toLowerCase().equals(wanted);
                }
                if (!matches) {
                    String uuid = volume.getUuid();
                    matches = uuid != null
                              && uuid.replace("-", "").toLowerCase().equals(wanted);
                }
                if (matches) {
                    String description = volume.getDescription(context);
                    return description == null ? "" : description.trim();
                }
            }
        } catch (Exception e) {
            Log.e("Makimedia", "could not describe volume " + mediaStoreVolumeName, e);
        }
        return "";
    }

    // One line per video, as uri\tname\tsize\tmodified\trelativePath. Flat
    // rather than parcelled objects: it crosses JNI and is walked once.
    public static String videosOnVolume(String volumeName) {
        if (m_instance == null) return "";
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return "";
        }

        StringBuilder out = new StringBuilder();
        android.database.Cursor cursor = null;
        try {
            Uri collection =
                android.provider.MediaStore.Video.Media.getContentUri(volumeName);

            String[] columns = {
                android.provider.MediaStore.Video.Media._ID,
                android.provider.MediaStore.Video.Media.DISPLAY_NAME,
                android.provider.MediaStore.Video.Media.SIZE,
                android.provider.MediaStore.Video.Media.DATE_MODIFIED,
                android.provider.MediaStore.Video.Media.RELATIVE_PATH
            };

            cursor = m_instance.getContentResolver().query(
                collection, columns, null, null,
                android.provider.MediaStore.Video.Media.RELATIVE_PATH + " ASC");

            if (cursor == null) return "";

            int idCol = cursor.getColumnIndexOrThrow(
                android.provider.MediaStore.Video.Media._ID);
            int nameCol = cursor.getColumnIndexOrThrow(
                android.provider.MediaStore.Video.Media.DISPLAY_NAME);
            int sizeCol = cursor.getColumnIndexOrThrow(
                android.provider.MediaStore.Video.Media.SIZE);
            int dateCol = cursor.getColumnIndexOrThrow(
                android.provider.MediaStore.Video.Media.DATE_MODIFIED);
            int pathCol = cursor.getColumnIndexOrThrow(
                android.provider.MediaStore.Video.Media.RELATIVE_PATH);

            while (cursor.moveToNext()) {
                Uri item = android.content.ContentUris.withAppendedId(
                    collection, cursor.getLong(idCol));

                String name = cursor.getString(nameCol);
                String relative = cursor.getString(pathCol);
                if (name == null) name = "";
                if (relative == null) relative = "";

                // A tab or newline in a name would corrupt the whole listing.
                if (name.indexOf('\t') >= 0 || name.indexOf('\n') >= 0
                    || relative.indexOf('\t') >= 0
                    || relative.indexOf('\n') >= 0) {
                    Log.w("Makimedia", "skipping media row with a separator "
                          + "in its name: " + name);
                    continue;
                }

                if (out.length() > 0) out.append('\n');
                out.append(item.toString()).append('\t')
                   .append(name).append('\t')
                   .append(cursor.getLong(sizeCol)).append('\t')
                   .append(cursor.getLong(dateCol)).append('\t')
                   .append(relative);
            }
        } catch (Exception e) {
            Log.e("Makimedia", "could not query videos on volume "
                  + volumeName, e);
        } finally {
            if (cursor != null) cursor.close();
        }
        return out.toString();
    }

    // The system's own thumbnail, written as a JPEG. Hardware-assisted and
    // cached; decoding a 4K file ourselves pegs a weak TV for twenty seconds.
    public static boolean writeMediaThumbnail(String uriString, String outputPath,
                                              int width, int height) {
        if (m_instance == null) return false;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return false;
        }

        java.io.FileOutputStream stream = null;
        try {
            Uri uri = Uri.parse(uriString);
            android.graphics.Bitmap bitmap =
                m_instance.getContentResolver().loadThumbnail(
                    uri, new android.util.Size(width, height), null);
            if (bitmap == null) return false;

            stream = new java.io.FileOutputStream(outputPath);
            boolean ok = bitmap.compress(
                android.graphics.Bitmap.CompressFormat.JPEG, 80, stream);
            stream.flush();
            bitmap.recycle();
            return ok;
        } catch (Exception e) {
            Log.w("Makimedia", "no system thumbnail for " + uriString + ": "
                  + e.getMessage());
            return false;
        } finally {
            if (stream != null) {
                try { stream.close(); } catch (Exception ignored) {}
            }
        }
    }

    public static String contentDisplayName(String uriString) {
        if (m_instance == null) return "";

        android.database.Cursor cursor = null;
        try {
            Uri uri = Uri.parse(uriString);
            cursor = m_instance.getContentResolver().query(uri, null, null, null, null);
            if (cursor != null && cursor.moveToFirst()) {
                int index = cursor.getColumnIndex(
                    android.provider.OpenableColumns.DISPLAY_NAME);
                if (index >= 0) return cursor.getString(index);
            }
        } catch (Exception e) {
            Log.e("Makimedia", "could not read display name for " + uriString, e);
        } finally {
            if (cursor != null) cursor.close();
        }
        return "";
    }

    private static final int REQUEST_SAVE_LOG = 2003;
    private static volatile String s_pendingLogPath;

    private static native void nativeLogSaved(int outcome);

    public static void saveLogFile(final String sourcePath, final boolean straightToDownloads) {
        if (m_instance == null) return;
        s_pendingLogPath = sourcePath;

        m_instance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (straightToDownloads) {
                    Log.i("Makimedia", "saving the log straight to Download");
                    s_pendingLogPath = null;
                    int outcome = saveLogToDownloads(sourcePath);
                    try { nativeLogSaved(outcome); } catch (UnsatisfiedLinkError l) {}
                    return;
                }
                try {
                    Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("text/plain");
                    intent.putExtra(Intent.EXTRA_TITLE, "makimedia.log");
                    m_instance.startActivityForResult(intent, REQUEST_SAVE_LOG);
                } catch (Exception e) {
                    Log.w("Makimedia", "no document picker, saving the log to Download", e);
                    s_pendingLogPath = null;
                    int outcome = saveLogToDownloads(sourcePath);
                    try { nativeLogSaved(outcome); } catch (UnsatisfiedLinkError l) {}
                }
            }
        });
    }

    private static boolean copyFileTo(String sourcePath, Uri target) {
        Context context = s_applicationContext;
        if (context == null || sourcePath == null || target == null) return false;

        try (java.io.InputStream in = new java.io.FileInputStream(sourcePath);
             java.io.OutputStream out = context.getContentResolver().openOutputStream(target)) {
            if (out == null) return false;
            byte[] buffer = new byte[65536];
            int read;
            while ((read = in.read(buffer)) > 0) {
                out.write(buffer, 0, read);
            }
            return true;
        } catch (Exception e) {
            Log.e("Makimedia", "could not copy " + sourcePath + " to " + target, e);
            return false;
        }
    }

    private static int saveLogToDownloads(String sourcePath) {
        Context context = s_applicationContext;
        if (context == null || sourcePath == null) return -1;
        if (Build.VERSION.SDK_INT < 29) return -3;

        android.content.ContentValues values = new android.content.ContentValues();
        values.put(android.provider.MediaStore.MediaColumns.DISPLAY_NAME, "makimedia.log");
        values.put(android.provider.MediaStore.MediaColumns.MIME_TYPE, "text/plain");
        values.put(android.provider.MediaStore.MediaColumns.RELATIVE_PATH,
                   android.os.Environment.DIRECTORY_DOWNLOADS);

        Uri target;
        try {
            target = context.getContentResolver().insert(
                android.provider.MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
        } catch (Exception e) {
            Log.e("Makimedia", "could not create the log in Download", e);
            return -1;
        }
        if (target == null) return -1;

        if (copyFileTo(sourcePath, target)) {
            Log.i("Makimedia", "log saved to Download as " + target);
            return 2;
        }
        try { context.getContentResolver().delete(target, null, null); } catch (Exception e) {}
        return -1;
    }

    private static native void nativeTextEntered(String text, boolean accepted);

    public static void requestTextInput(final String title, final String initialText) {
        if (m_instance == null) return;

        m_instance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                final android.widget.EditText field = new android.widget.EditText(m_instance);
                field.setSingleLine(true);
                field.setText(initialText == null ? "" : initialText);
                field.setSelection(field.getText().length());
                field.setImeOptions(android.view.inputmethod.EditorInfo.IME_ACTION_SEARCH);

                final boolean[] delivered = { false };
                final android.app.AlertDialog dialog = new android.app.AlertDialog.Builder(m_instance)
                    .setTitle(title)
                    .setPositiveButton(android.R.string.ok, (d, which) -> {
                        delivered[0] = true;
                        deliverText(field.getText().toString(), true);
                    })
                    .setNegativeButton(android.R.string.cancel, null)
                    .create();

                int padding = Math.round(24 * m_instance.getResources().getDisplayMetrics().density);
                dialog.setView(field, padding, padding / 2, padding, 0);

                field.setOnEditorActionListener((view, actionId, event) -> {
                    boolean enter = event != null
                        && event.getKeyCode() == android.view.KeyEvent.KEYCODE_ENTER
                        && event.getAction() == android.view.KeyEvent.ACTION_DOWN;
                    if (actionId == android.view.inputmethod.EditorInfo.IME_ACTION_SEARCH
                        || actionId == android.view.inputmethod.EditorInfo.IME_ACTION_DONE
                        || enter) {
                        delivered[0] = true;
                        deliverText(field.getText().toString(), true);
                        dialog.dismiss();
                        return true;
                    }
                    return false;
                });

                dialog.setOnDismissListener(d -> {
                    if (!delivered[0]) {
                        delivered[0] = true;
                        deliverText(field.getText().toString(), false);
                    }
                });

                if (dialog.getWindow() != null) {
                    dialog.getWindow().setSoftInputMode(
                        WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
                }
                dialog.show();

                field.requestFocus();
                field.postDelayed(() -> {
                    android.view.inputmethod.InputMethodManager imm =
                        (android.view.inputmethod.InputMethodManager)
                            m_instance.getSystemService(Context.INPUT_METHOD_SERVICE);
                    if (imm != null) {
                        imm.showSoftInput(field,
                            android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT);
                    }
                }, 200);
            }
        });
    }

    private static void deliverText(String text, boolean accepted) {
        Log.i("Makimedia", "text input " + (accepted ? "entered" : "cancelled"));
        try {
            nativeTextEntered(text, accepted);
        } catch (UnsatisfiedLinkError e) {
            Log.e("Makimedia", "nativeTextEntered is not bound", e);
        }
    }

    private void onLogDestinationPicked(int resultCode, Intent data) {
        String sourcePath = s_pendingLogPath;
        s_pendingLogPath = null;

        int outcome = 0;
        if (resultCode == RESULT_OK && data != null && data.getData() != null
            && sourcePath != null) {
            outcome = copyFileTo(sourcePath, data.getData()) ? 1 : -1;
        }

        Log.i("Makimedia", "log save finished with outcome " + outcome);
        try {
            nativeLogSaved(outcome);
        } catch (UnsatisfiedLinkError e) {
            Log.e("Makimedia", "nativeLogSaved is not bound", e);
        }
    }

    public static boolean persistContentPermission(String uriString) {
        if (m_instance == null) return false;

        try {
            Uri uri = Uri.parse(uriString);
            m_instance.getContentResolver().takePersistableUriPermission(
                uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            return true;
        } catch (Exception e) {
            Log.w("Makimedia", "content uri is not persistable: " + uriString);
            return false;
        }
    }

    public static boolean shouldShowVideoPermissionRationale() {
        if (m_instance == null) return false;
        return m_instance.shouldShowRequestPermissionRationale(videoPermissionName());
    }

    public static void openAppSettings() {
        if (m_instance == null) return;

        m_instance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                intent.setData(Uri.fromParts("package", m_instance.getPackageName(), null));
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                m_instance.startActivity(intent);
            }
        });
    }

    public static void requestVideoPermission() {
        if (m_instance == null) return;
        if (hasVideoPermission()) return;

        m_instance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                String[] permissions = Build.VERSION.SDK_INT >= 34
                    ? new String[] { videoPermissionName(), PARTIAL_MEDIA_PERMISSION }
                    : new String[] { videoPermissionName() };
                m_instance.requestPermissions(permissions, 1001);
            }
        });
    }
}
