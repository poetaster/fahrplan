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

#include "calendar_sfos_wrapper.h"

#include <QCoreApplication>
#include <QThread>
#include <QSettings>
#   include <extendedcalendar.h>
#   include <extendedstorage.h>
#   include <KCalendarCore/Event>
#   include <KCalendarCore/ICalFormat>
#   include <QDesktopServices>
#   include <QTemporaryFile>
#   include <QTextStream>
#   include <QDir>
#   include <QUrl>

QString formatStations(const QDateTime dateTime, const QString &stationName, const QString &info = QString())
{
    QString station;
    if (info.isEmpty()) {
        station = stationName;
    } else {
        //: STATION / PLATFORM
        station = CalendarSfosWrapper::tr("%1 / %2", "STATION / PLATFORM").arg(stationName, info);
    }

    //: DATE TIME   STATION
    return CalendarSfosWrapper::tr("%1 %2   %3", "DATE TIME   STATION").arg(
               // TODO: Don't force QLocale::ShortFormat for date, but make it configurable.
               dateTime.toString(QLocale().dateFormat(QLocale::ShortFormat)),
               // Always use short format for time, else you get something like "12:35:00 t".
               dateTime.toString(QLocale().timeFormat(QLocale::ShortFormat)),
               station);
}

CalendarSfosWrapper::CalendarSfosWrapper(JourneyDetailResultList *result, QObject *parent) :
    QObject(parent), m_result(result)
{
}

CalendarSfosWrapper::~CalendarSfosWrapper()
{

}

void CalendarSfosWrapper::addToCalendar()
{

    const QString viaStation = m_result->viaStation();
    QSettings settings(FAHRPLAN_SETTINGS_NAMESPACE, "fahrplan2");
    QString calendarEntryTitle;
    QString calendarEntryDesc;

    if (viaStation.isEmpty())
        calendarEntryTitle = tr("%1 to %2").arg(m_result->departureStation(),
                                                m_result->arrivalStation());
    else
        calendarEntryTitle = tr("%1 via %3 to %2").arg(m_result->departureStation(),
                                                       m_result->arrivalStation(),
                                                       viaStation);

    if (!m_result->info().isEmpty())
        calendarEntryDesc.append(m_result->info()).append("\n");

    const bool compactFormat = settings.value("compactCalendarEntries", false).toBool();
    for (int i=0; i < m_result->itemcount(); i++) {
        JourneyDetailResultItem *item = m_result->getItem(i);

        const QString train = item->direction().isEmpty()
                              ? item->train()
                              : tr("%1 to %2").arg(item->train(), item->direction());

        if (!compactFormat && !train.isEmpty())
            calendarEntryDesc.append("--- ").append(train).append(" ---\n");

        calendarEntryDesc.append(formatStations(item->departureDateTime(),
                                               item->departureStation(),
                                               item->departureInfo()));
        calendarEntryDesc.append("\n");

        if (compactFormat && !train.isEmpty())
            calendarEntryDesc.append("--- ").append(train).append(" ---\n");

        calendarEntryDesc.append(formatStations(item->arrivalDateTime(),
                                               item->arrivalStation(),
                                               item->arrivalInfo()));
        calendarEntryDesc.append("\n");

        if (!compactFormat) {
            if (!item->info().isEmpty())
                calendarEntryDesc.append(item->info()).append("\n");
            calendarEntryDesc.append("\n");
        }
    }

    if (!compactFormat)
        calendarEntryDesc.append(
            tr("-- \nAdded by Fahrplan. Please, re-check the information before your journey."));

    KCalendarCore::Event::Ptr event( new KCalendarCore::Event() );
    event->setSummary(calendarEntryTitle);
    event->setDescription(calendarEntryDesc);
    event->setDtStart( m_result->departureDateTime() );
    event->setDtEnd( m_result->arrivalDateTime() );
    KCalendarCore::ICalFormat format;
    QString icsData = format.toICalString(event);
    QTemporaryFile *tmpFile = new QTemporaryFile(
                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
	       	QDir::separator() + "event-XXXXXX.ics",
                this);
                // destructed and file deleted with this object

    //qDebug() << "IcalData: " << icsData;
    if (tmpFile->open()) 
    {
        QTextStream stream( tmpFile );
        stream << icsData;
        tmpFile->close();
        qDebug() << "Opening" << tmpFile->fileName();
        if ( !QDesktopServices::openUrl(QUrl::fromLocalFile(tmpFile->fileName())) ) 
	{
            qWarning() << "QDesktopServices::openUrl fails!";
            emit addCalendarEntryComplete(false);
        } else {
            emit addCalendarEntryComplete(true);
        }
    }
}
