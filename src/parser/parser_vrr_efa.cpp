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
**  with this program.  If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/

#include "parser_vrr_efa.h"

#include <QBuffer>
#include <QDomDocument>
#include <QFile>
#include <QtCore/QUrl>
#if defined(BUILD_FOR_QT5)
    #include <QUrlQuery>
#endif

//http://efa.vrr.de/standard/XML_STOPFINDER_REQUEST?doNotSearchForStops_sf=1&language=en&locationInfoActive=1&locationServerActive=1&name_sf=Solingen&serverInfo=1&type_sf=any

ParserVRREFA::ParserVRREFA(QObject *parent) :
    ParserEFA(parent)
{
    // new    https://www.vrr.de/vrr-efa
    //baseRestUrl = "https://www.vrr.de/vrr-efa/";
    baseRestUrl = "http://efa.vrr.de/standard/";
    acceptEncoding = "gzip";
}

QStringList ParserVRREFA::getTrainRestrictions()
{
    QStringList result;
    result.append(tr("All"));
    result.append(tr("S-Bahn"));
    result.append(tr("U-Bahn"));
    result.append(tr("Tram"));
    result.append(tr("Bus"));
    return result;
}

/*
 * efa in VRR requires some extra params which are ok for münich, but break sydney
*/
void ParserVRREFA::findStationsByName(const QString &stationName)
{
    // http://efa.vrr.de/standard/XML_STOPFINDER_REQUEST?doNotSearchForStops_sf=1&language=en&locationInfoActive=1&locationServerActive=1&name_sf=Solingen&serverInfo=1&type_sf=any

    qDebug() << "ParserEFA::findStationsByName(" <<  stationName << ")";
    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }
    currentRequestState = FahrplanNS::stationsByNameRequest;
    QUrl uri(baseRestUrl + "XML_STOPFINDER_REQUEST");

#if defined(BUILD_FOR_QT5)
    QUrlQuery query;
#else
    QUrl query;
#endif
    query.addQueryItem("language", "en");
    query.addQueryItem("locationServerActive", "1");
    query.addQueryItem("outputFormat", "XML");
    query.addQueryItem("type_sf", "any");  // could be any, poi or stop
    query.addQueryItem("coordOutputFormat","WGS84");
    //doNotSearchForStops_sf=1&language=en&locationInfoActive=1&locationServerActive=1
    // these break sydney
    query.addQueryItem("doNotSearchForStops_sf","1");
    query.addQueryItem("locationInfoActive","1");
    query.addQueryItem("locationServerActive","1");
    query.addQueryItem("name_sf", stationName);

    /*
     * convertAddressesITKernel2LocationServer 1
    convertCoord2LocationServer 1
    convertCrossingsITKernel2LocationServer 1
    convertPOIsITKernel2LocationServer 1
    convertStopsPTKernel2LocationServer 1
    coordOutputFormat WGS84[dd.ddddd]
    doNotSearchForStops_sf 1
    language en
    locationInfoActive 1
    locationServerActive 1
    name_sf Solingen
    outputFormat rapidJSON
    serverInfo 1
    type_sf any
    vrrStopFinderMacro 1
    */

#if defined(BUILD_FOR_QT5)
    uri.setQuery(query);
#else
    uri.setQueryItems(query.queryItems());
#endif
    sendHttpRequest(uri);
    //qDebug() << "search station url:" << uri;
}
