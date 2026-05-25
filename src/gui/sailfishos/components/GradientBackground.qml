// This file is part of Fahrplan.
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2022-2026 Mirian Margiani

import QtQuick 2.0
import Sailfish.Silica 1.0

SilicaItem {
    id: root

    property real startOpacity: 0.01
    property real endOpacity: 0.15

    SilicaItem {
        anchors.fill: parent
        clip: true

        Rectangle {
            width: parent.width * 2
            height: parent.height * 2

            transform: Rotation {
                angle: 30
                origin.x: 0
                origin.y: 0
            }

            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: Theme.rgba(
                               root.highlighted ?
                                   palette.secondaryHighlightColor :
                                   palette.secondaryColor, startOpacity
                               )
                }
                GradientStop {
                    position: 1.0
                    color: Theme.rgba(
                               root.highlighted ?
                                   palette.secondaryHighlightColor :
                                   palette.secondaryColor, endOpacity
                               )
                }
            }
        }
    }
}
