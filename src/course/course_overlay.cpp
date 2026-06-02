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

#include "course_overlay.h"

#include <cmath>

#include <QtMath>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVector>

#include "course/course.h"
#include "course/course_control.h"
#include "course/course_database.h"
#include "core/map_coord.h"
#include "core/map_view.h"
#include "gui/util_gui.h"
#include "gui/map/map_widget.h"


namespace OpenOrienteering {

namespace {

// IOF course purple, per ISOM 2017-2 / ISSprOM 2019 specification.
const QColor course_purple { 148, 0, 211 };

// Standard IOF control sizes in millimeters at map scale.
// These are the printed sizes on the map paper.
constexpr qreal circle_diameter_mm  = 5.0;   // regular control circle
constexpr qreal finish_outer_mm     = 7.0;   // finish outer circle
constexpr qreal finish_inner_mm     = 5.0;   // finish inner circle
constexpr qreal triangle_size_mm    = 6.0;   // start triangle side length
constexpr qreal line_width_mm       = 0.35;  // line width for all symbols
constexpr qreal number_offset_mm    = 3.5;   // control number offset from circle edge

}  // anonymous namespace


CourseOverlay::CourseOverlay(MapWidget* widget, const CourseDatabase& db, QObject* parent)
: QObject(parent)
, widget(widget)
, db(db)
{
    widget->setCourseOverlay(this);

    connect(&db, &CourseDatabase::controlAdded,   this, &CourseOverlay::onDatabaseChanged);
    connect(&db, &CourseDatabase::controlChanged, this, &CourseOverlay::onDatabaseChanged);
    connect(&db, &CourseDatabase::controlRemoved, this, &CourseOverlay::onDatabaseChanged);
    connect(&db, &CourseDatabase::courseAdded,    this, &CourseOverlay::onDatabaseChanged);
    connect(&db, &CourseDatabase::courseChanged,  this, &CourseOverlay::onDatabaseChanged);
    connect(&db, &CourseDatabase::courseRemoved,  this, &CourseOverlay::onDatabaseChanged);
}

CourseOverlay::~CourseOverlay()
{
    widget->setCourseOverlay(nullptr);
}

void CourseOverlay::setVisibleCourse(const Course* course)
{
    visible_course = course;
    widget->updateEverything();
}

void CourseOverlay::onDatabaseChanged()
{
    widget->updateEverything();
}


// --- Main paint entry point ---

void CourseOverlay::paint(QPainter* painter)
{
    if (db.numControls() == 0)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (visible_course)
        paintCourse(painter, *visible_course);
    else
        paintAllControls(painter);

    painter->restore();

    if (show_description_table && visible_course)
        paintDescriptionTable(painter);
}


// --- Course / controls rendering ---

void CourseOverlay::paintCourse(QPainter* painter, const Course& course) const
{
    if (course.entries.empty())
        return;

    // Collect resolved controls in order
    QVector<const CourseControl*> resolved;
    resolved.reserve(static_cast<int>(course.entries.size()));
    for (const auto& entry : course.entries)
    {
        if (const auto* ctrl = db.findById(entry.control_id))
            resolved.append(ctrl);
    }

    // Draw legs first (under symbols)
    for (int i = 1; i < resolved.size(); ++i)
        paintLeg(painter, toViewport(*resolved[i-1]), toViewport(*resolved[i]));

    // Draw symbols on top of legs
    int seq = 1;
    for (int i = 0; i < resolved.size(); ++i)
    {
        const auto* ctrl = resolved[i];
        QPointF pos = toViewport(*ctrl);

        switch (ctrl->type)
        {
        case ControlType::Start:
        {
            // Compute direction toward next control for triangle orientation
            double rot = 0.0;
            if (i + 1 < resolved.size())
            {
                QPointF next = toViewport(*resolved[i+1]);
                QPointF d = next - pos;
                rot = std::atan2(d.y(), d.x());
            }
            paintStart(painter, pos, rot);
            break;
        }
        case ControlType::Finish:
            paintFinish(painter, pos);
            break;
        case ControlType::CrossingPoint:
            paintCrossingPoint(painter, pos);
            break;
        default:
            paintControl(painter, pos, QString::number(seq));
            ++seq;
            break;
        }
    }
}

void CourseOverlay::paintAllControls(QPainter* painter) const
{
    for (int i = 0; i < db.numControls(); ++i)
    {
        const auto& ctrl = db.control(i);
        QPointF pos = toViewport(ctrl);

        switch (ctrl.type)
        {
        case ControlType::Start:
            paintStart(painter, pos, 0.0);
            break;
        case ControlType::Finish:
            paintFinish(painter, pos);
            break;
        case ControlType::CrossingPoint:
            paintCrossingPoint(painter, pos);
            break;
        default:
            paintControl(painter, pos, ctrl.description.code.isEmpty() ? ctrl.id : ctrl.description.code);
            break;
        }
    }
}


// --- Individual symbol painters ---

void CourseOverlay::paintLeg(QPainter* painter, QPointF from, QPointF to) const
{
    const qreal lw = mmToViewportPx(line_width_mm);
    painter->setPen(QPen(course_purple, lw));
    painter->setBrush(Qt::NoBrush);

    // Shorten the leg so it doesn't overlap the control circles
    const qreal r = mmToViewportPx(circle_diameter_mm / 2.0);
    QPointF d = to - from;
    const qreal len = std::sqrt(d.x() * d.x() + d.y() * d.y());
    if (len < 2 * r)
        return;

    const qreal f = r / len;
    QPointF start = from + f * d;
    QPointF end   = to   - f * d;

    painter->drawLine(start, end);
}

void CourseOverlay::paintStart(QPainter* painter, QPointF pos, double rotation_rad) const
{
    const qreal lw   = mmToViewportPx(line_width_mm);
    const qreal side = mmToViewportPx(triangle_size_mm);

    painter->setPen(QPen(course_purple, lw));
    painter->setBrush(Qt::NoBrush);

    // Equilateral triangle pointing toward the first control.
    // Vertices relative to center:
    const qreal h = side * (std::sqrt(3.0) / 2.0);  // height of equilateral triangle
    const qreal r_outer = h * 2.0 / 3.0;            // circumradius
    const qreal r_inner = h / 3.0;                  // inradius

    QPolygonF triangle;
    for (int k = 0; k < 3; ++k)
    {
        double angle = rotation_rad - M_PI / 2.0 + k * (2.0 * M_PI / 3.0);
        triangle << QPointF(pos.x() + r_outer * std::cos(angle),
                            pos.y() + r_outer * std::sin(angle));
    }
    Q_UNUSED(r_inner);

    painter->drawPolygon(triangle);
}

void CourseOverlay::paintControl(QPainter* painter, QPointF pos, const QString& number) const
{
    const qreal lw = mmToViewportPx(line_width_mm);
    const qreal r  = mmToViewportPx(circle_diameter_mm / 2.0);

    painter->setPen(QPen(course_purple, lw));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(pos, r, r);

    paintControlNumber(painter, pos, number);
}

void CourseOverlay::paintFinish(QPainter* painter, QPointF pos) const
{
    const qreal lw      = mmToViewportPx(line_width_mm);
    const qreal r_outer = mmToViewportPx(finish_outer_mm / 2.0);
    const qreal r_inner = mmToViewportPx(finish_inner_mm / 2.0);

    painter->setPen(QPen(course_purple, lw));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(pos, r_outer, r_outer);
    painter->drawEllipse(pos, r_inner, r_inner);
}

void CourseOverlay::paintCrossingPoint(QPainter* painter, QPointF pos) const
{
    const qreal lw = mmToViewportPx(line_width_mm);
    const qreal r  = mmToViewportPx(2.5);

    painter->setPen(QPen(course_purple, lw));
    painter->setBrush(Qt::NoBrush);
    // Two crossing arcs — approximated as an X
    painter->drawLine(QPointF(pos.x() - r, pos.y() - r), QPointF(pos.x() + r, pos.y() + r));
    painter->drawLine(QPointF(pos.x() - r, pos.y() + r), QPointF(pos.x() + r, pos.y() - r));
}

void CourseOverlay::paintControlNumber(QPainter* painter, QPointF center, const QString& number) const
{
    if (number.isEmpty())
        return;

    const qreal r      = mmToViewportPx(circle_diameter_mm / 2.0);
    const qreal offset = mmToViewportPx(number_offset_mm);

    QFont font;
    font.setPixelSize(static_cast<int>(mmToViewportPx(3.0)));
    font.setBold(true);
    painter->setFont(font);

    painter->setPen(course_purple);

    // Place the number to the upper-right of the circle
    const QPointF text_pos(center.x() + r + offset * 0.3, center.y() - r - offset * 0.1);
    painter->drawText(text_pos, number);
}


// --- Coordinate conversion ---

QPointF CourseOverlay::toViewport(const CourseControl& ctrl) const
{
    return widget->mapToViewport(MapCoordF(ctrl.position));
}

qreal CourseOverlay::mmToViewportPx(qreal mm) const
{
    // lengthToPixel takes native units (1/1000 mm), so multiply mm by 1000.
    return widget->getMapView()->lengthToPixel(mm * 1000.0);
}


// --- IOF description table ---

void CourseOverlay::paintDescriptionTable(QPainter* painter) const
{
    Q_ASSERT(visible_course);

    // Collect resolved controls (Regular only, in course order)
    struct Row {
        int    seq;
        QString code;
        QString part;        // C
        QString feature;     // D
        QString approach;    // E
        QString dims;        // F
        QString location;    // G
        QString other;       // H
    };

    QVector<Row> rows;
    int seq = 1;
    for (const auto& entry : visible_course->entries)
    {
        const auto* ctrl = db.findById(entry.control_id);
        if (!ctrl || ctrl->type != ControlType::Regular)
            continue;
        Row r;
        r.seq      = seq++;
        r.code     = ctrl->description.code.isEmpty() ? ctrl->id : ctrl->description.code;
        r.part     = ctrl->description.feature_part;
        r.feature  = ctrl->description.feature;
        r.approach = ctrl->description.approach;
        r.dims     = ctrl->description.dimensions;
        r.location = ctrl->description.location_detail;
        r.other    = ctrl->description.other_info;
        rows.append(r);
    }

    if (rows.isEmpty())
        return;

    painter->save();

    // Font setup
    QFont font;
    const int font_px = static_cast<int>(Util::mmToPixelLogical(2.8));
    font.setPixelSize(font_px);
    painter->setFont(font);
    const QFontMetrics fm(font);
    const int row_h    = fm.height() + 4;
    const int padding  = 3;
    const int header_h = row_h + 2;

    // Column widths (pixels)
    const int col_a = fm.horizontalAdvance(QLatin1String("99")) + 2 * padding;
    const int col_b = fm.horizontalAdvance(QLatin1String("999")) + 2 * padding;
    const int col_ceg = fm.horizontalAdvance(QLatin1String("NW")) + 2 * padding;
    const int col_d = fm.horizontalAdvance(QLatin1String("Charcoal ground")) + 2 * padding;
    const int col_f = fm.horizontalAdvance(QLatin1String("10x5")) + 2 * padding;
    const int col_h = fm.horizontalAdvance(QLatin1String("Radio")) + 2 * padding;

    const int cols[] = { col_a, col_b, col_ceg, col_d, col_ceg, col_f, col_ceg, col_h };
    constexpr int NCOLS = 8;
    int total_w = 0;
    for (int c : cols) total_w += c;

    const int total_h = header_h + row_h * rows.size();

    // Position: bottom-left of widget, 8px margin
    const int margin = 8;
    const QSize wsize = widget->size();
    const int x0 = margin;
    const int y0 = wsize.height() - total_h - margin;

    // Background
    painter->setPen(QPen(course_purple, 1));
    painter->setBrush(QColor(255, 255, 255, 220));
    painter->drawRect(x0, y0, total_w, total_h);

    // Header row: course name
    painter->fillRect(x0, y0, total_w, header_h, QColor(148, 0, 211, 30));
    painter->setPen(course_purple);
    font.setBold(true);
    painter->setFont(font);
    painter->drawText(QRect(x0 + padding, y0, total_w - 2 * padding, header_h),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      visible_course->name);

    // Column separators header
    {
        int cx = x0;
        painter->setPen(QPen(course_purple, 1));
        for (int c = 0; c < NCOLS - 1; ++c)
        {
            cx += cols[c];
            painter->drawLine(cx, y0, cx, y0 + total_h);
        }
    }

    font.setBold(false);
    painter->setFont(font);

    // Data rows
    for (int i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int ry = y0 + header_h + i * row_h;

        // Row separator
        painter->setPen(QPen(QColor(180, 180, 180), 1));
        painter->drawLine(x0, ry, x0 + total_w, ry);

        painter->setPen(Qt::black);

        const QString cells[NCOLS] = {
            QString::number(r.seq),
            r.code, r.part, r.feature, r.approach, r.dims, r.location, r.other
        };
        int cx = x0;
        for (int c = 0; c < NCOLS; ++c)
        {
            painter->drawText(QRect(cx + padding, ry, cols[c] - 2 * padding, row_h),
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                              cells[c]);
            cx += cols[c];
        }
    }

    painter->restore();
}


}  // namespace OpenOrienteering
