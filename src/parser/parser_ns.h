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

#ifndef PARSER_NS_H
#define PARSER_NS_H

/*
 * NS (Nederlandse Spoorwegen) backend – Dutch national railway.
 *
 * Covers trains only (Intercity, Sprinter, Thalys, international NS trains).
 * API documentation: https://apiportal.ns.nl/
 *
 * A free API key can be obtained at https://apiportal.ns.nl/
 * Replace NS_API_KEY in parser_ns.cpp with your personal subscription key.
 *
 * Endpoints used:
 *   v2/stations    – full station list, filtered client-side
 *   v2/departures  – departure board for a single station
 *   v2/arrivals    – arrival board for a single station
 *   v3/trips       – journey planner (from / to / via)
 */

#include "parser_abstract.h"
#include <QMap>

class QNetworkReply;

class ParserNs : public ParserAbstract
{
    Q_OBJECT

    struct {
        QDateTime firstOption;
        QDateTime lastOption;
        Station from;
        Station via;
        Station to;
        int restrictions;
        Mode mode;
    } lastsearch;

    struct {
        bool isValid;
        qreal latitude;
        qreal longitude;
    } lastCoordinates;

public:
    explicit ParserNs(QObject *parent = 0);
    virtual ~ParserNs();

    static QString getName() { return QString("%1 (ns.nl)").arg(tr("Netherlands - Trains")); }
    virtual QString name()       { return getName(); }
    virtual QString shortName()  { return "ns.nl"; }

public slots:
    virtual bool supportsGps()                { return true;  }
    virtual bool supportsVia()                { return true;  }
    virtual bool supportsTimeTable()          { return true;  }
    virtual bool supportsTimeTableDirection() { return true;  }
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
    void sendNsRequest(const QUrl &url);
    void parseStationsPayload(QNetworkReply *networkReply, bool byCoordinates);
    void parseJourneyResponse(QNetworkReply *networkReply);
    void parseJourneyOption(const QVariantMap &trip, const QString &cacheId);

    const QString apiKey;
    QString forwardContext;
    QString backwardContext;
    QString pendingStationQuery;   // saved station-name query while fetching all stations
    QString timetableStationName;  // station name for timetable display
    ParserAbstract::Mode timetableMode; // Departure or Arrival for current timetable request

    QMap<QString, JourneyDetailResultList *> cachedResults;
};

#endif // PARSER_NS_H
