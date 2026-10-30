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

#include "parser_barca.h"

#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <qmath.h>
#include <algorithm>

/*
 * TMB (Barcelona) API constants
 * The estacions endpoint covers metro stations
 * The parades and itransit endpoints are bus specific
 * The planner covers geo coordinate to coordinate.
 * Sigh.
*/
#define BARCA_BASE       "https://api.tmb.cat/v1/"
#define BARCA_PARADES    BARCA_BASE "transit/parades"
#define BARCA_ESTACIONS  BARCA_BASE "transit/estacions"
#define BARCA_ITTRANSIT  BARCA_BASE "itransit/bus/parades/"
#define BARCA_PLANNER    BARCA_BASE "planner/plan"

#define BARCA_APP_ID     "12a8c95b"
#define BARCA_APP_KEY    "77e2f53f847428a05f3c02791c43b0b4"

// Max stations returned for name / GPS search
static const int MAX_STATION_RESULTS = 20;

// Build a QUrlQuery preloaded with the mandatory app_id / app_key auth params.
static QUrlQuery barca_authQuery()
{
    QUrlQuery query;
    query.addQueryItem(QLatin1String("app_id"),  QLatin1String(BARCA_APP_ID));
    query.addQueryItem(QLatin1String("app_key"), QLatin1String(BARCA_APP_KEY));
    return query;
}

/* Haversine distance helper */
inline qreal barca_deg2rad(qreal deg) {
    return deg * 3.141592653589793238463 / 180;
}
inline int barca_distance(qreal lat1, qreal lon1, qreal lat2, qreal lon2) {
    const qreal sdLat = qSin(barca_deg2rad(lat2 - lat1) / 2);
    const qreal sdLon = qSin(barca_deg2rad(lon2 - lon1) / 2);
    const qreal cLat1 = qCos(barca_deg2rad(lat1));
    const qreal cLat2 = qCos(barca_deg2rad(lat2));
    const qreal a     = sdLat * sdLat + sdLon * sdLon * cLat1 * cLat2;
    const qreal c     = 2 * qAtan2(qSqrt(a), qSqrt(1 - a));
    return qRound(6371.0 * c * 1000);
}

/* Helpers */
// Convert an epoch-milliseconds value into a local QTime.
static QTime barca_epochMsToTime(qint64 epochMs)
{
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(epochMs);
    return dt.isValid() ? dt.toLocalTime().time() : QTime();
}

// Build the TMB 12h time token, e.g. "11:58am"
static QString barca_timeToken(const QDateTime &dt)
{
    const QTime t = dt.toLocalTime().time();
    int h12 = t.hour() % 12;
    if (h12 == 0)
        h12 = 12;
    const QString ampm = (t.hour() < 12) ? QLatin1String("am") : QLatin1String("pm");
    return QString(QLatin1String("%1:%2%3"))
        .arg(h12)
        .arg(t.minute(), 2, 10, QChar('0'))
        .arg(ampm);
}

/* Construction / destruction */

ParserBarca::ParserBarca(QObject *parent)
    : ParserAbstract(parent)
    , lastJourneyResultList(NULL)
{
    lastCoordinates.isValid = false;
    stationMergeRequestedEstacions = false;
    stationByCoordinates = false;
}

ParserBarca::~ParserBarca()
{
    clearJourney();
}

void ParserBarca::clearJourney()
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
}

/* Request helper – appends the mandatory app_id / app_key auth parameters */

void ParserBarca::sendBarcaRequest(const QUrl &url)
{
    // The auth parameters are already baked into the URL by the caller.
    sendHttpRequest(url, QByteArray());
}

/* Train restrictions  (TMB = buses / metro, a single catch-all entry) */

QStringList ParserBarca::getTrainRestrictions()
{
    return QStringList() << tr("All");
}

/* Station search – by name */

void ParserBarca::findStationsByName(const QString &stationName)
{
    pendingStationQuery = stationName;
    stationByCoordinates = false;
    stationMergeRequestedEstacions = false;
    stationAccumulator.clear();
    requestStationList(QLatin1String(BARCA_PARADES));
}

void ParserBarca::parseStationsByName(QNetworkReply *networkReply)
{
    handleStationListReply(networkReply);
}

/* Station search – by GPS coordinates */

void ParserBarca::findStationsByCoordinates(qreal longitude, qreal latitude)
{
    lastCoordinates.isValid   = true;
    lastCoordinates.latitude  = latitude;
    lastCoordinates.longitude = longitude;
    stationByCoordinates = true;
    stationMergeRequestedEstacions = false;
    stationAccumulator.clear();
    requestStationList(QLatin1String(BARCA_PARADES));
}

void ParserBarca::parseStationsByCoordinates(QNetworkReply *networkReply)
{
    handleStationListReply(networkReply);
}

/* Merged station search – bus stops (parades) + metro stations (estacions)
 * fetched sequentially, since the base class tracks a single in-flight reply. */

void ParserBarca::requestStationList(const QString &urlPath)
{
    QUrl uri(urlPath);
    uri.setQuery(barca_authQuery());
    sendBarcaRequest(uri);
    currentRequestState = stationByCoordinates
        ? FahrplanNS::stationsByCoordinatesRequest
        : FahrplanNS::stationsByNameRequest;
}

void ParserBarca::handleStationListReply(QNetworkReply *networkReply)
{
    const QByteArray allData = networkReply->readAll();
    const QVariantMap doc    = parseJson(allData);
    if (!doc.isEmpty())
        appendStationsFromDoc(doc);

    if (!stationMergeRequestedEstacions) {
        // First collection (bus stops) received – now fetch the metro stations.
        stationMergeRequestedEstacions = true;
        requestStationList(QLatin1String(BARCA_ESTACIONS));
        return;
    }

    // Second collection received – emit the merged result.
    finalizeStationSearch();
}

void ParserBarca::appendStationsFromDoc(const QVariantMap &doc)
{
    const QVariantList features = doc.value("features").toList();

    foreach (const QVariant &featVar, features) {
        const QVariantMap feat  = featVar.toMap();
        const QVariantMap props = feat.value("properties").toMap();

        // Bus stops expose CODI_PARADA/NOM_PARADA; metro stations expose
        // CODI_GRUP_ESTACIO/NOM_ESTACIO. Namespace the id so the timetable
        // lookup knows which kind of stop this is (and which feed applies).
        QString name;
        QString id;
        QString type;
        if (props.contains(QLatin1String("CODI_PARADA"))) {
            name = props.value("NOM_PARADA").toString();
            id   = QString(QLatin1String("B:%1")).arg(props.value("CODI_PARADA").toString());
            type = props.value("NOM_TIPUS_SIMPLE_PARADA").toString();
        } else if (props.contains(QLatin1String("CODI_GRUP_ESTACIO"))) {
            name = props.value("NOM_ESTACIO").toString();
            id   = QString(QLatin1String("M:%1")).arg(props.value("CODI_GRUP_ESTACIO").toString());
            type = props.value("PICTO").toString();   // metro line, e.g. "L4"
        } else {
            continue;
        }

        if (name.isEmpty())
            continue;
        if (!stationByCoordinates &&
            !name.contains(pendingStationQuery, Qt::CaseInsensitive))
            continue;

        const QVariantMap geom   = feat.value("geometry").toMap();
        const QVariantList coords = geom.value("coordinates").toList();
        if (coords.size() < 2)
            continue;
        const double lon = coords.at(0).toDouble();
        const double lat = coords.at(1).toDouble();

        Station s;
        s.valid = true;
        s.id    = id;
        s.name  = name;
        s.type  = type;
        s.latitude  = lat;
        s.longitude = lon;

        if (stationByCoordinates) {
            const int dist = barca_distance(lastCoordinates.latitude,
                                            lastCoordinates.longitude, lat, lon);
            //: Distance in meters
            s.miscInfo = tr("%n m", "", dist);
        }

        stationAccumulator.append(s);
    }
}

void ParserBarca::finalizeStationSearch()
{
    if (stationByCoordinates) {
        std::sort(stationAccumulator.begin(), stationAccumulator.end(),
                  [this](const Station &a, const Station &b) {
                      return barca_distance(lastCoordinates.latitude, lastCoordinates.longitude,
                                            a.latitude, a.longitude)
                           < barca_distance(lastCoordinates.latitude, lastCoordinates.longitude,
                                            b.latitude, b.longitude);
                  });
    }

    StationsList result;
    int count = 0;
    for (int i = 0; i < stationAccumulator.size() && count < MAX_STATION_RESULTS; ++i, ++count)
        result.append(stationAccumulator.at(i));

    lastCoordinates.isValid = false;
    stationAccumulator.clear();
    stationMergeRequestedEstacions = false;

    if (result.isEmpty())
        emit errorOccured(tr("No stations found"));
    else
        emit stationsResult(result);
}

/* Timetable (live bus arrivals / departures for a stop) */

void ParserBarca::getTimeTableForStation(const Station &currentStation,
                                        const Station &,
                                        const QDateTime &,
                                        ParserAbstract::Mode,
                                        int /*trainrestrictions*/)
{
    if (!currentStation.valid) {
        emit errorOccured(tr("No valid stop selected"));
        return;
    }

    // Station ids are namespaced "B:{codi}" (bus) / "M:{codi}" (metro).
    const QString id      = currentStation.id.toString();
    const int     sep     = id.indexOf(QLatin1Char(':'));
    const QString kind    = sep >= 0 ? id.left(sep) : QString();
    const QString code    = sep >= 0 ? id.mid(sep + 1) : id;

    // Metro stations have no live-arrivals endpoint in the TMB API – show a hint.
    if (kind == QLatin1String("M")) {
        TimetableEntriesList result;
        TimetableEntry hint;
        hint.trainType        = currentStation.type.isEmpty() ? tr("Metro") : currentStation.type;
        hint.destinationStation = tr("Live departures not available for metro stations");
        result.append(hint);
        emit timetableResult(result);
        return;
    }

    QUrl uri(QLatin1String(BARCA_ITTRANSIT));
    uri.setPath(uri.path() + code);
    uri.setQuery(barca_authQuery());
    sendBarcaRequest(uri);
    currentRequestState = FahrplanNS::getTimeTableForStationRequest;
}

void ParserBarca::parseTimeTable(QNetworkReply *networkReply)
{
    TimetableEntriesList result;
    const QByteArray allData = networkReply->readAll();

    const QVariantMap doc = parseJson(allData);
    if (doc.isEmpty()) {
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }

    const QVariantList parades = doc.value("parades").toList();

    foreach (const QVariant &paradeVar, parades) {
        const QVariantMap parade = paradeVar.toMap();
        const QVariantList trajectes = parade.value("linies_trajectes").toList();

        foreach (const QVariant &trajecteVar, trajectes) {
            const QVariantMap trajecte = trajecteVar.toMap();

            const QString line      = trajecte.value("nom_linia").toString();
            const QString desti     = trajecte.value("desti_trajecte").toString();
            const QVariantList busos = trajecte.value("propers_busos").toList();

            // Take the first (soonest) departure for this direction
            if (busos.isEmpty())
                continue;

            const QVariantMap firstBus = busos.first().toMap();
            const qint64 epochMs = firstBus.value("temps_arribada").toLongLong();
            if (epochMs <= 0)
                continue;

            TimetableEntry te;
            te.time  = barca_epochMsToTime(epochMs);
            te.trainType = line;
            te.destinationStation = desti;

            // Highlight the soonest service as "next"
            if (result.isEmpty())
                te.miscInfo = tr("Next");

            result.append(te);
        }
    }

    emit timetableResult(result);
}

/* Journey search */

void ParserBarca::buildPlannerUrl(QUrl &uri, const Station &from, const Station &to,
                                  const QDateTime &dateTime)
{
    uri = QUrl(QLatin1String(BARCA_PLANNER));
    QUrlQuery query = barca_authQuery();
    query.addQueryItem(QLatin1String("fromPlace"),
        QString("%1,%2").arg(from.latitude).arg(from.longitude));
    query.addQueryItem(QLatin1String("toPlace"),
        QString("%1,%2").arg(to.latitude).arg(to.longitude));
    query.addQueryItem(QLatin1String("date"), dateTime.toString(QLatin1String("dd-MM-yyyy")));
    query.addQueryItem(QLatin1String("time"), barca_timeToken(dateTime));
    query.addQueryItem(QLatin1String("mode"), QLatin1String("TRANSIT,WALK"));
    uri.setQuery(query);
}

void ParserBarca::searchJourney(const Station &departureStation,
                                const Station &,
                                const Station &arrivalStation,
                                const QDateTime &dateTime,
                                const ParserAbstract::Mode mode,
                                int trainrestrictions)
{
    lastsearch.from         = departureStation;
    lastsearch.to           = arrivalStation;
    lastsearch.restrictions = trainrestrictions;
    lastsearch.mode         = mode;

    QUrl uri;
    buildPlannerUrl(uri, departureStation, arrivalStation, dateTime);

    clearJourney();
    sendBarcaRequest(uri);
    currentRequestState = FahrplanNS::searchJourneyRequest;
}

void ParserBarca::searchJourneyLater()
{
    if (!lastsearch.lastOption.isValid())
        return;
    searchJourney(lastsearch.from, lastsearch.to, lastsearch.to,
                  lastsearch.lastOption.addSecs(60),
                  lastsearch.mode, lastsearch.restrictions);
}

void ParserBarca::searchJourneyEarlier()
{
    if (!lastsearch.firstOption.isValid())
        return;
    searchJourney(lastsearch.from, lastsearch.to, lastsearch.to,
                  lastsearch.firstOption.addSecs(-30 * 60),
                  lastsearch.mode, lastsearch.restrictions);
}

void ParserBarca::getJourneyDetails(const QString &id)
{
    if (cachedResults.contains(id))
        emit journeyDetailsResult(cachedResults.value(id));
}

/* Delegate all three journey response types to the shared parser */

void ParserBarca::parseSearchJourney(QNetworkReply *networkReply)
{
    parseJourneyResponse(networkReply);
}

void ParserBarca::parseSearchLaterJourney(QNetworkReply *networkReply)
{
    parseJourneyResponse(networkReply);
}

void ParserBarca::parseSearchEarlierJourney(QNetworkReply *networkReply)
{
    parseJourneyResponse(networkReply);
}

void ParserBarca::parseJourneyDetails(QNetworkReply *)
{
    // Details are fully cached during parseJourneyResponse().
}

/* Shared journey-response parser */

void ParserBarca::parseJourneyResponse(QNetworkReply *networkReply)
{
    const QByteArray allData = networkReply->readAll();

    const QVariantMap doc = parseJson(allData);
    if (doc.isEmpty()) {
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }

    const QVariantMap plan = doc.value("plan").toMap();
    const QVariantList itineraries = plan.value("itineraries").toList();
    if (itineraries.isEmpty()) {
        emit errorOccured(tr("No connections have been found that correspond to your request."));
        return;
    }

    lastJourneyResultList = new JourneyResultList();
    lastJourneyResultList->setDepartureStation(lastsearch.from.name);
    lastJourneyResultList->setArrivalStation(lastsearch.to.name);

    QDateTime firstDeparture;

    for (int i = 0; i < itineraries.size(); ++i) {
        const QVariantMap itin = itineraries.at(i).toMap();

        const QVariantList legs = itin.value("legs").toList();
        if (legs.isEmpty())
            continue;

        const QDateTime departure = QDateTime::fromMSecsSinceEpoch(itin.value("startTime").toLongLong()).toLocalTime();
        const QDateTime arrival   = QDateTime::fromMSecsSinceEpoch(itin.value("endTime").toLongLong()).toLocalTime();

        const QString cacheId = QString("%1_%2_%3").arg(departure.toString(Qt::ISODate)).arg(arrival.toString(Qt::ISODate)).arg(i);
        parseJourneyOption(itin, cacheId);

        JourneyResultItem *item = new JourneyResultItem;
        item->setDate(departure.date());
        item->setDepartureTime(departure.toString("HH:mm"));
        item->setArrivalTime(arrival.toString("HH:mm"));
        item->setId(cacheId);

        // Transit line summary (routeShortName of every transit leg)
        QStringList trainLabels;
        foreach (const QVariant &legVar, legs) {
            const QVariantMap leg = legVar.toMap();
            if (leg.value("transitLeg", true).toBool()) {
                const QString label = leg.value("routeShortName").toString();
                if (!label.isEmpty() && !trainLabels.contains(label))
                    trainLabels.append(label);
            }
        }
        item->setTrainType(trainLabels.join(", ").trimmed());

        item->setTransfers(QString::number(itin.value("transfers").toInt()));

        const int totalMins = departure.secsTo(arrival) / 60;
        item->setDuration(QString("%1:%2")
                              .arg(totalMins / 60)
                              .arg(totalMins % 60, 2, 10, QChar('0')));

        lastJourneyResultList->appendItem(item);

        if (i == 0) {
            firstDeparture = departure;
            lastJourneyResultList->setTimeInfo(departure.date().toString());
        }
    }

    lastsearch.firstOption = firstDeparture;
    lastsearch.lastOption  = firstDeparture; // single result set; re-request with same time
    emit journeyResult(lastJourneyResultList);
}

/* Per-itinerary detail caching */

void ParserBarca::parseJourneyOption(const QVariantMap &itin, const QString &cacheId)
{
    JourneyDetailResultList *result = new JourneyDetailResultList();
    result->setId(cacheId);

    const QVariantList legs = itin.value("legs").toList();

    QDateTime journeyDep;
    QDateTime journeyArr;

    foreach (const QVariant &legVar, legs) {
        const QVariantMap leg      = legVar.toMap();
        const QString     mode     = leg.value("mode").toString();

        const QVariantMap from      = leg.value("from").toMap();
        const QVariantMap to        = leg.value("to").toMap();

        const QDateTime legDep = QDateTime::fromMSecsSinceEpoch(leg.value("startTime").toLongLong()).toLocalTime();
        const QDateTime legArr = QDateTime::fromMSecsSinceEpoch(leg.value("endTime").toLongLong()).toLocalTime();

        if (!journeyDep.isValid()) journeyDep = legDep;
        journeyArr = legArr;

        JourneyDetailResultItem *item = new JourneyDetailResultItem();
        item->setDepartureStation(from.value("name").toString());
        item->setArrivalStation(to.value("name").toString());
        item->setDepartureDateTime(legDep);
        item->setArrivalDateTime(legArr);

        if (mode == QLatin1String("WALK")) {
            const int durSecs = leg.value("duration",
                            (leg.value("endTime").toLongLong() - leg.value("startTime").toLongLong()) / 1000).toInt();
            item->setTrain(tr("Walk for %1 min").arg(durSecs / 60));
            item->setDirection(QString());
        } else {
            const QString shortName = leg.value("routeShortName").toString();
            const QString route     = leg.value("route").toString();
            item->setTrain(shortName.isEmpty() ? route : shortName);
            item->setDirection(route);
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
        // The API's outer WALK legs name the endpoints literally "Origin" /
        // "Destination" (the raw GPS point). Show the actual stations the user
        // picked instead, on both the detail rows and the list header.
        QString depName = result->getItem(0)->departureStation();
        QString arrName = result->getItem(result->itemcount() - 1)->arrivalStation();
        if (!lastsearch.from.name.isEmpty())
            depName = lastsearch.from.name;
        if (!lastsearch.to.name.isEmpty())
            arrName = lastsearch.to.name;

        result->getItem(0)->setDepartureStation(depName);
        result->getItem(result->itemcount() - 1)->setArrivalStation(arrName);
        result->setDepartureStation(depName);
        result->setArrivalStation(arrName);
    }

    cachedResults.insert(cacheId, result);
}
