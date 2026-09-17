import QtQuick

import QGroundControl
import QGroundControl.Controls

// STRATUM MVO — S1 debug harness launcher (throwaway; replaced by the real wizard at S3).
// Opens a standalone window that drives QGroundControl.orchestration only. It drives
// the orchestration singleton, never the active-vehicle pointer (RE2). Additive tool-strip entry; changes no single-vehicle behaviour (RE1).
ToolStripAction {
    text:       qsTr("MVO (debug)")
    iconSource: "/qmlimages/StandoffMarker.svg"
    enabled:    true
    visible:    true

    property color accentColorOverride:     "#2E7D32"
    property color accentTextColorOverride: "#FFFFFF"

    property var _win: null

    onTriggered: {
        if (_win) { _win.raise(); _win.requestActivate(); return }
        var c = Qt.createComponent("QGroundControl.FlyView", "OrchestrationDebugPanel")
        if (c.status === Component.Ready) {
            _win = c.createObject(null)
            if (_win) _win.show()
        } else {
            console.error("MVO debug panel failed to load:", c.errorString())
        }
    }
}
