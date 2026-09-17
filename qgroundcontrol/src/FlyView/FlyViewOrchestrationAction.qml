import QtQuick

import QGroundControl
import QGroundControl.Controls

// STRATUM MVO — S1 debug harness launcher (throwaway; replaced by the real wizard at S3).
// Opens a standalone window that drives QGroundControl.orchestration only. It drives
// the orchestration singleton, never the active-vehicle pointer (RE2). Additive
// tool-strip entry; changes no single-vehicle behaviour (RE1).
//
// The launcher resolves the panel by URL relative to this file
// (FlyView/ -> FlyView/Orchestration/OrchestrationDebugPanel.qml) rather than by the
// (module, typeName) createComponent overload: a QML file in a module SUBDIRECTORY is
// not reliably exposed as a top-level type name, which is why the button previously
// did nothing — the component silently failed to load. The URL form is resolved
// against the compiled qrc resource and always finds the panel. Async status is
// handled so a not-yet-ready component still opens.
ToolStripAction {
    id: action

    text:       qsTr("MVO (debug)")
    iconSource: "/qmlimages/StandoffMarker.svg"
    enabled:    true
    visible:    true

    property color accentColorOverride:     "#2E7D32"
    property color accentTextColorOverride: "#FFFFFF"

    property var _win:       null
    property var _component: null

    onTriggered: {
        if (_win) {
            _win.raise()
            _win.requestActivate()
            return
        }
        if (!_component) {
            _component = Qt.createComponent(Qt.resolvedUrl("Orchestration/OrchestrationDebugPanel.qml"))
        }
        _spawn()
    }

    function _spawn() {
        if (_component.status === Component.Loading) {
            _component.statusChanged.connect(action._spawn)
            return
        }
        if (_component.status === Component.Ready) {
            _win = _component.createObject(null)
            if (_win) {
                _win.show()
                _win.raise()
                _win.requestActivate()
            } else {
                console.error("MVO debug: panel createObject failed:", _component.errorString())
            }
        } else {
            console.error("MVO debug: panel component failed to load:", _component.errorString())
        }
    }
}
