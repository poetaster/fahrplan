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

#ifndef PARSER_MOVAS_BAHNDE_H
#define PARSER_MOVAS_BAHNDE_H

#include <QObject>
#include <QtNetwork>
#include "parser_abstract.h"

/*
 * bahn.de parser using the movas API
 */
class ParserMovasBahnDe : public ParserAbstract
{
    Q_OBJECT
public:
    explicit ParserMovasBahnDe(QObject *parent = 0);
    static QString getName() { return QString("%1 (bahn.de)").arg(tr("Germany")); }
    virtual QString name() { return getName(); }
    virtual QString shortName() { return "bahn.de"; }

public slots:
    void getTimeTableForStation(const Station &currentStation, const Station &directionStation, const QDateTime &dateTime, ParserAbstract::Mode mode, int trainrestrictions);
    void findStationsByName(const QString &stationName);
    void findStationsByCoordinates(qreal longitude, qreal latitude);
    void searchJourney(const Station &departureStation, const Station &viaStation, const Station &arrivalStation, const QDateTime &dateTime, ParserAbstract::Mode mode, int trainrestrictions);
    void searchJourneyLater();
    void searchJourneyEarlier();
    void getJourneyDetails(const QString &id);
    bool supportsGps();
    bool supportsVia();
    bool supportsTimeTable();
    bool supportsTimeTableDirection();
    QStringList getTrainRestrictions();

protected:
private:
    struct {
        QDateTime dateTime;
        QDateTime firstOption;
        QDateTime lastOption;
        int resultCount;
        Station from;
        Station via;
        Station to;
        int restrictions;
        QStringList restrictionStrings;
        Mode mode;
    } lastJourneySearch;

    struct {
        Station currentStation;
        Station directionStation;
        QDateTime dateTime;
        Mode mode;
        int restrictions;
    } lastTimetableSearch;

    QString baseUrl;

    QMap<QString, JourneyDetailResultList*> cachedResults; // journey ID => detailed journey results
    // Keep track of the number of "earlier"/"later" searches we did without getting any new results
    int numberOfUnsuccessfulEarlierSearches;
    int numberOfUnsuccessfulLaterSearches;

    void internalSearchJourney(const Station &departureStation, const Station &viaStation,
                                       const Station &arrivalStation, const QDateTime &dateTime,
                                       Mode mode, int trainrestrictions);
    QList<JourneyDetailResultItem*> parseJourneySegments(const QVariantMap& itinerary);
    static QString formatDistance(double distance);

    QVariantList parseJsonList(const QByteArray &json) const;
    void sendHttpRequestMovas(QUrl url, QByteArray data, QLatin1String mimeType, const QList<QPair<QByteArray,QByteArray> > &additionalHeaders);
    void parseTimeTable(QNetworkReply *networkReply);
    void parseStationsByName(QNetworkReply *networkReply);
    void parseStationsByCoordinates(QNetworkReply *networkReply);
    void parseSearchJourney(QNetworkReply *networkReply);
    void parseSearchLaterJourney(QNetworkReply *networkReply);
    void parseSearchEarlierJourney(QNetworkReply *networkReply);
    void parseJourneyDetails(QNetworkReply *networkReply);

    QMap<QString, QString> parseJourneyLegAttributes(QVariantList attributesVarList);

    QString transportModeName(/*const QString& mode, */const QString& type);
    QString parseNodeName(const QVariantMap& node);
    QString trClass(QString travelClass);
    QStringList getTrainRestrictionsCodes(int trainrestrictions);

    bool isSameLocation(const QString& id1, const QString& id2);
    quint64 getLatitude(const QString& id);
    quint64 getLongitude(const QString& id);
};

#endif // PARSER_MOVAS_BAHNDE_H
