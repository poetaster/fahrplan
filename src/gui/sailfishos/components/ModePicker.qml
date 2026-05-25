// This file is part of Fahrplan.
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Mirian Margiani

import QtQuick 2.0
import Sailfish.Silica 1.0
import Fahrplan 1.0

Row {
    width: parent.width
    height: childrenRect.height

    Repeater {
        id: modeRepeater

        model: ListModel {
            ListElement {
                mode: FahrplanBackend.DepartureMode
                text: qsTr("Departure")
                icon: "mode-departure"
            }
            ListElement {
                mode: FahrplanBackend.NowMode
                text: qsTr("Departure: Now")
                icon: "mode-departure-now"
            }
            ListElement {
                mode: FahrplanBackend.ArrivalMode
                text: qsTr("Arrival")
                icon: "mode-arrival"
            }
        }

        delegate: BackgroundItem {
            width: parent.width / modeRepeater.count
            contentHeight: modeColumn.height
            height: contentHeight
            highlighted: down || fahrplanBackend.mode === model.mode
            _backgroundColor: "transparent"

            onClicked: {
                fahrplanBackend.mode = model.mode
            }

            Column {
                id: modeColumn

                width: parent.width
                height: childrenRect.height

                Item { width: 1; height: Theme.paddingMedium }

                HighlightImage {
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: Qt.resolvedUrl("../images/%1.png".arg(model.icon))
                    width: Theme.iconSizeMedium
                    height: width
                    color: highlighted ? palette.secondaryHighlightColor : palette.secondaryColor
                }

                Label {
                    width: parent.width - 2*x
                    x: Theme.paddingMedium
                    height: lineHeight * maximumLineCount

                    lineHeight: font.pixelSize + Theme.paddingSmall
                    lineHeightMode: Text.FixedHeight
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    fontSizeMode: Text.VerticalFit
                    minimumPixelSize: Theme.fontSizeSmall
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter

                    text: model.text
                }

                Item { width: 1; height: Theme.paddingMedium }
            }
        }
    }
}
