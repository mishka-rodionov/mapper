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
#include <QCursor>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>

#include "course/course.h"
#include "course/course_control.h"
#include "course/course_database.h"
#include "core/map_coord.h"
#include "core/map_view.h"
#include "gui/map/map_widget.h"


namespace OpenOrienteering {

namespace {

// IOF course purple, per ISOM 2017-2 / ISSprOM 2019 specification.
const QColor course_purple { 148, 0, 211 };

// Standard IOF control sizes in millimeters at map scale.
constexpr qreal circle_diameter_mm  = 5.0;
constexpr qreal finish_outer_mm     = 7.0;
constexpr qreal finish_inner_mm     = 5.0;
constexpr qreal triangle_size_mm    = 6.0;
constexpr qreal line_width_mm       = 0.35;
constexpr qreal number_offset_mm    = 3.5;

// IOF Control Description cell size (mm on paper), per ISCD 2004 standard.
constexpr qreal cell_mm = 7.0;

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

    QVector<const CourseControl*> resolved;
    resolved.reserve(static_cast<int>(course.entries.size()));
    for (const auto& entry : course.entries)
    {
        if (const auto* ctrl = db.findById(entry.control_id))
            resolved.append(ctrl);
    }

    for (int i = 1; i < resolved.size(); ++i)
        paintLeg(painter, toViewport(*resolved[i-1]), toViewport(*resolved[i]));

    int seq = 1;
    for (int i = 0; i < resolved.size(); ++i)
    {
        const auto* ctrl = resolved[i];
        QPointF pos = toViewport(*ctrl);

        switch (ctrl->type)
        {
        case ControlType::Start:
        {
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

    const qreal h = side * (std::sqrt(3.0) / 2.0);
    const qreal r_outer = h * 2.0 / 3.0;
    const qreal r_inner = h / 3.0;

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
    return widget->getMapView()->lengthToPixel(mm * 1000.0);
}


// --- Mouse event handlers for legend dragging ---

bool CourseOverlay::mousePressEvent(QMouseEvent* event)
{
    if (!show_description_table || !visible_course)
        return false;
    if (event->button() != Qt::LeftButton)
        return false;
    if (legend_bounds_cache.isNull())
        return false;

    if (legend_bounds_cache.contains(event->pos()))
    {
        legend_dragging = true;
        legend_drag_offset = QPointF(event->pos()) - legend_bounds_cache.topLeft();
        widget->setCursor(Qt::ClosedHandCursor);
        return true;
    }
    return false;
}

bool CourseOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (!show_description_table || !visible_course)
        return false;

    if (legend_dragging)
    {
        const QPointF new_tl = QPointF(event->pos()) - legend_drag_offset;
        legend_anchor = widget->viewportToMapF(new_tl);
        widget->updateEverything();
        return true;
    }

    // Hover: change cursor when over legend
    if (!legend_bounds_cache.isNull() && legend_bounds_cache.contains(event->pos()))
    {
        widget->setCursor(Qt::SizeAllCursor);
        return false;  // don't consume — tool still gets the event
    }
    return false;
}

bool CourseOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (legend_dragging && event->button() == Qt::LeftButton)
    {
        legend_dragging = false;
        widget->setCursor(Qt::ArrowCursor);
        return true;
    }
    return false;
}


// ============================================================
// IOF description table
// ============================================================

void CourseOverlay::paintDescriptionTable(QPainter* painter)
{
    Q_ASSERT(visible_course);

    struct Row {
        int     seq;
        QString code;
        QString part;
        QString feature;
        QString approach;
        QString dims;
        QString location;
        QString other;
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
    painter->setRenderHint(QPainter::Antialiasing, true);

    // All cells are cell_mm × cell_mm per IOF ISCD standard
    const qreal cell_px = mmToViewportPx(cell_mm);
    const int cell = static_cast<int>(std::round(cell_px));

    // 8 equal-width columns (A–H), header row of the same height
    constexpr int NCOLS = 8;
    const int total_w = NCOLS * cell;
    const int header_h = cell;
    const int row_h    = cell;
    const int total_h  = header_h + row_h * rows.size();

    // Initialize anchor to bottom-left of widget on first display
    if (!legend_anchor_initialized)
    {
        const int margin = static_cast<int>(mmToViewportPx(5.0));
        const QPointF default_tl(margin, widget->size().height() - total_h - margin);
        legend_anchor = widget->viewportToMapF(default_tl);
        legend_anchor_initialized = true;
    }

    // Compute viewport top-left from map anchor
    const QPointF tl = widget->mapToViewport(legend_anchor);
    const int x0 = static_cast<int>(std::round(tl.x()));
    const int y0 = static_cast<int>(std::round(tl.y()));

    // Cache bounds for hit-testing in mouse events
    legend_bounds_cache = QRectF(x0, y0, total_w, total_h);

    // Background
    painter->setPen(QPen(course_purple, 1));
    painter->setBrush(QColor(255, 255, 255, 230));
    painter->drawRect(x0, y0, total_w, total_h);

    // Header: course name on purple tint
    painter->fillRect(x0, y0, total_w, header_h, QColor(148, 0, 211, 35));
    painter->setPen(course_purple);
    {
        QFont hf;
        hf.setPixelSize(std::max(8, static_cast<int>(cell_px * 0.45)));
        hf.setBold(true);
        painter->setFont(hf);
    }
    const int pad = std::max(2, cell / 8);
    painter->drawText(QRect(x0 + pad, y0, total_w - 2 * pad, header_h),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      visible_course->name);

    // Column grid lines
    painter->setPen(QPen(course_purple, 1));
    for (int c = 1; c < NCOLS; ++c)
        painter->drawLine(x0 + c * cell, y0, x0 + c * cell, y0 + total_h);

    // Data rows
    QFont df;
    df.setPixelSize(std::max(7, static_cast<int>(cell_px * 0.40)));
    painter->setFont(df);

    for (int i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int ry = y0 + header_h + i * row_h;

        // Row separator
        painter->setPen(QPen(QColor(180, 180, 180), 1));
        painter->drawLine(x0, ry, x0 + total_w, ry);

        const QString cells[NCOLS] = {
            QString::number(r.seq),
            r.code, r.part, r.feature, r.approach, r.dims, r.location, r.other
        };

        for (int c = 0; c < NCOLS; ++c)
        {
            const QRectF cell_rect(x0 + c * cell, ry, cell, row_h);
            painter->setPen(Qt::black);
            paintISCDCell(painter, cells[c], cell_rect, c);
        }
    }

    // Outer border (redraw on top of content)
    painter->setPen(QPen(course_purple, 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(x0, y0, total_w, total_h);

    painter->restore();
}


// ============================================================
// ISCD cell renderer
// ============================================================

void CourseOverlay::paintISCDCell(QPainter* painter, const QString& text,
                                   const QRectF& cell, int column) const
{
    if (text.isEmpty())
        return;

    const qreal mg = cell.width() * 0.12;
    const QRectF inner = cell.adjusted(mg, mg, -mg, -mg);

    switch (column)
    {
    case 2:  // C: feature part
        drawISCDPartSymbol(painter, text, inner);
        break;
    case 3:  // D: main feature
        drawISCDFeatureSymbol(painter, text, inner);
        break;
    case 4:  // E: appearance / approach
        drawISCDApproachSymbol(painter, text, inner);
        break;
    case 6:  // G: location detail
        drawISCDLocationSymbol(painter, text, inner);
        break;
    default:
        // Text columns: A (seq), B (code), F (dims), H (other)
        painter->setPen(Qt::black);
        painter->drawText(cell, Qt::AlignCenter | Qt::TextSingleLine, text);
        break;
    }
}


// ============================================================
// ISCD column D — main feature symbol
// ============================================================

void CourseOverlay::drawISCDFeatureSymbol(QPainter* painter,
                                           const QString& feature,
                                           const QRectF& rect) const
{
    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal w  = rect.width();
    const qreal h  = rect.height();
    const qreal r  = std::min(w, h) * 0.44;
    const qreal lw = std::max(1.0, r * 0.18);

    painter->save();
    painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::black);

    const QString f = feature.toLower();

    // ---- Terrain features ----

    if (f == QLatin1String("boulder"))
    {
        // Filled irregular polygon
        QPolygonF poly;
        poly << QPointF(cx - r * 0.25, cy - r)
             << QPointF(cx + r * 0.65, cy - r * 0.45)
             << QPointF(cx + r * 0.90, cy + r * 0.20)
             << QPointF(cx + r * 0.45, cy + r * 0.85)
             << QPointF(cx - r * 0.40, cy + r * 0.90)
             << QPointF(cx - r * 0.90, cy + r * 0.25)
             << QPointF(cx - r * 0.80, cy - r * 0.55);
        painter->drawPolygon(poly);
    }
    else if (f == QLatin1String("boulder cluster") || f == QLatin1String("cluster of boulders"))
    {
        // Three filled circles in triangular arrangement
        const qreal br = r * 0.38;
        painter->drawEllipse(QPointF(cx, cy - r * 0.45), br, br);
        painter->drawEllipse(QPointF(cx - r * 0.52, cy + r * 0.35), br, br);
        painter->drawEllipse(QPointF(cx + r * 0.52, cy + r * 0.35), br, br);
    }
    else if (f == QLatin1String("boulder field") || f == QLatin1String("stony ground"))
    {
        // Many small dots
        const qreal dr = r * 0.18;
        painter->drawEllipse(QPointF(cx - r * 0.55, cy - r * 0.55), dr, dr);
        painter->drawEllipse(QPointF(cx + r * 0.10, cy - r * 0.65), dr, dr);
        painter->drawEllipse(QPointF(cx + r * 0.60, cy - r * 0.35), dr, dr);
        painter->drawEllipse(QPointF(cx - r * 0.70, cy + r * 0.10), dr, dr);
        painter->drawEllipse(QPointF(cx + r * 0.00, cy + r * 0.10), dr, dr);
        painter->drawEllipse(QPointF(cx + r * 0.65, cy + r * 0.20), dr, dr);
        painter->drawEllipse(QPointF(cx - r * 0.35, cy + r * 0.60), dr, dr);
        painter->drawEllipse(QPointF(cx + r * 0.40, cy + r * 0.65), dr, dr);
    }
    else if (f == QLatin1String("depression") || f == QLatin1String("small depression"))
    {
        // U-shaped arc with inward tick marks (depression contour)
        painter->setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(cx - r, cy);
        path.cubicTo(cx - r, cy + r * 1.1,
                     cx + r, cy + r * 1.1,
                     cx + r, cy);
        painter->drawPath(path);
        // Tick marks pointing inward (downward into the depression)
        const qreal tick = r * 0.28;
        for (int k = 0; k < 5; ++k)
        {
            const qreal angle = M_PI * 0.15 + k * M_PI * 0.175;
            const QPointF p(cx + r * std::cos(M_PI - angle), cy + r * 0.6 * std::sin(M_PI - angle) + r * 0.4);
            // Simple inward ticks along the arc
            Q_UNUSED(p);
        }
        // Simpler: just draw 4 tick marks on the curve
        const qreal ticklen = r * 0.25;
        for (int k = 1; k <= 4; ++k)
        {
            const qreal t = k / 5.0;  // param 0..1
            const qreal bx = cx - r + 2 * r * t;
            const qreal by = cy + r * 1.1 * 4 * t * (1 - t);  // approx bezier top
            painter->drawLine(QPointF(bx, by), QPointF(bx, by + ticklen));
        }
    }
    else if (f == QLatin1String("pit") || f == QLatin1String("erosion gully"))
    {
        // Circle with filled centre dot
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), r, r);
        painter->setBrush(Qt::black);
        painter->drawEllipse(QPointF(cx, cy), r * 0.25, r * 0.25);
    }
    else if (f == QLatin1String("knoll"))
    {
        // Small filled oval
        painter->drawEllipse(QPointF(cx, cy), r * 0.65, r * 0.5);
    }
    else if (f == QLatin1String("hill"))
    {
        // Half-ellipse on baseline
        QPainterPath path;
        path.moveTo(cx - r, cy + r * 0.2);
        path.cubicTo(cx - r, cy - r * 0.9,
                     cx + r, cy - r * 0.9,
                     cx + r, cy + r * 0.2);
        path.closeSubpath();
        painter->setBrush(Qt::black);
        painter->drawPath(path);
    }
    else if (f == QLatin1String("saddle"))
    {
        // Two opposing half-arcs
        painter->setBrush(Qt::NoBrush);
        // Top arc (concave downward)
        QPainterPath p1;
        p1.moveTo(cx - r, cy - r * 0.15);
        p1.cubicTo(cx - r * 0.5, cy - r * 0.8,
                   cx + r * 0.5, cy - r * 0.8,
                   cx + r, cy - r * 0.15);
        painter->drawPath(p1);
        // Bottom arc (concave upward)
        QPainterPath p2;
        p2.moveTo(cx - r, cy + r * 0.15);
        p2.cubicTo(cx - r * 0.5, cy + r * 0.8,
                   cx + r * 0.5, cy + r * 0.8,
                   cx + r, cy + r * 0.15);
        painter->drawPath(p2);
    }
    else if (f == QLatin1String("re-entrant") || f == QLatin1String("re entrant"))
    {
        // V-shape of contour arcs pointing inward (toward center)
        painter->setBrush(Qt::NoBrush);
        QPainterPath p1;
        p1.moveTo(cx - r, cy - r * 0.1);
        p1.cubicTo(cx - r * 0.4, cy - r * 0.5, cx - r * 0.1, cy - r * 0.6, cx, cy);
        painter->drawPath(p1);
        QPainterPath p2;
        p2.moveTo(cx + r, cy - r * 0.1);
        p2.cubicTo(cx + r * 0.4, cy - r * 0.5, cx + r * 0.1, cy - r * 0.6, cx, cy);
        painter->drawPath(p2);
    }
    else if (f == QLatin1String("spur"))
    {
        // V-shape opening outward (pointing up)
        painter->setBrush(Qt::NoBrush);
        QPainterPath p1;
        p1.moveTo(cx, cy);
        p1.cubicTo(cx - r * 0.1, cy - r * 0.6, cx - r * 0.4, cy - r * 0.5, cx - r, cy + r * 0.1);
        painter->drawPath(p1);
        QPainterPath p2;
        p2.moveTo(cx, cy);
        p2.cubicTo(cx + r * 0.1, cy - r * 0.6, cx + r * 0.4, cy - r * 0.5, cx + r, cy + r * 0.1);
        painter->drawPath(p2);
    }
    else if (f == QLatin1String("earth bank") || f == QLatin1String("embankment"))
    {
        // Thick horizontal line with downward tick marks
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(Qt::black, lw * 2, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
        painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::FlatCap));
        const int nticks = 5;
        for (int k = 0; k < nticks; ++k)
        {
            const qreal tx = cx - r + (2.0 * r * k) / (nticks - 1);
            painter->drawLine(QPointF(tx, cy), QPointF(tx, cy + r * 0.55));
        }
    }
    else if (f == QLatin1String("broken ground") || f == QLatin1String("rough open land"))
    {
        // Three small irregular shapes
        const qreal dr = r * 0.22;
        painter->drawEllipse(QPointF(cx - r * 0.55, cy), dr * 1.3, dr);
        painter->drawEllipse(QPointF(cx + r * 0.10, cy - r * 0.35), dr, dr * 1.2);
        painter->drawEllipse(QPointF(cx + r * 0.55, cy + r * 0.20), dr * 1.1, dr);
    }

    // ---- Rock ----

    else if (f == QLatin1String("cliff") || f == QLatin1String("rock face"))
    {
        // Horizontal line with downward vertical marks (cliff symbol)
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(Qt::black, lw * 2.5, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(QPointF(cx - r, cy - r * 0.15), QPointF(cx + r, cy - r * 0.15));
        painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::FlatCap));
        const int nticks = 6;
        for (int k = 0; k < nticks; ++k)
        {
            const qreal tx = cx - r + (2.0 * r * k) / (nticks - 1);
            const qreal len = (k % 2 == 0) ? r * 0.7 : r * 0.45;
            painter->drawLine(QPointF(tx, cy - r * 0.15), QPointF(tx, cy - r * 0.15 + len));
        }
    }
    else if (f == QLatin1String("cave"))
    {
        // Open arch at bottom
        painter->setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(cx - r * 0.7, cy + r * 0.3);
        path.cubicTo(cx - r * 0.7, cy - r * 0.8,
                     cx + r * 0.7, cy - r * 0.8,
                     cx + r * 0.7, cy + r * 0.3);
        painter->drawPath(path);
        painter->drawLine(QPointF(cx - r * 0.7, cy + r * 0.3), QPointF(cx - r, cy + r * 0.3));
        painter->drawLine(QPointF(cx + r * 0.7, cy + r * 0.3), QPointF(cx + r, cy + r * 0.3));
    }
    else if (f == QLatin1String("rocky ground") || f == QLatin1String("rock pillars") || f == QLatin1String("stone wall"))
    {
        // Zigzag line
        painter->setBrush(Qt::NoBrush);
        QPolygonF zz;
        const int nz = 5;
        for (int k = 0; k <= nz; ++k)
        {
            const qreal zx = cx - r + (2.0 * r * k) / nz;
            const qreal zy = (k % 2 == 0) ? cy - r * 0.3 : cy + r * 0.3;
            zz << QPointF(zx, zy);
        }
        painter->drawPolyline(zz);
    }

    // ---- Water ----

    else if (f == QLatin1String("lake") || f == QLatin1String("pond") ||
             f == QLatin1String("pool"))
    {
        // Filled oval
        painter->drawEllipse(QPointF(cx, cy), r, r * 0.65);
    }
    else if (f == QLatin1String("marsh") || f == QLatin1String("narrow marsh") ||
             f == QLatin1String("indistinct marsh") || f == QLatin1String("firm ground in marsh"))
    {
        // Three vertical lines with wavy baseline
        painter->setBrush(Qt::NoBrush);
        const qreal base = cy + r * 0.5;
        // Wavy base
        QPainterPath wave;
        wave.moveTo(cx - r, base);
        wave.cubicTo(cx - r * 0.5, base - r * 0.25,
                     cx,            base + r * 0.25,
                     cx + r * 0.5,  base - r * 0.25);
        wave.cubicTo(cx + r * 0.75, base - r * 0.38,
                     cx + r,        base,
                     cx + r,        base);
        painter->drawPath(wave);
        // Three vertical strokes
        painter->drawLine(QPointF(cx - r * 0.5, cy - r * 0.5), QPointF(cx - r * 0.5, base));
        painter->drawLine(QPointF(cx,            cy - r * 0.6), QPointF(cx,           base));
        painter->drawLine(QPointF(cx + r * 0.5,  cy - r * 0.5), QPointF(cx + r * 0.5, base));
    }
    else if (f == QLatin1String("stream") || f == QLatin1String("river") ||
             f == QLatin1String("major river") || f == QLatin1String("wide stream"))
    {
        // Wavy horizontal line
        painter->setBrush(Qt::NoBrush);
        QPainterPath wave;
        wave.moveTo(cx - r, cy);
        wave.cubicTo(cx - r * 0.5, cy - r * 0.4,
                     cx,           cy + r * 0.4,
                     cx + r * 0.5, cy - r * 0.4);
        wave.cubicTo(cx + r * 0.75, cy - r * 0.55,
                     cx + r,        cy,
                     cx + r,        cy);
        painter->drawPath(wave);
    }
    else if (f == QLatin1String("ditch") || f == QLatin1String("channel") ||
             f == QLatin1String("trench"))
    {
        // Two close parallel horizontal lines
        painter->setBrush(Qt::NoBrush);
        const qreal gap = r * 0.3;
        painter->drawLine(QPointF(cx - r, cy - gap), QPointF(cx + r, cy - gap));
        painter->drawLine(QPointF(cx - r, cy + gap), QPointF(cx + r, cy + gap));
    }
    else if (f == QLatin1String("well") || f == QLatin1String("water tank") ||
             f == QLatin1String("cistern") || f == QLatin1String("spring") ||
             f == QLatin1String("source"))
    {
        // Square with cross inside
        painter->setBrush(Qt::NoBrush);
        const qreal s = r * 0.75;
        painter->drawRect(QRectF(cx - s, cy - s, 2 * s, 2 * s));
        painter->drawLine(QPointF(cx - s, cy), QPointF(cx + s, cy));
        painter->drawLine(QPointF(cx, cy - s), QPointF(cx, cy + s));
    }

    // ---- Vegetation ----

    else if (f == QLatin1String("open land") || f == QLatin1String("field") ||
             f == QLatin1String("cultivated land"))
    {
        // Three evenly spaced horizontal lines (open-land stipple)
        painter->setBrush(Qt::NoBrush);
        const qreal spacing = h / 4.0;
        for (int k = 1; k <= 3; ++k)
        {
            const qreal ly = rect.top() + spacing * k;
            painter->drawLine(QPointF(cx - r * 0.85, ly), QPointF(cx + r * 0.85, ly));
        }
    }
    else if (f == QLatin1String("distinct tree") || f == QLatin1String("distinctive tree"))
    {
        // Lollipop: circle on a vertical stick
        painter->drawEllipse(QPointF(cx, cy - r * 0.4), r * 0.45, r * 0.45);
        painter->drawLine(QPointF(cx, cy - r * 0.0), QPointF(cx, cy + r * 0.7));
        painter->drawLine(QPointF(cx - r * 0.35, cy + r * 0.7),
                          QPointF(cx + r * 0.35, cy + r * 0.7));
    }
    else if (f == QLatin1String("clearing") || f == QLatin1String("small clearing"))
    {
        // Circle outline
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), r * 0.8, r * 0.8);
    }
    else if (f == QLatin1String("copse") || f == QLatin1String("grove") ||
             f == QLatin1String("forest corner"))
    {
        // Circle with small tree inside
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), r * 0.8, r * 0.8);
        painter->setBrush(Qt::black);
        painter->drawEllipse(QPointF(cx, cy - r * 0.15), r * 0.3, r * 0.3);
    }
    else if (f == QLatin1String("linear thicket") || f == QLatin1String("hedge"))
    {
        // Elongated oval
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), r, r * 0.4);
    }
    else if (f == QLatin1String("charcoal burning ground"))
    {
        // Circle outline (historically round shape)
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), r * 0.8, r * 0.8);
        painter->setPen(QPen(Qt::black, lw));
        painter->drawLine(QPointF(cx - r * 0.5, cy), QPointF(cx + r * 0.5, cy));
        painter->drawLine(QPointF(cx, cy - r * 0.5), QPointF(cx, cy + r * 0.5));
    }

    // ---- Man-made ----

    else if (f == QLatin1String("building"))
    {
        // Filled square
        const qreal s = r * 0.72;
        painter->drawRect(QRectF(cx - s, cy - s, 2 * s, 2 * s));
    }
    else if (f == QLatin1String("ruin") || f == QLatin1String("ruined building"))
    {
        // Dashed square outline
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(Qt::black, lw, Qt::DashLine, Qt::FlatCap));
        const qreal s = r * 0.72;
        painter->drawRect(QRectF(cx - s, cy - s, 2 * s, 2 * s));
    }
    else if (f == QLatin1String("wall") || f == QLatin1String("stone wall"))
    {
        // Thick horizontal line
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(Qt::black, lw * 3.5, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
    }
    else if (f == QLatin1String("fence") || f == QLatin1String("crossing point"))
    {
        // Thin line with perpendicular ticks
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
        const qreal tick = r * 0.3;
        for (int k = 0; k <= 4; ++k)
        {
            const qreal tx = cx - r + (2.0 * r * k) / 4;
            painter->drawLine(QPointF(tx, cy - tick), QPointF(tx, cy + tick));
        }
    }
    else if (f == QLatin1String("path") || f == QLatin1String("track") ||
             f == QLatin1String("footpath") || f == QLatin1String("narrow ride"))
    {
        // Dashed horizontal line
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(Qt::black, lw * 1.5, Qt::DashLine, Qt::FlatCap));
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
    }
    else if (f == QLatin1String("paved area") || f == QLatin1String("road"))
    {
        // Filled rectangle (narrower than full cell)
        const qreal pw = r * 0.5;
        painter->drawRect(QRectF(cx - r, cy - pw, 2 * r, 2 * pw));
    }
    else if (f == QLatin1String("bridge"))
    {
        // Horizontal line with short perpendiculars at ends
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
        const qreal cap = r * 0.35;
        painter->drawLine(QPointF(cx - r, cy - cap), QPointF(cx - r, cy + cap));
        painter->drawLine(QPointF(cx + r, cy - cap), QPointF(cx + r, cy + cap));
    }
    else if (f == QLatin1String("tower"))
    {
        // T / cross shape
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(cx, cy - r), QPointF(cx, cy + r));
        painter->drawLine(QPointF(cx - r * 0.65, cy - r * 0.3),
                          QPointF(cx + r * 0.65, cy - r * 0.3));
    }
    else if (f == QLatin1String("monument") || f == QLatin1String("statue") ||
             f == QLatin1String("cairn") || f == QLatin1String("boundary stone"))
    {
        // Diamond/square on point
        painter->setBrush(Qt::black);
        const qreal s = r * 0.55;
        QPolygonF diamond;
        diamond << QPointF(cx, cy - s)
                << QPointF(cx + s, cy)
                << QPointF(cx, cy + s)
                << QPointF(cx - s, cy);
        painter->drawPolygon(diamond);
    }
    else if (f == QLatin1String("anthill") || f == QLatin1String("termite mound"))
    {
        // Hill shape (like knoll but rounder)
        QPainterPath path;
        path.moveTo(cx - r, cy + r * 0.25);
        path.cubicTo(cx - r, cy - r * 0.9,
                     cx + r, cy - r * 0.9,
                     cx + r, cy + r * 0.25);
        painter->setBrush(Qt::black);
        path.closeSubpath();
        painter->drawPath(path);
    }
    else
    {
        // Fallback: show abbreviated text
        painter->setPen(Qt::black);
        QFont small;
        small.setPixelSize(std::max(6, static_cast<int>(std::min(w, h) * 0.38)));
        painter->setFont(small);
        painter->drawText(rect.adjusted(-rect.width()*0.12, -rect.height()*0.12,
                                        rect.width()*0.12,  rect.height()*0.12),
                          Qt::AlignCenter | Qt::TextSingleLine | Qt::TextWordWrap,
                          feature.left(6));
    }

    painter->restore();
}


// ============================================================
// ISCD column C — part of feature
// ============================================================

void CourseOverlay::drawISCDPartSymbol(QPainter* painter,
                                        const QString& part,
                                        const QRectF& rect) const
{
    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal r  = std::min(rect.width(), rect.height()) * 0.44;
    const qreal lw = std::max(1.0, r * 0.16);

    painter->save();
    painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::black);

    const QString p = part.toLower().trimmed();

    if (p == QLatin1String("upper") || p == QLatin1String("top"))
    {
        // Horizontal bar near top
        painter->drawLine(QPointF(cx - r, cy - r * 0.55), QPointF(cx + r, cy - r * 0.55));
    }
    else if (p == QLatin1String("lower") || p == QLatin1String("bottom"))
    {
        // Horizontal bar near bottom
        painter->drawLine(QPointF(cx - r, cy + r * 0.55), QPointF(cx + r, cy + r * 0.55));
    }
    else if (p == QLatin1String("middle"))
    {
        // Horizontal bar at center
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
    }
    else
    {
        // Cardinal/intercardinal direction: bold letter
        QFont f;
        f.setPixelSize(std::max(7, static_cast<int>(r * 1.5)));
        f.setBold(true);
        painter->setFont(f);
        painter->drawText(rect.adjusted(-rect.width()*0.12, -rect.height()*0.12,
                                        rect.width()*0.12,  rect.height()*0.12),
                          Qt::AlignCenter | Qt::TextSingleLine,
                          part.toUpper());
    }

    painter->restore();
}


// ============================================================
// ISCD column E — appearance / approach
// ============================================================

void CourseOverlay::drawISCDApproachSymbol(QPainter* painter,
                                            const QString& approach,
                                            const QRectF& rect) const
{
    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal r  = std::min(rect.width(), rect.height()) * 0.44;
    const qreal lw = std::max(1.0, r * 0.16);

    painter->save();
    painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);

    const QString a = approach.toLower().trimmed();

    if (a == QLatin1String("shallow"))
    {
        // Shallow open U arc
        QPainterPath path;
        path.moveTo(cx - r, cy);
        path.cubicTo(cx - r, cy + r * 0.5, cx + r, cy + r * 0.5, cx + r, cy);
        painter->drawPath(path);
    }
    else if (a == QLatin1String("deep"))
    {
        // Deep V shape
        QPolygonF v;
        v << QPointF(cx - r, cy - r * 0.3)
          << QPointF(cx,     cy + r * 0.7)
          << QPointF(cx + r, cy - r * 0.3);
        painter->drawPolyline(v);
    }
    else if (a == QLatin1String("overgrown") || a == QLatin1String("vegetation"))
    {
        // Small tree dot
        painter->setBrush(Qt::black);
        painter->drawEllipse(QPointF(cx, cy - r * 0.3), r * 0.45, r * 0.45);
        painter->drawLine(QPointF(cx, cy + r * 0.1), QPointF(cx, cy + r * 0.6));
    }
    else if (a == QLatin1String("open"))
    {
        // Open rectangle
        painter->drawRect(QRectF(cx - r * 0.8, cy - r * 0.5, 1.6 * r, r));
    }
    else if (a == QLatin1String("rocky") || a == QLatin1String("stony"))
    {
        // Small dots in a line
        painter->setBrush(Qt::black);
        const qreal dr = r * 0.2;
        painter->drawEllipse(QPointF(cx - r * 0.55, cy), dr, dr);
        painter->drawEllipse(QPointF(cx,             cy), dr, dr);
        painter->drawEllipse(QPointF(cx + r * 0.55,  cy), dr, dr);
    }
    else if (a == QLatin1String("marshy") || a == QLatin1String("wet"))
    {
        // Wavy line
        QPainterPath wave;
        wave.moveTo(cx - r, cy);
        wave.cubicTo(cx - r * 0.5, cy - r * 0.4, cx, cy + r * 0.4, cx + r * 0.5, cy - r * 0.4);
        wave.cubicTo(cx + r * 0.75, cy - r * 0.55, cx + r, cy, cx + r, cy);
        painter->drawPath(wave);
    }
    else if (a == QLatin1String("sandy"))
    {
        // Dotted pattern (3×3 dots)
        painter->setBrush(Qt::black);
        const qreal dr = r * 0.15;
        const qreal sp = r * 0.45;
        for (int row = -1; row <= 1; ++row)
            for (int col = -1; col <= 1; ++col)
                painter->drawEllipse(QPointF(cx + col * sp, cy + row * sp), dr, dr);
    }
    else if (a == QLatin1String("ruined") || a == QLatin1String("deteriorated"))
    {
        // Dashed square
        painter->setPen(QPen(Qt::black, lw, Qt::DashLine, Qt::FlatCap));
        const qreal s = r * 0.72;
        painter->drawRect(QRectF(cx - s, cy - s, 2 * s, 2 * s));
    }
    else
    {
        // Fallback text
        QFont f;
        f.setPixelSize(std::max(6, static_cast<int>(r * 1.2)));
        painter->setFont(f);
        painter->setPen(Qt::black);
        painter->drawText(rect.adjusted(-rect.width()*0.12, -rect.height()*0.12,
                                        rect.width()*0.12,  rect.height()*0.12),
                          Qt::AlignCenter | Qt::TextSingleLine,
                          approach.left(6));
    }

    painter->restore();
}


// ============================================================
// ISCD column G — location detail
// ============================================================

void CourseOverlay::drawISCDLocationSymbol(QPainter* painter,
                                            const QString& location,
                                            const QRectF& rect) const
{
    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal r  = std::min(rect.width(), rect.height()) * 0.44;
    const qreal lw = std::max(1.0, r * 0.16);
    const qreal dr = r * 0.22;

    painter->save();
    painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::black);

    const QString loc = location.toLower().trimmed();

    if (loc == QLatin1String("top") || loc == QLatin1String("summit"))
    {
        // Filled dot at upper centre
        painter->drawEllipse(QPointF(cx, cy - r * 0.45), dr, dr);
    }
    else if (loc == QLatin1String("foot") || loc == QLatin1String("base"))
    {
        // Filled dot at lower centre
        painter->drawEllipse(QPointF(cx, cy + r * 0.45), dr, dr);
    }
    else if (loc == QLatin1String("edge") || loc == QLatin1String("margin"))
    {
        // Dot at right edge
        painter->drawEllipse(QPointF(cx + r * 0.55, cy), dr, dr);
    }
    else if (loc == QLatin1String("corner"))
    {
        // Right-angle at top-right
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(cx + r * 0.6, cy - r * 0.6),
                          QPointF(cx + r * 0.6, cy + r * 0.1));
        painter->drawLine(QPointF(cx + r * 0.6, cy + r * 0.1),
                          QPointF(cx - r * 0.1, cy + r * 0.1));
    }
    else if (loc == QLatin1String("tip") || loc == QLatin1String("end"))
    {
        // Dot at right
        painter->drawEllipse(QPointF(cx + r * 0.55, cy), dr, dr);
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx + r * 0.33, cy));
    }
    else if (loc == QLatin1String("junction") || loc == QLatin1String("intersection"))
    {
        // Cross (+)
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
        painter->drawLine(QPointF(cx, cy - r), QPointF(cx, cy + r));
    }
    else if (loc == QLatin1String("between") || loc == QLatin1String("in"))
    {
        // Two dots with space between
        painter->drawEllipse(QPointF(cx - r * 0.5, cy), dr, dr);
        painter->drawEllipse(QPointF(cx + r * 0.5, cy), dr, dr);
    }
    else if (loc == QLatin1String("north side") || loc == QLatin1String("n side"))
    {
        painter->drawEllipse(QPointF(cx, cy - r * 0.5), dr, dr);
    }
    else if (loc == QLatin1String("south side") || loc == QLatin1String("s side"))
    {
        painter->drawEllipse(QPointF(cx, cy + r * 0.5), dr, dr);
    }
    else if (loc == QLatin1String("east side") || loc == QLatin1String("e side"))
    {
        painter->drawEllipse(QPointF(cx + r * 0.5, cy), dr, dr);
    }
    else if (loc == QLatin1String("west side") || loc == QLatin1String("w side"))
    {
        painter->drawEllipse(QPointF(cx - r * 0.5, cy), dr, dr);
    }
    else if (!location.isEmpty())
    {
        // Fallback text
        QFont f;
        f.setPixelSize(std::max(6, static_cast<int>(r * 1.2)));
        painter->setFont(f);
        painter->setPen(Qt::black);
        painter->drawText(rect.adjusted(-rect.width()*0.12, -rect.height()*0.12,
                                        rect.width()*0.12,  rect.height()*0.12),
                          Qt::AlignCenter | Qt::TextSingleLine,
                          location.left(6));
    }

    painter->restore();
}


}  // namespace OpenOrienteering
