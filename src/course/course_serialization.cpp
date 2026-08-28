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

#include "course_serialization.h"

#include <QDir>
#include <QFileInfo>
#include <QLatin1String>
#include <QtGlobal>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "core/map_coord.h"
#include "course/course.h"
#include "course/course_control.h"
#include "course/course_database.h"
#include "util/xml_stream_util.h"


namespace OpenOrienteering {

namespace {

// Element / attribute name constants
static const QLatin1String tag_courses       ("courses");
static const QLatin1String tag_control       ("control");
static const QLatin1String tag_description   ("description");
static const QLatin1String tag_course        ("course");
static const QLatin1String tag_entry         ("entry");

static const QLatin1String attr_version      ("version");
static const QLatin1String attr_event        ("event");
static const QLatin1String attr_id           ("id");
static const QLatin1String attr_type         ("type");
static const QLatin1String attr_x            ("x");
static const QLatin1String attr_y            ("y");
static const QLatin1String attr_number_dx    ("number_dx");
static const QLatin1String attr_number_dy    ("number_dy");
static const QLatin1String attr_name         ("name");
static const QLatin1String attr_control      ("control");
static const QLatin1String attr_climb        ("climb");
static const QLatin1String attr_points       ("points");
static const QLatin1String attr_default_points("default_points");
static const QLatin1String attr_description_scale("description_scale");
static const QLatin1String attr_legend_x         ("legend_x");
static const QLatin1String attr_legend_y         ("legend_y");

// ControlDescription attributes
static const QLatin1String attr_code         ("code");
static const QLatin1String attr_feature_part ("feature_part");
static const QLatin1String attr_feature      ("feature");
static const QLatin1String attr_approach     ("approach");
static const QLatin1String attr_dimensions   ("dimensions");
static const QLatin1String attr_location     ("location");
static const QLatin1String attr_other        ("other");

constexpr int current_courses_version = 1;
constexpr double min_description_scale = 0.25;
constexpr double max_description_scale = 2.0;


QString controlTypeToString(ControlType t)
{
    switch (t)
    {
    case ControlType::Start:        return QStringLiteral("start");
    case ControlType::Finish:       return QStringLiteral("finish");
    case ControlType::CrossingPoint: return QStringLiteral("crossing");
    default:                        return QStringLiteral("regular");
    }
}

ControlType controlTypeFromString(const QStringRef& s)
{
    if (s == QLatin1String("start"))    return ControlType::Start;
    if (s == QLatin1String("finish"))   return ControlType::Finish;
    if (s == QLatin1String("crossing")) return ControlType::CrossingPoint;
    return ControlType::Regular;
}

QString courseTypeToString(CourseType t)
{
    return (t == CourseType::Score) ? QStringLiteral("score") : QStringLiteral("linear");
}

CourseType courseTypeFromString(const QStringRef& s)
{
    return (s == QLatin1String("score")) ? CourseType::Score : CourseType::Linear;
}


// --- Save helpers ---

void saveDescription(QXmlStreamWriter& xml, const ControlDescription& d)
{
    if (d.code.isEmpty() && d.feature_part.isEmpty() && d.feature.isEmpty()
        && d.approach.isEmpty() && d.dimensions.isEmpty()
        && d.location_detail.isEmpty() && d.other_info.isEmpty())
        return;

    XmlElementWriter elem(xml, tag_description);
    if (!d.code.isEmpty())           elem.writeAttribute(attr_code,         d.code);
    if (!d.feature_part.isEmpty())   elem.writeAttribute(attr_feature_part, d.feature_part);
    if (!d.feature.isEmpty())        elem.writeAttribute(attr_feature,      d.feature);
    if (!d.approach.isEmpty())       elem.writeAttribute(attr_approach,     d.approach);
    if (!d.dimensions.isEmpty())     elem.writeAttribute(attr_dimensions,   d.dimensions);
    if (!d.location_detail.isEmpty()) elem.writeAttribute(attr_location,    d.location_detail);
    if (!d.other_info.isEmpty())     elem.writeAttribute(attr_other,        d.other_info);
}

void saveControl(QXmlStreamWriter& xml, const CourseControl& ctrl)
{
    XmlElementWriter elem(xml, tag_control);
    elem.writeAttribute(attr_id,   ctrl.id);
    elem.writeAttribute(attr_type, controlTypeToString(ctrl.type));
    elem.writeAttribute(attr_x,    ctrl.position.nativeX());
    elem.writeAttribute(attr_y,    ctrl.position.nativeY());
    if (!qFuzzyIsNull(ctrl.number_offset.x()) || !qFuzzyIsNull(ctrl.number_offset.y()))
    {
        elem.writeAttribute(attr_number_dx, ctrl.number_offset.x());
        elem.writeAttribute(attr_number_dy, ctrl.number_offset.y());
    }
    saveDescription(xml, ctrl.description);
}

void saveCourse(QXmlStreamWriter& xml, const Course& c)
{
    XmlElementWriter elem(xml, tag_course);
    elem.writeAttribute(attr_name, c.name);
    elem.writeAttribute(attr_type, courseTypeToString(c.type));
    if (c.climb_m > 0)
        elem.writeAttribute(attr_climb, c.climb_m);
    if (c.description_scale != 1.0)
        elem.writeAttribute(attr_description_scale, c.description_scale);
    if (c.type == CourseType::Score)
        elem.writeAttribute(attr_default_points, c.default_points);
    for (const auto& entry : c.entries)
    {
        XmlElementWriter entry_elem(xml, tag_entry);
        entry_elem.writeAttribute(attr_control, entry.control_id);
        if (c.type == CourseType::Score)
            entry_elem.writeAttribute(attr_points, entry.points);
    }
}


// --- Load helpers ---

void loadDescription(QXmlStreamReader& xml, ControlDescription& d)
{
    const auto attrs = xml.attributes();
    d.code            = attrs.value(attr_code).toString();
    d.feature_part    = attrs.value(attr_feature_part).toString();
    d.feature         = attrs.value(attr_feature).toString();
    d.approach        = attrs.value(attr_approach).toString();
    d.dimensions      = attrs.value(attr_dimensions).toString();
    d.location_detail = attrs.value(attr_location).toString();
    d.other_info      = attrs.value(attr_other).toString();
    xml.skipCurrentElement();
}

CourseControl loadControl(QXmlStreamReader& xml)
{
    CourseControl ctrl;
    const auto attrs = xml.attributes();
    ctrl.id       = attrs.value(attr_id).toString();
    ctrl.type     = controlTypeFromString(attrs.value(attr_type));
    auto x = attrs.value(attr_x).toInt();
    auto y = attrs.value(attr_y).toInt();
    ctrl.position = MapCoord::fromNative(x, y);
    bool dx_ok = false;
    bool dy_ok = false;
    const auto dx = attrs.value(attr_number_dx).toDouble(&dx_ok);
    const auto dy = attrs.value(attr_number_dy).toDouble(&dy_ok);
    if (dx_ok && dy_ok)
        ctrl.number_offset = MapCoordF(dx, dy);

    while (xml.readNextStartElement())
    {
        if (xml.name() == tag_description)
            loadDescription(xml, ctrl.description);
        else
            xml.skipCurrentElement();
    }
    return ctrl;
}

Course loadCourse(QXmlStreamReader& xml)
{
    Course c;
    const auto attrs = xml.attributes();
    c.name    = attrs.value(attr_name).toString();
    c.type    = courseTypeFromString(attrs.value(attr_type));
    c.climb_m = attrs.value(attr_climb).toInt();
    bool scale_ok = false;
    const double scale = attrs.value(attr_description_scale).toDouble(&scale_ok);
    if (scale_ok && scale > 0.0)
        c.description_scale = qBound(min_description_scale, scale, max_description_scale);
    bool dp_ok = false;
    const int default_points = attrs.value(attr_default_points).toInt(&dp_ok);
    if (dp_ok && default_points > 0)
        c.default_points = default_points;

    while (xml.readNextStartElement())
    {
        if (xml.name() == tag_entry)
        {
            CourseEntry entry;
            entry.control_id = xml.attributes().value(attr_control).toString();
            entry.points     = xml.attributes().value(attr_points).toInt();
            c.entries.push_back(std::move(entry));
            xml.skipCurrentElement();
        }
        else
        {
            xml.skipCurrentElement();
        }
    }
    return c;
}

}  // anonymous namespace


namespace CourseSerialization {

void save(QXmlStreamWriter& xml, const CourseDatabase& db)
{
    XmlElementWriter courses_elem(xml, tag_courses);
    courses_elem.writeAttribute(attr_version, current_courses_version);
    if (!db.eventName().isEmpty())
        courses_elem.writeAttribute(attr_event, db.eventName());
    if (db.hasLegendAnchor())
    {
        courses_elem.writeAttribute(attr_legend_x, db.legendAnchor().x());
        courses_elem.writeAttribute(attr_legend_y, db.legendAnchor().y());
    }

    for (int i = 0; i < db.numControls(); ++i)
        saveControl(xml, db.control(i));

    for (int i = 0; i < db.numCourses(); ++i)
        saveCourse(xml, db.course(i));
}


void load(QXmlStreamReader& xml, CourseDatabase& db)
{
    // Caller has positioned reader at the <courses> start element.
    const auto attrs = xml.attributes();
    db.setEventName(attrs.value(attr_event).toString());

    bool lx_ok = false, ly_ok = false;
    const double lx = attrs.value(attr_legend_x).toDouble(&lx_ok);
    const double ly = attrs.value(attr_legend_y).toDouble(&ly_ok);
    if (lx_ok && ly_ok)
        db.setLegendAnchor(MapCoordF(lx, ly));
    else
        db.clearLegendAnchor();

    while (xml.readNextStartElement())
    {
        if (xml.name() == tag_control)
        {
            db.addControl(loadControl(xml));
        }
        else if (xml.name() == tag_course)
        {
            db.addCourse(loadCourse(xml));
        }
        else
        {
            xml.skipCurrentElement();
        }
    }
}


QString coursesSidecarPath(const QString& map_path)
{
    return map_path + QLatin1String(".courses");
}


QString defaultCourseFileName(const QString& map_path)
{
    return QFileInfo(map_path).fileName() + QLatin1String(".courses");
}


QString resolveCourseFilePath(const QString& map_path, const QString& relative_name)
{
    return QFileInfo(map_path).dir().filePath(relative_name);
}


QString pickAvailableCourseFileName(const QString& map_path, const QStringList& taken_names)
{
    const auto default_name = defaultCourseFileName(map_path);
    if (!taken_names.contains(default_name))
        return default_name;

    const QFileInfo info(default_name);
    const auto base = info.completeBaseName();  // "MyMap.omap" (strips only ".courses")
    const auto suffix = QLatin1String(".courses");
    for (int n = 2; ; ++n)
    {
        QString candidate = base + QLatin1Char('-') + QString::number(n) + suffix;
        if (!taken_names.contains(candidate))
            return candidate;
    }
}


}  // namespace CourseSerialization


}  // namespace OpenOrienteering
