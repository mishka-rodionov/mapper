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

#ifndef OPENORIENTEERING_COURSE_H
#define OPENORIENTEERING_COURSE_H

#include <vector>

#include <QString>

namespace OpenOrienteering {


/** Type (format) of a course. */
enum class CourseType
{
    Linear,  ///< Controls visited in a fixed order
    Score    ///< Controls scored independently, visited in any order
};


/**
 * One entry in an ordered course — a reference to a control in the database.
 */
struct CourseEntry
{
    QString control_id;  ///< Matches CourseControl::id in the CourseDatabase

    bool operator==(const CourseEntry& other) const noexcept { return control_id == other.control_id; }
    bool operator!=(const CourseEntry& other) const noexcept { return !(*this == other); }
};


/**
 * A named orienteering course — an ordered (or scored) sequence of controls.
 *
 * Multiple courses can share controls from the same CourseDatabase.
 */
struct Course
{
    QString name;
    CourseType type = CourseType::Linear;
    std::vector<CourseEntry> entries;  ///< Ordered [Start, controls..., Finish]
    int climb_m = 0;                   ///< Climb in meters (entered manually)

    bool operator==(const Course& other) const noexcept;
    bool operator!=(const Course& other) const noexcept { return !(*this == other); }
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_H
