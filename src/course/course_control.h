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

#ifndef OPENORIENTEERING_COURSE_CONTROL_H
#define OPENORIENTEERING_COURSE_CONTROL_H

#include <QString>

#include "core/map_coord.h"

namespace OpenOrienteering {


/** Type of a control point in a course. */
enum class ControlType
{
    Start,
    Regular,
    Finish,
    CrossingPoint
};


/**
 * IOF control description data for a single control point.
 *
 * Corresponds to IOF Control Descriptions 2004 columns B through H.
 */
struct ControlDescription
{
    QString code;            ///< Column B: control code printed on the prism
    QString feature_part;   ///< Column C: which part of the feature (N/S/E/W/upper/lower/...)
    QString feature;         ///< Column D: the IOF feature symbol name (boulder/depression/...)
    QString approach;        ///< Column E: appearance / characteristic
    QString dimensions;      ///< Column F: size/height/depth
    QString location_detail; ///< Column G: location qualifier (top/foot/edge/corner/...)
    QString other_info;      ///< Column H: free text

    bool operator==(const ControlDescription& other) const noexcept;
    bool operator!=(const ControlDescription& other) const noexcept { return !(*this == other); }
};


/**
 * A single control point in the course database.
 *
 * Controls are identified by a unique string id (e.g. "S1", "F1", "101").
 * Multiple courses can reference the same control.
 */
struct CourseControl
{
    QString      id;                            ///< Unique identifier within the database
    MapCoord     position;                      ///< Position in native map coordinates (1/1000 mm)
    ControlType  type = ControlType::Regular;   ///< Role of the control
    ControlDescription description;            ///< IOF control description

    bool operator==(const CourseControl& other) const noexcept;
    bool operator!=(const CourseControl& other) const noexcept { return !(*this == other); }
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_CONTROL_H
