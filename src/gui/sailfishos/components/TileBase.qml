// This file is part of Fahrplan.
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Mirian Margiani

import QtQuick 2.0
import Sailfish.Silica 1.0

ListItem {
    id: root

    // Important: width and contentHeight must be set manually!

    property alias text: textLabel.text
    property alias textLabel: textLabel

    property bool highlightBackgroundPress: false
    property bool isAlternateHighlight: false
    property color alternateHighlightColor: Theme.rgba(Theme.highlightBackgroundColor, 0.15)

    property string backgroundImage  // optional

    _backgroundColor: isAlternateHighlight ?
            alternateHighlightColor :
            (highlightBackgroundPress && _showPress ?
                 highlightedColor : "transparent")

    HighlightImage {
        anchors {
            top: parent.top
            right: parent.right
            margins: Theme.paddingMedium
        }

        fillMode: Image.PreserveAspectFit
        source: backgroundImage
        color: highlighted ? palette.highlightColor : palette.primaryColor
        opacity: 0.1
    }

    Label {
        id: textLabel

        anchors {
            fill: parent
            margins: Theme.paddingMedium
        }

        font {
            pixelSize: Theme.fontSizeSmall
            family: Theme.fontFamilyHeading
        }

        verticalAlignment: Text.AlignBottom
        horizontalAlignment: Text.AlignLeft
        fontSizeMode: Text.VerticalFit
        minimumPixelSize: Theme.fontSizeExtraSmall
        elide: Text.ElideRight
        truncationMode: TruncationMode.None
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        color: root.highlighted ? palette.highlightColor : palette.primaryColor

        textFormat: Text.StyledText
    }

    GradientBackground {
        anchors.fill: parent
    }
}
