// This file is part of Fahrplan.
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Mirian Margiani

import QtQuick 2.0
import Sailfish.Silica 1.0
import Fahrplan 1.0

ListItem {
    id: root

    property ListView listView: ListView.view
    property string highlightQuery
    property bool remorseWhenRemovingFavorite: true

    property var _queryRegex: new RegExp(highlightQuery, 'i')
    property bool _isFavorite: model.isFavorite

    signal stationSelected(var stationIndex)

    function _updateIsFavorite() {
        // model.isFavorite doesn't notify changes for some reason;
        // this forces a change signal but keeps the binding.
        // Simply calling _isFavoriteChanged() doesn't help.
        _isFavorite = Qt.binding(function(){ return model.isFavorite })
    }

    function toggleFavorite() {
        if (model.isFavorite) {
            if (remorseWhenRemovingFavorite) {
                remorseDelete(function(){
                    console.log("Removing %1 from favorites".arg(this.id))
                    this.listView.model.removeByIdFromFavorites(this.id)
                    _updateIsFavorite()
                }.bind({id: model.id, listView: listView}))
            } else {
                listView.model.removeByIdFromFavorites(model.id)
                _updateIsFavorite()
            }
        } else {
            listView.model.addToFavorites(model.index)
            _updateIsFavorite()
        }
    }

    width: listView.width
    contentHeight: Theme.itemSizeMedium

    menu: Component {
        ContextMenu {
            id: contextMenu

            MenuItem {
                text: _isFavorite ? qsTr("Remove from favorites") :
                                    qsTr("Add to favorites")
                onClicked: {
                    toggleFavorite()
                }
            }
        }
    }

    onClicked: {
        stationSelected(model.index)
    }

    Row {
        width: parent.width - 2*x
        height: childrenRect.height
        x: Theme.horizontalPageMargin
        anchors.verticalCenter: parent.verticalCenter

        Column {
            height: childrenRect.height
            width: parent.width - spacing - star.width

            Label {
                width: parent.width
                text: Theme.highlightText(model.name,_queryRegex, Theme.highlightColor).
                      replace(/^(.*?,\s*)?(.*?)$/, '$1<b>$2</b>')
            }

            Label {
                width: parent.width
                text: model.miscInfo

                font.pixelSize: Theme.fontSizeSmall
                color: highlighted ? palette.secondaryHighlightColor : palette.secondaryColor
            }
        }

        IconButton {
            id: star

            highlighted: down || root.highlighted

            icon {
                anchors {
                    right: star.right
                    rightMargin: -Theme.paddingMedium
                    verticalCenter: star.verticalCenter
                    centerIn: undefined
                }

                source: _isFavorite ?
                    "image://theme/icon-m-favorite-selected" :
                    "image://theme/icon-m-favorite"
            }

            onClicked: {
                toggleFavorite()
            }
        }
    }
}
