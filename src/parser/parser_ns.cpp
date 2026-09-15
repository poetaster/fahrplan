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

#include "parser_ns.h"

#include <QUrl>
#include <QNetworkReply>
#ifdef BUILD_FOR_QT5
#   include <QUrlQuery>
#else
#   define setQuery(q) setQueryItems(q.queryItems())
#endif
#include <qmath.h>
#include <algorithm>

// ---------------------------------------------------------------------------
// Register at https://apiportal.ns.nl/ to obtain a free API key.
// ---------------------------------------------------------------------------

#define NS_BASE_URL  "https://gateway.apiportal.ns.nl/reisinformatie-api/api/"
#define NS_STATIONS  NS_BASE_URL "v2/stations"
#define NS_DEPARTURES NS_BASE_URL "v2/departures"
#define NS_ARRIVALS   NS_BASE_URL "v2/arrivals"
#define NS_TRIPS     NS_BASE_URL "v3/trips"

// Max stations returned for name / GPS search
static const int MAX_STATION_RESULTS = 20;

// ---------------------------------------------------------------------------
// Haversine distance helper
// ---------------------------------------------------------------------------
inline qreal ns_deg2rad(qreal deg) {
    return deg * 3.141592653589793238463 / 180;
}
inline int ns_distance(qreal lat1, qreal lon1, qreal lat2, qreal lon2) {
    const qreal sdLat = qSin(ns_deg2rad(lat2 - lat1) / 2);
    const qreal sdLon = qSin(ns_deg2rad(lon2 - lon1) / 2);
    const qreal cLat1 = qCos(ns_deg2rad(lat1));
    const qreal cLat2 = qCos(ns_deg2rad(lat2));
    const qreal a     = sdLat * sdLat + sdLon * sdLon * cLat1 * cLat2;
    const qreal c     = 2 * qAtan2(qSqrt(a), qSqrt(1 - a));
    return qRound(6371.0 * c * 1000);
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

ParserNs::ParserNs(QObject *parent)
    : ParserAbstract(parent)
    , lastJourneyResultList(NULL)
    , apiKey(QLatin1String("e451f9e5b6324cc6a562e1e7ff1ea0a6"))
{
    lastCoordinates.isValid = false;
}

ParserNs::~ParserNs()
{
    clearJourney();
}

void ParserNs::clearJourney()
{
    if (lastJourneyResultList) {
        delete lastJourneyResultList;
        lastJourneyResultList = NULL;
    }
    for (QMap<QString, JourneyDetailResultList *>::Iterator it = cachedResults.begin();
         it != cachedResults.end(); ) {
        JourneyDetailResultList *jdrl = it.value();
        it = cachedResults.erase(it);
        delete jdrl;
    }
    forwardContext.clear();
    backwardContext.clear();
}

// ---------------------------------------------------------------------------
// NS request helper (adds Ocp-Apim-Subscription-Key header)
// ---------------------------------------------------------------------------

void ParserNs::sendNsRequest(const QUrl &url)
{
    QList<QPair<QByteArray, QByteArray> > headers;
    headers << qMakePair(QByteArray("Ocp-Apim-Subscription-Key"), apiKey.toLatin1());
    // Passing a null QByteArray triggers GET in sendHttpRequest()
    sendHttpRequest(url, QByteArray(), headers);
}

// ---------------------------------------------------------------------------
// Train restrictions  (NS = trains only, one entry suffices)
// ---------------------------------------------------------------------------

QStringList ParserNs::getTrainRestrictions()
{
    return QStringList() << tr("All trains");
}

// ===========================================================================
// Station search – by name
// ===========================================================================

void ParserNs::findStationsByName(const QString &stationName)
{
    pendingStationQuery = stationName;
    sendNsRequest(QUrl(QLatin1String(NS_STATIONS)));
    currentRequestState = FahrplanNS::stationsByNameRequest;
}

void ParserNs::parseStationsByName(QNetworkReply *networkReply)
{
    parseStationsPayload(networkReply, false);
}

// ===========================================================================
// Station search – by GPS coordinates
// ===========================================================================

void ParserNs::findStationsByCoordinates(qreal longitude, qreal latitude)
{
    lastCoordinates.isValid   = true;
    lastCoordinates.latitude  = latitude;
    lastCoordinates.longitude = longitude;
    sendNsRequest(QUrl(QLatin1String(NS_STATIONS)));
    currentRequestState = FahrplanNS::stationsByCoordinatesRequest;
}

void ParserNs::parseStationsByCoordinates(QNetworkReply *networkReply)
{
    parseStationsPayload(networkReply, true);
}

// ---------------------------------------------------------------------------
// Shared stations parser  (both name and GPS search use the same endpoint)
// ---------------------------------------------------------------------------

void ParserNs::parseStationsPayload(QNetworkReply *networkReply, bool byCoordinates)
{
    const QByteArray allData = networkReply->readAll();
    const QVariantMap doc    = parseJson(allData);

    if (doc.isEmpty()) {
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }
    if (doc.contains("error")) {
        emit errorOccured(doc.value("message", tr("Unknown error")).toString());
        return;
    }

    const QVariantList payload = doc.value("payload").toList();

    // Build raw list of stations
    QList<QPair<int, Station> > withDistance; // (distance, station)

    foreach (const QVariant &stVar, payload) {
        const QVariantMap st = stVar.toMap();

        // Skip stations without departure times (small halts / service stops)
        if (!st.value("heeftVertrektijden", true).toBool())
            continue;

        const QString code = st.value("code").toString();
        const QString name = st.value("namen").toMap().value("lang").toString();

        if (byCoordinates) {
            // Filter by distance; include any station within reasonable radius
        } else {
            // Filter by name (case-insensitive contains)
            if (!name.contains(pendingStationQuery, Qt::CaseInsensitive))
                continue;
        }

        Station s;
        s.id   = code;
        s.name = name;

        bool okLat, okLon;
        const double lat = st.value("lat").toDouble(&okLat);
        const double lon = st.value("lng").toDouble(&okLon);
        if (okLat && okLon) {
            s.latitude  = lat;
            s.longitude = lon;
        }

        int dist = 0;
        if (byCoordinates && okLat && okLon) {
            dist = ns_distance(lastCoordinates.latitude, lastCoordinates.longitude, lat, lon);
            //: Distance in meters
            s.miscInfo = tr("%n m", "", dist);
        }

        withDistance.append(qMakePair(dist, s));
    }

    // Sort by distance (ascending) for GPS search; name search keeps natural order
    if (byCoordinates) {
        std::sort(withDistance.begin(), withDistance.end(),
                  [](const QPair<int,Station> &a, const QPair<int,Station> &b) {
                      return a.first < b.first;
                  });
    }

    StationsList result;
    int count = 0;
    for (int i = 0; i < withDistance.size() && count < MAX_STATION_RESULTS; ++i, ++count)
        result.append(withDistance.at(i).second);

    lastCoordinates.isValid = false;
    emit stationsResult(result);
}

// ===========================================================================
// Timetable (departure board)
// ===========================================================================

void ParserNs::getTimeTableForStation(const Station &currentStation,
                                       const Station &,
                                       const QDateTime &dateTime,
                                       ParserAbstract::Mode mode,
                                       int /*trainrestrictions*/)
{
    // Convert to UTC with explicit 'Z' suffix – the NS API interprets bare local-time
    // strings as UTC, which causes a 1-2 h shift in CEST and returns wrong/empty results.
    const QDateTime utcDt = (dateTime.isValid() ? dateTime
                                                : QDateTime::currentDateTime()).toUTC();

    timetableMode        = mode;
    timetableStationName = currentStation.name;

    // arrivals and departures are separate NS API endpoints
    const bool isArrival = (mode == Arrival);
    QUrl uri(QLatin1String(isArrival ? NS_ARRIVALS : NS_DEPARTURES));
#if defined(BUILD_FOR_QT5)
    QUrlQuery query;
#else
    QUrl query;
#endif
    query.addQueryItem("station",  currentStation.id.toString());
    query.addQueryItem("dateTime", utcDt.toString(Qt::ISODate)); // yields "…Z"
    uri.setQuery(query);

    sendNsRequest(uri);
    currentRequestState = FahrplanNS::getTimeTableForStationRequest;
}

void ParserNs::parseTimeTable(QNetworkReply *networkReply)
{
    TimetableEntriesList result;
    const QByteArray allData = networkReply->readAll();
    qDebug() << "NS timetable REPLY:" << allData;

    const QVariantMap doc = parseJson(allData);

    if (doc.isEmpty()) {
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }

    const bool hasError = doc.contains("error")
        || (doc.contains("statusCode") && doc.value("statusCode").toInt() >= 400)
        || (doc.contains("status")     && doc.value("status").toInt()     >= 400);
    if (hasError) {
        emit errorOccured(doc.value("message", tr("Unknown error from server")).toString());
        return;
    }

    const QVariantMap payload = doc.value("payload").toMap();

    // arrivals endpoint uses "arrivals" key; departures uses "departures"
    const bool isArrival = (timetableMode == Arrival);
    const QVariantList entries = isArrival ? payload.value("arrivals").toList()
                                           : payload.value("departures").toList();

    qDebug() << "NS timetable count:" << entries.size() << (isArrival ? "(arrivals)" : "(departures)");

    foreach (const QVariant &entryVar, entries) {
        const QVariantMap entry = entryVar.toMap();

        if (entry.value("cancelled", false).toBool())
            continue;

        TimetableEntry te;
        te.currentStation = timetableStationName;

        if (isArrival) {
            // For arrivals: "origin" is where the train comes from
            te.destinationStation = entry.value("origin").toString();
        } else {
            // For departures: "direction" is where the train is going
            te.destinationStation = entry.value("direction").toString();
        }

        // Both arrivals and departures use plannedDateTime / actualDateTime
        const QString plannedStr = entry.value("plannedDateTime").toString();
        const QDateTime planned  = QDateTime::fromString(plannedStr, Qt::ISODate);
        if (planned.isValid())
            te.time = planned.toLocalTime().time();
        else
            te.time = QTime::fromString(plannedStr.mid(11, 5), "HH:mm");

        const QString actualStr = entry.value("actualDateTime").toString();
        if (!actualStr.isEmpty()) {
            const QDateTime actual = QDateTime::fromString(actualStr, Qt::ISODate);
            if (actual.isValid() && actual > planned) {
                const int delayMins = planned.secsTo(actual) / 60;
                te.miscInfo = QString(QLatin1String("<span style=\"color:#b30;\">+%1 min</span>"))
                                  .arg(delayMins);
            } else {
                te.miscInfo = tr("On-Time");
            }
        }

        const QString actualTrack  = entry.value("actualTrack").toString();
        const QString plannedTrack = entry.value("plannedTrack").toString();
        te.platform = actualTrack.isEmpty() ? plannedTrack : actualTrack;

        te.trainType = entry.value("name").toString();

        result.append(te);
    }

    emit timetableResult(result);
}

// ===========================================================================
// Journey search
// ===========================================================================

void ParserNs::searchJourney(const Station &departureStation,
                               const Station &viaStation,
                               const Station &arrivalStation,
                               const QDateTime &dateTime,
                               const ParserAbstract::Mode mode,
                               int trainrestrictions)
{
    lastsearch.from         = departureStation;
    lastsearch.to           = arrivalStation;
    lastsearch.via          = viaStation;
    lastsearch.restrictions = trainrestrictions;
    lastsearch.mode         = mode;

    QUrl uri(QLatin1String(NS_TRIPS));
#if defined(BUILD_FOR_QT5)
    QUrlQuery query;
#else
    QUrl query;
#endif
    query.addQueryItem("fromStation",       departureStation.id.toString());
    query.addQueryItem("toStation",         arrivalStation.id.toString());
    if (viaStation.valid)
        query.addQueryItem("viaStation",    viaStation.id.toString());
    // Use UTC with Z suffix to avoid CEST/CET ambiguity on the NS API server
    query.addQueryItem("dateTime",          dateTime.toUTC().toString(Qt::ISODate));
    query.addQueryItem("searchForArrival",  mode == Arrival ? "true" : "false");
    uri.setQuery(query);

    clearJourney();
    sendNsRequest(uri);
    currentRequestState = FahrplanNS::searchJourneyRequest;
}

void ParserNs::searchJourneyLater()
{
    if (forwardContext.isEmpty()) {
        searchJourney(lastsearch.from, lastsearch.via, lastsearch.to,
                      lastsearch.lastOption.addSecs(60),
                      Departure, lastsearch.restrictions);
        return;
    }

    QUrl uri(QLatin1String(NS_TRIPS));
#if defined(BUILD_FOR_QT5)
    QUrlQuery query;
#else
    QUrl query;
#endif
    query.addQueryItem("fromStation",      lastsearch.from.id.toString());
    query.addQueryItem("toStation",        lastsearch.to.id.toString());
    if (lastsearch.via.valid)
        query.addQueryItem("viaStation",   lastsearch.via.id.toString());
    query.addQueryItem("searchForArrival", lastsearch.mode == Arrival ? "true" : "false");
    query.addQueryItem("context",          forwardContext);
    uri.setQuery(query);

    clearJourney();
    sendNsRequest(uri);
    currentRequestState = FahrplanNS::searchJourneyLaterRequest;
}

void ParserNs::searchJourneyEarlier()
{
    if (backwardContext.isEmpty()) {
        searchJourney(lastsearch.from, lastsearch.via, lastsearch.to,
                      lastsearch.firstOption.addSecs(-30 * 60),
                      Departure, lastsearch.restrictions);
        return;
    }

    QUrl uri(QLatin1String(NS_TRIPS));
#if defined(BUILD_FOR_QT5)
    QUrlQuery query;
#else
    QUrl query;
#endif
    query.addQueryItem("fromStation",      lastsearch.from.id.toString());
    query.addQueryItem("toStation",        lastsearch.to.id.toString());
    if (lastsearch.via.valid)
        query.addQueryItem("viaStation",   lastsearch.via.id.toString());
    query.addQueryItem("searchForArrival", lastsearch.mode == Arrival ? "true" : "false");
    query.addQueryItem("context",          backwardContext);
    uri.setQuery(query);

    clearJourney();
    sendNsRequest(uri);
    currentRequestState = FahrplanNS::searchJourneyEarlierRequest;
}

void ParserNs::getJourneyDetails(const QString &id)
{
    if (cachedResults.contains(id))
        emit journeyDetailsResult(cachedResults.value(id));
}

// ---------------------------------------------------------------------------
// Delegate all three response types to the shared parser
// ---------------------------------------------------------------------------

void ParserNs::parseSearchJourney(QNetworkReply *networkReply)
{
    parseJourneyResponse(networkReply);
}

void ParserNs::parseSearchLaterJourney(QNetworkReply *networkReply)
{
    parseJourneyResponse(networkReply);
}

void ParserNs::parseSearchEarlierJourney(QNetworkReply *networkReply)
{
    parseJourneyResponse(networkReply);
}

void ParserNs::parseJourneyDetails(QNetworkReply *)
{
    // Details are fully cached during parseJourneyResponse().
}

// ---------------------------------------------------------------------------
// Shared journey-response parser
// ---------------------------------------------------------------------------

void ParserNs::parseJourneyResponse(QNetworkReply *networkReply)
{
    const QByteArray  allData = networkReply->readAll();
    qDebug() << "NS trips REPLY:" << allData;

    const QVariantMap doc = parseJson(allData);
    if (doc.isEmpty()) {
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }

    // NS error responses contain a "message" field and a numeric "status"
    if (doc.contains("status") && doc.value("status").toInt() >= 400) {
        emit errorOccured(doc.value("message", tr("Unknown error")).toString());
        return;
    }

    const QVariantList trips = doc.value("trips").toList();
    if (trips.isEmpty()) {
        emit errorOccured(tr("No connections have been found that correspond to your request."));
        return;
    }

    // Pagination contexts
    forwardContext  = doc.value("scrollRequestForwardContext").toString();
    backwardContext = doc.value("scrollRequestBackwardContext").toString();

    lastJourneyResultList = new JourneyResultList();
    lastJourneyResultList->setDepartureStation(lastsearch.from.name);
    lastJourneyResultList->setArrivalStation(lastsearch.to.name);

    QDateTime firstDeparture;
    QDateTime lastDeparture;

    for (int i = 0; i < trips.size(); ++i) {
        const QVariantMap trip = trips.at(i).toMap();

        // Skip cancelled trips
        if (trip.value("status").toString() == QLatin1String("CANCELLED"))
            continue;

        const QVariantList legs = trip.value("legs").toList();
        if (legs.isEmpty())
            continue;

        // Departure = origin of first leg, Arrival = destination of last leg
        const QVariantMap firstLeg = legs.first().toMap();
        const QVariantMap lastLeg  = legs.last().toMap();

        const QString depStr = firstLeg.value("origin").toMap()
                                        .value("plannedDateTime").toString();
        const QString arrStr = lastLeg.value("destination").toMap()
                                       .value("plannedDateTime").toString();
        const QDateTime departure = QDateTime::fromString(depStr, Qt::ISODate).toLocalTime();
        const QDateTime arrival   = QDateTime::fromString(arrStr, Qt::ISODate).toLocalTime();

        const QString cacheId = QString("%1_%2_%3").arg(depStr).arg(arrStr).arg(i);
        parseJourneyOption(trip, cacheId);

        JourneyResultItem *item = new JourneyResultItem;
        item->setDate(departure.date());
        item->setDepartureTime(departure.toString("HH:mm"));
        item->setArrivalTime(arrival.toString("HH:mm"));
        item->setId(cacheId);

        // Train type summary from PUBLIC_TRANSIT legs
        QStringList trainLabels;
        foreach (const QVariant &legVar, legs) {
            const QVariantMap leg = legVar.toMap();
            if (leg.value("travelType").toString() == QLatin1String("PUBLIC_TRANSIT")) {
                const QString label = leg.value("product").toMap()
                                         .value("shortCategoryName").toString();
                if (!label.isEmpty())
                    trainLabels.append(label);
            }
        }
        trainLabels.removeDuplicates();
        item->setTrainType(trainLabels.join(", ").trimmed());

        item->setTransfers(QString::number(trip.value("transfers").toInt()));

        const int totalMins = departure.secsTo(arrival) / 60;
        item->setDuration(QString("%1:%2")
                              .arg(totalMins / 60)
                              .arg(totalMins % 60, 2, 10, QChar('0')));

        lastJourneyResultList->appendItem(item);

        if (i == 0) {
            firstDeparture = departure;
            lastJourneyResultList->setTimeInfo(departure.date().toString());
        }
        lastDeparture = departure;
    }

    lastsearch.firstOption = firstDeparture;
    lastsearch.lastOption  = lastDeparture;

    emit journeyResult(lastJourneyResultList);
}

// ---------------------------------------------------------------------------
// Per-trip detail caching
// ---------------------------------------------------------------------------

void ParserNs::parseJourneyOption(const QVariantMap &trip, const QString &cacheId)
{
    JourneyDetailResultList *result = new JourneyDetailResultList();
    result->setId(cacheId);

    const QVariantList legs = trip.value("legs").toList();

    QDateTime journeyDep;
    QDateTime journeyArr;

    foreach (const QVariant &legVar, legs) {
        const QVariantMap leg      = legVar.toMap();
        const QString     travelType = leg.value("travelType").toString();

        const QVariantMap origin      = leg.value("origin").toMap();
        const QVariantMap destination = leg.value("destination").toMap();

        const QDateTime legDep = QDateTime::fromString(
            origin.value("plannedDateTime").toString(), Qt::ISODate).toLocalTime();
        const QDateTime legArr = QDateTime::fromString(
            destination.value("plannedDateTime").toString(), Qt::ISODate).toLocalTime();

        if (!journeyDep.isValid()) journeyDep = legDep;
        journeyArr = legArr;

        // Skip pure transfer sections (same station, no vehicle)
        if (travelType == QLatin1String("TRANSFER"))
            continue;

        JourneyDetailResultItem *item = new JourneyDetailResultItem();
        item->setDepartureStation(origin.value("name").toString());
        item->setArrivalStation(destination.value("name").toString());
        item->setDepartureDateTime(legDep);
        item->setArrivalDateTime(legArr);

        if (travelType == QLatin1String("PUBLIC_TRANSIT")) {
            const QVariantMap product = leg.value("product").toMap();
            // e.g. "IC 1234" or "SPR 5678"
            const QString trainName = leg.value("name").toString();
            item->setTrain(trainName.isEmpty()
                           ? product.value("shortCategoryName").toString()
                           : trainName);
            item->setDirection(leg.value("direction").toString());

            // Platforms from origin / destination
            const QString depTrack = origin.value("actualTrack",
                                       origin.value("plannedTrack")).toString();
            const QString arrTrack = destination.value("actualTrack",
                                       destination.value("plannedTrack")).toString();
            if (!depTrack.isEmpty())
                item->setDepartureInfo(tr("Pl. %1").arg(depTrack));
            if (!arrTrack.isEmpty())
                item->setArrivalInfo(tr("Pl. %1").arg(arrTrack));

            // Delay info from actual vs planned departure
            const QDateTime actualDep = QDateTime::fromString(
                origin.value("actualDateTime").toString(), Qt::ISODate).toLocalTime();
            if (actualDep.isValid() && actualDep > legDep) {
                const int delayMins = legDep.secsTo(actualDep) / 60;
                item->setInfo(QString(QLatin1String(
                    "<span style=\"color:#b30;\">+%1 min</span>")).arg(delayMins));
            }
        } else {
            // WALK or unknown
            const int durSecs = leg.value("durationInSeconds",
                                           leg.value("duration", 0)).toInt();
            item->setTrain(tr("Walk for %1 min").arg(durSecs / 60));
        }

        result->appendItem(item);
    }

    result->setDepartureDateTime(journeyDep);
    result->setArrivalDateTime(journeyArr);

    if (journeyDep.isValid() && journeyArr.isValid()) {
        const int totalMins = journeyDep.secsTo(journeyArr) / 60;
        result->setDuration(QString("%1:%2")
                                .arg(totalMins / 60)
                                .arg(totalMins % 60, 2, 10, QChar('0')));
    }

    if (result->itemcount() > 0) {
        result->setDepartureStation(result->getItem(0)->departureStation());
        result->setArrivalStation(
            result->getItem(result->itemcount() - 1)->arrivalStation());
    }

    cachedResults.insert(cacheId, result);
}
