/*
 * This file is part of fahrplan.
 * Copyright (C) 2026 <blueprint@poetaster.de> Mark Washeim
 *
 * Fahrplan is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fahrplan is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with harbour-dwd.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

import QtQuick 2.6
import Sailfish.Silica 1.0
import QtPositioning 5.4
import Sailfish.WebView 1.0
import Sailfish.WebEngine 1.0


Page {
    property bool debug: true
    property string lat
    property string lon
    property string dailyDate
    property string name
    property string rurl
    property string location

    Component.onCompleted: {

            if (debug) console.debug("comp.comp")
            if (debug) console.debug(lat)
            if (debug) console.debug(lon)
            webview.url =Qt.resolvedUrl("https://www.openbusmap.org/#" + lon + ";" + lat + ";15")
            //webview.runJavaScript("return doBasicSearchClick('q')")
            //webview.url = Qt.resolvedUrl("https://api.transitous.org/#15/"+lat+"/"+lon)
            //webview.url = Qt.resolvedUrl("https://www.flosm.org/en/publictransport.html?lat=" + lat +"&lon=" + lon + "&r=1612.0581&st=0&sw=busroute,busstation,busstop,ferryroute,ferrystop,ferryterminal,lightrailroute,lightrailstop,railstop,station,subwayentrance,subwayroute,subwaystop,trainroute,tramroute,tramstop,trolleyroute")

    }

    onStatusChanged: {

        if (PageStatus.Activating) {
            rurl="https://www.openbusmap.org/#2.1547;41.3793;14"
            if (debug) console.debug("activate")
            if (debug) console.debug(lat)
            if (debug) console.debug(lon)
         }

        /*
        switch (status) {
            case PageStatus.Activating:
                indicator.visible = true;
                errorMsg.visible = false;
                break;
            case PageStatus.Deactivating:
                errorMsg.visible = false;
                break;
        }
        */
    }
    PositionSource {
        id: positionSource
        active: false
        onPositionChanged: {
            var coord = positionSource.position.coordinate;
            if (debug) console.log("Coordinate:", coord.longitude, coord.latitude);
            //lat = coord.latitude;
            //lon = coord.longitude;
        }

        onSourceErrorChanged: {
            if (sourceError == PositionSource.NoError)
                return
            if (debug) console.log("Source error: " + sourceError)
            //activityText.fadeOut = true
            //stop()
        }
    }

    id:radarView
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent

        WebView {
            id: webview
            anchors.fill: parent
            /* This will probably be required from 4.4 on. */
            Component.onCompleted: {
                WebEngineSettings.pixelRatio = 3
                WebEngineSettings.setPreference("security.disable_cors_checks", true, WebEngineSettings.BoolPref)
                WebEngineSettings.setPreference("security.fileuri.strict_origin_policy", false, WebEngineSettings.BoolPref)
            }

            //https://api.transitous.org/#15/41.35255/2.08155
            url: Qt.resolvedUrl("https://api.transitous.org/#15/"+lat+"/"+lon)
            //url: Qt.resolvedUrl("https://www.flosm.org/en/publictransport.html?lat=" + lat +"&lon=" + lon + "&r=1612.0581&st=0&sw=busroute,busstation,busstop,ferryroute,ferrystop,ferryterminal,lightrailroute,lightrailstop,railstop,station,subwayentrance,subwayroute,subwaystop,trainroute,tramroute,tramstop,trolleyroute")
            //url: Qt.resolvedUrl("https://www.openbusmap.org")

            onViewInitialized: {
                //webview.loadFrameScript(Qt.resolvedUrl("../html/framescript.js"));
                //webview.addMessageListener("webview:action");
            }

            on_PageOrientationChanged: {
                //if ( data.topic !== lon ) {
                //        webview.runJavaScript("return latlon('" + lat + "','" + lon + "')");
                //}

            }

            onRecvAsyncMessage: {
                if (debug) console.debug(message)
                //webview.runJavaScript("return latlon('" + lat + "','" + lon + "')");
                switch (message) {
                case "embed:contentOrientationChanged":
                    break
                case "webview:action":
                    if ( data.topic != lon ) {
                        webview.runJavaScript("return latlon('" + lat + "','" + lon + "')");
                        if (debug) console.debug(data.topic)
                    }
                    break
                }
            }
        }


    }

}
