/*
 *    Copyright 2026 OpenOrienteering contributors
 *
 *    This file is part of OpenOrienteering.
 *
 *    OpenOrienteering is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    OpenOrienteering is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENORIENTEERING_ISCD_SYMBOL_LIBRARY_H
#define OPENORIENTEERING_ISCD_SYMBOL_LIBRARY_H

#include <QString>

class QPainter;
class QRectF;

namespace OpenOrienteering {

/**
 * Standard IOF control description (ISCD) pictograms, taken from the
 * Course_Design symbol set bundled with Mapper.
 *
 * Each description symbol in that set is a point symbol whose number is the
 * ISCD code (e.g. "2.4" for Boulder), drawn for a 6 x 6 mm cell centered at
 * the origin. The set is loaded lazily from the Qt resources on first use.
 */
namespace IscdSymbolLibrary {

/// Control description columns with pictograms (values as in the legend table).
enum Column
{
    ColumnC = 2,  ///< ControlDescription::feature_part
    ColumnD = 3,  ///< ControlDescription::feature
    ColumnE = 4,  ///< ControlDescription::approach
    ColumnG = 6,  ///< ControlDescription::location_detail
};

/**
 * Returns the ISCD code for a key as stored in ControlDescription for the
 * given column (e.g. ColumnD, "Boulder" -> "2.4").
 *
 * A key which already is an ISCD code is returned unchanged.
 * Returns an empty string for unknown keys.
 */
QString code(Column column, const QString& key);

/**
 * Draws the symbol with the given ISCD code so that the symbol's 6 mm
 * description cell fills \a cell.
 *
 * Returns false if the library is unavailable or has no such symbol.
 */
bool draw(QPainter* painter, const QString& code, const QRectF& cell);

}  // namespace IscdSymbolLibrary

}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_ISCD_SYMBOL_LIBRARY_H
