// This file is part of Fahrplan.
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Mirian Margiani

import QtQuick 2.0
import Sailfish.Silica 1.0
import Fahrplan 1.0

import "../components"

TileBase {
    id: root

    property var listModel
    property int modelIndex
    property string name
    property string type
    property string ident

    // Alternating background colors: every second entry in
    // each of the two columns is highlighted. Highlighting
    // starts low, peaks in the middle, and ends low again.
    property int _modelMiddle: listModel.count / 2
    property real _opacityIndex: (
        _modelMiddle -
        (Math.pow(modelIndex-_modelMiddle, 2) / _modelMiddle)
    ) / _modelMiddle

    // FIXME only portrait is currently supported
    width: Screen.width / 2
    contentHeight: Theme.itemSizeExtraLarge
    opacity: !containsPress &&
             !drag.active &&
             !!stationsContainer.currentItem &&
             stationsContainer.currentItem != root ?
                 0.4 : 1.0

    isAlternateHighlight: (index % 2 == 0 && index/2 % 2 == 0) ||
                          (index % 2 == 1 && (index+1)/2 % 2 == 0)
    alternateHighlightColor: Theme.rgba(Theme.highlightBackgroundColor,
                                        Math.min(0.25, (_opacityIndex + 0.15) * 0.8))

    text: name.replace(/^((.*?),\s*)?(.*?)$/, '$2<br><b>$3</b>')
    textLabel {
        anchors {
            leftMargin: (index % 2 == 0) ? Theme.horizontalPageMargin : Theme.paddingMedium
            rightMargin: (index % 2 == 1) ? Theme.horizontalPageMargin : Theme.paddingMedium
            bottomMargin: Theme.paddingMedium
            topMargin: Theme.paddingMedium
        }

        verticalAlignment: Text.AlignBottom
        horizontalAlignment: Text.AlignLeft
    }

    // The background image is optional. Most backends don't set
    // the station type or they set it in a unsupported way. See
    // the search.ch backend implementation for detail/reference.
    // The already saved favorites will not be updated even when
    // the backend gets support for station types. Only new ones.
    backgroundImage: !!type ? Qt.resolvedUrl("../images/tile-bg-%1.png".arg(type)) : ""

    menu: Component {
        ContextMenu {
            MenuItem {
                text: model.isFavorite ?
                          qsTr("Remove from favorites") :
                          qsTr("Add to favorites")
                onClicked: {
                    if (model.isFavorite) {
                        remorseDelete(function(){
                            console.log("Removing %1 from favorites".arg(this.id))
                            this.model.removeByIdFromFavorites(this.id)
                        }.bind({id: ident, model: listModel}))
                    } else {
                        listModel.addToFavorites(modelIndex)
                    }
                }
            }
        }
    }

    property bool dragActive: Drag.active
    Drag.active: allowSelectTwo && drag.active
    Drag.hotSpot.x: 0
    Drag.hotSpot.y: 0
    Drag.keys: ["route"]

    drag.target: dragProxy
    drag.minimumX: 0
    drag.maximumX: stationsContainer.width
    drag.minimumY: 0
    drag.maximumY: stationsContainer.height

    onPressed: {
        if (!allowSelectTwo) return

        stationsContainer.currentItem = null

        var point = root.mapToItem(stationsContainer, mouse.x, mouse.y)
        point.x = Math.min(Math.max(point.x, 0), stationsContainer.width)
        point.y = Math.min(Math.max(point.y, 0), stationsContainer.height)

        dragProxy.x = point.x
        dragProxy.y = point.y
        dragConnector.startX = point.x
        dragConnector.startY = point.y
    }

    onReleased: {
        Drag.cancel()
        dragConnector.clear()
        stationsContainer.currentItem = null
    }

    onDragActiveChanged: {
        if (!dragActive) {
            if (!!stationsContainer.currentItem &&
                    stationsContainer.currentItem != root) {
                stationsContainer.finalizeDrag(root)
            }

            stationsContainer.currentItem = null
            dragConnector.clear()
        }
    }

    Binding on highlighted {
        when: drag.active || stationsContainer.currentItem == root
        value: true
    }

    Behavior on opacity {
        FadeAnimator {}
    }
}
