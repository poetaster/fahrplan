/****************************************************************************
**
**  This file is a part of Fahrplan.
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License along
**  with this program.  If not, see <https://www.gnu.org/licenses/>.
**
****************************************************************************/

import QtQuick 2.0
import Sailfish.Silica 1.0
import Fahrplan 1.0

// source paths:    src/gui/sailfishos/pages/AboutPage.qml
//                          -----1---- --2--
//                  src/gui/about.js -> two levels difference
// installed paths: harbour-fahrplan2/qml/pages/AboutPage.qml
//                                        --1--
//                  harbour-fahrplan2/qml/about.js -> one level!
// This is necessary because Sailfish doesn't allow pointing
// outside of the harbour-fahrplan2/qml directory.
import "../about.js" as About

Page {
    id: aboutPage
    property FahrplanBackend fahrplanBackend: null

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height
        contentWidth: parent.width

        VerticalScrollDecorator {}

        Item {
            id: column
            width: parent.width
            height: childrenRect.height

            PageHeader {
                id: header
                title: qsTr("About Fahrplan")
            }

            Column {
                width: parent.width

                anchors {
                    top: parent.top
                    topMargin: 120
                }

                id: aboutContainer

                spacing: 25

                Image {
                    source: "qrc:///data/sailfishos/harbour-fahrplan2.png"
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                    }
                }

                Label {
                    text: qsTr("Version") + " " + fahrplanBackend.version
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                    }
                }

                Label {
                    text: About.copyright
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                    }
                }

                Column {
                    width: parent.width - 20
                    anchors {
                        leftMargin: 10
                        rightMargin: 10
                        left: parent.left
                        right: parent.right
                    }

                    Label {
                        text: qsTr("Maintainers")
                        font.bold: true
                    }

                    Repeater {
                        model: About.maintainers

                        delegate: Label {
                            text: About.maintainers[index]
                        }
                    }

                    Label {
                        /* spacer */
                        text: " "
                    }

                    Label {
                        text: qsTr("Code Contributors")
                        font.bold: true
                    }

                    Repeater {
                        model: About.codeContributors
                        delegate: Label {
                            text: About.codeContributors[index]
                        }
                    }

                    Label {
                        /* spacer */
                        text: " "
                    }

                    Label {
                        text: qsTr("Translators")
                        font.bold: true
                    }

                    Repeater {
                        model: About.translators
                        delegate: Label {
                            anchors {
                                left: parent.left
                                right: parent.right
                            }

                            wrapMode: Text.WordWrap
                            text: About.translators[index]
                        }
                    }

                    Label {
                        /* spacer */
                        text: " "
                    }

                    Label {
                        text: qsTr("Support")
                        font.bold: true
                    }

                    Label {
                        wrapMode: Text.WordWrap
                        width: parent.width
                        textFormat: Text.RichText
                        onLinkActivated : Qt.openUrlExternally(link)
                        text: About.support
                    }

                    Label {
                        /* spacer */
                        text: " "
                    }

                    Label {
                        text: qsTr("Station background images")
                        font.bold: true
                    }

                    Label {
                        wrapMode: Text.WordWrap
                        width: parent.width
                        textFormat: Text.StyledText
                        onLinkActivated : Qt.openUrlExternally(link)
                        text: "<a href='https://www.maptiler.com/copyright/'>© MapTiler</a> " +
                              "<a href='https://www.openstreetmap.org/copyright'>© OpenStreetMap contributors</a>"
                    }

                    Label {
                        /* spacer */
                        text: " "
                    }

                    Label {
                        text: qsTr("License")
                        font.bold: true
                    }

                    Label {
                        wrapMode: Text.WordWrap
                        width: parent.width
                        textFormat: Text.RichText
                        onLinkActivated : Qt.openUrlExternally(link)
                        text: About.license
                    }

                    Label {
                        /* spacer */
                        text: " "
                    }
                }
            }
        }
    }
}
