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

#include "iof_course_export_full.h"

#include <cmath>

#include <Qt>
#include <QDateTime>
#include <QLatin1String>
#include <QXmlStreamWriter>

#include "mapper_config.h"
#include "core/georeferencing.h"
#include "core/latlon.h"
#include "core/map.h"
#include "core/map_coord.h"
#include "course/course.h"
#include "course/course_control.h"
#include "course/course_database.h"
#include "util/xml_stream_util.h"


namespace OpenOrienteering {

// static
QString IofCourseExportFull::formatDescription()
{
    return ImportExport::tr("IOF Data Standard 3.0 (Full course database)");
}

// static
QString IofCourseExportFull::filenameExtension()
{
    return QStringLiteral("xml");
}


IofCourseExportFull::IofCourseExportFull(const QString& path, const Map* map, const MapView* view)
: Exporter(path, map, view)
{}

IofCourseExportFull::~IofCourseExportFull() = default;


bool IofCourseExportFull::exportImplementation()
{
    const auto& db = map->courseDatabase();

    if (db.numControls() == 0 && db.numCourses() == 0)
    {
        addWarning(tr("The course database is empty. "
                      "Use the Course Planning panel to add controls and courses."));
        return false;
    }

    QXmlStreamWriter writer(device());
    writer.setAutoFormatting(true);
    xml = &writer;
    xml->writeStartDocument();
    writeDocument(db);
    xml->writeEndDocument();
    xml = nullptr;

    return true;
}


void IofCourseExportFull::writeDocument(const CourseDatabase& db)
{
    const auto stamp = QDateTime::currentDateTime();
    xml->writeDefaultNamespace(QLatin1String("http://www.orienteering.org/datastandard/3.0"));

    XmlElementWriter course_data(*xml, QLatin1String("CourseData"));
    course_data.writeAttribute(QLatin1String("iofVersion"), QLatin1String("3.0"));
    course_data.writeAttribute(QLatin1String("creator"),
                               QLatin1String("OpenOrienteering Mapper " APP_VERSION));
    course_data.writeAttribute(QLatin1String("createTime"), stamp.toString(Qt::ISODate));

    {
        XmlElementWriter event(*xml, QLatin1String("Event"));
        const QString event_name = db.eventName().isEmpty()
                                   ? tr("Unnamed event") : db.eventName();
        xml->writeTextElement(QLatin1String("Name"), event_name);
    }

    {
        XmlElementWriter rcd(*xml, QLatin1String("RaceCourseData"));
        writeControls(db);
        for (int i = 0; i < db.numCourses(); ++i)
            writeCourse(db.course(i), db);
    }
}


void IofCourseExportFull::writeControls(const CourseDatabase& db)
{
    const bool georef_ok =
        map->getGeoreferencing().getState() == Georeferencing::Geospatial;

    if (!georef_ok)
        addWarning(tr("The map has no valid georeferencing. "
                      "Control positions will be omitted from the export."));

    for (int i = 0; i < db.numControls(); ++i)
        writeSingleControl(db.control(i), georef_ok);
}


void IofCourseExportFull::writeSingleControl(const CourseControl& ctrl, bool georef_ok)
{
    XmlElementWriter control(*xml, QLatin1String("Control"));

    xml->writeTextElement(QLatin1String("Id"), ctrl.id);

    // Geographic position (only if georeferenced)
    if (georef_ok)
        writePosition(map->getGeoreferencing().toGeographicCoords(MapCoordF(ctrl.position)));

    // IOF description columns B–H (Mapper extension element).
    // Standard IOF tools will skip unknown elements gracefully.
    const auto& d = ctrl.description;
    const bool has_desc = !d.code.isEmpty() || !d.feature_part.isEmpty()
                          || !d.feature.isEmpty() || !d.approach.isEmpty()
                          || !d.dimensions.isEmpty() || !d.location_detail.isEmpty()
                          || !d.other_info.isEmpty();
    if (has_desc)
    {
        XmlElementWriter desc(*xml, QLatin1String("Description"));
        if (!d.code.isEmpty())            desc.writeAttribute(QLatin1String("code"),     d.code);
        if (!d.feature_part.isEmpty())    desc.writeAttribute(QLatin1String("part"),     d.feature_part);
        if (!d.feature.isEmpty())         desc.writeAttribute(QLatin1String("feature"),  d.feature);
        if (!d.approach.isEmpty())        desc.writeAttribute(QLatin1String("approach"), d.approach);
        if (!d.dimensions.isEmpty())      desc.writeAttribute(QLatin1String("dims"),     d.dimensions);
        if (!d.location_detail.isEmpty()) desc.writeAttribute(QLatin1String("location"), d.location_detail);
        if (!d.other_info.isEmpty())      desc.writeAttribute(QLatin1String("other"),    d.other_info);
    }
}


void IofCourseExportFull::writeCourse(const Course& course, const CourseDatabase& db)
{
    XmlElementWriter course_elem(*xml, QLatin1String("Course"));
    xml->writeTextElement(QLatin1String("Name"), course.name);

    {
        double total_mm = 0.0;
        const CourseControl* prev = nullptr;
        for (const auto& entry : course.entries)
        {
            const auto* ctrl = db.findById(entry.control_id);
            if (ctrl && prev)
            {
                const MapCoordF p1(prev->position), p2(ctrl->position);
                const double dx = p1.x() - p2.x(), dy = p1.y() - p2.y();
                total_mm += std::sqrt(dx * dx + dy * dy);
            }
            if (ctrl) prev = ctrl;
        }
        const int length_m = static_cast<int>(
            total_mm * map->getScaleDenominator() / 1000.0 + 0.5);
        if (length_m > 0)
            xml->writeTextElement(QLatin1String("Length"), QString::number(length_m));
    }

    if (course.climb_m > 0)
        xml->writeTextElement(QLatin1String("Climb"), QString::number(course.climb_m));

    for (const auto& entry : course.entries)
    {
        // Determine IOF CourseControl type from the control's ControlType
        QString type_str = QLatin1String("Control");
        const auto* ctrl = db.findById(entry.control_id);
        if (ctrl)
        {
            switch (ctrl->type)
            {
            case ControlType::Start:         type_str = QLatin1String("Start");   break;
            case ControlType::Finish:        type_str = QLatin1String("Finish");  break;
            case ControlType::CrossingPoint: type_str = QLatin1String("CrossingPoint"); break;
            default:                         type_str = QLatin1String("Control"); break;
            }
        }

        XmlElementWriter cc(*xml, QLatin1String("CourseControl"));
        cc.writeAttribute(QLatin1String("type"), type_str);
        xml->writeTextElement(QLatin1String("Control"), entry.control_id);
        if (course.type == CourseType::Score && type_str == QLatin1String("Control"))
            xml->writeTextElement(QLatin1String("Score"), QString::number(entry.points));
    }
}


void IofCourseExportFull::writePosition(const LatLon& latlon)
{
    XmlElementWriter position(*xml, QLatin1String("Position"));
    position.writeAttribute(QLatin1String("lat"), latlon.latitude(),  7);
    position.writeAttribute(QLatin1String("lng"), latlon.longitude(), 7);
}


}  // namespace OpenOrienteering
