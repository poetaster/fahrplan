// This file is part of Fahrplan.
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Mirian Margiani

import QtQuick 2.0
import Sailfish.Silica 1.0
import Fahrplan 1.0
import com.blackgrain.qml.quickdownload 1.0

import "../components"

TileBase {
    id: root

    property var listModel
    property int modelIndex
    property string name
    property string type
    property string ident
    property real latitude
    property real longitude

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
             allowSelectTwo &&
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
    tileIcon: !!type ? Qt.resolvedUrl("../images/tile-bg-%1.png".arg(type)) : fallbackIcon
    fallbackIcon: Qt.resolvedUrl("../images/tile-bg-generic.png")
    backgroundImage: ""

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

    Download {
        id: coverDownload

        running: false
        followRedirects: true
        overwrite: false

        onError: {
            if (errorCode == Download.ErrorDestination &&
                    /^Overwriting not allowed/.test(errorString)) {
                console.log("using cached station cover:", ident, latitude, longitude)
                backgroundImage = destination
            } else {
                console.error("failed to fetch station cover:", ident, latitude, longitude, errorString)
            }
        }
        onFinished: {
            console.log("station cover downloaded:", ident, latitude, longitude)
            backgroundImage = destination
        }

        Component.onCompleted: {
            // @disable-check M126
            if (MAPS_KEY == "") {
                // Station covers are disabled if we don't have an API key for
                // downloading map tiles.
                return
            }

            if (latitude < -90 || latitude > 90 ||
                longitude < -180 || longitude > 180 ||
                (latitude == 0.0 && longitude == 0.0)
            ) {
                return
            }

            var zoom_level = 18  // looks best
            var tile_url = 'https://api.maptiler.com/tiles/satellite-v2/%1/%2/%3.jpg?key=%4'

            // SPDX-SnippetBegin
            // SPDX-SnippetCopyrightText: OSM Wiki Contributors
            // SPDX-License-Identifier: CC-BY-SA-2.0
            // Source: https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
            function lon2tile(lon, zoom) {
                return (Math.floor((lon+180)/360*Math.pow(2,zoom)));
            }
            function lat2tile(lat, zoom) {
                return (Math.floor((1-Math.log(Math.tan(lat*Math.PI/180)
                                               + 1/Math.cos(lat*Math.PI/180))/Math.PI)/2
                                   * Math.pow(2,zoom)));
            }
            // SPDX-SnippetEnd

            var xtile = lon2tile(longitude, zoom_level)
            var ytile = lat2tile(latitude, zoom_level)
            var final_url = tile_url.arg(zoom_level).arg(xtile).arg(ytile).arg(MAPS_KEY)

            destination = "%1/%2x%3x%4"
                .arg(StandardPaths.cache)
                .arg(zoom_level)
                .arg(xtile)
                .arg(ytile)
            url = final_url
            running = true
        }
    }
}
