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

#ifndef PARSER_TRANSITOUS_H
#define PARSER_TRANSITOUS_H

#include <QList>

#include "parser_abstract.h"

class ParserTransitous : public ParserAbstract
{
    Q_OBJECT
public:
    explicit ParserTransitous(QObject *parent = 0);

    static QString getName()
    {
        return "Worldwide (Transitous)";
    }
    virtual QString name() { return getName(); }
    virtual QString shortName() { return "transitous.org"; }

public slots:
    virtual bool supportsGps();
    virtual bool supportsVia();
    virtual bool supportsTimeTable();
    virtual bool supportsTimeTableDirection();
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

protected:
    virtual void parseStationsByName(QNetworkReply *networkReply);
    virtual void parseStationsByCoordinates(QNetworkReply *networkReply);
    virtual void parseTimeTable(QNetworkReply *networkReply);
    virtual void parseSearchJourney(QNetworkReply *networkReply);
    virtual void parseSearchLaterJourney(QNetworkReply *networkReply);
    virtual void parseSearchEarlierJourney(QNetworkReply *networkReply);
    virtual void parseJourneyDetails(QNetworkReply *networkReply);

private:
    QUrl planUrl(const Station &from, const Station &via, const Station &to,
                 const QDateTime &dateTime, const QString &pageCursor) const;
    QUrl stoptimesUrl(const Station &station) const;
    QUrl stopsUrl(const Station &station) const;
    QUrl geocodeUrl(const QString &text) const;

    static bool isTransitMode(const QString &mode);
    static QString stationTypeFromModes(const QVariantList &modes);
    static bool modeMatchesRestriction(const QString &mode, int restriction);
    static bool itineraryMatchesRestriction(const QVariantMap &itinerary, int restriction);

    static QDateTime placeTime(const QVariantMap &place, bool departure);
    static QString placeName(const QVariantMap &place);
    static QString lineName(const QVariantMap &leg);
    static QString coordString(const Station &station);

    void handleJourneyReply(QNetworkReply *networkReply, bool isInitialSearch);

    Station m_timetableStation;
    int m_timetableRestriction;

    Station m_journeyFrom;
    Station m_journeyVia;
    Station m_journeyTo;
    QDateTime m_journeyWhen;
    int m_journeyRestriction;
    QString m_nextPageCursor;
    QString m_previousPageCursor;

    QList<JourneyDetailResultList *> m_details;
};

#endif // PARSER_TRANSITOUS_H
