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

#ifndef RECENTSTATIONS_H
#define RECENTSTATIONS_H

#include "stationsearchresults.h"

class MostRecentStations : public StationSearchResults
{
    Q_OBJECT

public:
    explicit MostRecentStations(Fahrplan *parent = 0);

public slots:
    void reload();
    void pushEntry(Fahrplan::StationType _, const Station &station);

private slots:
    void onCountChanged();

private:
    QSettings *m_settings {nullptr};

    void loadSettings();
    void saveSettings();

    bool isFavorite(const Station &station);
};

#endif // RECENTSTATIONS_H
