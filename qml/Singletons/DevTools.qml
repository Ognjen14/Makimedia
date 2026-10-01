pragma Singleton

import QtQuick

QtObject {
    id: root

    readonly property bool showDeveloperSurfaces: false

    property bool forceWelcome: false
    property bool forcePermissionGate: false
    property bool touchCapable: false
    property bool forceSoftwareDecode: false
    property bool audioFocusTools: false

    signal fileMissingRequested(string displayName)
    signal folderUnavailableRequested(string displayName)
    signal errorRequested(string message)
    signal playbackFailureRequested(string reason)
}
