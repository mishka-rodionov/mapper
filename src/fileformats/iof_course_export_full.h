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

#ifndef OPENORIENTEERING_IOF_COURSE_EXPORT_FULL_H
#define OPENORIENTEERING_IOF_COURSE_EXPORT_FULL_H

#include <QHash>
#include <QString>

#include "fileformats/file_import_export.h"

class QXmlStreamWriter;

namespace OpenOrienteering {

class Course;
class CourseControl;
class CourseDatabase;
class LatLon;
class Map;
class MapView;


/**
 * Exports all courses in the map's CourseDatabase to IOF DataStandard 3.0 XML.
 *
 * Generates:
 *  - One <Control> element per control in the database (with optional position
 *    if the map is georeferenced, and a <Description> extension element for
 *    columns B-H).
 *  - One <Course> element per course with <CourseControl> child elements.
 *
 * A control is identified by its code (description column B), as shown in the
 * Course Planning panel and on the map, and by its internal id if the code is
 * empty. Crossing points are not exported: they are drawn on the map, but
 * there is no station to punch.
 *
 * Course rules that IOF 3.0 has no element for are written to the standard
 * <Extensions> elements, read by Competra (IOFXmlParser in eSport):
 *  - <Course><Extensions><FreeOrder minControls="18"/></Extensions> for a
 *    FreeOrder course; minControls is omitted if all controls must be taken.
 *  - <CourseControl><Extensions><Required>true</Required></Extensions> for a
 *    required control of a Score or FreeOrder course.
 *
 * Unlike IofCourseExport (which exports a single hand-drawn path),
 * this exporter reads directly from the CourseDatabase.
 */
class IofCourseExportFull : public Exporter
{
public:
    static QString formatDescription();
    static QString filenameExtension();

    IofCourseExportFull(const QString& path, const Map* map, const MapView* view);
    ~IofCourseExportFull() override;

protected:
    bool exportImplementation() override;

private:
    /**
     * Fills export_ids. Returns false (with a warning) if two controls would
     * be exported with the same Id.
     */
    bool assignExportIds(const CourseDatabase& db);

    /**
     * Returns false (with a warning) if a FreeOrder course asks for more
     * controls than it has.
     */
    bool checkMinControls(const CourseDatabase& db);
    
    /** Returns the Id under which the control is written to the XML file. */
    QString exportId(const CourseControl& ctrl) const;
    
    void writeDocument(const CourseDatabase& db);
    void writeControls(const CourseDatabase& db);
    void writeSingleControl(const CourseControl& ctrl, bool georef_ok);
    void writeCourse(const Course& course, const CourseDatabase& db);
    void writePosition(const LatLon& latlon);

    QXmlStreamWriter* xml = nullptr;
    QHash<QString, QString> export_ids;  ///< CourseControl::id -> exported Id
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_IOF_COURSE_EXPORT_FULL_H
