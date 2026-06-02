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

#ifndef OPENORIENTEERING_COURSE_SERIALIZATION_H
#define OPENORIENTEERING_COURSE_SERIALIZATION_H

class QXmlStreamReader;
class QXmlStreamWriter;

namespace OpenOrienteering {

class CourseDatabase;


/**
 * XML serialization helpers for CourseDatabase.
 *
 * save() writes a <courses> element (including start/end tags).
 * load() expects the reader positioned at the <courses> start element
 * and consumes it entirely.
 */
namespace CourseSerialization {

void save(QXmlStreamWriter& xml, const CourseDatabase& db);

void load(QXmlStreamReader& xml, CourseDatabase& db);

}  // namespace CourseSerialization


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_SERIALIZATION_H
