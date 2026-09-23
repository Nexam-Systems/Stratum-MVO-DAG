import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView

Item {
    required property var guidedValueSlider

    id:     control
    width:  parent.width
    height: ScreenTools.toolbarHeight

    // STRATUM: the standoff / AOP entry commands now live in the centre of this ribbon
    // (moved off the left command strip). The handlers are wired in FlyView.qml, which
    // owns the widget layer that hosts the standoff panel and the AOP map editor.
    signal defineAOP()
    signal setStandoff()

    property var    _activeVehicle:     QGroundControl.multiVehicleManager.activeVehicle
    property bool   _communicationLost: _activeVehicle ? _activeVehicle.vehicleLinkManager.communicationLost : false
    property real   _leftRightMargin:   ScreenTools.defaultFontPixelWidth * 0.75
    property var    _guidedController:  globals.guidedControllerFlyView

    // STRATUM: solid ribbon colour reflects operational state. Kept in lock-step with
    // FlyViewToolStrip.qml and FlightMap/MapItems/VehicleMapItem.qml.
    readonly property string _abortModeName:      qsTr("Abort")
    readonly property string _engagementModeName: qsTr("Engagement")
    // All three engagement modes take the red ribbon: it encodes "committed to a terminal
    // run", not which guidance law is running. Vision Engagement was previously absent
    // from this test and showed the green operations ribbon; it is added with PN.
    readonly property string _visionEngagementModeName: qsTr("Vision Engagement")
    readonly property string _pnEngagementModeName:     qsTr("PN Engagement")
    readonly property string _holdModeName:       _activeVehicle ? _activeVehicle.pauseFlightMode : qsTr("Hold")
    property color _ribbonColor: {
        if (!_activeVehicle) {
            return "#6B7280"                    // disconnected / no vehicle
        }
        if (_communicationLost) {
            return "#6B7280"                    // disconnected
        }
        var mode = _activeVehicle.flightMode
        if (mode === _abortModeName) {
            return "#F59E0B"                    // abort
        }
        if (mode === _engagementModeName || mode === _visionEngagementModeName || mode === _pnEngagementModeName) {
            return "#DC2626"                    // engagement (coordinate, vision, or PN)
        }
        if (mode === qsTr("Standoff") || mode === qsTr("Takeoff") || mode === _holdModeName || _activeVehicle.flying) {
            return "#22C55E"                    // normal operations
        }
        return "#1E88E5"                        // connected, on the ground / ready
    }
    // STRATUM: ribbon content (logo, status text, mode, telemetry) binds to qgcPal.text
    // (STRATUM-Agent-Context.md §2/§4: dark palette resolves this to #F1F4F7).
    readonly property color _ribbonTextColor: qgcPal.text

    function dropMainStatusIndicatorTool() {
        mainStatusIndicator.dropMainStatusIndicator();
    }

    QGCPalette { id: qgcPal }

    Rectangle {
        anchors.fill:   parent
        color:          _ribbonColor
    }

    QGCFlickable {
        anchors.fill:       parent
        contentWidth:       toolBarLayout.width
        flickableDirection: Flickable.HorizontalFlick

        Row {
            id:         toolBarLayout
            height:     parent.height
            spacing:    0

            Item {
                id:     leftPanel
                width:  leftPanelLayout.implicitWidth
                height: parent.height

                RowLayout {
                    id:         leftPanelLayout
                    height:     parent.height
                    spacing:    ScreenTools.defaultFontPixelWidth * 2

                    RowLayout {
                        id:         mainStatusLayout
                        height:     parent.height
                        spacing:    0

                        QGCToolBarButton {
                            id:                 qgcButton
                            objectName:         "toolbar_qgcLogo"
                            Layout.fillHeight:  true
                            // STRATUM: NEXAM (NX) company mark on the left, tinted to the
                            // ribbon text token (qgcPal.text) so it matches other ribbon content.
                            icon.source:        "/res/NXLogo.svg"
                            logo:               true
                            logoColor:          _ribbonTextColor
                            onClicked:          mainWindow.showToolSelectDialog()
                        }

                        // STRATUM (§3.1): button-style chip wrapper for the arm status.
                        // Outer Item hosts the border rectangle (anchored, non-participating
                        // in any Row/ColumnLayout) plus the indicator. Padding = -0.35em L/R,
                        // 0.15em T/B, radius 0.3em per the design tokens.
                        Item {
                            id:                     mainStatusChip
                            Layout.fillHeight:      true
                            Layout.preferredWidth:  mainStatusIndicator.implicitWidth
                                                    + ScreenTools.defaultFontPixelWidth * 0.7

                            Rectangle {
                                anchors.fill:           parent
                                anchors.topMargin:      ScreenTools.defaultFontPixelHeight * 0.15
                                anchors.bottomMargin:   ScreenTools.defaultFontPixelHeight * 0.15
                                color:                  "transparent"
                                border.color:           _ribbonTextColor
                                border.width:           1
                                radius:                 ScreenTools.defaultFontPixelHeight * 0.3
                            }

                            MainStatusIndicator {
                                id:                 mainStatusIndicator
                                objectName:         "toolbar_mainStatusIndicator"
                                anchors.centerIn:   parent
                                height:             parent.height
                                ribbonTextColor:    _ribbonTextColor
                            }
                        }
                    }

                    QGCButton {
                        id:         disconnectButton
                        text:       qsTr("Disconnect")
                        onClicked:  _activeVehicle.closeVehicle()
                        visible:    _activeVehicle && _communicationLost
                    }

                    // STRATUM (§3.1): matching chip wrapper for the read-only flight-mode
                    // display (mode picker moved to the left tool strip).
                    Item {
                        id:                     flightModeChip
                        Layout.fillHeight:      true
                        Layout.preferredWidth:  flightModeIndicator.implicitWidth
                                                + ScreenTools.defaultFontPixelWidth * 0.7
                        visible:                _activeVehicle

                        Rectangle {
                            anchors.fill:           parent
                            anchors.topMargin:      ScreenTools.defaultFontPixelHeight * 0.15
                            anchors.bottomMargin:   ScreenTools.defaultFontPixelHeight * 0.15
                            color:                  "transparent"
                            border.color:           _ribbonTextColor
                            border.width:           1
                            radius:                 ScreenTools.defaultFontPixelHeight * 0.3
                        }

                        FlightModeIndicator {
                            id:                 flightModeIndicator
                            objectName:         "toolbar_flightModeIndicator"
                            anchors.centerIn:   parent
                            height:             parent.height
                            ribbonTextColor:    _ribbonTextColor
                        }
                    }
                }
            }
            Item {
                id:     centerPanel
                // STRATUM: centre of the ribbon carries the Define AOP and Set Standoff
                // command buttons (relocated from the left command strip).
                width:  Math.max(0, control.width - (leftPanel.width + rightPanel.width))
                height: parent.height

                Row {
                    anchors.centerIn:   parent
                    spacing:            ScreenTools.defaultFontPixelWidth

                    QGCButton {
                        text:       qsTr("Define AOP")
                        onClicked:  control.defineAOP()
                    }

                    QGCButton {
                        text:       qsTr("Set Standoff")
                        onClicked:  control.setStandoff()
                    }
                }
            }

            Item {
                id:     rightPanel
                width:  flyViewIndicators.width
                height: parent.height

                FlyViewToolBarIndicators {
                    id:                 flyViewIndicators
                    height:             parent.height
                    ribbonTextColor:    _ribbonTextColor
                }
            }
        }
    }

    // STRATUM: the guided-action confirm bar and its message display were moved to the
    // bottom edge of the fly view. See FlyView.qml (guidedActionConfirmBottomBar).

    ParameterDownloadProgress {
        anchors.fill: parent
    }
}
