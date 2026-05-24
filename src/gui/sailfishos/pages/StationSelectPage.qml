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
import QtPositioning 5.2
import Fahrplan 1.0

import "../delegates"
import "../components"

Dialog {
    id: root

    property bool allowSelectTwo: true
    property bool focusSearchImmediately: false
    property string searchString
    property int type: FahrplanBackend.DepartureStation
    property FahrplanBackend fahrplanBackend: null

    property bool _showFavoritesAsList: false
    property bool _showingSearchResults: false
    property bool _haveStationTiles: (
           fahrplanBackend.favorites.count > 0
        || fahrplanBackend.mostRecentStations.count > 0
    )

    function selectOneStation(model, modelIndex) {
        model.selectStation(root.type, modelIndex)

        acceptDestination = pageStack.previousPage()
        acceptDestinationAction = PageStackAction.Pop
        canAccept = true
        accept()
    }

    function selectTwoStations(fromModel, fromIndex, toModel, toIndex) {
        fromModel.selectStation(FahrplanBackend.DepartureStation, fromIndex)
        toModel.selectStation(FahrplanBackend.ArrivalStation, toIndex)

        acceptDestination = pageStack.previousPage()
        acceptDestinationAction = PageStackAction.Pop
        canAccept = true
        accept()
    }

    function selectOneAndSearch(model, firstIndex, firstType) {
        model.selectStation(firstType, firstIndex)

        canAccept = true
        acceptDestinationAction = PageStackAction.Push
        acceptDestinationProperties = {
            type: firstType === FahrplanBackend.DepartureStation ?
                      FahrplanBackend.ArrivalStation :
                      FahrplanBackend.DepartureStation,
            fahrplanBackend: root.fahrplanBackend,
            allowSelectTwo: false,
            focusSearchImmediately: true,
            acceptDestinationAction: PageStackAction.Pop,
            acceptDestination: pageStack.previousPage()
        }

        if (!!acceptDestinationInstance) {
            // Going back from the second dialog and changing the direction
            // means acceptDestinationInstance is already instantiated
            // when acceptDestinationProperties are modified in this
            // function. We have to manually update acceptDestinationInstance.
            acceptDestinationInstance.type = acceptDestinationProperties.type
        }

        // important: properties must be defined before acceptDestination!
        acceptDestination = Qt.resolvedUrl("StationSelectPage.qml")
        stationsContainer.currentItem = null
        accept()
    }

    canAccept: false

    onSearchStringChanged: {
        fahrplanBackend.findStationsByName(searchString)
    }

    onStatusChanged: {
        if (status === PageStatus.Activating) {
            gpsButton.visible = fahrplanBackend.parser.supportsGps()
        }
    }

    PositionSource {
        id: positionSource

        active: false

        onPositionChanged: {
            if (positionSource.active === false) {
                return  // only do something if active
            }

            if (positionSource.position.latitudeValid && positionSource.position.longitudeValid) {
                console.log("Searching for stations...")

                fahrplanBackend.findStationsByCoordinates(
                    positionSource.position.coordinate.longitude,
                    positionSource.position.coordinate.latitude
                )
                _showingSearchResults = true
            } else {
                console.log("Waiting for GPS lock...")
                positionSource.update()
            }
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height
        contentWidth: parent.width

        VerticalScrollDecorator {}

        Column {
            id: column

            spacing: Theme.paddingLarge
            width: parent.width

            PageHeader {
                id: header

                title: {
                    if (allowSelectTwo) {
                        qsTr("Select station")
                    } else if (type == FahrplanBackend.DepartureStation) {
                        qsTr("Select departure station")
                    } else if (type == FahrplanBackend.ArrivalStation) {
                        qsTr("Select destination")
                    } else if (type == FahrplanBackend.ViaStation) {
                        qsTr("Select intermediate station")
                    }
                }
            }

            Row {
                id: searchRow

                width: parent.width

                SearchField {
                    id: searchField
                    width: parent.width - gpsButton.width - 10

                    Binding {
                        target: root
                        property: "searchString"
                        value: searchField.text.trim()
                    }

                    onActiveFocusChanged: {
                        if (activeFocus) {
                            _showingSearchResults = true
                        }
                    }

                    Component.onCompleted: {
                        if (focusSearchImmediately) forceActiveFocus()
                    }
                }

                IconButton {
                    id: gpsButton
                    width: icon.width + Theme.horizontalPageMargin
                    height: parent.height

                    icon {
                        source: "image://theme/icon-m-gps"
                        horizontalAlignment: Image.AlignLeft
                    }

                    onClicked: {
                        fahrplanBackend.stationSearchResults.clear();
                        positionSource.update();
                    }
                }
            }

            Column {
                visible: _showingSearchResults
                width: parent.width

                TileBase {
                    visible: _haveStationTiles
                    width: parent.width
                    contentHeight: Theme.itemSizeSmall

                    highlightBackgroundPress: true
                    text: qsTr("Show favorites") + "&nbsp;&nbsp;\u2022 \u2022 \u2022"
                    textLabel {
                        anchors {
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                        }

                        font.pixelSize: Theme.fontSizeMedium
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignRight
                    }

                    onClicked: {
                        _showingSearchResults = false
                    }
                }

                SectionHeader {
                    text: qsTr("Search")
                    visible: fahrplanBackend.stationSearchResults.count > 0
                }

                ListView {
                    model: fahrplanBackend.stationSearchResults
                    width: parent.width
                    height: contentHeight
                    interactive: false

                    currentIndex: -1

                    delegate: StationListDelegate {
                        highlightQuery: searchField.text
                        remorseWhenRemovingFavorite: false
                        onStationSelected:  {
                            selectOneStation(ListView.view.model, stationIndex)
                        }
                    }
                }
            }

            Item {
                id: stationsContainer

                property var currentItem: null

                visible: opacity > 0.0 && _haveStationTiles
                opacity: !_showFavoritesAsList && !_showingSearchResults ? 1.0 : 0.0
                Behavior on opacity { FadeAnimator {} }

                width: parent.width
                height: stationsPickerContainer.height + 2*Theme.paddingMedium

                function finalizeDrag(from) {
                    if (from === customStation || stationsContainer.currentItem == customStation) {
                        root.selectOneAndSearch(
                            fahrplanBackend.favorites,
                            from === customStation ? stationsContainer.currentItem.modelIndex :
                                                     from.modelIndex,
                            from === customStation ? FahrplanBackend.ArrivalStation :
                                                     FahrplanBackend.DepartureStation
                        )
                    } else {
                        root.selectTwoStations(
                            from.listModel,
                            from.modelIndex,
                            stationsContainer.currentItem.listModel,
                            stationsContainer.currentItem.modelIndex
                        )
                    }

                    stationsContainer.currentItem = null
                }

                Rectangle {
                    id: dragProxy

                    width: 1
                    height: 1
                    visible: false

                    property point position: Qt.point(x, y)

                    onPositionChanged: {
                        if (!allowSelectTwo) return

                        dragConnector.endX = x
                        dragConnector.endY = y
                        dragConnector.requestPaint()

                        var point = parent.mapToItem(stationsPickerContainer, x, y)
                        var container = stationsPickerContainer.childAt(point.x, point.y)

                        var newItem = null
                        if (!!container) {
                            if (container === mostRecentFlow) {
                                point = parent.mapToItem(mostRecentFlow, x, y)
                                newItem = mostRecentFlow.childAt(point.x, point.y)
                            } else if (container === stationsFlow) {
                                point = parent.mapToItem(stationsFlow, x, y)
                                newItem = stationsFlow.childAt(point.x, point.y)
                            } else if (container === customStation) {
                                newItem = customStation
                            }
                        }

                        if (!!newItem && newItem.containsPress) {
                            newItem = null
                        }

                        stationsContainer.currentItem = newItem
                    }
                }

                Canvas {
                    id: dragConnector

                    property int startX: -1
                    property int startY: -1
                    property int endX: 0
                    property int endY: 0

                    function clear() {
                        startX = -1
                        startY = -1
                        requestPaint()
                    }

                    anchors.fill: stationsContainer
                    z: 99999

                    onPaint: {
                        var ctx = getContext("2d")

                        if (startX < 0 || startY < 0) {
                            ctx.clearRect(0, 0, width, height)
                            return
                        }

                        var opacity = (
                               endY < stationsPickerContainer.y
                            || endY > stationsPickerContainer.y + stationsPickerContainer.height
                        ) ? 0.8 : 1.0

                        ctx.reset()

                        ctx.fillStyle = Theme.rgba(Theme.highlightColor, opacity)
                        ctx.strokeStyle = Theme.rgba(Theme.highlightColor, opacity)
                        ctx.lineCap = "round"
                        ctx.lineWidth = Theme.paddingSmall

                        ctx.beginPath()
                        ctx.moveTo(startX, startY)
                        ctx.arc(startX, startY, 1.2*Theme.paddingSmall, 0, 360, true)
                        ctx.fill()

                        ctx.beginPath()
                        ctx.moveTo(startX, startY)
                        // ctx.bezierCurveTo(startX, height/2, endX, height/2, endX, endY)
                        ctx.lineTo(endX, endY)
                        ctx.stroke()

                        ctx.beginPath()
                        ctx.moveTo(endX, endY)
                        ctx.arc(endX, endY, 1.2*Theme.paddingSmall, 0, 360, true)
                        ctx.fill()
                    }
                }

                Column {
                    id: stationsPickerContainer

                    width: parent.width
                    height: childrenRect.height

                    Item {
                        width: parent.width
                        height: Theme.paddingMedium
                    }

                    Flow {
                        id: mostRecentFlow

                        width: parent.width

                        Repeater {
                            model: fahrplanBackend.mostRecentStations

                            delegate: StationTileDelegate {
                                modelIndex: index
                                listModel: fahrplanBackend.mostRecentStations
                                name: model.name
                                type: model.type
                                ident: model.id
                                latitude: model.latitude
                                longitude: model.longitude

                                onClicked: {
                                    selectOneStation(fahrplanBackend.mostRecentStations, modelIndex)
                                }
                            }
                        }
                    }

                    Separator {
                        visible: fahrplanBackend.mostRecentStations.count > 0
                        width: parent.width
                        color: Theme.highlightColor
                        horizontalAlignment: Qt.AlignHCenter
                    }

                    Flow {
                        id: stationsFlow

                        width: parent.width

                        property int maxCount: Math.floor(
                            (root.height-header.height-searchRow.height-2*column.spacing
                             - Theme.itemSizeSmall
                             - customStation.height
                             - mostRecentFlow.height
                            ) / Theme.itemSizeExtraLarge) * 2

                        Repeater {
                            model: fahrplanBackend.favorites

                            delegate: StationTileDelegate {
                                visible: index < stationsFlow.maxCount

                                modelIndex: index
                                listModel: fahrplanBackend.favorites
                                name: model.name
                                type: model.type
                                ident: model.id
                                latitude: model.latitude
                                longitude: model.longitude

                                onClicked: {
                                    selectOneStation(fahrplanBackend.favorites, modelIndex)
                                }
                            }
                        }
                    }

                    StationTileDelegate {
                        id: customStation

                        property string _dynamicLabel: {
                            if (dragActive || down) {
                                qsTr("From")
                            } else if (stationsContainer.currentItem == customStation) {
                                qsTr("To")
                            } else {
                                qsTr("Other station")
                            }
                        }

                        menu: null
                        property int index: -1
                        modelIndex: index
                        listModel: fahrplanBackend.favorites

                        isAlternateHighlight: false
                        alternateHighlightColor: "transparent"

                        visible: allowSelectTwo
                        width: parent.width
                        contentHeight: allowSelectTwo ? Theme.itemSizeExtraLarge : 0

                        tileIcon: !dragActive && stationsContainer.currentItem == customStation ?
                            Qt.resolvedUrl("../images/tile-bg-to.png") :
                            Qt.resolvedUrl("../images/tile-bg-from.png")
                        backgroundImage: ""
                        text: "<b>" + _dynamicLabel + "</b>&nbsp;&nbsp;\u2022 \u2022 \u2022"
                    }
                }

                TileBase {
                    id: showMoreFavoritesButton

                    visible: fahrplanBackend.favorites.count > stationsFlow.maxCount
                    contentHeight: root.height - (stationsContainer.y + y)
                    anchors {
                        top: stationsPickerContainer.bottom
                        left: parent.left
                        right: parent.right
                    }

                    highlightBackgroundPress: true
                    text: qsTr("Show more") + "&nbsp;&nbsp;\u2022 \u2022 \u2022"
                    textLabel {
                        anchors {
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                        }

                        font.pixelSize: Theme.fontSizeMedium
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignRight
                    }

                    onClicked: {
                        root._showFavoritesAsList = true
                    }
                }
            }

            Column {
                width: parent.width

                visible: opacity > 0.0
                opacity: _showFavoritesAsList && !_showingSearchResults ? 1.0 : 0.0
                Behavior on opacity { FadeAnimator {} }

                SectionHeader {
                    text: qsTr("Favorites")
                }

                ViewPlaceholder {
                    id: favoritesHint

                    enabled: !_haveStationTiles
                             && (!_showingSearchResults
                                 || (fahrplanBackend.stationSearchResults.count == 0
                                     && searchString == ""))
                    text: qsTr("Click and hold in the search results to " +
                               "add or remove a station as a favorite")
                }

                ListView {
                    model: fahrplanBackend.favorites
                    width: parent.width
                    height: contentHeight
                    interactive: false

                    currentIndex: -1

                    delegate: StationListDelegate {
                        ListView.onRemove: animateRemoval()

                        onStationSelected: {
                            selectOneStation(ListView.view.model, stationIndex)
                        }
                    }
                }
            }
        }
    }
}
