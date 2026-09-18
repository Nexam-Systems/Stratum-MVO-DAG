import QtQuick

import QGroundControl
import QGroundControl.Controls

// STRATUM MVO — S1 debug harness launcher (throwaway; replaced by the real wizard at S3).
// Opens the standalone debug window that drives QGroundControl.orchestration only (RE2);
// additive tool-strip entry, changes no single-vehicle behaviour (RE1).
//
// The window is created and shown exactly the way MainWindow.createWindowedAnalyzePage
// does it — parented to mainWindow and made visible via `visible = true`, NOT
// createObject(null) + show(), which failed to display. Any load/create failure is
// surfaced as an in-app message dialog so the reason is visible without a console.
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

    onTriggered: action._open()

    function _open() {
        if (_win) {
            _win.visible = true
            _win.raise()
            _win.requestActivate()
            return
        }
        if (!_component) {
            _component = Qt.createComponent(Qt.resolvedUrl("Orchestration/OrchestrationDebugPanel.qml"))
        }
        action._spawn()
    }

    function _spawn() {
        if (_component.status === Component.Loading) {
            _component.statusChanged.connect(action._spawn)
            return
        }
        if (_component.status !== Component.Ready) {
            QGroundControl.showMessageDialog(mainWindow, qsTr("MVO debug"),
                qsTr("Panel failed to load:\n%1").arg(_component.errorString()))
            return
        }
        _win = _component.createObject(mainWindow)
        if (!_win) {
            QGroundControl.showMessageDialog(mainWindow, qsTr("MVO debug"),
                qsTr("Panel failed to create:\n%1").arg(_component.errorString()))
            return
        }
        _win.visible = true
        _win.raise()
        _win.requestActivate()
    }
}
