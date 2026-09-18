import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import QtPositioning

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView   // OrchestrationManager / VehicleAgent enums

// STRATUM MVO — S2 debug harness. NOT a product surface: it exists to drive the
// OrchestrationManager Q_INVOKABLE surface against multi-instance SITL and observe the
// per-agent FSM, the solver/planner proposal, and the FailsafeVerifier ARM gate. The
// real five-page wizard (ICD §6) replaces it at S3.
//
// S2 note: the S1 "force armable" bypass is retired. ARM is now gated by the real
// FailsafeVerifier (RE6/D8/D9) — a vehicle missing the failsafe block, carrying
// STDF_SEQ in {2,4}, or sharing a MAV_SYS_ID cannot reach armable, and the findings
// print in the log below. Provision the D8 block on each SITL instance to pass preflight.
Window {
    id:     root
    width:  520
    height: 760
    minimumWidth:  440
    // Centre on the primary screen so the window can never open off-screen on a
    // multi-monitor Windows setup (a silent "nothing appeared" failure mode).
    x:      Screen.width  > 0 ? (Screen.width  - width)  / 2 : 100
    y:      Screen.height > 0 ? (Screen.height - height) / 2 : 100
    flags:  Qt.Window
    title:  qsTr("STRATUM MVO — S2 Debug Harness")
    color:  qgcPal.window

    property var orch: QGroundControl.orchestration
    property var mvm:  QGroundControl.multiVehicleManager
    property string log: ""

    readonly property var _agentStateNames: [
        "UNASSIGNED","ASSIGNED","PREFLIGHT","LAUNCH_QUEUED","TAKEOFF",
        "CLIMB_TO_TRANSIT_LEVEL","COMMIT_QUEUED","STANDOFF_COMMANDED",
        "RUN_IN","ON_STATION","HOLD","ENGAGE_COMMANDED","ENGAGING","RESLOT","RTL","LINK_LOST" ]
    readonly property var _missionStateNames: [
        "IDLE","DEFINING_RING","ASSIGNING_SLOTS","PLAN_REVIEW","PREFLIGHT",
        "EXECUTING","ON_STATION","RESLOTTING","TERMINATING" ]

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    // Surface every advisory (assignment summary, preflight findings) into the log.
    Connections {
        target: root.orch
        function onAdvisory(text) {
            root.log = text + "\n" + root.log
        }
    }

    ColumnLayout {
        anchors.fill:    parent
        anchors.margins: 12
        spacing:         8

        QGCLabel {
            text: qsTr("Mission: %1   Vehicles: %2   Armable: %3   Degraded: %4   EngageAuth: %5")
                    .arg(root._missionStateNames[root.orch.missionState])
                    .arg(root.mvm.vehicles.count)
                    .arg(root.orch.armable ? "yes" : "no")
                    .arg(root.orch.degraded ? "yes" : "no")
                    .arg(root.orch.engageAuthorized ? "YES" : "no")
        }

        RowLayout {
            spacing: 6
            QGCLabel { text: qsTr("Target lat/lon") }
            QGCTextField { id: latField; text: "0.0";  Layout.preferredWidth: 90 }
            QGCTextField { id: lonField; text: "0.0";  Layout.preferredWidth: 90 }
        }
        RowLayout {
            spacing: 6
            QGCLabel { text: qsTr("R (m) / Standoff H (m)") }
            QGCTextField { id: radiusField; text: "300"; Layout.preferredWidth: 70 }
            QGCTextField { id: heightField; text: "60";  Layout.preferredWidth: 70 }
        }
        RowLayout {
            spacing: 6
            QGCLabel { text: qsTr("Engage stagger (s/vehicle)") }
            QGCTextField { id: engageDelayField; text: "3"; Layout.preferredWidth: 70 }
            QGCLabel { text: qsTr("0 = simultaneous"); opacity: 0.7 }
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
                text: qsTr("4. Propose assignment (solver)")
                Layout.fillWidth: true
                onClicked: root.orch.proposeAssignment()
            }
            QGCButton {
                text: qsTr("5. Review plan")
                Layout.fillWidth: true
                onClicked: root.orch.reviewPlan()
            }
            QGCButton {
                text: qsTr("6. Run preflight (verify)")
                Layout.fillWidth: true
                onClicked: root.orch.runPreflight()
            }
            QGCButton {
                text: qsTr("7. Execute — commit all")
                Layout.fillWidth: true
                onClicked: root.orch.execute()
            }
            QGCButton {
                text: qsTr("8. %1 engagement").arg(root.orch.engageAuthorized ? "De-authorize" : "Authorize")
                Layout.fillWidth: true
                onClicked: root.orch.authorizeEngagement(!root.orch.engageAuthorized)
            }
            QGCButton {
                text: qsTr("9. Engage all (coord, sub=21)")
                Layout.fillWidth: true
                enabled: root.orch.engageAuthorized
                onClicked: root.orch.engageAll(parseFloat(engageDelayField.text))
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
            QGCButton {
                text: qsTr("Clear log")
                Layout.fillWidth: true
                onClicked: root.log = ""
            }
        }

        QGCLabel { text: qsTr("Agents (%1):").arg(root.orch.agents.count) }

        ListView {
            Layout.fillWidth:  true
            Layout.preferredHeight: 200
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
                        text: qsTr("bearing %1°   transit %2 m   run-in %3 m   eta %4 s   link %5")
                                .arg(object.slot ? object.slot.bearingDeg.toFixed(0) : "—")
                                .arg(object.transitLevel.toFixed(0))
                                .arg(object.runInLengthM.toFixed(0))
                                .arg(object.etaSeconds.toFixed(0))
                                .arg(object.linkHealthy ? "ok" : "LOST")
                    }
                }
            }
        }

        QGCLabel { text: qsTr("Advisory / findings log:") }

        Flickable {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: logText.implicitHeight
            QGCLabel {
                id: logText
                width: parent.width
                wrapMode: Text.Wrap
                font.pointSize: ScreenTools.smallFontPointSize
                text: root.log
            }
        }
    }
}
