import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import QtPositioning

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.FlyView   // OrchestrationManager / VehicleAgent enums

// STRATUM MVO — S1 debug harness. NOT a product surface: it exists to exercise the
// OrchestrationManager Q_INVOKABLE surface against multi-instance SITL and observe the
// per-agent FSM. The real five-page wizard (ICD §6) replaces it at S3.
Window {
    id:     root
    width:  480
    height: 660
    minimumWidth:  420
    title:  qsTr("STRATUM MVO — S1 Debug Harness")
    color:  qgcPal.window

    property var orch: QGroundControl.orchestration
    property var mvm:  QGroundControl.multiVehicleManager

    readonly property var _agentStateNames: [
        "UNASSIGNED","ASSIGNED","PREFLIGHT","LAUNCH_QUEUED","TAKEOFF",
        "CLIMB_TO_TRANSIT_LEVEL","COMMIT_QUEUED","STANDOFF_COMMANDED",
        "RUN_IN","ON_STATION","HOLD","RESLOT","RTL","LINK_LOST" ]
    readonly property var _missionStateNames: [
        "IDLE","DEFINING_RING","ASSIGNING_SLOTS","PLAN_REVIEW","PREFLIGHT",
        "EXECUTING","ON_STATION","RESLOTTING","TERMINATING" ]

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    ColumnLayout {
        anchors.fill:    parent
        anchors.margins: 12
        spacing:         8

        QGCLabel {
            text: qsTr("Mission: %1     Vehicles: %2     Armable: %3     Degraded: %4")
                    .arg(root._missionStateNames[root.orch.missionState])
                    .arg(root.mvm.vehicles.count)
                    .arg(root.orch.armable ? "yes" : "no")
                    .arg(root.orch.degraded ? "yes" : "no")
        }

        RowLayout {
            spacing: 6
            QGCLabel { text: qsTr("Target lat/lon") }
            QGCTextField { id: latField; text: "0.0";  Layout.preferredWidth: 90 }
            QGCTextField { id: lonField; text: "0.0";  Layout.preferredWidth: 90 }
        }
        RowLayout {
            spacing: 6
            QGCLabel { text: qsTr("R (m) / H (m)") }
            QGCTextField { id: radiusField; text: "300"; Layout.preferredWidth: 70 }
            QGCTextField { id: heightField; text: "60";  Layout.preferredWidth: 70 }
        }

        GridLayout {
            columns: 2
            columnSpacing: 6
            rowSpacing: 6
            Layout.fillWidth: true

            QGCButton {
                text: qsTr("1. Begin mission")
                Layout.fillWidth: true
                onClicked: root.orch.beginMission()
            }
            QGCButton {
                text: qsTr("2. Set target")
                Layout.fillWidth: true
                onClicked: root.orch.setTarget(QtPositioning.coordinate(parseFloat(latField.text), parseFloat(lonField.text)))
            }
            QGCButton {
                text: qsTr("3. Set ring")
                Layout.fillWidth: true
                onClicked: root.orch.setRing(parseFloat(radiusField.text), parseFloat(heightField.text))
            }
            QGCButton {
                text: qsTr("4. Assign first 3 @ 0/120/240")
                Layout.fillWidth: true
                onClicked: {
                    var n = Math.min(3, root.mvm.vehicles.count)
                    for (var i = 0; i < n; ++i) {
                        var v = root.mvm.vehicles.get(i)
                        if (root.orch.assignVehicleToSlot(v.id, i)) {
                            var agent = root.orch.agents.get(root.orch.agents.count - 1)
                            if (agent && agent.slot) agent.slot.bearingDeg = i * 120.0
                        }
                    }
                }
            }
            QGCButton {
                text: qsTr("5. Force armable (S1 debug)")
                Layout.fillWidth: true
                onClicked: root.orch.debugForceArmable(true)
            }
            QGCButton {
                text: qsTr("6. Execute — commit all")
                Layout.fillWidth: true
                onClicked: root.orch.execute()
            }
            QGCButton {
                text: qsTr("Hold all")
                Layout.fillWidth: true
                onClicked: root.orch.holdAll()
            }
            QGCButton {
                text: qsTr("Terminate")
                Layout.fillWidth: true
                onClicked: root.orch.terminate()
            }
        }

        QGCLabel { text: qsTr("Agents (%1):").arg(root.orch.agents.count) }

        ListView {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            clip:  true
            model: root.orch.agents
            spacing: 4
            delegate: Rectangle {
                width:  ListView.view.width
                height: agentCol.implicitHeight + 8
                color:  qgcPal.windowShade
                radius: 3
                ColumnLayout {
                    id: agentCol
                    anchors.left:    parent.left
                    anchors.right:   parent.right
                    anchors.margins: 6
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1
                    QGCLabel {
                        text: qsTr("Vehicle %1  —  %2")
                                .arg(object.vehicle ? object.vehicle.id : -1)
                                .arg(root._agentStateNames[object.state])
                    }
                    QGCLabel {
                        font.pointSize: ScreenTools.smallFontPointSize
                        text: qsTr("bearing %1°   transit %2 m   link %3")
                                .arg(object.slot ? object.slot.bearingDeg.toFixed(0) : "—")
                                .arg(object.transitLevel.toFixed(0))
                                .arg(object.linkHealthy ? "ok" : "LOST")
                    }
                }
            }
        }
    }
}
