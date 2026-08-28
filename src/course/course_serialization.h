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

#include <QString>
#include <QStringList>

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

/**
 * Returns the path of the sidecar file which holds the course data
 * belonging to the map file at the given path (i.e. "<map_path>.courses").
 *
 * This is the *default* course file name, used for legacy maps that
 * predate explicit course-file tracking, and as the first choice when
 * auto-naming a course file for a map that never had one before.
 */
QString coursesSidecarPath(const QString& map_path);

/**
 * Returns the default course file name for the map at the given path,
 * as a name relative to the map's directory (e.g. "MyMap.omap.courses").
 */
QString defaultCourseFileName(const QString& map_path);

/**
 * Resolves a course file name (as stored in CourseDatabase::activeFile()
 * or ::recentFiles()) against the directory of the map at map_path,
 * returning an absolute path.
 */
QString resolveCourseFilePath(const QString& map_path, const QString& relative_name);

/**
 * Picks a course file name for the map at map_path that is not already
 * present in taken_names: the default name if free, otherwise the default
 * name with a "-2", "-3", ... suffix. Used to avoid silently reusing the
 * name of a course file the map was previously (and deliberately) detached
 * from.
 */
QString pickAvailableCourseFileName(const QString& map_path, const QStringList& taken_names);

}  // namespace CourseSerialization


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_SERIALIZATION_H
