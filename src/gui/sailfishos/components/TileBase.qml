// This file is part of Fahrplan.
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Mirian Margiani

import QtQuick 2.0
import Sailfish.Silica 1.0

ListItem {
    id: root

    // Important: width and contentHeight must be set manually!

    property string text
    property alias textLabel: textLabel

    property bool highlightBackgroundPress: false
    property bool isAlternateHighlight: false
    property color alternateHighlightColor: Theme.rgba(Theme.highlightBackgroundColor, 0.15)

    property string backgroundImage  // optional
    property string tileIcon  // optional
    property string fallbackIcon  // optional, fallback if tileIcon fails to load

    readonly property bool _noBackgroundImage: backgroundImageItem.status !== Image.Ready &&
                                               backgroundImageItem.status !== Image.Loading

    _backgroundColor: isAlternateHighlight && _noBackgroundImage ?
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
        source: tileIcon
        color: palette.primaryColor
        highlightColor: palette.highlightColor
        opacity: 0.1

        visible: _noBackgroundImage

        onStatusChanged: {
            if (status == Image.Error
                && source !== fallbackIcon
                && !!fallbackIcon
            ) {
                source = fallbackIcon
            }
        }
    }

    HighlightImage {
        id: backgroundImageItem

        width: parent.width
        height: parent.height

        anchors {
            top: parent.top
            right: parent.right
        }

        fillMode: Image.PreserveAspectCrop
        clip: true
        source: backgroundImage
        highlightColor: palette.highlightColor
        opacity: 0.3
    }

    TextEdit {
        // TODO find a way to add padding left and right of
        // each line before enabling this
        visible: false // !_noBackgroundImage

        // Solid background for each styled text line to
        // make it stand out more against the background image
        enabled: false
        anchors.fill: textLabel
        wrapMode: textLabel.wrapMode
        horizontalAlignment: textLabel.horizontalAlignment
        verticalAlignment: textLabel.verticalAlignment
        font: textLabel.font
        color: textLabel.color
        textFormat: Text.AutoText
        selectedTextColor: textLabel.color
        selectionColor: Theme.highlightDimmerColor

        text: textLabel.text
        onTextChanged: selectAll()
        Component.onCompleted: selectAll()
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
        text: root.text
    }

    GradientBackground {
        anchors.fill: parent
    }
}
