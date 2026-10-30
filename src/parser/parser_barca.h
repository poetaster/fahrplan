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

#ifndef PARSER_BARCA_H
#define PARSER_BARCA_H

/*
 * TMB Barcelona backend (Transports Metropolitans de Barcelona / Bicing "Barca").
 *
 * Covers bus stops, metro and the TMB route planner for the city of Barcelona.
 * API base: https://api.tmb.cat/v1/
 *
 * Endpoints used:
 *   transit/parades                          - full list of bus stops (GeoJSON)
 *   transit/estacions                        - full list of metro stations (GeoJSON)
 *   itransit/bus/parades/{codi_parada}       - live bus arrivals / departures
 *   planner/plan                             - journey planner (from / to by coords)
 *
 * Station ids are namespaced so the timetable lookup knows which stop it is:
 *   "B:{codi_parada}"        - bus stop  (live arrivals via itransit/bus/parades)
 *   "M:{id_estacio}"         - metro station (live arrivals via itransit/metro/estacions)
 *
 * Every request needs the two public auth query parameters:
 *   app_id  = 12a8c95b
 *   app_key = 77e2f53f847428a05f3c02791c43b0b4
 */

#include "parser_abstract.h"
#include <QMap>

class QNetworkReply;

class ParserBarca : public ParserAbstract
{
    Q_OBJECT

    struct {
        QDateTime firstOption;
        QDateTime lastOption;
        Station from;
        Station to;
        int restrictions;
        Mode mode;
    } lastsearch;

    struct {
        bool isValid;
        qreal latitude;
        qreal longitude;
    } lastCoordinates;

    QString pendingStationQuery;   // saved station-name query while fetching all stops

public:
    explicit ParserBarca(QObject *parent = 0);
    virtual ~ParserBarca();

    static QString getName() { return QString("%1 (tmb.cat)").arg(tr("Barcelona - TMB")); }
    virtual QString name()       { return getName(); }
    virtual QString shortName()  { return "tmb.cat"; }

public slots:
    virtual bool supportsGps()                { return true;  }
    virtual bool supportsVia()                { return false; }
    virtual bool supportsTimeTable()          { return true;  }
    virtual bool supportsTimeTableDirection() { return false; }
    virtual QStringList getTrainRestrictions();

    virtual void findStationsByName(const QString &stationName);
    virtual void findStationsByCoordinates(qreal longitude, qreal latitude);
    virtual void getTimeTableForStation(const Station &currentStation,
                                        const Station &directionStation,
                                        const QDateTime &dateTime,
                                        ParserAbstract::Mode mode,
                                        int trainrestrictions);
    virtual void searchJourney(const Station &departureStation,
                                const Station &viaStation,
                                const Station &arrivalStation,
                                const QDateTime &dateTime,
                                ParserAbstract::Mode mode,
                                int trainrestrictions);
    virtual void searchJourneyLater();
    virtual void searchJourneyEarlier();
    virtual void getJourneyDetails(const QString &id);
    virtual void clearJourney();

protected:
    virtual void parseTimeTable(QNetworkReply *networkReply);
    virtual void parseStationsByName(QNetworkReply *networkReply);
    virtual void parseStationsByCoordinates(QNetworkReply *networkReply);
    virtual void parseSearchJourney(QNetworkReply *networkReply);
    virtual void parseSearchLaterJourney(QNetworkReply *networkReply);
    virtual void parseSearchEarlierJourney(QNetworkReply *networkReply);
    virtual void parseJourneyDetails(QNetworkReply *networkReply);

    JourneyResultList *lastJourneyResultList;

private:
    void sendBarcaRequest(const QUrl &url);
    void buildPlannerUrl(QUrl &uri, const Station &from, const Station &to, const QDateTime &dateTime);
    void requestStationList(const QString &urlPath);
    void handleStationListReply(QNetworkReply *networkReply);
    void appendStationsFromDoc(const QVariantMap &doc);
    void finalizeStationSearch();
    void parseMetroTimeTable(const QVariantMap &doc, TimetableEntriesList &result);
    void parseJourneyResponse(QNetworkReply *networkReply);
    void parseJourneyOption(const QVariantMap &itinerary, const QString &cacheId);

    // Merged bus + metro station search state
    QList<Station> stationAccumulator;       // collected stops/stations across both collections
    bool stationMergeRequestedEstacions;     // whether the metro collection has been requested
    bool stationByCoordinates;               // current search mode (name vs. GPS)
    bool timetableIsMetro;                   // whether the pending timetable reply is metro

    QMap<QString, JourneyDetailResultList *> cachedResults;
};

#endif // PARSER_BARCA_H
