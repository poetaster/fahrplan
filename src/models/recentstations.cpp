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

#include "recentstations.h"

#include "fahrplan.h"
#include "favorites.h"
#include "fahrplan_parser_thread.h"

namespace {
const static uint8_t MAX_RECENT_ENTRIES = {4};
}

MostRecentStations::MostRecentStations(Fahrplan *parent)
    : StationSearchResults(parent)
{
    // When favorites change, we need to trigger view update
    // so that stars states in the search results are updated.
    connect(parent->favorites(), &StationsListModel::countChanged,
            this, &MostRecentStations::onCountChanged);

    reload();
}

void MostRecentStations::reload()
{
    if (m_settings) {
        m_settings->deleteLater();
    }

    m_settings = new QSettings(FAHRPLAN_SETTINGS_NAMESPACE, "fahrplan2", this);

    if (!m_settings->group().isEmpty()) {
        m_settings->endGroup();
    }

    m_settings->beginGroup(qobject_cast<Fahrplan *>(QObject::parent())->parser()->uid());

    loadSettings();
}

void MostRecentStations::pushEntry(Fahrplan::StationType _, const Station& station)
{
    Q_UNUSED(_)

    if (m_list.contains(station) || isFavorite(station)) {
        return;
    }

    // Push new entries in the front.
    beginInsertRows(QModelIndex(), 0, 0);
    m_list.push_front(station);
    endInsertRows();

    // Drop old entries from the back.
    if (m_list.length() > MAX_RECENT_ENTRIES) {
        beginRemoveRows(QModelIndex(), MAX_RECENT_ENTRIES, m_list.length()-1);
        m_list.erase(m_list.begin() + MAX_RECENT_ENTRIES, m_list.end());
        endRemoveRows();
    }

    emit countChanged();

    saveSettings();
}

void MostRecentStations::onCountChanged()
{
    emit dataChanged(createIndex(0, 0), createIndex(rowCount() - 1, 0));
}

void MostRecentStations::loadSettings()
{
    beginResetModel();
    m_list.clear();

    int size = m_settings->beginReadArray("mostRecentStations");

    for (int k = 0; k < size; ++k) {
        if (m_list.length() >= MAX_RECENT_ENTRIES) {
            break;
        }

        m_settings->setArrayIndex(k);

        Station station;
        station.id = m_settings->value("id");

        if (m_list.contains(station) || isFavorite(station)) {
            continue;  // ignore duplicates
        }

        station.name = m_settings->value("name").toString();
        station.type = m_settings->value("type").toString();
        station.miscInfo = m_settings->value("miscInfo").toString();
        station.latitude = m_settings->value("latitude").toFloat();
        station.longitude = m_settings->value("longitude").toFloat();
        m_list << station;
    }

    m_settings->endArray();

    // The model is intentionally sorted by usage and not by name.
    // not: qSort(m_list);

    endResetModel();
}

void MostRecentStations::saveSettings()
{
    // Clean up before storing. If we don't do it and the list is shorter than
    // the previous one was, there will be junk entries left in the .ini file.
    m_settings->remove("mostRecentStations");

    for (int i = m_list.size()-1; i >= 0; --i) {
        if (isFavorite(m_list.at(i))) {
            m_list.removeAt(i);
        }
    }

    m_settings->beginWriteArray("mostRecentStations");
    int size = m_list.size();

    for (int k = 0; k < size; ++k) {
        m_settings->setArrayIndex(k);
        m_settings->setValue("id", m_list.at(k).id);
        m_settings->setValue("name", m_list.at(k).name);
        m_settings->setValue("type", m_list.at(k).type);
        m_settings->setValue("miscInfo", m_list.at(k).miscInfo);
        m_settings->setValue("latitude", m_list.at(k).latitude);
        m_settings->setValue("longitude", m_list.at(k).longitude);
    }

    m_settings->endArray();
}

bool MostRecentStations::isFavorite(const Station& station)
{
    return qobject_cast<Fahrplan *>(QObject::parent())->favorites()->isFavorite(station);
}
