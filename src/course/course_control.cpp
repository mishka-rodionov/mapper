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

#include "course_control.h"

namespace OpenOrienteering {


bool ControlDescription::operator==(const ControlDescription& other) const noexcept
{
    return code            == other.code
        && feature_part    == other.feature_part
        && feature         == other.feature
        && approach        == other.approach
        && dimensions      == other.dimensions
        && location_detail == other.location_detail
        && other_info      == other.other_info;
}


bool CourseControl::operator==(const CourseControl& other) const noexcept
{
    return id            == other.id
        && position      == other.position
        && number_offset == other.number_offset
        && type          == other.type
        && description   == other.description;
}


}  // namespace OpenOrienteering
