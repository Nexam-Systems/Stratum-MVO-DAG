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
// reference (_root._displayLabel, _root._activeVehicle, etc.).
ToolStripAction {
    id:         _root

    text:       qsTr("Flight Mode")
    iconSource: "/qmlimages/FlightModesComponentIcon.png"
    visible:    true
    enabled:    !!_activeVehicle

    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle

    // STRATUM: whitelist of operator-facing modes; excludes exotic PX4 modes
    // (Offboard / Mission / Follow Me / Manual...) so the picker stays uncluttered.
    readonly property var _allowedModes: [
        "Takeoff", "Land", "Safe Recovery", "Return",
        "Position", "Standoff", "Engagement", "Hold", "Abort"
    ]

    // STRATUM: PX4 "Position" is surfaced to operators as "Manual" (matches the
    // ribbon indicator).
    function _displayLabel(mode) {
        return mode === "Position" ? qsTr("Manual") : mode
    }

    dropPanelComponent: Component {
        ColumnLayout {
            id:      panelColumn
            spacing: ScreenTools.defaultFontPixelHeight / 2

            QGCLabel {
                Layout.fillWidth: true
                text:      qsTr("Current: %1").arg(_root._activeVehicle
                                                    ? _root._displayLabel(_root._activeVehicle.flightMode)
                                                    : qsTr("N/A"))
                font.bold: true
            }

            Repeater {
                model: _root._activeVehicle ? _root._activeVehicle.flightModes : []

                QGCButton {
                    Layout.fillWidth:      true
                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 20
                    visible:               _root._allowedModes.indexOf(modelData) !== -1
                    text:                  _root._displayLabel(modelData)

                    onClicked: {
                        var modeName = modelData
                        dropPanel.hide()
                        if (!_root._activeVehicle) {
                            return
                        }
                        QGroundControl.showMessageDialog(
                            mainWindow,
                            qsTr("Change Flight Mode"),
                            qsTr("Switch vehicle to %1?").arg(_root._displayLabel(modeName)),
                            Dialog.Ok | Dialog.Cancel,
                            function() {
                                if (_root._activeVehicle) {
                                    _root._activeVehicle.flightMode = modeName
                                }
                            }
                        )
                    }
                }
            }
        }
    }
}
