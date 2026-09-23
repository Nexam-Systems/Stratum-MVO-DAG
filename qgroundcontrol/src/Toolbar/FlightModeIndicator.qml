import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls

Item {
    id:                     control
    Layout.preferredWidth:  mainLayout.width

    property bool   showIndicator:          true
    property bool   waitForParameters:      false
    property color  ribbonTextColor:        qgcPal.text   // STRATUM: ribbon text token (§2, resolves to #F1F4F7 on dark)

    property real fontPointSize:    ScreenTools.largeFontPointSize
    property var  activeVehicle:    QGroundControl.multiVehicleManager.activeVehicle

    // STRATUM (§3.1): read-only display; the mode picker moved to the left tool
    // strip as FlyViewFlightModeAction. No dropdown, no drawer, no MouseArea.

    property bool _isVTOL:          activeVehicle ? activeVehicle.vtol : false
    property bool _vtolInFWDFlight: activeVehicle ? activeVehicle.vtolInFwdFlight : false
    property var  _vehicleInAir:    activeVehicle ? activeVehicle.flying || activeVehicle.landing : false

    // STRATUM: "TRACKING Active" ribbon cue. Derived state -- bound to the inbound
    // NEXAM_TARGET_TRACK (42004) fact group on Vehicle. status.value === 1 is
    // StatusTracking (see VehicleTargetTrackFactGroup.h). The fact group's 300 ms
    // staleness timeout clears status on stream loss, so this self-extinguishes without
    // extra logic. Guarded for a null vehicle and for the fact group being absent,
    // mirroring VisionEngagementStatus.qml's _hasGroup pattern.
    property var  _targetTrack:     activeVehicle ? activeVehicle.targetTrack : null
    property bool _hasTrackGroup:   !!_targetTrack
    property bool _tracking:        _hasTrackGroup ? (_targetTrack.status.value === 1) : false

    QGCPalette { id: qgcPal }

    RowLayout {
        id:                     mainLayout
        anchors.verticalCenter: parent.verticalCenter
        spacing:                ScreenTools.defaultFontPixelWidth / 2

        QGCColoredImage {
            id:                     flightModeIcon
            Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 3
            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight
            fillMode:               Image.PreserveAspectFit
            mipmap:                 true
            color:                  ribbonTextColor
            source:                 "/qmlimages/FlightModesComponentIcon.png"
        }

        QGCLabel {
            id:                 flightModeLabel
            // STRATUM: surface PX4 "Position" as "Manual" for operator-facing labelling.
            text:               activeVehicle
                                    ? (activeVehicle.flightMode === "Position" ? qsTr("Manual") : activeVehicle.flightMode)
                                    : qsTr("N/A", "No data to display")
            color:              ribbonTextColor
            font.pointSize:     fontPointSize
        }

        QGCLabel {
            id:                     vtolModeLabel
            Layout.alignment:       Qt.AlignVCenter
            horizontalAlignment:    Text.AlignHCenter
            text:                   _vtolInFWDFlight ? qsTr("FW\nVTOL") : qsTr("MR\nVTOL")
            font.pointSize:         ScreenTools.smallFontPointSize
            wrapMode:               Text.WordWrap
            visible:                _isVTOL
        }

        // STRATUM: "TRACKING Active" pill, shown only while the companion tracker reports
        // StatusTracking (targetTrack.status === 1). Uses the ribbon text colour for the
        // outline/label so it reads correctly on the coloured fly-view ribbon.
        Rectangle {
            id:                     trackingIndicator
            Layout.alignment:       Qt.AlignVCenter
            Layout.leftMargin:      ScreenTools.defaultFontPixelWidth
            implicitWidth:          trackingLabel.implicitWidth + ScreenTools.defaultFontPixelWidth * 1.5
            implicitHeight:         trackingLabel.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.35
            radius:                 height / 2
            color:                  "transparent"
            border.color:           ribbonTextColor
            border.width:           1
            visible:                _tracking

            QGCLabel {
                id:                 trackingLabel
                anchors.centerIn:   parent
                text:               qsTr("TRACKING Active")
                color:              ribbonTextColor
                font.bold:          true
                font.pointSize:     ScreenTools.smallFontPointSize
            }
        }
    }

}
