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

#include "parser_movas_bahnde.h"

#ifdef BUILD_FOR_QT5
#include <QJsonDocument>
#else
#include <QScriptEngine>
#endif
#include <QRegExp>

ParserMovasBahnDe::ParserMovasBahnDe(QObject *parent) :
        ParserAbstract(parent)
{
     baseUrl = "https://app.vendo.noncd.db.de/mob";
     #ifdef BUILD_FOR_UBUNTU
     sendHttpRequestViaCurl = true;
     #endif
}

bool ParserMovasBahnDe::supportsGps()
{
    return true;
}

bool ParserMovasBahnDe::supportsVia()
{
    return true;
}

bool ParserMovasBahnDe::supportsTimeTable()
{
    return true;
}

bool ParserMovasBahnDe::supportsTimeTableDirection()
{
    return true;
}

void ParserMovasBahnDe::getTimeTableForStation(const Station &currentStation, const Station &directionStation, const QDateTime &dateTime, ParserAbstract::Mode mode, int trainrestrictions)
{
    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    currentRequestState = FahrplanNS::getTimeTableForStationRequest;

    QVariantMap request;

    lastTimetableSearch.currentStation = currentStation;
    lastTimetableSearch.directionStation = directionStation;
    lastTimetableSearch.dateTime = dateTime;
    lastTimetableSearch.mode = mode;
    lastTimetableSearch.restrictions = trainrestrictions;

    request["anfragezeit"] = dateTime.toString("HH:mm");
    request["datum"] = dateTime.toString("yyyy-MM-dd");
    request["ursprungsBahnhofId"] = currentStation.id.toString();
    request["verkehrsmittel"] = getTrainRestrictionsCodes(trainrestrictions);

    QList<QPair<QByteArray,QByteArray> > additionalHeaders;

    if(mode == ParserAbstract::Mode::Departure)
    {
        sendHttpRequestMovas(QUrl(baseUrl+"/bahnhofstafel/abfahrt"), serializeToJson(request), QLatin1String("application/x.db.vendo.mob.bahnhofstafeln.v2+json"), additionalHeaders);
    }
    else
    {
        sendHttpRequestMovas(QUrl(baseUrl+"/bahnhofstafel/ankunft"), serializeToJson(request), QLatin1String("application/x.db.vendo.mob.bahnhofstafeln.v2+json"), additionalHeaders);
    }
}

void ParserMovasBahnDe::parseTimeTable(QNetworkReply *networkReply)
{
    TimetableEntriesList result;
    ParserAbstract::Mode mode;

    QByteArray allData(networkReply->readAll());
    if (networkReply->rawHeader("Content-Encoding") == "gzip") {
        allData = gzipDecompress(allData);
    }

    //qDebug() << "Reply:\n" << allData;

    QVariantMap doc = parseJson(allData);

    if (doc.isEmpty()) {
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }

    //qDebug() << "doc:\n" << doc;

    if (doc.value("status").toString().contains("ERROR")) {
        emit errorOccured(tr("Backend returns an error: ") + ":\n\n" + doc.value("code").toString());
        return;
    }

    //qDebug() << "doc:\n" << doc;

    QVariantList entries;

    // returned timetable could be either bahnhofstafelAbfahrtPositionen (departures) 
    // or bahnhofstafelAnkunftPositionen (arrivals)
    if(doc.contains("bahnhofstafelAbfahrtPositionen"))
    {
        mode = ParserAbstract::Mode::Departure;
        entries = doc.value("bahnhofstafelAbfahrtPositionen").toList();
    }
    else if(doc.contains("bahnhofstafelAnkunftPositionen"))
    {
        mode = ParserAbstract::Mode::Arrival;
        entries = doc.value("bahnhofstafelAnkunftPositionen").toList();
    }
    else
    {
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }

    Q_FOREACH (const QVariant& entryVar, entries) {
        TimetableEntry item;
        QVariantMap entry(entryVar.toMap());

        // Note: id field contents sadly do not match. have to strip both down to
        // location to compare
        /*if(!isSameLocation(entry.value("abfrageOrt").toMap().value("locationId").toString(),lastTimetableSearch.currentStation.id.toString()))
        {
            continue;
        }*/

        QString train(entry.value("mitteltext").toString());
        QString station(entry.value("abfrageOrt").toMap().value("name").toString());
        QString dest;

        if(mode == ParserAbstract::Mode::Departure)
        {
            dest = entry.value("richtung").toString();
        }
        else
        {
            dest = entry.value("abgangsOrt").toMap().value("name").toString();
        }

        //TODO: probably should query id of dest and do compare by location
        if(lastTimetableSearch.directionStation.name.length() > 0
           && dest.compare(lastTimetableSearch.directionStation.name,Qt::CaseSensitivity::CaseSensitive) != 0)
        {
            continue;
        }

        item.currentStation = station;
        item.destinationStation = dest;
        item.trainType = train;

        if(!entry.value("ezGleis").toString().isEmpty())
        {
            item.platform = entry.value("ezGleis").toString();
        }
        else
        {
            item.platform = entry.value("gleis").toString();
        }

        QDateTime dateTime;
        QDateTime realDateTime;

        if(mode == ParserAbstract::Mode::Departure)
        {
            dateTime = (QDateTime::fromString(entry.value("abgangsDatum").toString(), Qt::ISODate));
            realDateTime = (QDateTime::fromString(entry.value("ezAbgangsDatum").toString(), Qt::ISODate));
        }
        else
        {
            dateTime = (QDateTime::fromString(entry.value("ankunftsDatum").toString(), Qt::ISODate));
            realDateTime = (QDateTime::fromString(entry.value("ezAnkunftsDatum").toString(), Qt::ISODate));
        }

        // Delay
        qint64 delaySecs(dateTime.secsTo(realDateTime));

        QVariantList realtimeNotesList = entry.value("echtzeitNotizen").toList();
        QString miscInfo = "";

        bool canceled = false;

        QString realtimeNotes = "";

        Q_FOREACH (const QVariant& realtimeNoteVar, realtimeNotesList) {
            QString realtimeNote = realtimeNoteVar.toString();
            if (realtimeNote.length() > 0) {
                if (realtimeNote.contains("Halt entfällt") || realtimeNotes.contains("Stop cancelled")) {
                    canceled = true;
                }

                realtimeNotes += realtimeNote + "<br />";
            }
        }

        if(canceled)
        {
            miscInfo = QString("<span style=\"color:#b30;\">%1</span>")
                       .arg(tr("Canceled!"));
        }
        else if(delaySecs <= 0)
        {
            miscInfo = tr("On-Time");
        }
        else
        {
            qint64 minutesTo = delaySecs / 60;
            if (minutesTo > 0) {
                if(mode == ParserAbstract::Mode::Departure)
                {
                    miscInfo = tr("Departure delayed: %n min", "", minutesTo);
                }
                else
                {
                    miscInfo = tr("Arrival delayed: %n min", "", minutesTo);
                }
            }
        }

        if(realtimeNotes.length() > 0)
        {
            miscInfo += "<br />"+realtimeNotes;
        }

        item.time = dateTime.time();
        item.miscInfo = miscInfo;

        //parse latitude and longitute from station id
        QStringList idElements = entry.value("abfrageOrt").toMap().value("locationId").toString().split("@");
        for (int i = 0; i <= idElements.length() -1; i++) {
            if (idElements[i].split("=").length() == 2) {
                if(idElements[i].split("=")[0].startsWith("X"))
                {
                    item.latitude = idElements[i].split("=")[1].toInt();
                }
                if(idElements[i].split("=")[0].startsWith("Y"))
                {
                    item.longitude = idElements[i].split("=")[1].toInt();
                }
            }
        }

        result << item;
    }

    emit timetableResult(result);
}

/* Inspired by ParserAbstract::parseJson */
QVariantList ParserMovasBahnDe::parseJsonList(const QByteArray &json) const
{
    QVariantList doc;
#ifdef BUILD_FOR_QT5
    doc = QJsonDocument::fromJson(json).toVariant().toList();
#else
    QString utf8(QString::fromUtf8(json));

    // Validation of JSON according to RFC 4627, section 6
    QString tmp(utf8);
    if (tmp.replace(QRegExp("\"(\\\\.|[^\"\\\\])*\""), "")
           .contains(QRegExp("[^,:{}\\[\\]0-9.\\-+Eaeflnr-u \\n\\r\\t]")))
        return doc;

    QScriptEngine *engine = new QScriptEngine();
    doc = engine->evaluate("(" + utf8 + ")").toVariant().toList();
    delete engine;
#endif

    return doc;
}

void ParserMovasBahnDe::sendHttpRequestMovas(QUrl url, QByteArray data, QLatin1String mimeType, const QList<QPair<QByteArray,QByteArray> > &additionalHeaders)
{
    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
    request.setRawHeader("User-Agent", userAgent.toLatin1());
    request.setRawHeader("Cache-Control", "no-cache");
    for (QList<QPair<QByteArray,QByteArray> >::ConstIterator it = additionalHeaders.constBegin(); it != additionalHeaders.constEnd(); ++it)
        request.setRawHeader(it->first, it->second);

    QUuid correlationId(QUuid::createUuid());
    // movas API does not allow UUID enclosed in {}
    request.setRawHeader("X-Correlation-ID", correlationId.toString().remove('{').remove('}').toLatin1());
    request.setRawHeader("Accept", mimeType.latin1());
    if (!acceptEncoding.isEmpty()) {
        request.setRawHeader("Accept-Encoding", acceptEncoding);
    }

#if 0
    qDebug() << "sendHttpRequestMovas Url";
    qDebug() << request.url();

    qDebug() << "sendHttpRequestMovas Headers";

    for(auto header: request.rawHeaderList())
    {
      qDebug() << header << ":" << request.rawHeader(header);
    }

    qDebug() << "sendHttpRequestMovas Data";
    qDebug() << data;
#endif

    if (data.isNull()) {
        lastRequest = NetworkManager->get(request);
    } else {
        lastRequest = NetworkManager->post(request, data);
    }

    requestTimeout->start(30000);

    connect(lastRequest, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(networkReplyDownloadProgress(qint64,qint64)));
}

void ParserMovasBahnDe::findStationsByName(const QString &stationName)
{
    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    currentRequestState = FahrplanNS::stationsByNameRequest;
    QString internalStationName = stationName;
    internalStationName.replace("\"", "");

    QList<QPair<QByteArray,QByteArray> > additionalHeaders;

    QVariantMap request;
    request["searchTerm"] = internalStationName;
    request["locationTypes"] = QStringList() << "ALL";
    request["maxResults"] = 50;

    sendHttpRequestMovas(QUrl(baseUrl+"/location/search"), serializeToJson(request), QLatin1String("application/x.db.vendo.mob.location.v3+json"), additionalHeaders);
}

void ParserMovasBahnDe::findStationsByCoordinates(qreal longitude, qreal latitude)
{
    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    currentRequestState = FahrplanNS::stationsByCoordinatesRequest;

    QList<QPair<QByteArray,QByteArray> > additionalHeaders;

    //TODO: replace with query to location/nearby endpoint
    QVariantMap request;
    request["searchTerm"] = QString::number(qRound64(longitude))+QLatin1String(",")+QString::number(qRound64(latitude));
    request["locationTypes"] = QStringList() << "ALL";
    request["maxResults"] = 50;

    sendHttpRequestMovas(QUrl(baseUrl+"/location/search"), serializeToJson(request), QLatin1String("application/x.db.vendo.mob.location.v3+json"), additionalHeaders);
}

void ParserMovasBahnDe::parseStationsByName(QNetworkReply *networkReply)
{
    QByteArray allData(networkReply->readAll());
    if (networkReply->rawHeader("Content-Encoding") == "gzip") {
        allData = gzipDecompress(allData);
    }

    //qDebug() << "Reply:\n" << allData;

    QVariantMap docErr = parseJson(allData);

    if (!docErr.isEmpty()) {
        //qDebug() << "docErr:\n" << docErr;

        if (docErr.value("status").toString().contains("ERROR")) {
            emit errorOccured(tr("Backend returns an error: ") + ":\n\n" + docErr.value("code").toString());
            return;
        }
    }

    QVariantList doc = parseJsonList(allData);

    if (doc.isEmpty()) {
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }

    QVariantList foundstations = doc;
    StationsList results;
    Q_FOREACH (const QVariant& stationData, foundstations) {
        const QVariantMap& station(stationData.toMap());
        const QVariantMap& coordinates(station.value("coordinates").toMap());
        if (coordinates.size() != 2) {
            qDebug() << "Invalid coordinates:" << station;
            continue;
        }
        Station s;
        s.id = station.value("locationId");
        s.name = station.value("name").toString();
        s.type = station.value("layer").toString();

        s.longitude = coordinates.value("longitude").toDouble();
        s.latitude = coordinates.value("latitude").toDouble();
        results.append(s);

        //qDebug() << s.miscInfo << s.id.toString() << s.name << s.latitude << s.longitude;
    }
    //qDebug() << "Found" << results.size() << "results";
    emit stationsResult(results);
}

void ParserMovasBahnDe::parseStationsByCoordinates(QNetworkReply *networkReply)
{
    parseStationsByName(networkReply);
}

void ParserMovasBahnDe::searchJourney(const Station &departureStation, const Station &viaStation,
                                   const Station &arrivalStation, const QDateTime &dateTime,
                                   Mode mode, int trainRestrictions)
{
    if (currentRequestState != FahrplanNS::noneRequest)
        return;
    currentRequestState = FahrplanNS::searchJourneyRequest;
    numberOfUnsuccessfulEarlierSearches = 0;
    numberOfUnsuccessfulLaterSearches = 0;
    internalSearchJourney(departureStation, viaStation, arrivalStation, dateTime, mode, trainRestrictions);
}

void ParserMovasBahnDe::internalSearchJourney(const Station &departureStation, const Station &viaStation,
                                               const Station &arrivalStation, const QDateTime &dateTime,
                                               Mode mode, int trainRestrictions)
{
    lastJourneySearch.dateTime = dateTime;
    lastJourneySearch.from = departureStation;
    lastJourneySearch.to = arrivalStation;
    lastJourneySearch.via = viaStation;
    lastJourneySearch.restrictions = trainRestrictions;
    lastJourneySearch.mode = mode;
    lastJourneySearch.resultCount = 0;
    lastJourneySearch.restrictionStrings = getTrainRestrictionsCodes(trainRestrictions);

    QVariantMap request;
    request["autonomeReservierung"] = false;
    request["einstiegsTypList"] = QStringList() << "STANDARD";
    // second class
    request["klasse"] = "KLASSE_2"; // other option: KLASSE_1 (first class)

    QVariantMap wunsch;
    wunsch["abgangsLocationId"] = departureStation.id.toString();
    wunsch["verkehrsmittel"] = getTrainRestrictionsCodes(trainRestrictions);

    QVariantList viaLocations;
    QVariantMap viaLocation;

    if(viaStation.id.toString().length() > 0)
    {
        viaLocation["locationId"] = viaStation.id.toString();
        viaLocation["verkehrsmittel"] = getTrainRestrictionsCodes(trainRestrictions);
        viaLocations.append(viaLocation);
        wunsch["viaLocations"] = viaLocations;
    }

    QVariantMap zeitWunsch;
    QDateTime berlinTime(dateTime.toTimeZone(QTimeZone("Europe/Berlin")));
    zeitWunsch["reiseDatum"] = berlinTime.toString(Qt::ISODate);
    if (mode == Arrival)
    {
        zeitWunsch["zeitPunktArt"] = "ANKUNFT";
    }
    else
    {
        zeitWunsch["zeitPunktArt"] = "ABFAHRT";
    }

    wunsch["zeitWunsch"] = zeitWunsch;
    wunsch["zielLocationId"] = arrivalStation.id.toString();

    QVariantMap reiseHin;
    reiseHin["wunsch"] = wunsch;
    request["reiseHin"] = reiseHin;

    QVariantMap reisender;

    reisender["ermaessigungen"] = QStringList() << "KEINE_ERMAESSIGUNG KLASSENLOS";
    reisender["reisendenTyp"] = "ERWACHSENER";

    QVariantList reisende;
    reisende.append(reisender);

    QVariantMap reisendenProfil;
    reisendenProfil["reisende"] = reisende;

    request["reisendenProfil"] = reisendenProfil;
    request["reservierungsKontingenteVorhanden"] = false;

    //qDebug() << "request (QVariantMap):\n" << request;

    QList<QPair<QByteArray,QByteArray> > additionalHeaders;

    sendHttpRequestMovas(QUrl(baseUrl+"/angebote/fahrplan"), serializeToJson(request), QLatin1String("application/x.db.vendo.mob.verbindungssuche.v8+json"), additionalHeaders);
}

void ParserMovasBahnDe::parseSearchJourney(QNetworkReply *networkReply)
{
    JourneyResultList *journeyList = new JourneyResultList();
    QByteArray allData(networkReply->readAll());
    if (networkReply->rawHeader("Content-Encoding") == "gzip") {
        allData = gzipDecompress(allData);
    }

    //qDebug() << "Reply:\n" << allData;

    QVariantMap doc = parseJson(allData);

    if (doc.isEmpty()) {
        emit errorOccured(tr("Cannot parse reply from the server"));
        return;
    }

    //qDebug() << "doc:\n" << doc;

    if (doc.value("status").toString().contains("ERROR")) {
        emit errorOccured(tr("Backend returns an error: ") + ":\n\n" + doc.value("code").toString());
        return;
    }

    //qDebug() << "doc:\n" << doc;

    const QVariantList& itineraries(doc.value("verbindungen").toList());
    int journeyCounter = 0;
    bool hasFoundWalkOnlyRoute = false;
    Q_FOREACH (const QVariant itineraryVar, itineraries) {
        const QVariantMap& itinerary(itineraryVar.toMap().value("verbindung").toMap());
        QString journeyID = QString::number(journeyCounter);
        QList<JourneyDetailResultItem*> segments = parseJourneySegments(itinerary);
        // FIXME have not seen walk distance in movas yet
#if 0
        bool walkDistanceOK;
        double walkDistance = itinerary.value("distanz").toDouble(&walkDistanceOK);
#endif

        if (segments.isEmpty())
            continue;

        // Duration
        bool durationOK;
        int minutes = itinerary.value("reiseDauer").toDouble(&durationOK) / 60;
        int hours = minutes / 60;
        minutes = minutes % 60;
        QString duration = QString("%1:%2").arg(hours).arg(minutes, 2, 10, QChar('0'));

        // Compile list of transport modes used
        QStringList transportModes;
        foreach (JourneyDetailResultItem* segment, segments) {
            if (segment->internalData1() != "WALK")
                transportModes.append(segment->train());
        }
        // When the distance is short, an option with only "walk" can be present
        if (transportModes.count() == 0 && segments.count() == 1) {
            if (hasFoundWalkOnlyRoute) {
                // For some reason there sometimes are several "walk only" routes - only use the first one
                qDebug() << "Skipping walk only route (have already got one)";
                continue;
            } else {
                transportModes.append(segments.first()->train());
                hasFoundWalkOnlyRoute = true;
            }
        }

        JourneyDetailResultList* journeyDetails = new JourneyDetailResultList;
        foreach (JourneyDetailResultItem* segment, segments)
            journeyDetails->appendItem(segment);
        journeyDetails->setId(journeyID);
        journeyDetails->setDepartureStation(segments.first()->departureStation());
        journeyDetails->setDepartureDateTime(segments.first()->departureDateTime());
        journeyDetails->setArrivalStation(segments.last()->arrivalStation());
        journeyDetails->setArrivalDateTime(segments.last()->arrivalDateTime());
        if (durationOK)
            journeyDetails->setDuration(duration);
        cachedResults.insert(journeyID, journeyDetails);

        // Indicate in the departure/arrival times if they are another day (e.g. "14:37+1")
        int depDayDiff = lastJourneySearch.dateTime.date().daysTo(journeyDetails->departureDateTime().date());
        QString depTime = journeyDetails->departureDateTime().toString("HH:mm");
        if (depDayDiff > 0)
            depTime += "+" + QString::number(depDayDiff);
        else if (depDayDiff < 0)
            depTime += QString::number(depDayDiff);
        int arrDayDiff = lastJourneySearch.dateTime.date().daysTo(journeyDetails->arrivalDateTime().date());
        QString arrTime = journeyDetails->arrivalDateTime().toString("HH:mm");
        if (arrDayDiff > 0)
            arrTime += "+" + QString::number(arrDayDiff);
        else if (arrDayDiff < 0)
            arrTime += QString::number(arrDayDiff);

        JourneyResultItem* journey = new JourneyResultItem;
        journey->setId(journeyID);
        journey->setDate(segments.first()->departureDateTime().date());
        journey->setDepartureTime(depTime);
        journey->setArrivalTime(arrTime);
        journey->setTrainType(transportModes.join(", "));
        journey->setDuration(journeyDetails->duration());
        journey->setTransfers(QString::number(transportModes.count()-1));
        //FIXME have not seen walkDistance in movas yet
#if 0
        if (walkDistanceOK && walkDistance > 0.0)
            journey->setMiscInfo(tr("Walk %1").arg(formatDistance(walkDistance)));
#endif
        journeyList->appendItem(journey);

        if (journeyCounter == 0) {
            if (lastJourneySearch.mode == Departure)
                lastJourneySearch.firstOption = journeyDetails->departureDateTime();
            else
                lastJourneySearch.firstOption = journeyDetails->arrivalDateTime();
        }
        if (lastJourneySearch.mode == Departure)
            lastJourneySearch.lastOption = journeyDetails->departureDateTime();
        else
            lastJourneySearch.lastOption = journeyDetails->arrivalDateTime();

        ++journeyCounter;
    }
    lastJourneySearch.resultCount = journeyCounter;
    journeyList->setDepartureStation(lastJourneySearch.from.name);
    journeyList->setArrivalStation(lastJourneySearch.to.name);
    QString modeString;
    if (lastJourneySearch.mode == Arrival)
        modeString = tr("Arrivals");
    else
        modeString = tr("Departures");
    journeyList->setTimeInfo(modeString + " " + lastJourneySearch.dateTime.toString(tr("ddd MMM d, HH:mm")));

    emit journeyResult(journeyList);
}

QList<JourneyDetailResultItem*> ParserMovasBahnDe::parseJourneySegments(const QVariantMap& itinerary)
{
    QList<JourneyDetailResultItem*> results;

    Q_FOREACH (const QVariant& legVar, itinerary.value("verbindungsAbschnitte").toList()) {
        const QVariantMap& leg(legVar.toMap());
        //qDebug() << "Parsing journey segment";

        //qDebug() << "leg:" << leg;

        const QVariantMap from(leg.value("abgangsOrt").toMap());
        const QVariantMap to(leg.value("ankunftsOrt").toMap());
        if (from.isEmpty() || to.isEmpty())
            continue;
        JourneyDetailResultItem* journeySegment = new JourneyDetailResultItem();
        QString transportMode = leg.value("produktGattung").toString();
        QString transportType = leg.value("typ").toString();

        //qDebug() << "Transport mode:" << transportMode;
        //qDebug() << "Transport type:" << transportType;

        QStringList info;
        // TODO: Check if distance information is available
        // for some connections
#if 0
        bool distanceOK;
        double distance = leg.value("distance").toDouble(&distanceOK);
        if (distanceOK)
            info.append(formatDistance(distance));
#endif
        journeySegment->setDepartureStation(parseNodeName(from));
        journeySegment->setArrivalStation(parseNodeName(to));
        QDateTime depDt(QDateTime::fromString(leg.value("abgangsDatum").toString(), Qt::ISODate));
        QDateTime realDepDt(QDateTime::fromString(leg.value("ezAbgangsDatum").toString(), Qt::ISODate));
#ifdef BUILD_FOR_QT5
        depDt.setTimeZone(QTimeZone("Europe/Berlin"));
        realDepDt.setTimeZone(QTimeZone("Europe/Berlin"));
#endif
        journeySegment->setDepartureDateTime(depDt.toLocalTime());
        QDateTime arrDt(QDateTime::fromString(leg.value("ankunftsDatum").toString(), Qt::ISODate));
        QDateTime realArrDt(QDateTime::fromString(leg.value("ezAnkunftsDatum").toString(), Qt::ISODate));
#ifdef BUILD_FOR_QT5
        arrDt.setTimeZone(QTimeZone("Europe/Berlin"));
        realArrDt.setTimeZone(QTimeZone("Europe/Berlin"));
#endif
        journeySegment->setArrivalDateTime(arrDt.toLocalTime());

        // Delay
        qint64 departureDelaySecs(depDt.secsTo(realDepDt));
        qint64 arrivalDelaySecs(arrDt.secsTo(realArrDt));

        // Information for all in-between stops

        // TODO: could extend UI to display all in-between stops per leg since
        // that information is contained in "halte"

        // Note: skipped for Transfer (transportType == "FUSSWEG") since
        // that leads to duplicated platform displays
        if(leg.contains("halte") && transportType != "FUSSWEG")
        {
           QVariantList stops(leg.value("halte").toList());

           if(stops.length() > 1)
           {
               //qDebug() << "num stops:" << stops.length();

               QVariantMap firstStop = stops.first().toMap();
               if(firstStop.contains("ezGleis") && firstStop.value("ezGleis").toString().length() > 0)
               {
                   journeySegment->setDepartureInfo(tr("Track changed to %1","Departure").arg(firstStop.value("ezGleis").toString()));
                   journeySegment->setDepartureInfo(journeySegment->departureInfo()+"<br>"+tr("(From %1)","Departure").arg(firstStop.value("gleis").toString()));
               }
               else if(firstStop.contains("gleis") && firstStop.value("gleis").toString().length() > 0)
               {
                   journeySegment->setDepartureInfo(tr("Track %1","Departure").arg(firstStop.value("gleis").toString()));
                   //qDebug() << "firstStop:" << firstStop.value("gleis").toString();
               }

               QVariantMap lastStop = stops.last().toMap();
               if(lastStop.contains("ezGleis") && lastStop.value("ezGleis").toString().length() > 0)
               {
                   journeySegment->setArrivalInfo(tr("Track changed to %1","Arrival").arg(lastStop.value("ezGleis").toString()));
                   journeySegment->setArrivalInfo(journeySegment->arrivalInfo()+"<br>"+tr("(From %1)","Arrival").arg(lastStop.value("gleis").toString()));
               }
               else if(lastStop.contains("gleis") && lastStop.value("gleis").toString().length() > 0)
               {
                   journeySegment->setArrivalInfo(tr("Track %1","Arrival").arg(lastStop.value("gleis").toString()));
                   //qDebug() << "lastStop:" << lastStop.value("gleis").toString();
               }
           }
        }

        // Note: skipped for Transfer (transportType == "FUSSWEG") since
        // that leads to duplicated delay displays
        if(departureDelaySecs > 0 && transportType != "FUSSWEG")
        {
            qint64 minutesTo = departureDelaySecs / 60;
            if (minutesTo > 0) {
                journeySegment->setDepartureInfo(journeySegment->departureInfo()
                                                       + QString("<br/><span style=\"color:#b30;\">%1"
                                                       "</span>").arg(tr("%n min late",
                                                                         "",
                                                                         minutesTo)));
            } else {
                journeySegment->setDepartureInfo(journeySegment->departureInfo()
                                                       + QString("<br/><span style=\"color:#093;"
                                                       " font-weight: normal;\">%1</span>")
                                               .arg(tr("on time")));
            }
        }

        // Note: skipped for Transfer (transportType == "FUSSWEG") since
        // that leads to duplicated delay displays
        if(arrivalDelaySecs > 0 && transportType != "FUSSWEG")
        {
            qint64 minutesTo = arrivalDelaySecs / 60;
            if (minutesTo > 0) {
                journeySegment->setArrivalInfo(journeySegment->arrivalInfo()
                                                       + QString("<br/><span style=\"color:#b30;\">%1"
                                                       "</span>").arg(tr("%n min late",
                                                                         "",
                                                                         minutesTo)));
            } else {
                journeySegment->setArrivalInfo(journeySegment->arrivalInfo()
                                                       + QString("<br/><span style=\"color:#093;"
                                                       " font-weight: normal;\">%1</span>")
                                               .arg(tr("on time")));
            }
        }

        bool departureCanceled = false;
        bool arrivalCanceled = false;

        QVariantList departureInfoVarList = leg.value("echtzeitNotizen").toList();
        if(!departureInfoVarList.isEmpty())
        {
            QString departureInfo;
            Q_FOREACH (const QVariant& infoVar, departureInfoVarList) {
                QString infoPart(infoVar.toString());

                if(infoPart.length() > 0)
                {
                    departureInfo.append(infoPart);
                    departureInfo.append("<br />");
                }

                if(infoPart.contains("Halt entfällt") || infoPart.contains("Stop cancelled"))
                {
                    departureCanceled = true;
                }
            }

            if(departureInfo.length() > 0)
            {
                journeySegment->setDepartureInfo(journeySegment->departureInfo() + "<br />" + departureInfo);
            }
        }

        QVariantList arrivalInfoVarList = leg.value("echtzeitNotizen").toList();
        if(!arrivalInfoVarList.isEmpty())
        {
            QString arrivalInfo;
            Q_FOREACH (const QVariant& infoVar, arrivalInfoVarList) {
                QString infoPart(infoVar.toString());

                if(infoPart.length() > 0)
                {
                    arrivalInfo.append(infoPart);
                    arrivalInfo.append("<br />");
                }

                if(infoPart.contains("Halt entfällt") || infoPart.contains("Stop cancelled"))
                {
                    arrivalCanceled = true;
                }
            }

            if(arrivalInfo.length() > 0)
            {
                journeySegment->setArrivalInfo(journeySegment->arrivalInfo() + "<br />" + arrivalInfo);
            }
        }

        if (departureCanceled && arrivalCanceled) {
            info << QString("<span style=\"color:#b30;\"><b>%1</b></span>")
                    .arg(tr("Train canceled!"));
        } else if (departureCanceled) {
            info << QString("<span style=\"color:#b30;\"><b>%1</b></span>")
                    .arg(tr("Departure stop canceled!"));
        } else if (arrivalCanceled) {
            info << QString("<span style=\"color:#b30;\"><b>%1</b></span>")
                    .arg(tr("Arrival stop canceled!"));
        }

        QStringList announcements;

        QVariantList announcementsVarList(leg.value("himNotizen").toList());

        Q_FOREACH (const QVariant& announcementVar, announcementsVarList) {
            QVariantMap announcement(announcementVar.toMap());
            announcements.append(announcement.value("ueberschrift").toString());
            announcements.append(announcement.value("text").toString());
        }

        if (announcements.count() > 0) {
            info << QString("<span style=\"color:#b30;\">%1</span>")
                    .arg(announcements.join("<br />"));
        }

        QVariantList demandInfoVarList(leg.value("auslastungsInfos").toList());
        QStringList demandInfos;

        Q_FOREACH (const QVariant& demandInfoVar, demandInfoVarList) {
            QVariantMap demandInfo(demandInfoVar.toMap());
            QString travelClass = demandInfo.value("klasse").toString();
            QString displayText = demandInfo.value("anzeigeTextKurz").toString();
            QString demand;

            if(displayText.length() < 1)
            {
                continue;
            }

            // No demand information available
            if(displayText.contains("Keine Auslastungsinformation verfügbar") ||
               displayText.contains("No occupancy information available"))
            {
                continue;
            }

            if(displayText.contains("Geringe Auslastung erwartet") ||
               displayText.contains("Low demand expected"))
            {
                demand = tr("Low demand expected");
            }
            else if(displayText.contains("Mittlere Auslastung erwartet") ||
                    displayText.contains("Medium demand expected"))
            {
                demand = tr("Medium demand expected");
            }
            else if(displayText.contains("Hohe Auslastung erwartet") ||
                    displayText.contains("High demand expected"))
            {
                demand = tr("High demand expected");
            }
            else if(displayText.contains("Außergewöhnlich hohe Auslastung erwartet") ||
                    displayText.contains("Exceptionally high demand expected"))
            {
                demand = tr("Exceptionally high demand expected");
            }
            else
            {
                demand = displayText;
            }

            demandInfos.append(tr("%1: %2", "travel class: expected demand").arg(trClass(travelClass)).arg(demand));
        }

        if (demandInfos.count() > 0) {
            info << QString("<span style=\"color:#b30;\">%1</span>")
                    .arg(demandInfos.join("<br />"));
        }

        // extra attributes
        QVariantList attributesVarList(leg.value("attributNotizen").toList());

        QMap<QString,QString> legAttributes(parseJourneyLegAttributes(attributesVarList));

        qint64 duration = leg.value("abschnittsDauer").toInt() / 60;
        // FIXME: have not seen distance in the data from movas yet
        //qint64 distance = leg.value("").toInt();

        // FIXME: assuming FUSSWEG = Transfer for now since there has been
        // no indication for walk only routes in movas yet
        if (transportType == "FUSSWEG") {
            QString type = tr("Transfer");
            journeySegment->setTrain(tr("%1 (%n min)", "Transfer", duration).arg(type));
            journeySegment->setInternalData1("WALK");
            // FIXME: have not seen distance in the data from movas yet
#if 0
            if (distance > 0)
                journeySegment->setInfo(tr("Distance %n meter(s)", "", distance));
#endif
        } else {
            QString transportString(transportModeName(/*transportMode, */transportType));
            QString routeName(leg.value("mitteltext").toString());

            //qDebug() << "Route short name:" << routeName;

            if (!routeName.isEmpty())
            {
                if(transportString.isEmpty())
                {
                  transportString = routeName;
                }
                else
                {
                  transportString += " " + routeName;
                }
            }

            if (legAttributes.contains("agencyName") && !legAttributes["agencyName"].isEmpty()) {
                info.append(legAttributes["agencyName"]);
            }

            QString tripHeadSign(leg.value("richtung").toString());
            if (!tripHeadSign.isEmpty())
                journeySegment->setDirection(tripHeadSign);
            journeySegment->setTrain(transportString);
        }
        if (!info.isEmpty())
            journeySegment->setInfo(info.join("<br/>"));

        results.append(journeySegment);
    }
    return results;
}

// TODO: check which other attributes and attribute values are available and
// how to integrate them into info field
QMap<QString, QString> ParserMovasBahnDe::parseJourneyLegAttributes(QVariantList attributesVarList)
{
   QMap<QString, QString> attributes;

   Q_FOREACH (const QVariant& attributeVar, attributesVarList) {
       QVariantMap attribute(attributeVar.toMap());
       QString key(attribute.value("key").toString());
       QString text(attribute.value("text").toString());
       if(key.compare("FB",Qt::CaseSensitivity::CaseSensitive) == 0)
       {
           if(text.compare("Fahrradmitnahme begrenzt möglich",Qt::CaseSensitivity::CaseSensitive) == 0)
           {
               attributes["bikeInfo"] = tr("Bike take along limited");
           }
           else
           {
               attributes["bikeInfo"] = text;
           }
       }
       if(key.compare("RO",Qt::CaseSensitivity::CaseSensitive) == 0)
       {
           if(text.compare("Rollstuhlstellplatz",Qt::CaseSensitivity::CaseSensitive) == 0)
           {
               attributes["wheelchairInfo"] = tr("Wheelchair parking");
           }
           else
           {
               attributes["wheelchairInfo"] = text;
           }
       }
       else if(key.compare("EH",Qt::CaseSensitivity::CaseSensitive) == 0)
       {
           if(text.compare("Fahrzeuggebundene Einstiegshilfe vorhanden",Qt::CaseSensitivity::CaseSensitive) == 0)
           {
               attributes["accessebilityInfo"] = tr("Vehicle bound boarding aid available");
           }
           else
           {
               attributes["accessebilityInfo"] = text;
           }
       }
       else if(key.compare("EA",Qt::CaseSensitivity::CaseSensitive) == 0)
       {
           if(text.compare("Behindertengerechte Ausstattung",Qt::CaseSensitivity::CaseSensitive) == 0)
           {
               attributes["accessebilityInfo2"] = tr("Handicapped accessible facilities");
           }
           else
           {
               attributes["accessebilityInfo2"] = text;
           }
       }
       else if(key.compare("LS",Qt::CaseSensitivity::CaseSensitive) == 0)
       {
           if(text.compare("Laptop-Steckdosen",Qt::CaseSensitivity::CaseSensitive) == 0)
           {
               attributes["laptopPowerInfo"] = tr("Laptop power sockets");
           }
           else
           {
               attributes["laptopPowerInfo"] = text;
           }
       }
       else if(key.compare("KL",Qt::CaseSensitivity::CaseSensitive) == 0)
       {
           if(text.compare("Klimaanlage",Qt::CaseSensitivity::CaseSensitive) == 0)
           {
               attributes["ACInfo"] = tr("Air conditioning");
           }
           else
           {
               attributes["ACInfo"] = text;
           }
       }
       else if(key.compare("WV",Qt::CaseSensitivity::CaseSensitive) == 0)
       {
           if(text.compare("WLAN verfügbar",Qt::CaseSensitivity::CaseSensitive) == 0)
           {
               attributes["wifiInfo"] = tr("Wifi available");
           }
           else
           {
               attributes["wifiInfo"] = text;
           }
       }
       else if(key.compare("OP",Qt::CaseSensitivity::CaseSensitive) == 0)
       {
           attributes["agencyName"] = text;
       }
   }

   return attributes;
}

void ParserMovasBahnDe::getJourneyDetails(const QString &id)
{
    if (cachedResults.contains(id))
        emit journeyDetailsResult(cachedResults.value(id));
    else
        emit errorOccured(tr("No journey details found."));
}

void ParserMovasBahnDe::searchJourneyLater()
{
    // If the last "later" search didn't give any new results, try searching one
    // hour later than last time, otherwise search on the time of the last result option.
    QDateTime time;
    if (lastJourneySearch.resultCount > 0 && lastJourneySearch.mode == Departure)
        time = lastJourneySearch.lastOption.addSecs(numberOfUnsuccessfulLaterSearches * 3600);
    else
        time = lastJourneySearch.dateTime.addSecs(3600);

    currentRequestState = FahrplanNS::searchJourneyLaterRequest;
    internalSearchJourney(lastJourneySearch.from, lastJourneySearch.via, lastJourneySearch.to,
                          time, lastJourneySearch.mode, lastJourneySearch.restrictions);
}

void ParserMovasBahnDe::parseSearchLaterJourney(QNetworkReply *networkReply)
{
    QDateTime oldFirstOption = lastJourneySearch.firstOption;
    QDateTime oldLastOption = lastJourneySearch.lastOption;
    parseSearchJourney(networkReply);
    if (oldFirstOption != lastJourneySearch.firstOption)
        numberOfUnsuccessfulEarlierSearches = 0;
    if (oldLastOption != lastJourneySearch.lastOption)
        numberOfUnsuccessfulLaterSearches = 0;
    else
        ++numberOfUnsuccessfulLaterSearches;
}

void ParserMovasBahnDe::searchJourneyEarlier()
{
    // If last the "earlier" search didn't give any new results, try searching one
    // hour earlier than last time, otherwise search on the time of the first result
    // option minus one hour.
    QDateTime time;
    if (lastJourneySearch.resultCount > 0 && lastJourneySearch.mode == Arrival)
        time = lastJourneySearch.firstOption.addSecs(numberOfUnsuccessfulEarlierSearches * -3600);
    else
        time = lastJourneySearch.dateTime.addSecs(-3600);
    currentRequestState = FahrplanNS::searchJourneyEarlierRequest;
    internalSearchJourney(lastJourneySearch.from, lastJourneySearch.via, lastJourneySearch.to,
                          time, lastJourneySearch.mode, lastJourneySearch.restrictions);
}

void ParserMovasBahnDe::parseSearchEarlierJourney(QNetworkReply *networkReply)
{
    QDateTime oldFirstOption = lastJourneySearch.firstOption;
    QDateTime oldLastOption = lastJourneySearch.lastOption;
    parseSearchJourney(networkReply);
    if (oldFirstOption != lastJourneySearch.firstOption)
        numberOfUnsuccessfulEarlierSearches = 0;
    else
        ++numberOfUnsuccessfulEarlierSearches;
    if (oldLastOption != lastJourneySearch.lastOption)
        numberOfUnsuccessfulLaterSearches = 0;
}

void ParserMovasBahnDe::parseJourneyDetails(QNetworkReply *networkReply)
{
    Q_UNUSED(networkReply)
    // Should never happen since we get all info about all journeys at once
    // and therefore never need to send any journeyDetailsRequest.
}

QString ParserMovasBahnDe::formatDistance(double distance)
{
    if (distance > 10000)
        // E.g. 16 km
        return QString("%1 %2").arg(QString::number(distance/1000.0, 'f', 0)).arg("km");
    else if (distance > 1000)
        // E.g. 5.6 km
        return QString("%1 %2").arg(QString::number(distance/1000.0, 'f', 1)).arg("km");
    else
        // E.g. 391 m
        return QString("%1 %2").arg(QString::number(distance, 'f', 0)).arg("m");
}

QString ParserMovasBahnDe::parseNodeName(const QVariantMap& node)
{
    QString name(node.value("name").toString());
    return name;
}

QString ParserMovasBahnDe::trClass(QString travelClass)
{
    if(travelClass.startsWith("KLASSE_1"))
    {
        return tr("First class");
    }

    if(travelClass.startsWith("KLASSE_2"))
    {
        return tr("Second class");
    }

    return "";
}

QString ParserMovasBahnDe::transportModeName(/*const QString& mode, */const QString& type)
{
    // description given in routeName are explanatory enough (except for walk)
    if (type == "FUSSWEG")
        return tr("Walk");

    return "";

    // just keeping this for reference
#if 0
    if (mode == "SCHIFF")
    {
        return tr("Ship");
    }
    else if (mode == "BUS")
    {
        return tr("Bus");
    }
    else if (mode == "ZUG" || mode.contains("ZUEGE"))
    {
        return tr("Train");
    }
    else if (mode == "SBAHN")
    {
        return tr("S-Bahn");
    }
    else if (mode == "UBAHN")
    {
        return tr("Metro");
    }
    else if (mode == "STRASSENBAHN")
    {
        return tr("Tram");
    }
    else
        return tr("Unknown transport type: %1").arg(mode);
#endif
}

QStringList ParserMovasBahnDe::getTrainRestrictions()
{
    QStringList result;
    result.append(tr("All"));
    result.append(tr("All without ICE"));
    result.append(tr("Only local transport"));
    result.append(tr("Local transport without S-Bahn"));
    return result;
}

// TODO: add more supported restrictions and more granular restriction selection
QStringList ParserMovasBahnDe::getTrainRestrictionsCodes(int trainrestrictions)
{
    QStringList trainrestr;
    if (trainrestrictions == 0) {
        //ALL
        trainrestr.append("HOCHGESCHWINDIGKEITSZUEGE");
        trainrestr.append("INTERCITYUNDEUROCITYZUEGE");
        trainrestr.append("INTERREGIOUNDSCHNELLZUEGE");
        trainrestr.append("NAHVERKEHRSONSTIGEZUEGE");
        trainrestr.append("SBAHNEN");
        trainrestr.append("UBAHN");
        trainrestr.append("STRASSENBAHN");
        trainrestr.append("BUSSE");
        trainrestr.append("SCHIFFE");
        trainrestr.append("ANRUFPFLICHTIGEVERKEHRE");
    } else if (trainrestrictions == 1) {
        //All without ICE
        trainrestr.append("INTERCITYUNDEUROCITYZUEGE");
        trainrestr.append("INTERREGIOUNDSCHNELLZUEGE");
        trainrestr.append("NAHVERKEHRSONSTIGEZUEGE");
        trainrestr.append("SBAHNEN");
        trainrestr.append("UBAHN");
        trainrestr.append("STRASSENBAHN");
        trainrestr.append("BUSSE");
        trainrestr.append("SCHIFFE");
        trainrestr.append("ANRUFPFLICHTIGEVERKEHRE");
    } else if (trainrestrictions == 2) {
        //Only local transport
        trainrestr.append("NAHVERKEHRSONSTIGEZUEGE");
        trainrestr.append("SBAHNEN");
        trainrestr.append("UBAHN");
        trainrestr.append("STRASSENBAHN");
        trainrestr.append("BUSSE");
        trainrestr.append("SCHIFFE");
        trainrestr.append("ANRUFPFLICHTIGEVERKEHRE");
    } else if (trainrestrictions == 3) {
        //Only local transport without S-Bahn
        trainrestr.append("NAHVERKEHRSONSTIGEZUEGE");
        trainrestr.append("SBAHNEN");
        trainrestr.append("UBAHN");
        trainrestr.append("BUSSE");
        trainrestr.append("SCHIFFE");
        trainrestr.append("ANRUFPFLICHTIGEVERKEHRE");
    } else {
        //if nothing matches also use values for ALL
        trainrestr.append("HOCHGESCHWINDIGKEITSZUEGE");
        trainrestr.append("INTERCITYUNDEUROCITYZUEGE");
        trainrestr.append("INTERREGIOUNDSCHNELLZUEGE");
        trainrestr.append("NAHVERKEHRSONSTIGEZUEGE");
        trainrestr.append("SBAHNEN");
        trainrestr.append("UBAHN");
        trainrestr.append("STRASSENBAHN");
        trainrestr.append("BUSSE");
        trainrestr.append("SCHIFFE");
        trainrestr.append("ANRUFPFLICHTIGEVERKEHRE");
    }

    return trainrestr;
}

bool ParserMovasBahnDe::isSameLocation(const QString& id1, const QString& id2)
{
  return getLatitude(id1) == getLatitude(id2) && getLongitude(id1) == getLongitude(id2);
}

quint64 ParserMovasBahnDe::getLatitude(const QString& id)
{
  QStringList idElements = id.split("@");
  for (int i = 0; i <= idElements.length() -1; i++) {
      if (idElements[i].split("=").length() == 2) {
          if(idElements[i].split("=")[0].startsWith("X"))
          {
              return idElements[i].split("=")[1].toInt();
          }
      }
  }

  return 0;
}

quint64 ParserMovasBahnDe::getLongitude(const QString& id)
{
  QStringList idElements = id.split("@");
  for (int i = 0; i <= idElements.length() -1; i++) {
      if (idElements[i].split("=").length() == 2) {
          if(idElements[i].split("=")[0].startsWith("Y"))
          {
              return idElements[i].split("=")[1].toInt();
          }
      }
  }

  return 0;
}
