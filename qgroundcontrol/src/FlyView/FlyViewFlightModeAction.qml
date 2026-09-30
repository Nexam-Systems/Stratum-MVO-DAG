import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

// STRATUM: Flight-mode picker on the left command strip. Replaces the interactive
// flight-mode drawer previously hosted on the top ribbon (FlightModeIndicator.qml
// was rewritten as a read-only display, per STRATUM-Agent-Context.md §3.1). The
// action opens a drop panel with a filtered list of PX4 flight modes; selecting
// one confirms via Ok/Cancel then writes vehicle.flightMode, which routes to
// PX4FirmwarePlugin::setFlightMode (case-insensitive match against the vehicle's
// advertised mode names).
//
// STRATUM-Agent-Context.md §5 gotcha 1: NEVER use `id: action` on an action that
// owns a QGCButton -- AbstractButton.action shadows the outer id inside the button
// binding scope and produces empty rows. Use `id: _root` and qualify every
// reference (_root._displayLabel, _root._commandMode, etc.).
//
// STRATUM regression fix: the mode-change confirm dialog and its accept
// callback MUST live on _root, not inline inside the drop-panel button's
// onClicked handler. dropPanel.hide() destroys the ColumnLayout mid-click, so
// invoking QGroundControl.showMessageDialog from that dying scope produced a
// closure whose captured references (_root._activeVehicle) went stale by the
// time the operator pressed OK -- the flightMode write then silently no-oped.
// Routing through _root._commandMode() keeps the dialog owner alive and lets
// the callback re-fetch multiVehicleManager.activeVehicle at accept time.
ToolStripAction {
    id:         _root

    text:       qsTr("Flight Mode")
    iconSource: "/qmlimages/FlightModesComponentIcon.png"
    visible:    true
    enabled:    !!QGroundControl.multiVehicleManager.activeVehicle

    // STRATUM: whitelist of operator-facing modes; excludes exotic PX4 modes
    // (Offboard / Mission / Follow Me / Manual...) so the picker stays uncluttered.
    // Empty list = show every mode reported by the firmware plugin.
    readonly property var _allowedModes: [
        "Takeoff", "Land", "Safe Recovery", "Return",
        "Position", "Standoff", "Engagement", "Hold", "Abort"
    ]

    // STRATUM: PX4 "Position" (POSCTL) is surfaced to operators as "Manual"
    // (matches the ribbon indicator).
    function _displayLabel(mode) {
        return mode === "Position" ? qsTr("Manual") : mode
    }

    function _commandMode(modeName) {
        var vehicle = QGroundControl.multiVehicleManager.activeVehicle
        if (!vehicle) return
        QGroundControl.showMessageDialog(
            mainWindow,
            qsTr("Change Flight Mode"),
            qsTr("Switch the vehicle to %1 flight mode?").arg(_root._displayLabel(modeName)),
            Dialog.Ok | Dialog.Cancel,
            function() {
                var v = QGroundControl.multiVehicleManager.activeVehicle
                if (v) {
                    v.flightMode = modeName
                }
            })
    }

    dropPanelComponent: Component {
        ColumnLayout {
            id:      panelColumn
            spacing: ScreenTools.defaultFontPixelHeight * 0.25

            property var _vehicle: QGroundControl.multiVehicleManager.activeVehicle
            property var _modes: {
                if (!_vehicle) return []
                if (_root._allowedModes.length === 0) return _vehicle.flightModes
                return _vehicle.flightModes.filter(function(m) {
                    return _root._allowedModes.indexOf(m) !== -1
                })
            }

            QGCPalette { id: qgcPal }

            QGCLabel {
                Layout.fillWidth:       true
                horizontalAlignment:    Text.AlignHCenter
                text:                   panelColumn._vehicle
                                            ? qsTr("Current: %1").arg(_root._displayLabel(panelColumn._vehicle.flightMode))
                                            : qsTr("No vehicle")
                font.bold:              true
                color:                  qgcPal.text
            }

            Repeater {
                model: panelColumn._modes

                QGCButton {
                    Layout.fillWidth:       true
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 20
                    text:                   _root._displayLabel(modelData)
                    highlighted:            panelColumn._vehicle && panelColumn._vehicle.flightMode === modelData

                    onClicked: {
                        var mode = modelData
                        dropPanel.hide()
                        _root._commandMode(mode)
                    }
                }
            }
        }
    }
}
