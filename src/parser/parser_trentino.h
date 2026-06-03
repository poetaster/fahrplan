#ifndef PARSER_TRENTINO_H
#define PARSER_TRENTINO_H

#include <QMap>
#include <QNetworkReply>

#include "parser_abstract.h"

class ParserTrentinoTrasporti : public ParserAbstract {
  Q_OBJECT
public:
  explicit ParserTrentinoTrasporti(QObject *parent = 0);
  static QString getName() {
    return QString("%1 (Trentino Trasporti)").arg(tr("Italy"));
  }
  virtual QString name() { return getName(); }
  virtual QString shortName() { return "trentinotrasporti.it"; }

public slots:
  virtual bool supportsGps();
  virtual bool supportsVia();
  virtual bool supportsTimeTable();
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
                             ParserAbstract::Mode mode, int trainrestrictions);
  virtual void getJourneyDetails(const QString &id);

protected:
  virtual QStringList getTrainRestrictions();
  virtual void parseStationsByName(QNetworkReply *);
  virtual void parseStationsByCoordinates(QNetworkReply *);
  virtual void parseTimeTable(QNetworkReply *);
  virtual void parseSearchJourney(QNetworkReply *);
  virtual void searchJourneyLater();
  virtual void searchJourneyEarlier();

private slots:
  void onRoutesReplyFinished();
  void doGetTimeTableForStation();

private:
  void sendRequest(QUrl url);
  QVariantList parseJsonList(const QByteArray &json) const;
  void parseStationRow(StationsList &results, const QVariantMap &props);
  void fetchRoutes();
  QDateTime jodaTimeToQDateTime(const QVariantMap &dt) const;

  QMap<int, QString> m_routeNames;
  QMap<int, QVariantMap> m_stopsById;
  StationsList m_allStops;
  bool m_stopsLoaded;
  QString m_lastSearchTerm;

  Station m_timetableStation;
  Station m_timetableDirection;
  QDateTime m_timetableDateTime;
  ParserAbstract::Mode m_timetableMode;
  int m_timetableRestrictions;
  bool m_routesLoaded;

  Station m_journeyFrom;
  Station m_journeyTo;
  QDateTime m_journeyWhen;
  ParserAbstract::Mode m_journeyMode;
  int m_journeyRestrictions;
  QList<JourneyDetailResultList *> m_details;
};

#endif
