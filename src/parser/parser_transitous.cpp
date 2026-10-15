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

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

#include "parser_transitous.h"

// Transitous (MOTIS 2 API) is a worldwide GTFS-RT based journey planner,
// see https://transitous.org. Its usage policy requires a User-Agent
// containing the application name and a way of contacting us.
// https://api.transitous.org/api/v1/geocode?text=Berlin fetches stations
static const QString BASE_URL = "https://api.transitous.org/api/v6/";
static const QString GEOCODE_URL = "https://api.transitous.org/api/v1/geocode";
static const int STOP_SEARCH_RADIUS_M = 100;
static const int DEPARTURES_COUNT = 30;

ParserTransitous::ParserTransitous(QObject *parent)
    : ParserAbstract(parent),
      m_timetableRestriction(0),
      m_journeyRestriction(0)
{
    userAgent = "Fahrplan2.0 (+https://github.com/poetaster/fahrplan)";
}

bool ParserTransitous::supportsGps()
{
    return true;
}

bool ParserTransitous::supportsVia()
{
    return true;
}

bool ParserTransitous::supportsTimeTable()
{
    return true;
}

bool ParserTransitous::supportsTimeTableDirection()
{
    return false;
}

QStringList ParserTransitous::getTrainRestrictions()
{
    QStringList result;
    result.append(tr("All"));
    result.append(tr("Bus"));
    result.append(tr("Tram"));
    result.append(tr("Metro"));
    result.append(tr("Ferry"));
    result.append(tr("Train"));
    return result;
}

bool ParserTransitous::isTransitMode(const QString &mode)
{
    return mode != "WALK" && mode != "CYCLE";
}

bool ParserTransitous::modeMatchesRestriction(const QString &mode, int restriction)
{
    switch (restriction) {
    case 1: // Bus
        return mode == "BUS" || mode == "COACH";
    case 2: // Tram
        return mode == "TRAM" || mode == "CABLE_CAR" || mode == "FUNICULAR";
    case 3: // Metro
        return mode == "SUBWAY" || mode == "METRO";
    case 4: // Ferry
        return mode == "FERRY";
    case 5: // Train
        return mode.endsWith("RAIL") || mode == "LONG_DISTANCE" || mode == "SUBURBAN";
    default:
        return true;
    }
}

bool ParserTransitous::itineraryMatchesRestriction(const QVariantMap &itinerary, int restriction)
{
    if (restriction == 0) {
        return true;
    }

    QVariantList legs = itinerary.value("legs").toList();
    Q_FOREACH (const QVariant& legVar, legs) {
        QVariantMap leg = legVar.toMap();
        QString mode = leg.value("mode").toString();
        if (isTransitMode(mode) && modeMatchesRestriction(mode, restriction)) {
            return true;
        }
    }
    return false;
}

QDateTime ParserTransitous::placeTime(const QVariantMap &place, bool departure)
{
    const QString scheduledKey = departure ? "scheduledDeparture" : "scheduledArrival";
    const QString realtimeKey = departure ? "departure" : "arrival";

    QDateTime dt = QDateTime::fromString(place.value(scheduledKey).toString(), Qt::ISODate);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(place.value(realtimeKey).toString(), Qt::ISODate);
    }
    if (dt.isValid()) {
        dt = dt.toLocalTime();
    }
    return dt;
}

QString ParserTransitous::placeName(const QVariantMap &place)
{
    QString name = place.value("name").toString();
    if (name.isEmpty()) {
        name = place.value("description").toString();
    }
    if (name.isEmpty()) {
        name = tr("Unknown stop");
    }
    return name;
}

QString ParserTransitous::lineName(const QVariantMap &leg)
{
    QString name = leg.value("routeShortName").toString();
    if (name.isEmpty()) {
        name = leg.value("displayName").toString();
    }
    if (name.isEmpty()) {
        name = leg.value("tripShortName").toString();
    }
    return name;
}

QString ParserTransitous::coordString(const Station &station)
{
    return QString::number(station.latitude, 'f', 6) + "," +
           QString::number(station.longitude, 'f', 6);
}

QUrl ParserTransitous::planUrl(const Station &from, const Station &via, const Station &to,
                               const QDateTime &dateTime, const QString &pageCursor) const
{
    QUrl url(BASE_URL + "plan");
    QUrlQuery query;
    query.addQueryItem("detailedTransfers", "false");
    query.addQueryItem("fromPlace", coordString(from));
    query.addQueryItem("toPlace", coordString(to));
    if (via.valid && via.latitude != 0.0 && via.longitude != 0.0) {
        query.addQueryItem("viaPlace", coordString(via));
    }
    if (!dateTime.isNull()) {
        query.addQueryItem("time", dateTime.toUTC().toString(Qt::ISODate));
    }
    if (!pageCursor.isEmpty()) {
        query.addQueryItem("pageCursor", pageCursor);
    }
    url.setQuery(query);
    return url;
}

QUrl ParserTransitous::stoptimesUrl(const Station &station) const
{
    QUrl url(BASE_URL + "stoptimes");
    QUrlQuery query;
    query.addQueryItem("fetchStops", "true");
    query.addQueryItem("center", coordString(station));
    query.addQueryItem("radius", QString::number(STOP_SEARCH_RADIUS_M));
    query.addQueryItem("n", QString::number(DEPARTURES_COUNT));
    url.setQuery(query);
    return url;
}

QUrl ParserTransitous::stopsUrl(const Station &station) const
{
    QUrl url(BASE_URL + "stops");
    QUrlQuery query;
    query.addQueryItem("center", coordString(station));
    query.addQueryItem("radius", QString::number(STOP_SEARCH_RADIUS_M));
    url.setQuery(query);
    return url;
}

QUrl ParserTransitous::geocodeUrl(const QString &text) const
{
    QUrl url(GEOCODE_URL);
    QUrlQuery query;
    query.addQueryItem("text", text);
    url.setQuery(query);
    return url;
}

QString ParserTransitous::stationTypeFromModes(const QVariantList &modes)
{
    bool hasTrain = false;
    bool hasTram = false;
    bool hasFerry = false;
    Q_FOREACH (const QVariant& modeVar, modes) {
        QString mode = modeVar.toString();
        if (mode.endsWith("RAIL") || mode == "LONG_DISTANCE" || mode == "SUBURBAN"
            || mode == "SUBWAY" || mode == "METRO" || mode == "MONORAIL") {
            hasTrain = true;
        } else if (mode == "TRAM" || mode == "CABLE_CAR" || mode == "FUNICULAR") {
            hasTram = true;
        } else if (mode == "FERRY") {
            hasFerry = true;
        }
    }
    if (hasTrain) {
        return "train";
    } else if (hasTram) {
        return "tram";
    } else if (hasFerry) {
        return "ferry";
    }
    return "bus";
}

void ParserTransitous::findStationsByName(const QString &stationName)
{
    if (stationName.trimmed().isEmpty()) {
        return;
    }
    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    currentRequestState = FahrplanNS::stationsByNameRequest;
    sendHttpRequest(geocodeUrl(stationName.trimmed()));
}

void ParserTransitous::findStationsByCoordinates(qreal longitude, qreal latitude)
{
    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    Station station;
    station.valid = true;
    station.latitude = latitude;
    station.longitude = longitude;
    station.name = tr("Nearest stop");

    currentRequestState = FahrplanNS::stationsByCoordinatesRequest;
    sendHttpRequest(stopsUrl(station));
}

void ParserTransitous::parseStationsByCoordinates(QNetworkReply *networkReply)
{
    QByteArray allData(networkReply->readAll());
    QVariantMap doc = parseJson(allData);

    QVariantMap place = doc.value("place").toMap();
    if (place.isEmpty()) {
        qDebug() << "Invalid reply:" << allData;
        emit errorOccured(tr("No stop found near your position."));
        return;
    }

    Station station;
    station.valid = true;
    station.id = place.value("stopId");
    station.name = placeName(place);
    station.latitude = place.value("lat").toDouble();
    station.longitude = place.value("lon").toDouble();

    QVariantList modes = place.value("modes").toList();
    station.type = stationTypeFromModes(modes);

    QStringList routeNames;
    QVariantList routes = doc.value("routes").toList();
    Q_FOREACH (const QVariant& routeVar, routes) {
        QString shortName = routeVar.toMap().value("routeShortName").toString();
        if (!shortName.isEmpty() && !routeNames.contains(shortName)) {
            routeNames.append(shortName);
        }
    }
    if (!routeNames.isEmpty()) {
        station.miscInfo = routeNames.join(" / ");
    }

    StationsList results;
    results.append(station);
    emit stationsResult(results);
}

void ParserTransitous::parseStationsByName(QNetworkReply *networkReply)
{
    QByteArray allData(networkReply->readAll());
    QVariantList doc = QJsonDocument::fromJson(allData).toVariant().toList();

    if (doc.isEmpty()) {
        qDebug() << "Invalid reply:" << allData;
        emit errorOccured(tr("No stations found for your search."));
        return;
    }

    StationsList results;
    Q_FOREACH (const QVariant& entryVar, doc) {
        QVariantMap entry = entryVar.toMap();

        Station station;
        station.valid = true;
        station.id = entry.value("id");
        station.name = entry.value("name").toString();
        station.latitude = entry.value("lat").toDouble();
        station.longitude = entry.value("lon").toDouble();

        if (entry.value("type").toString() == "STOP") {
            QVariantList modes = entry.value("modes").toList();
            station.type = stationTypeFromModes(modes);
            QStringList modeNames;
            Q_FOREACH (const QVariant& modeVar, modes) {
                modeNames.append(modeVar.toString());
            }
            if (!modeNames.isEmpty()) {
                station.miscInfo = modeNames.join(" / ");
            }
        }

        QString country = entry.value("country").toString();
        if (!country.isEmpty()) {
            station.miscInfo = station.miscInfo.isEmpty()
                    ? country
                    : station.miscInfo + ", " + country;
        }

        if (station.name.isEmpty()) {
            continue;
        }
        results.append(station);
    }

    if (results.isEmpty()) {
        emit errorOccured(tr("No stations found for your search."));
        return;
    }

    emit stationsResult(results);
}

void ParserTransitous::getTimeTableForStation(const Station &currentStation,
                                              const Station &directionStation,
                                              const QDateTime &dateTime,
                                              ParserAbstract::Mode mode,
                                              int trainrestrictions)
{
    Q_UNUSED(directionStation);
    Q_UNUSED(dateTime);
    Q_UNUSED(mode);

    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    if (currentStation.latitude == 0.0 && currentStation.longitude == 0.0) {
        emit errorOccured(tr("This station has no coordinates, so no timetable can be requested."));
        return;
    }

    m_timetableStation = currentStation;
    m_timetableRestriction = trainrestrictions;

    currentRequestState = FahrplanNS::getTimeTableForStationRequest;
    sendHttpRequest(stoptimesUrl(m_timetableStation));
}

void ParserTransitous::parseTimeTable(QNetworkReply *networkReply)
{
    TimetableEntriesList timetable;

    QByteArray allData(networkReply->readAll());
    QVariantMap doc = parseJson(allData);

    QVariantList stopTimes = doc.value("stopTimes").toList();
    if (stopTimes.isEmpty()) {
        qDebug() << "Invalid or empty reply:" << allData;
        emit errorOccured(tr("No departures found for this station."));
        return;
    }

    Q_FOREACH (const QVariant& stopTimeVar, stopTimes) {
        QVariantMap st = stopTimeVar.toMap();
        QString mode = st.value("mode").toString();

        if (!modeMatchesRestriction(mode, m_timetableRestriction)) {
            continue;
        }

        QVariantMap place = st.value("place").toMap();
        if (place.isEmpty()) {
            continue;
        }

        QDateTime scheduled = placeTime(place, true);
        if (!scheduled.isValid()) {
            continue;
        }

        TimetableEntry entry;
        entry.currentStation = m_timetableStation.name;

        QString destination = st.value("headsign").toString();
        if (destination.isEmpty()) {
            destination = placeName(st.value("tripTo").toMap());
        }
        entry.destinationStation = destination;

        QString line = st.value("routeShortName").toString();
        if (line.isEmpty()) {
            line = st.value("displayName").toString();
        }
        entry.trainType = line;

        QString platform = place.value("track").toString();
        if (platform.isEmpty()) {
            platform = place.value("scheduledTrack").toString();
        }
        entry.platform = platform;

        entry.time = scheduled.time();
        entry.latitude = place.value("lat").toDouble();
        entry.longitude = place.value("lon").toDouble();

        // Delay / early information
        QString misc;
        bool cancelled = st.value("cancelled").toBool()
            || st.value("tripCancelled").toBool()
            || place.value("cancelled").toBool();
        if (st.value("realTime").toBool()) {
            QDateTime scheduledUtc = QDateTime::fromString(place.value("scheduledDeparture").toString(), Qt::ISODate);
            QDateTime realUtc = QDateTime::fromString(place.value("departure").toString(), Qt::ISODate);
            if (scheduledUtc.isValid() && realUtc.isValid()) {
                int diff = scheduledUtc.secsTo(realUtc);
                int minutes = qAbs(diff) / 60;
                if (minutes >= 1) {
                    misc = (diff > 0 ? tr("Delay: %1 min") : tr("Early: %1 min")).arg(minutes);
                } else if (diff != 0) {
                    misc = diff > 0 ? tr("Delayed by less than a minute")
                                   : tr("Early by less than a minute");
                }
            }
        }
        if (cancelled) {
            misc = tr("Cancelled") + (misc.isEmpty() ? "" : QString("; ") + misc);
        }
        entry.miscInfo = misc;

        timetable.append(entry);
    }

    if (timetable.isEmpty()) {
        emit errorOccured(tr("No departures found for this station."));
        return;
    }

    emit timetableResult(timetable);
}

void ParserTransitous::searchJourney(const Station &departureStation,
                                     const Station &viaStation,
                                     const Station &arrivalStation,
                                     const QDateTime &dateTime,
                                     ParserAbstract::Mode mode,
                                     int trainrestrictions)
{
    Q_UNUSED(mode);

    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    m_journeyFrom = departureStation;
    m_journeyVia = viaStation;
    m_journeyTo = arrivalStation;
    m_journeyWhen = dateTime;
    m_journeyRestriction = trainrestrictions;
    m_nextPageCursor.clear();
    m_previousPageCursor.clear();

    while (!m_details.isEmpty()) {
        delete m_details.takeFirst();
    }

    currentRequestState = FahrplanNS::searchJourneyRequest;
    sendHttpRequest(planUrl(m_journeyFrom, m_journeyVia, m_journeyTo, m_journeyWhen, QString()));
}

void ParserTransitous::searchJourneyLater()
{
    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    if (m_nextPageCursor.isEmpty()) {
        emit errorOccured(tr("No later journeys available."));
        return;
    }

    currentRequestState = FahrplanNS::searchJourneyLaterRequest;
    sendHttpRequest(planUrl(m_journeyFrom, m_journeyVia, m_journeyTo, QDateTime(), m_nextPageCursor));
}

void ParserTransitous::searchJourneyEarlier()
{
    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    if (m_previousPageCursor.isEmpty()) {
        emit errorOccured(tr("No earlier journeys available."));
        return;
    }

    currentRequestState = FahrplanNS::searchJourneyEarlierRequest;
    sendHttpRequest(planUrl(m_journeyFrom, m_journeyVia, m_journeyTo, QDateTime(), m_previousPageCursor));
}

void ParserTransitous::handleJourneyReply(QNetworkReply *networkReply, bool isInitialSearch)
{
    QByteArray allData(networkReply->readAll());
    QVariantMap doc = parseJson(allData);

    QVariantList itineraries = doc.value("itineraries").toList();
    if (doc.isEmpty() && itineraries.isEmpty()) {
        qDebug() << "Invalid reply:" << allData;
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }

    m_nextPageCursor = doc.value("nextPageCursor").toString();
    m_previousPageCursor = doc.value("previousPageCursor").toString();

    if (isInitialSearch) {
        while (!m_details.isEmpty()) {
            delete m_details.takeFirst();
        }
    }

    JourneyResultList *result = new JourneyResultList();
    result->setDepartureStation(m_journeyFrom.name);
    result->setArrivalStation(m_journeyTo.name);
    if (m_journeyVia.valid) {
        result->setViaStation(m_journeyVia.name);
    }
    result->setTimeInfo(tr("Journeys from %1").arg(
        m_journeyWhen.toLocalTime().toString(tr("ddd MMM d, HH:mm"))));

    Q_FOREACH (const QVariant& itineraryVar, itineraries) {
        QVariantMap itinerary = itineraryVar.toMap();
        QVariantList legs = itinerary.value("legs").toList();
        if (legs.isEmpty()) {
            continue;
        }

        if (!itineraryMatchesRestriction(itinerary, m_journeyRestriction)) {
            continue;
        }

        QVariantMap firstLeg = legs.first().toMap();
        QVariantMap lastLeg = legs.last().toMap();

        QDateTime firstDeparture = placeTime(firstLeg.value("from").toMap(), true);
        QDateTime lastArrival = placeTime(lastLeg.value("to").toMap(), false);
        if (!firstDeparture.isValid() || !lastArrival.isValid()) {
            continue;
        }

        // Summary of the journey
        JourneyResultItem *item = new JourneyResultItem();
        item->setId(QString::number(m_details.size()));
        item->setDate(firstDeparture.date());
        item->setDepartureTime(firstDeparture.toString("HH:mm"));
        item->setArrivalTime(lastArrival.toString("HH:mm"));

        int totalSeconds = qAbs(firstDeparture.secsTo(lastArrival));
        item->setDuration(QString("%1:%2")
            .arg(totalSeconds / 3600)
            .arg((totalSeconds % 3600) / 60, 2, 10, QChar('0')));

        QStringList lineNames;
        int transitLegs = 0;
        Q_FOREACH (const QVariant& legVar, legs) {
            QVariantMap leg = legVar.toMap();
            QString mode = leg.value("mode").toString();
            if (isTransitMode(mode)) {
                transitLegs++;
                QString line = lineName(leg);
                if (!line.isEmpty() && !lineNames.contains(line)) {
                    lineNames.append(line);
                }
            }
        }
        item->setTrainType(lineNames.join(" / "));

        int transfers = qMax(0, transitLegs - 1);
        if (transfers > 0) {
            item->setTransfers(QString::number(transfers));
        }

        result->appendItem(item);

        // Details: one item per transit leg
        JourneyDetailResultList *details = new JourneyDetailResultList();
        details->setId(item->id());
        details->setDepartureStation(m_journeyFrom.name);
        details->setArrivalStation(m_journeyTo.name);
        details->setDepartureDateTime(firstDeparture);
        details->setArrivalDateTime(lastArrival);
        details->setDuration(item->duration());
        if (transfers > 0) {
            details->setInfo(tr("%1 transfers").arg(transfers));
        }

        Q_FOREACH (const QVariant& legVar, legs) {
            QVariantMap leg = legVar.toMap();
            QString mode = leg.value("mode").toString();
            if (!isTransitMode(mode)) {
                continue;
            }

            QVariantMap from = leg.value("from").toMap();
            QVariantMap to = leg.value("to").toMap();

            QDateTime legDeparture = placeTime(from, true);
            QDateTime legArrival = placeTime(to, false);

            JourneyDetailResultItem *segment = new JourneyDetailResultItem();
            segment->setDepartureStation(placeName(from));
            segment->setDepartureInfo(from.value("track").toString());
            segment->setDepartureDateTime(legDeparture);
            segment->setArrivalStation(placeName(to));
            segment->setArrivalInfo(to.value("track").toString());
            segment->setArrivalDateTime(legArrival);
            segment->setTrain(lineName(leg));
            segment->setDirection(leg.value("headsign").toString());

            QVariantList intermediateStops = leg.value("intermediateStops").toList();
            if (!intermediateStops.isEmpty()) {
                segment->setInfo(tr("%1 intermediate stops").arg(intermediateStops.size()));
            }

            details->appendItem(segment);
        }

        if (!details->itemcount()) {
            delete details;
        } else {
            m_details.append(details);
        }
    }

    if (result->itemcount() == 0) {
        delete result;
        emit errorOccured(tr("No journeys found."));
        return;
    }

    emit journeyResult(result);
}

void ParserTransitous::parseSearchJourney(QNetworkReply *networkReply)
{
    handleJourneyReply(networkReply, true);
}

void ParserTransitous::parseSearchLaterJourney(QNetworkReply *networkReply)
{
    handleJourneyReply(networkReply, false);
}

void ParserTransitous::parseSearchEarlierJourney(QNetworkReply *networkReply)
{
    handleJourneyReply(networkReply, false);
}

void ParserTransitous::parseJourneyDetails(QNetworkReply *networkReply)
{
    Q_UNUSED(networkReply);
    // Journey details are available from the journey search response,
    // this slot is therefore not used by this parser.
}

void ParserTransitous::getJourneyDetails(const QString &id)
{
    int index = id.toInt();
    if (index >= 0 && index < m_details.size()) {
        emit journeyDetailsResult(m_details[index]);
    } else {
        emit errorOccured(tr("No journey details found."));
    }
}
