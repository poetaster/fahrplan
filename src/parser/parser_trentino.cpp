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

#include <QUrl>
#include <QUrlQuery>
#include <QTimeZone>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QRegExp>
#include <QNetworkRequest>
#include <QTimer>

#include "parser_trentino.h"

// Taken from Muoversi in Trentino Android app
static const QString BASE_URL = "https://app-tpl.tndigit.it/gtlservice";
static const QString AUTH_USER = "mittmobile";
static const QString AUTH_PASS = "ecGsp.RHB3";

ParserTrentinoTrasporti::ParserTrentinoTrasporti(QObject *parent)
    : ParserAbstract(parent), m_stopsLoaded(false), m_routesLoaded(false)
{
}

bool ParserTrentinoTrasporti::supportsGps()
{
    return true;
}

bool ParserTrentinoTrasporti::supportsVia()
{
    return false;
}

bool ParserTrentinoTrasporti::supportsTimeTable()
{
    return true;
}

QStringList ParserTrentinoTrasporti::getTrainRestrictions()
{
    QStringList result;
    result.append(tr("All"));
    result.append(tr("Bus"));
    result.append(tr("Train"));
    result.append(tr("Cableway"));
    return result;
}

void ParserTrentinoTrasporti::sendRequest(QUrl url)
{
    QByteArray auth = (QString("%1:%2").arg(AUTH_USER, AUTH_PASS)).toUtf8().toBase64();
    QList<QPair<QByteArray, QByteArray>> headers;
    headers.append(QPair<QByteArray, QByteArray>("Authorization", "Basic " + auth));
    headers.append(QPair<QByteArray, QByteArray>("X-Requested-With", "it.tndigit.mit"));
    headers.append(QPair<QByteArray, QByteArray>("Content-Type", "application/json"));

    qDebug() << "Sending request to" << url.toString();
    sendHttpRequest(url, NULL, headers);
}

QVariantList ParserTrentinoTrasporti::parseJsonList(const QByteArray &json) const
{
    return QJsonDocument::fromJson(json).array().toVariantList();
}

void ParserTrentinoTrasporti::parseStationRow(StationsList& results, const QVariantMap& props)
{
    Station s;
    s.id = props.value("stopId");
    s.name = props.value("stopName").toString();
    s.latitude = props.value("stopLat").toReal();
    s.longitude = props.value("stopLon").toReal();

    QString town = props.value("town").toString();
    if (!town.isEmpty()) {
        s.miscInfo = town;
    }

    const QVariantList routes = props.value("routes").toList();
    bool hasTrain = false;
    bool hasCableway = false;
    Q_FOREACH (const QVariant& r, routes) {
        int rt = r.toMap().value("routeType").toInt();
        if (rt == 2) {
            hasTrain = true;
        } else if (rt == 5) {
            hasCableway = true;
        }
    }

    if (hasTrain) {
        s.type = "train";
    } else if (hasCableway) {
        s.type = "cablecar";
    } else {
        s.type = "bus";
    }

    m_stopsById[s.id.toInt()] = props;

    results.append(s);
}

void ParserTrentinoTrasporti::findStationsByName(const QString &stationName)
{
    if (stationName.length() < 2) {
        return;
    }

    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    m_lastSearchTerm = stationName;

    if (m_stopsLoaded) {
        StationsList results;
        Q_FOREACH (const Station& s, m_allStops) {
            if (s.name.contains(stationName, Qt::CaseInsensitive)) {
                results.append(s);
            }
        }
        emit stationsResult(results);
        return;
    }

    currentRequestState = FahrplanNS::stationsByNameRequest;

    QUrl url(BASE_URL + "/stops");

    sendRequest(url);
}

void ParserTrentinoTrasporti::findStationsByCoordinates(qreal longitude, qreal latitude)
{
    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    m_lastSearchTerm = QString::number(latitude) + "," + QString::number(longitude);

    if (m_stopsLoaded) {
        QStringList coords = m_lastSearchTerm.split(",");
        qreal refLat = coords.value(0).toDouble();
        qreal refLon = coords.value(1).toDouble();

        QMultiMap<qreal, Station> sorted;
        Q_FOREACH (const Station& s, m_allStops) {
            qreal dist = (refLat - s.latitude) * (refLat - s.latitude)
                       + (refLon - s.longitude) * (refLon - s.longitude);
            sorted.insert(dist, s);
        }

        StationsList results;
        Q_FOREACH (const Station& s, sorted.values()) {
            results.append(s);
            if (results.size() >= 50) {
                break;
            }
        }
        emit stationsResult(results);
        return;
    }

    currentRequestState = FahrplanNS::stationsByCoordinatesRequest;

    QUrl url(BASE_URL + "/stops");

    sendRequest(url);
}

void ParserTrentinoTrasporti::parseStationsByName(QNetworkReply *networkReply)
{
    QByteArray allData(networkReply->readAll());

    QVariantList stations = parseJsonList(allData);
    if (stations.isEmpty()) {
        qDebug() << "Invalid reply:" << allData;
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }

    if (!m_stopsLoaded) {
        m_allStops.clear();
        Q_FOREACH (const QVariant& featureData, stations) {
            parseStationRow(m_allStops, featureData.toMap());
        }
        m_stopsLoaded = true;
    }

    StationsList results;
    Q_FOREACH (const Station& s, m_allStops) {
        if (s.name.contains(m_lastSearchTerm, Qt::CaseInsensitive)) {
            results.append(s);
        }
    }

    emit stationsResult(results);
}

void ParserTrentinoTrasporti::parseStationsByCoordinates(QNetworkReply *networkReply)
{
    QByteArray allData(networkReply->readAll());

    QVariantList stations = parseJsonList(allData);
    if (stations.isEmpty()) {
        qDebug() << "Invalid reply:" << allData;
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }

    if (!m_stopsLoaded) {
        m_allStops.clear();
        Q_FOREACH (const QVariant& featureData, stations) {
            parseStationRow(m_allStops, featureData.toMap());
        }
        m_stopsLoaded = true;
    }

    QStringList coords = m_lastSearchTerm.split(",");
    qreal refLat = coords.value(0).toDouble();
    qreal refLon = coords.value(1).toDouble();

    QMultiMap<qreal, Station> sorted;
    Q_FOREACH (const Station& s, m_allStops) {
        qreal dist = (refLat - s.latitude) * (refLat - s.latitude)
                   + (refLon - s.longitude) * (refLon - s.longitude);
        sorted.insert(dist, s);
    }

    StationsList results;
    Q_FOREACH (const Station& s, sorted.values()) {
        results.append(s);
        if (results.size() >= 50) {
            break;
        }
    }

    emit stationsResult(results);
}

void ParserTrentinoTrasporti::fetchRoutes()
{
    QUrl url(BASE_URL + "/routes");
    QNetworkRequest request(url);

    QByteArray auth = (QString("%1:%2").arg(AUTH_USER, AUTH_PASS)).toUtf8().toBase64();
    request.setRawHeader("Authorization", "Basic " + auth);
    request.setRawHeader("X-Requested-With", "it.tndigit.mit");
    request.setRawHeader("User-Agent", userAgent.toLatin1());
    request.setRawHeader("Content-Type", "application/json");

    qDebug() << "Fetching routes from" << url.toString();
    QNetworkReply *reply = NetworkManager->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(onRoutesReplyFinished()));
}

void ParserTrentinoTrasporti::onRoutesReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    QByteArray allData = reply->readAll();
    QVariantList routes = parseJsonList(allData);

    Q_FOREACH (const QVariant& v, routes) {
        QVariantMap r = v.toMap();
        int routeId = r.value("routeId").toInt();
        QString shortName = r.value("routeShortName").toString();
        m_routeNames[routeId] = shortName;
    }

    m_routesLoaded = true;
    reply->deleteLater();

    QTimer::singleShot(0, this, SLOT(doGetTimeTableForStation()));
}

void ParserTrentinoTrasporti::getTimeTableForStation(const Station &currentStation,
    const Station &directionStation, const QDateTime &dateTime, Mode mode,
    int restrictions)
{
    Q_UNUSED(directionStation);

    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    m_timetableStation = currentStation;
    m_timetableDirection = directionStation;
    m_timetableDateTime = dateTime;
    m_timetableMode = mode;
    m_timetableRestrictions = restrictions;

    if (!m_routesLoaded) {
        fetchRoutes();
        return;
    }

    doGetTimeTableForStation();
}

void ParserTrentinoTrasporti::doGetTimeTableForStation()
{
    currentRequestState = FahrplanNS::getTimeTableForStationRequest;

    QVariantMap stop = m_stopsById.value(m_timetableStation.id.toInt());
    QString stopType = stop.value("type", "U").toString();

    QUrl url(BASE_URL + "/trips_new");
    QUrlQuery query;
    query.addQueryItem("stopId", m_timetableStation.id.toString());
    query.addQueryItem("type", stopType);
    query.addQueryItem("limit", "20");
    query.addQueryItem("refDateTime", m_timetableDateTime.toUTC().toString(Qt::ISODate));
    url.setQuery(query);

    sendRequest(url);
}

void ParserTrentinoTrasporti::parseTimeTable(QNetworkReply *networkReply)
{
    TimetableEntriesList timetable;
    QByteArray allData(networkReply->readAll());

    QVariantList trips = parseJsonList(allData);
    if (trips.isEmpty()) {
        qDebug() << "Invalid reply or empty timetable:" << allData;
        emit errorOccured(tr("No departures found for this station."));
        return;
    }

    int restriction = m_timetableRestrictions;

    Q_FOREACH (const QVariant& v, trips) {
        QVariantMap trip = v.toMap();

        int routeType = 0;
        int routeId = trip.value("routeId").toInt();
        QString routeName = m_routeNames.value(routeId, "");

        QVariantMap stop = m_stopsById.value(m_timetableStation.id.toInt());
        const QVariantList routes = stop.value("routes").toList();
        Q_FOREACH (const QVariant& r, routes) {
            if (r.toMap().value("routeId").toInt() == routeId) {
                routeType = r.toMap().value("routeType").toInt();
                break;
            }
        }

        // Apply train restrictions
        if (restriction == 1 && routeType != 3) { // Bus only
            continue;
        }
        if (restriction == 2 && routeType != 2) { // Train only
            continue;
        }
        if (restriction == 3 && routeType != 5) { // Cableway only
            continue;
        }

        TimetableEntry entry;
        entry.currentStation = m_timetableStation.name;
        entry.destinationStation = trip.value("tripHeadsign").toString();

        if (!routeName.isEmpty()) {
            entry.trainType = routeName;
        } else {
            entry.trainType = entry.destinationStation;
        }

        QString effectiveTime = trip.value("oraArrivoEffettivaAFermataSelezionata").toString();
        QString scheduledTime = trip.value("oraArrivoProgrammataAFermataSelezionata").toString();

        QDateTime dt;
        if (!effectiveTime.isEmpty()) {
            dt = QDateTime::fromString(effectiveTime, Qt::ISODate);
        }
        if (!dt.isValid() && !scheduledTime.isEmpty()) {
            dt = QDateTime::fromString(scheduledTime, Qt::ISODate);
        }
        if (dt.isValid()) {
            entry.time = dt.toLocalTime().time();
        }

        QVariant delay = trip.value("delay");
        if (!delay.isNull() && delay.toDouble() > 0.0) {
            entry.miscInfo = tr("Delay: %1'").arg(QString::number((int)delay.toDouble()));
        }

        timetable.append(entry);
    }

    emit timetableResult(timetable);
}

QDateTime ParserTrentinoTrasporti::jodaTimeToQDateTime(const QVariantMap &dt) const
{
    int year = dt.value("year").toInt();
    int month = dt.value("monthOfYear").toInt();
    int day = dt.value("dayOfMonth").toInt();
    int hour = dt.value("hourOfDay").toInt();
    int minute = dt.value("minuteOfHour").toInt();
    QTimeZone tz("Europe/Rome");
    return QDateTime(QDate(year, month, day), QTime(hour, minute), tz).toLocalTime();
}

void ParserTrentinoTrasporti::searchJourney(const Station &departureStation,
    const Station &viaStation, const Station &arrivalStation,
    const QDateTime &dateTime, Mode mode, int restrictions)
{
    Q_UNUSED(viaStation);

    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    currentRequestState = FahrplanNS::searchJourneyRequest;

    m_journeyFrom = departureStation;
    m_journeyTo = arrivalStation;
    m_journeyWhen = dateTime;
    m_journeyMode = mode;
    m_journeyRestrictions = restrictions;

    QUrl url(BASE_URL + "/direction");
    QUrlQuery query;
    QString fromStr = QString::number(departureStation.latitude) + ","
                    + QString::number(departureStation.longitude);
    QString toStr = QString::number(arrivalStation.latitude) + ","
                  + QString::number(arrivalStation.longitude);
    query.addQueryItem("from", fromStr);
    query.addQueryItem("to", toStr);
    query.addQueryItem("lang", "it");
    query.addQueryItem("refDateTime", dateTime.toUTC().toString(Qt::ISODate));
    url.setQuery(query);

    sendRequest(url);
}

void ParserTrentinoTrasporti::parseSearchJourney(QNetworkReply *networkReply)
{
    QByteArray allData(networkReply->readAll());
    QVariantMap doc = parseJson(allData);

    if (doc.isEmpty()) {
        qDebug() << "Invalid reply:" << allData;
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }

    JourneyResultList *result = new JourneyResultList();
    result->setDepartureStation(m_journeyFrom.name);
    result->setArrivalStation(m_journeyTo.name);
    if (m_journeyMode == Arrival) {
        result->setTimeInfo(tr("Arrivals %1").arg(
            m_journeyWhen.toString(tr("ddd MMM d, HH:mm"))));
    } else {
        result->setTimeInfo(tr("Departures %1").arg(
            m_journeyWhen.toString(tr("ddd MMM d, HH:mm"))));
    }

    while (!m_details.isEmpty()) {
        delete m_details.takeFirst();
    }

    QVariantList routes = doc.value("routes").toList();
    int connId = 0;

    Q_FOREACH (const QVariant& routeVar, routes) {
        QVariantMap route = routeVar.toMap();
        QVariantList legs = route.value("legs").toList();
        if (legs.isEmpty()) continue;

        QVariantList steps = legs.first().toMap().value("steps").toList();
        if (steps.isEmpty()) continue;

        // Collect transit segments
        struct TransitStep {
            QString departureStation;
            QDateTime departureTime;
            QString arrivalStation;
            QDateTime arrivalTime;
            QString lineShortName;
            QString headsign;
            int numStops;
            double delay;
            QString vehicleType;
        };
        QList<TransitStep> transitSteps;

        Q_FOREACH (const QVariant& stepVar, steps) {
            QVariantMap step = stepVar.toMap();
            QString travelMode = step.value("travelMode").toString();
            if (travelMode != "TRANSIT") continue;

            QVariantMap td = step.value("transitDetails").toMap();
            TransitStep ts;
            ts.departureStation = td.value("departureStop").toMap().value("name").toString();
            ts.arrivalStation = td.value("arrivalStop").toMap().value("name").toString();
            ts.departureTime = jodaTimeToQDateTime(td.value("departureTime").toMap());
            ts.arrivalTime = jodaTimeToQDateTime(td.value("arrivalTime").toMap());
            ts.lineShortName = td.value("line").toMap().value("shortName").toString();
            ts.headsign = td.value("headsign").toString();
            ts.numStops = td.value("numStops").toInt();
            ts.delay = td.value("delay").toDouble();
            ts.vehicleType = td.value("line").toMap().value("vehicle").toMap()
                .value("type").toString();
            transitSteps.append(ts);
        }

        if (transitSteps.isEmpty()) continue;

        // Apply train restrictions
        if (m_journeyRestrictions > 0) {
            bool skip = false;
            Q_FOREACH (const TransitStep& ts, transitSteps) {
                QString vt = ts.vehicleType;
                if (m_journeyRestrictions == 1 && (vt == "TRAIN" || vt == "CABLE_CAR")) {
                    skip = true; break;
                }
                if (m_journeyRestrictions == 2 && (vt == "BUS" || vt == "CABLE_CAR")) {
                    skip = true; break;
                }
                if (m_journeyRestrictions == 3 && (vt == "BUS" || vt == "TRAIN")) {
                    skip = true; break;
                }
            }
            if (skip) continue;
        }

        // Build JourneyResultItem (summary)
        JourneyResultItem *item = new JourneyResultItem();
        item->setId(QString::number(connId));

        QDateTime firstDep = transitSteps.first().departureTime;
        QDateTime lastArr = transitSteps.last().arrivalTime;
        item->setDate(firstDep.date());
        item->setDepartureTime(firstDep.toLocalTime().toString("HH:mm"));
        item->setArrivalTime(lastArr.toLocalTime().toString("HH:mm"));

        int totalSecs = firstDep.secsTo(lastArr);
        item->setDuration(QString("%1:%2").arg(totalSecs / 3600)
            .arg((totalSecs % 3600) / 60, 2, 10, QChar('0')));

        QStringList lineNames;
        Q_FOREACH (const TransitStep& ts, transitSteps) {
            if (!ts.lineShortName.isEmpty()) {
                lineNames.append(ts.lineShortName);
            }
        }
        item->setTrainType(lineNames.join(" "));

        int transfers = transitSteps.size() - 1;
        if (transfers > 0) {
            item->setTransfers(QString::number(transfers));
        }

        // Check for delays
        Q_FOREACH (const TransitStep& ts, transitSteps) {
            if (ts.delay > 0.0) {
                item->setMiscInfo(tr("Delay: %1'").arg(QString::number((int)ts.delay)));
                break;
            }
        }

        result->appendItem(item);

        // Build JourneyDetailResultList (segments)
        JourneyDetailResultList *details = new JourneyDetailResultList();
        details->setId(QString::number(connId));
        details->setDepartureStation(m_journeyFrom.name);
        details->setArrivalStation(m_journeyTo.name);
        details->setDepartureDateTime(firstDep);
        details->setArrivalDateTime(lastArr);
        details->setDuration(item->duration());

        Q_FOREACH (const TransitStep& ts, transitSteps) {
            JourneyDetailResultItem *segment = new JourneyDetailResultItem();
            segment->setDepartureStation(ts.departureStation);
            segment->setDepartureDateTime(ts.departureTime);
            segment->setArrivalStation(ts.arrivalStation);
            segment->setArrivalDateTime(ts.arrivalTime);
            segment->setTrain(ts.lineShortName);
            segment->setDirection(ts.headsign);

            if (ts.delay > 0.0) {
                segment->setInfo(tr("Delay: %1'").arg(QString::number((int)ts.delay)));
            }

            details->appendItem(segment);
        }

        m_details.append(details);
        connId++;
    }

    emit journeyResult(result);
}

void ParserTrentinoTrasporti::getJourneyDetails(const QString &id)
{
    int i = id.toInt();
    if (m_details.length() > i) {
        emit journeyDetailsResult(m_details[i]);
    } else {
        emit errorOccured(tr("No journey details found."));
    }
}

void ParserTrentinoTrasporti::searchJourneyLater()
{
    QDateTime nextQueryTime;
    if (m_details.length() > 0) {
        JourneyDetailResultList *lastDetail = m_details.last();
        nextQueryTime = lastDetail->departureDateTime().addSecs(60);
    } else {
        nextQueryTime = m_journeyWhen.addSecs(3600);
    }
    searchJourney(m_journeyFrom, Station(false), m_journeyTo,
                  nextQueryTime, Departure, m_journeyRestrictions);
}

void ParserTrentinoTrasporti::searchJourneyEarlier()
{
    QDateTime nextQueryTime;
    if (m_details.length() > 0) {
        JourneyDetailResultList *firstDetail = m_details.first();
        nextQueryTime = firstDetail->arrivalDateTime().addSecs(-60);
    } else {
        nextQueryTime = m_journeyWhen.addSecs(-3600);
    }
    searchJourney(m_journeyFrom, Station(false), m_journeyTo,
                  nextQueryTime, Arrival, m_journeyRestrictions);
}
