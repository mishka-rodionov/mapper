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
#include <QTransform>
#include <QVector>

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include "core/map.h"
#include "course/course.h"
#include "course/course_control.h"
#include "course/course_database.h"
#include "course/course_undo.h"
#include "core/map_coord.h"
#include "core/map_view.h"
#include "gui/map/map_widget.h"


namespace OpenOrienteering {

// ── Custom symbol paths (edited in ISCDSymbolEditor) ─────────────────────────

QJsonObject CourseOverlay::s_custom;
static bool s_custom_loaded = false;

static void ensureCustomPathsLoaded()
{
    if (s_custom_loaded) return;
    s_custom_loaded = true;
    const QString path = QCoreApplication::applicationDirPath()
                         + QLatin1String("/iscd_symbols.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject())
        CourseOverlay::setCustomSymbolPaths(doc.object());
}

void CourseOverlay::loadCustomSymbolPaths(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject())
        s_custom = doc.object();
    s_custom_loaded = true;
}

void CourseOverlay::saveCustomSymbolPaths(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(s_custom).toJson());
}

QJsonObject CourseOverlay::customSymbolPaths() { return s_custom; }
void CourseOverlay::setCustomSymbolPaths(const QJsonObject& d)
{
    s_custom = d;
    s_custom_loaded = true;
}

// ─────────────────────────────────────────────────────────────────────────────

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

qreal boundedDescriptionScale(double scale)
{
    if (!(scale > 0.0))
        return 1.0;
    if (scale < 0.25)
        return 0.25;
    if (scale > 2.0)
        return 2.0;
    return static_cast<qreal>(scale);
}

QRectF legendResizeHandleRect(const QRectF& bounds)
{
    const qreal size = 12.0;
    return QRectF(bounds.right() - size, bounds.bottom() - size, size, size);
}

bool courseUsesControl(const Course& course, const QString& control_id)
{
    for (const auto& entry : course.entries)
    {
        if (entry.control_id == control_id)
            return true;
    }
    return false;
}

// ── Custom-path rendering helpers ────────────────────────────────────────────

// Compute a polyline approximating the arc through 3 points (screen coords).
QPolygonF arcPolyline(QPointF p1, QPointF pmid, QPointF p3)
{
    const double ax = p1.x(),   ay = p1.y();
    const double bx = pmid.x(), by = pmid.y();
    const double cx = p3.x(),   cy = p3.y();
    const double D  = 2.0*(ax*(by-cy)+bx*(cy-ay)+cx*(ay-by));
    if (std::abs(D) < 1e-4) { QPolygonF l; l << p1 << p3; return l; }
    const double ux = ((ax*ax+ay*ay)*(by-cy)+(bx*bx+by*by)*(cy-ay)+(cx*cx+cy*cy)*(ay-by))/D;
    const double uy = ((ax*ax+ay*ay)*(cx-bx)+(bx*bx+by*by)*(ax-cx)+(cx*cx+cy*cy)*(bx-ax))/D;
    const double R  = std::hypot(ax-ux, ay-uy);
    auto ang = [&](double px, double py){ return std::atan2(py-uy, px-ux); };
    const double a1 = ang(ax,ay), a2 = ang(bx,by), a3 = ang(cx,cy);
    double sweep = a3-a1; if (sweep <= 0) sweep += 2*M_PI;
    double a2r = a2-a1;   if (a2r <= 0)  a2r += 2*M_PI;
    if (a2r > sweep) sweep -= 2*M_PI;
    const int n = std::max(12, static_cast<int>(std::abs(sweep)*R/3.0));
    QPolygonF poly;
    for (int i = 0; i <= n; ++i)
        poly << QPointF(ux + R*std::cos(a1 + sweep*i/n), uy + R*std::sin(a1 + sweep*i/n));
    return poly;
}

// Try to render symbol from custom JSON paths; returns true if drawn.
bool tryDrawCustomPath(QPainter* painter, const QString& key, const QRectF& rect, qreal lw)
{
    ensureCustomPathsLoaded();
    const QJsonObject all = CourseOverlay::customSymbolPaths();
    if (!all.contains(key)) return false;
    const QJsonArray strokes = all[key].toArray();
    if (strokes.isEmpty()) return false;

    const double cx    = rect.center().x();
    const double cy    = rect.center().y();
    const double scale = std::min(rect.width(), rect.height()) * 0.5;
    auto u2p = [&](const QJsonArray& pt) -> QPointF {
        return { cx + pt[0].toDouble()*scale, cy + pt[1].toDouble()*scale };
    };

    painter->save();
    const QPen outlinePen { Qt::black, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin };
    painter->setPen(outlinePen);
    painter->setBrush(Qt::NoBrush);
    for (const QJsonValue& sv : strokes) {
        const QJsonObject s   = sv.toObject();
        const QJsonArray  pts = s[QLatin1String("p")].toArray();
        const QString     t   = s[QLatin1String("t")].toString();
        if (t == QLatin1String("L") && pts.size() >= 2) {
            painter->drawLine(u2p(pts[0].toArray()), u2p(pts[1].toArray()));
        } else if (t == QLatin1String("A") && pts.size() >= 3) {
            painter->drawPolyline(arcPolyline(u2p(pts[0].toArray()),
                                              u2p(pts[1].toArray()),
                                              u2p(pts[2].toArray())));
        } else if (t == QLatin1String("O") && pts.size() >= 2) {
            painter->drawEllipse(QRectF(u2p(pts[0].toArray()), u2p(pts[1].toArray())).normalized());
        } else if (t == QLatin1String("C") && pts.size() >= 2) {
            const QPointF c = u2p(pts[0].toArray());
            const QPointF e = u2p(pts[1].toArray());
            const double  r = std::hypot(e.x()-c.x(), e.y()-c.y());
            painter->drawEllipse(c, r, r);
        } else if (t == QLatin1String("R") && pts.size() >= 2) {
            painter->drawRect(QRectF(u2p(pts[0].toArray()), u2p(pts[1].toArray())).normalized());
        } else if (t == QLatin1String("P") && pts.size() >= 1) {
            const QPointF c = u2p(pts[0].toArray());
            const double  r = s[QLatin1String("d")].toDouble(0.1) * scale * 0.5;
            painter->setPen(Qt::NoPen);
            painter->setBrush(Qt::black);
            painter->drawEllipse(c, r, r);
            painter->setPen(outlinePen);
            painter->setBrush(Qt::NoBrush);
        }
    }
    painter->restore();
    return true;
}

}  // anonymous namespace


CourseOverlay::CourseOverlay(MapWidget* widget, CourseDatabase& db, QObject* parent)
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

void CourseOverlay::setPlanningActive(bool active)
{
    if (planning_active == active)
        return;

    planning_active = active;
    if (!planning_active)
    {
        legend_dragging = false;
        legend_resizing = false;
        number_dragging = false;
        number_drag_control_id.clear();
        number_hit_cache.clear();
        legend_bounds_cache = {};
        legend_resize_handle_cache = {};
    }

    widget->updateEverything();
}

void CourseOverlay::onDatabaseChanged()
{
    if (planning_active)
        widget->updateEverything();
}


// --- Main paint entry point ---

void CourseOverlay::paint(QPainter* painter)
{
    PaintContext context;
    paint(painter, context);
}

void CourseOverlay::paintForPrint(QPainter* painter, const QTransform& map_to_painter,
                                  const QSizeF& page_size, qreal pixels_per_mm)
{
    PaintContext context;
    context.map_to_viewport = &map_to_painter;
    context.viewport_size = page_size;
    context.pixels_per_mm = pixels_per_mm;
    context.interactive = false;
    paint(painter, context);
}

void CourseOverlay::paint(QPainter* painter, const PaintContext& context)
{
    if (context.interactive)
    {
        number_hit_cache.clear();
        if (!planning_active)
            return;
    }

    if (db.numControls() == 0)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (context.interactive)
        paintAllControls(painter, context);

    if (visible_course)
        paintCourse(painter, *visible_course, context);

    painter->restore();

    if (show_description_table && visible_course)
        paintDescriptionTable(painter, context);
}


// --- Course / controls rendering ---

void CourseOverlay::paintCourse(QPainter* painter, const Course& course, const PaintContext& context) const
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
        paintLeg(painter, toViewport(*resolved[i-1], context), toViewport(*resolved[i], context), context);

    int seq = 1;
    for (int i = 0; i < resolved.size(); ++i)
    {
        const auto* ctrl = resolved[i];
        QPointF pos = toViewport(*ctrl, context);

        switch (ctrl->type)
        {
        case ControlType::Start:
        {
            double rot = 0.0;
            if (i + 1 < resolved.size())
            {
                QPointF next = toViewport(*resolved[i+1], context);
                QPointF d = next - pos;
                rot = std::atan2(d.y(), d.x());
            }
            paintStart(painter, pos, rot, context);
            break;
        }
        case ControlType::Finish:
            paintFinish(painter, pos, context);
            break;
        case ControlType::CrossingPoint:
            paintCrossingPoint(painter, pos, context);
            break;
        default:
            paintControl(painter, pos, *ctrl, QString::number(seq), context);
            ++seq;
            break;
        }
    }
}

void CourseOverlay::paintAllControls(QPainter* painter, const PaintContext& context) const
{
    for (int i = 0; i < db.numControls(); ++i)
    {
        const auto& ctrl = db.control(i);
        if (visible_course && courseUsesControl(*visible_course, ctrl.id))
            continue;

        QPointF pos = toViewport(ctrl, context);

        switch (ctrl.type)
        {
        case ControlType::Start:
            paintStart(painter, pos, 0.0, context);
            break;
        case ControlType::Finish:
            paintFinish(painter, pos, context);
            break;
        case ControlType::CrossingPoint:
            paintCrossingPoint(painter, pos, context);
            break;
        default:
            paintControl(painter, pos, ctrl, ctrl.description.code.isEmpty() ? ctrl.id : ctrl.description.code, context);
            break;
        }
    }
}


// --- Individual symbol painters ---

void CourseOverlay::paintLeg(QPainter* painter, QPointF from, QPointF to, const PaintContext& context) const
{
    const qreal lw = mmToViewportPx(line_width_mm, context);
    painter->setPen(QPen(course_purple, lw));
    painter->setBrush(Qt::NoBrush);

    const qreal r = mmToViewportPx(circle_diameter_mm / 2.0, context);
    QPointF d = to - from;
    const qreal len = std::sqrt(d.x() * d.x() + d.y() * d.y());
    if (len < 2 * r)
        return;

    const qreal f = r / len;
    QPointF start = from + f * d;
    QPointF end   = to   - f * d;

    painter->drawLine(start, end);
}

void CourseOverlay::paintStart(QPainter* painter, QPointF pos, double rotation_rad, const PaintContext& context) const
{
    const qreal lw   = mmToViewportPx(line_width_mm, context);
    const qreal side = mmToViewportPx(triangle_size_mm, context);

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

void CourseOverlay::paintControl(QPainter* painter, QPointF pos, const CourseControl& ctrl, const QString& number, const PaintContext& context) const
{
    const qreal lw = mmToViewportPx(line_width_mm, context);
    const qreal r  = mmToViewportPx(circle_diameter_mm / 2.0, context);

    painter->setPen(QPen(course_purple, lw));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(pos, r, r);

    paintControlNumber(painter, pos, ctrl, number, context);
}

void CourseOverlay::paintFinish(QPainter* painter, QPointF pos, const PaintContext& context) const
{
    const qreal lw      = mmToViewportPx(line_width_mm, context);
    const qreal r_outer = mmToViewportPx(finish_outer_mm / 2.0, context);
    const qreal r_inner = mmToViewportPx(finish_inner_mm / 2.0, context);

    painter->setPen(QPen(course_purple, lw));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(pos, r_outer, r_outer);
    painter->drawEllipse(pos, r_inner, r_inner);
}

void CourseOverlay::paintCrossingPoint(QPainter* painter, QPointF pos, const PaintContext& context) const
{
    const qreal lw = mmToViewportPx(line_width_mm, context);
    const qreal r  = mmToViewportPx(2.5, context);

    painter->setPen(QPen(course_purple, lw));
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(pos.x() - r, pos.y() - r), QPointF(pos.x() + r, pos.y() + r));
    painter->drawLine(QPointF(pos.x() - r, pos.y() + r), QPointF(pos.x() + r, pos.y() - r));
}

void CourseOverlay::paintControlNumber(QPainter* painter, QPointF center, const CourseControl& ctrl, const QString& number, const PaintContext& context) const
{
    if (number.isEmpty())
        return;

    QFont font;
    font.setPixelSize(static_cast<int>(mmToViewportPx(3.0, context)));
    font.setBold(true);
    painter->setFont(font);

    painter->setPen(course_purple);

    const QPointF text_pos = numberTextPosition(center, ctrl, context);
    painter->drawText(text_pos, number);
    rememberNumberHit(painter, ctrl, number, text_pos, context);
}

QPointF CourseOverlay::numberOffsetToViewport(const CourseControl& ctrl, const PaintContext& context) const
{
    const MapCoordF position(ctrl.position);
    return toViewport(position + ctrl.number_offset, context) - toViewport(position, context);
}

QPointF CourseOverlay::numberTextPosition(QPointF center, const CourseControl& ctrl, const PaintContext& context) const
{
    const qreal r      = mmToViewportPx(circle_diameter_mm / 2.0, context);
    const qreal offset = mmToViewportPx(number_offset_mm, context);
    const QPointF default_pos(center.x() + r + offset * 0.3, center.y() - r - offset * 0.1);
    return default_pos + numberOffsetToViewport(ctrl, context);
}

void CourseOverlay::rememberNumberHit(QPainter* painter, const CourseControl& ctrl, const QString& number, QPointF text_pos, const PaintContext& context) const
{
    if (!context.interactive || number.isEmpty())
        return;

    const QFontMetricsF metrics(painter->font());
    auto bounds = metrics.boundingRect(number).translated(text_pos);
    bounds.adjust(-4.0, -4.0, 4.0, 4.0);
    number_hit_cache.push_back(NumberHit { ctrl.id, bounds });
}

int CourseOverlay::controlIndex(const QString& id) const
{
    for (int i = 0; i < db.numControls(); ++i)
    {
        if (db.control(i).id == id)
            return i;
    }
    return -1;
}

const CourseOverlay::NumberHit* CourseOverlay::numberHitAt(const QPoint& pos) const
{
    for (int i = number_hit_cache.size() - 1; i >= 0; --i)
    {
        if (number_hit_cache[i].bounds.contains(pos))
            return &number_hit_cache[i];
    }
    return nullptr;
}


// --- Coordinate conversion ---

QPointF CourseOverlay::toViewport(const CourseControl& ctrl, const PaintContext& context) const
{
    return toViewport(MapCoordF(ctrl.position), context);
}

QPointF CourseOverlay::toViewport(const MapCoordF& coord, const PaintContext& context) const
{
    if (context.map_to_viewport)
        return context.map_to_viewport->map(static_cast<QPointF>(coord));

    return widget->mapToViewport(static_cast<QPointF>(coord));
}

MapCoordF CourseOverlay::viewportToMap(const QPointF& point, const PaintContext& context) const
{
    if (context.map_to_viewport)
    {
        bool invertible = false;
        const auto viewport_to_map = context.map_to_viewport->inverted(&invertible);
        if (invertible)
            return MapCoordF(viewport_to_map.map(point));
    }

    return widget->viewportToMapF(point);
}

QSizeF CourseOverlay::viewportSize(const PaintContext& context) const
{
    if (context.viewport_size.isValid() && !context.viewport_size.isEmpty())
        return context.viewport_size;

    return widget->size();
}

qreal CourseOverlay::mmToViewportPx(qreal mm, const PaintContext& context) const
{
    if (context.map_to_viewport)
        return mm * context.pixels_per_mm;

    return widget->getMapView()->lengthToPixel(mm * 1000.0);
}


// --- Mouse event handlers for legend dragging/resizing ---

bool CourseOverlay::mousePressEvent(QMouseEvent* event)
{
    if (!planning_active)
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    if (show_description_table && visible_course && !legend_bounds_cache.isNull()
        && legend_resize_handle_cache.contains(event->pos()))
    {
        legend_resizing = true;
        legend_resize_start_bounds = legend_bounds_cache;
        legend_resize_start_scale = boundedDescriptionScale(visible_course->description_scale);
        legend_resize_current_scale = legend_resize_start_scale;
        widget->setCursor(Qt::SizeFDiagCursor);
        return true;
    }

    if (show_description_table && visible_course && !legend_bounds_cache.isNull()
        && legend_bounds_cache.contains(event->pos()))
    {
        legend_dragging = true;
        legend_drag_offset = QPointF(event->pos()) - legend_bounds_cache.topLeft();
        widget->setCursor(Qt::ClosedHandCursor);
        return true;
    }

    if (const auto* hit = numberHitAt(event->pos()))
    {
        const int index = controlIndex(hit->control_id);
        if (index < 0)
            return false;

        number_dragging = true;
        number_drag_control_id = hit->control_id;
        number_drag_start_map = widget->viewportToMapF(event->pos());
        number_drag_start_offset = db.control(index).number_offset;
        widget->setCursor(Qt::ClosedHandCursor);
        return true;
    }

    return false;
}

bool CourseOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (!planning_active)
        return false;

    if (number_dragging)
    {
        const int index = controlIndex(number_drag_control_id);
        if (index >= 0)
        {
            auto updated = db.control(index);
            const auto current_map = widget->viewportToMapF(event->pos());
            updated.number_offset = number_drag_start_offset + (current_map - number_drag_start_map);
            db.updateControl(index, updated);
        }

        widget->setCursor(Qt::ClosedHandCursor);
        return true;
    }

    if (show_description_table && visible_course && legend_resizing)
    {
        const QPointF base = legend_resize_start_bounds.topLeft();
        const QPointF start_delta = legend_resize_start_bounds.bottomRight() - base;
        const QPointF current_delta = QPointF(event->pos()) - base;
        const qreal denom = QPointF::dotProduct(start_delta, start_delta);
        if (denom > 0.0)
        {
            const qreal ratio = QPointF::dotProduct(current_delta, start_delta) / denom;
            const double scale = boundedDescriptionScale(legend_resize_start_scale * ratio);
            if (scale != legend_resize_current_scale)
            {
                legend_resize_current_scale = scale;
                emit visibleCourseDescriptionScaleChangeRequested(scale, false);
                widget->updateEverything();
            }
        }
        return true;
    }

    if (show_description_table && visible_course && legend_dragging)
    {
        const QPointF new_tl = QPointF(event->pos()) - legend_drag_offset;
        legend_anchor = widget->viewportToMapF(new_tl);
        widget->updateEverything();
        return true;
    }

    if (show_description_table && visible_course && !legend_resize_handle_cache.isNull()
        && legend_resize_handle_cache.contains(event->pos()))
    {
        widget->setCursor(Qt::SizeFDiagCursor);
        return false;
    }

    // Hover: change cursor when over legend
    if (show_description_table && visible_course && !legend_bounds_cache.isNull()
        && legend_bounds_cache.contains(event->pos()))
    {
        widget->setCursor(Qt::SizeAllCursor);
        return false;  // don't consume — tool still gets the event
    }

    if (numberHitAt(event->pos()))
    {
        widget->setCursor(Qt::OpenHandCursor);
        return false;
    }

    return false;
}

bool CourseOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (!planning_active)
        return false;

    if (number_dragging && event->button() == Qt::LeftButton)
    {
        const int index = controlIndex(number_drag_control_id);
        if (index >= 0)
        {
            const auto new_offset = db.control(index).number_offset;
            if (new_offset != number_drag_start_offset)
            {
                auto* map = widget->getMapView()->getMap();
                map->push(new MoveControlNumberUndoStep(
                    map, number_drag_control_id, number_drag_start_offset, new_offset));
            }
        }

        number_dragging = false;
        number_drag_control_id.clear();
        widget->setCursor(Qt::ArrowCursor);
        return true;
    }

    if (legend_resizing && event->button() == Qt::LeftButton)
    {
        legend_resizing = false;
        emit visibleCourseDescriptionScaleChangeRequested(legend_resize_current_scale, true);
        widget->setCursor(Qt::ArrowCursor);
        return true;
    }

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

qreal CourseOverlay::computeCourseDistanceM(const Course& course) const
{
    if (course.entries.size() < 2)
        return 0.0;

    QVector<const CourseControl*> resolved;
    resolved.reserve(static_cast<int>(course.entries.size()));
    for (const auto& entry : course.entries)
    {
        if (const auto* ctrl = db.findById(entry.control_id))
            resolved.append(ctrl);
    }

    if (resolved.size() < 2)
        return 0.0;

    qreal total_mm = 0.0;
    for (int i = 1; i < resolved.size(); ++i)
    {
        const MapCoordF p1(resolved[i-1]->position);
        const MapCoordF p2(resolved[i  ]->position);
        const qreal dx = p1.x() - p2.x();
        const qreal dy = p1.y() - p2.y();
        total_mm += std::sqrt(dx * dx + dy * dy);
    }

    const Map* map = widget->getMapView()->getMap();
    const qreal scale = map ? static_cast<qreal>(map->getScaleDenominator()) : 10000.0;
    return total_mm * scale / 1000.0;
}


void CourseOverlay::paintStartCell(QPainter* painter, const QRectF& cell) const
{
    const qreal cx  = cell.center().x();
    const qreal cy  = cell.center().y();
    const qreal r   = std::min(cell.width(), cell.height()) * 0.36;
    const qreal lw  = std::max(1.0, r * 0.18);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(course_purple, lw));
    painter->setBrush(Qt::NoBrush);

    QPolygonF tri;
    for (int k = 0; k < 3; ++k)
    {
        const double angle = -M_PI / 2.0 + k * (2.0 * M_PI / 3.0);
        tri << QPointF(cx + r * std::cos(angle), cy + r * std::sin(angle));
    }
    painter->drawPolygon(tri);
    painter->restore();
}


void CourseOverlay::paintFinishCell(QPainter* painter, const QRectF& cell) const
{
    const qreal cx      = cell.center().x();
    const qreal cy      = cell.center().y();
    const qreal r_outer = std::min(cell.width(), cell.height()) * 0.38;
    const qreal r_inner = r_outer * (finish_inner_mm / finish_outer_mm);
    const qreal lw      = std::max(1.0, r_outer * 0.15);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(course_purple, lw));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF(cx, cy), r_outer, r_outer);
    painter->drawEllipse(QPointF(cx, cy), r_inner, r_inner);
    painter->restore();
}


void CourseOverlay::paintDescriptionTable(QPainter* painter, const PaintContext& context)
{
    Q_ASSERT(visible_course);
    if (context.interactive)
    {
        legend_bounds_cache = QRectF();
        legend_resize_handle_cache = QRectF();
    }

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

    // Collect regular-control rows and detect Start/Finish presence
    QVector<Row> rows;
    const CourseControl* start_ctrl  = nullptr;
    const CourseControl* finish_ctrl = nullptr;
    int seq = 1;
    for (const auto& entry : visible_course->entries)
    {
        const auto* ctrl = db.findById(entry.control_id);
        if (!ctrl)
            continue;
        if (ctrl->type == ControlType::Start)
        {
            start_ctrl = ctrl;
        }
        else if (ctrl->type == ControlType::Finish)
        {
            finish_ctrl = ctrl;
        }
        else if (ctrl->type == ControlType::Regular)
        {
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
    }

    const bool has_start  = (start_ctrl  != nullptr);
    const bool has_finish = (finish_ctrl != nullptr);

    // Need at least something to display
    if (rows.isEmpty() && !has_start && !has_finish)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal description_scale = boundedDescriptionScale(visible_course->description_scale);
    const qreal cell_px = mmToViewportPx(cell_mm * description_scale, context);
    const int cell = static_cast<int>(std::round(cell_px));

    constexpr int NCOLS = 8;
    const int total_w  = NCOLS * cell;
    const int header_h = 2 * cell;  // two-line header: name + info row
    const int row_h    = cell;
    const int total_h  = header_h
                       + (has_start  ? row_h : 0)
                       + row_h * rows.size()
                       + (has_finish ? row_h : 0);

    // Initialize anchor (default: bottom-left of widget)
    if (!legend_anchor_initialized)
    {
        const int margin = static_cast<int>(mmToViewportPx(5.0, context));
        const auto size = viewportSize(context);
        const QPointF default_tl(margin, size.height() - total_h - margin);
        legend_anchor = viewportToMap(default_tl, context);
        legend_anchor_initialized = true;
    }

    const QPointF tl = toViewport(legend_anchor, context);
    const int x0 = static_cast<int>(std::round(tl.x()));
    const int y0 = static_cast<int>(std::round(tl.y()));

    const QRectF legend_bounds(x0, y0, total_w, total_h);
    const QRectF legend_resize_handle = legendResizeHandleRect(legend_bounds);
    if (context.interactive)
    {
        legend_bounds_cache = legend_bounds;
        legend_resize_handle_cache = legend_resize_handle;
    }

    // Background
    painter->setPen(QPen(course_purple, 1));
    painter->setBrush(context.interactive ? QColor(255, 255, 255, 230) : QColor(Qt::white));
    painter->drawRect(x0, y0, total_w, total_h);

    // ── Double-height header ──────────────────────────────────────────
    painter->fillRect(x0, y0, total_w, header_h,
                      context.interactive ? QColor(148, 0, 211, 35) : QColor(246, 238, 250));

    const int pad = std::max(2, cell / 8);
    const int name_h = cell;          // top cell: course name
    const int info_h = header_h - name_h;  // bottom cell: stats

    // Course name (bold, top line)
    {
        QFont hf;
        hf.setPixelSize(std::max(8, static_cast<int>(cell_px * 0.45)));
        hf.setBold(true);
        painter->setFont(hf);
        painter->setPen(course_purple);
        painter->drawText(QRect(x0 + pad, y0, total_w - 2 * pad, name_h),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          visible_course->name);
    }

    // Info line: "N controls - D km - Climb H m"
    {
        QFont inf;
        inf.setPixelSize(std::max(7, static_cast<int>(cell_px * 0.38)));
        painter->setFont(inf);
        painter->setPen(course_purple);

        const int n_regular = rows.size();
        const qreal dist_m  = computeCourseDistanceM(*visible_course);

        QString info_text;
        if (n_regular > 0)
            info_text += tr("%n control(s)", "", n_regular);

        if (dist_m > 0.5)
        {
            if (!info_text.isEmpty()) info_text += QLatin1String("  -  ");
            if (dist_m >= 1000.0)
                info_text += tr("%1 km").arg(QString::number(dist_m / 1000.0, 'f', 1));
            else
                info_text += tr("%1 m").arg(QString::number(qRound(dist_m)));
        }

        if (visible_course->climb_m > 0)
        {
            if (!info_text.isEmpty()) info_text += QLatin1String("  -  ");
            info_text += tr("Climb %1 m").arg(visible_course->climb_m);
        }

        painter->drawText(QRect(x0 + pad, y0 + name_h, total_w - 2 * pad, info_h),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          info_text);
    }

    // Separator between header and data rows
    painter->setPen(QPen(course_purple, 1));
    painter->drawLine(x0, y0 + header_h, x0 + total_w, y0 + header_h);

    // Column grid lines (only over data rows, not header)
    for (int c = 1; c < NCOLS; ++c)
        painter->drawLine(x0 + c * cell, y0 + header_h, x0 + c * cell, y0 + total_h);

    int current_y = y0 + header_h;

    // ── Start row ────────────────────────────────────────────────────
    if (has_start)
    {
        painter->setPen(QPen(QColor(200, 200, 200), 1));
        painter->drawLine(x0, current_y, x0 + total_w, current_y);

        const QRectF start_cell(x0, current_y, cell, row_h);
        paintStartCell(painter, start_cell);
        current_y += row_h;
    }

    // ── Regular control rows ─────────────────────────────────────────
    QFont df;
    df.setPixelSize(std::max(7, static_cast<int>(cell_px * 0.40)));
    painter->setFont(df);

    for (int i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int ry = current_y;

        painter->setPen(QPen(QColor(180, 180, 180), 1));
        painter->drawLine(x0, ry, x0 + total_w, ry);

        const QString cells[NCOLS] = {
            QString::number(r.seq),
            r.code, r.part, r.feature, r.approach, r.dims, r.location, r.other
        };
        for (int c = 0; c < NCOLS; ++c)
        {
            painter->setPen(Qt::black);
            paintISCDCell(painter, cells[c], QRectF(x0 + c * cell, ry, cell, row_h), c);
        }
        current_y += row_h;
    }

    // ── Finish row ───────────────────────────────────────────────────
    if (has_finish)
    {
        painter->setPen(QPen(QColor(200, 200, 200), 1));
        painter->drawLine(x0, current_y, x0 + total_w, current_y);

        const QRectF finish_cell(x0, current_y, cell, row_h);
        paintFinishCell(painter, finish_cell);
        current_y += row_h;
    }

    // Outer border
    painter->setPen(QPen(course_purple, 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(x0, y0, total_w, total_h);

    painter->setPen(QPen(course_purple, 1));
    if (context.interactive)
    {
        const QRectF grip = legend_resize_handle.adjusted(2, 2, -2, -2);
        painter->drawLine(grip.bottomLeft(), grip.topRight());
        painter->drawLine(QPointF(grip.left() + grip.width() * 0.45, grip.bottom()),
                          QPointF(grip.right(), grip.top() + grip.height() * 0.45));
    }

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
                                           const QRectF& rect)
{
    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal w  = rect.width();
    const qreal h  = rect.height();
    const qreal r  = std::min(w, h) * 0.44;
    const qreal lw = std::max(1.0, r * 0.18);

    if (tryDrawCustomPath(painter, feature.toLower().trimmed(), rect, lw)) return;

    painter->save();
    painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);

    // Draw filled equilateral triangle pointing up, centred at (cx_t, cy_t), height h_t
    auto drawTriangle = [&](qreal cx_t, qreal cy_t, qreal h_t, bool filled) {
        const qreal hb = h_t / std::sqrt(3.0);
        QPolygonF tri;
        tri << QPointF(cx_t,      cy_t - h_t * 2.0 / 3.0)
            << QPointF(cx_t + hb, cy_t + h_t / 3.0)
            << QPointF(cx_t - hb, cy_t + h_t / 3.0);
        if (filled) painter->setBrush(Qt::black); else painter->setBrush(Qt::NoBrush);
        painter->drawPolygon(tri);
        painter->setBrush(Qt::NoBrush);
    };

    const QString f = feature.toLower();

    // ---- Terrain features (IOF 1.x) ----

    if (f == QLatin1String("re-entrant") || f == QLatin1String("re entrant"))
    {
        // 1.3 Лощина: ∩ straight legs + semicircular arch open at bottom
        const qreal hw  = r * 0.55;
        const qreal mid = cy - r * 0.30;
        const qreal bot = cy + r * 0.85;
        QPainterPath path;
        path.moveTo(cx - hw, bot);
        path.lineTo(cx - hw, mid);
        path.arcTo(QRectF(cx - hw, mid - hw, hw * 2.0, hw * 2.0), 180, -180);
        path.lineTo(cx + hw, bot);
        painter->drawPath(path);
    }
    else if (f == QLatin1String("spur"))
    {
        // 1.2 Нос: wedge pointing right — two lines converging at right apex
        painter->drawLine(QPointF(cx - r,       cy - r * 0.55), QPointF(cx + r * 0.75, cy));
        painter->drawLine(QPointF(cx - r,       cy + r * 0.55), QPointF(cx + r * 0.75, cy));
    }
    else if (f == QLatin1String("earth bank") || f == QLatin1String("embankment"))
    {
        // 1.4 Грунтовый обрыв: thick horizontal + 5 downward ticks
        painter->setPen(QPen(Qt::black, lw * 2.5, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(QPointF(cx - r, cy - r * 0.2), QPointF(cx + r, cy - r * 0.2));
        painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::FlatCap));
        for (int k = 0; k < 5; ++k)
        {
            const qreal tx = cx - r + (2.0 * r * k) / 4.0;
            painter->drawLine(QPointF(tx, cy - r * 0.2), QPointF(tx, cy + r * 0.5));
        }
    }
    else if (f == QLatin1String("erosion gully"))
    {
        // 1.7 Промоина: ∧ two lines meeting at apex (top)
        painter->drawLine(QPointF(cx - r, cy + r * 0.55), QPointF(cx, cy - r * 0.55));
        painter->drawLine(QPointF(cx,     cy - r * 0.55), QPointF(cx + r, cy + r * 0.55));
    }
    else if (f == QLatin1String("pit"))
    {
        // 1.5 Карьер: arc open at top (C-shape, rotated to open upward)
        QPainterPath path;
        path.moveTo(cx - r * 0.8, cy - r * 0.2);
        path.cubicTo(cx - r * 0.8, cy + r * 1.0,
                     cx + r * 0.8, cy + r * 1.0,
                     cx + r * 0.8, cy - r * 0.2);
        painter->drawPath(path);
    }
    else if (f == QLatin1String("hill"))
    {
        // 1.9 Бугор: oval outline only (no fill)
        painter->drawEllipse(QPointF(cx, cy), r * 0.85, r * 0.6);
    }
    else if (f == QLatin1String("knoll"))
    {
        // 1.10 Бугорок: small filled circle
        painter->setBrush(Qt::black);
        painter->drawEllipse(QPointF(cx, cy), r * 0.42, r * 0.42);
    }
    else if (f == QLatin1String("saddle"))
    {
        // 1.11 Седло: )( two opposing arcs
        QPainterPath p1;
        p1.moveTo(cx - r, cy - r * 0.15);
        p1.cubicTo(cx - r * 0.5, cy - r * 0.8,
                   cx + r * 0.5, cy - r * 0.8,
                   cx + r, cy - r * 0.15);
        painter->drawPath(p1);
        QPainterPath p2;
        p2.moveTo(cx - r, cy + r * 0.15);
        p2.cubicTo(cx - r * 0.5, cy + r * 0.8,
                   cx + r * 0.5, cy + r * 0.8,
                   cx + r, cy + r * 0.15);
        painter->drawPath(p2);
    }
    else if (f == QLatin1String("depression"))
    {
        // 1.12 Яма: horizontal ellipse + horizontal bar through centre
        painter->drawEllipse(QPointF(cx, cy), r * 0.85, r * 0.55);
        painter->drawLine(QPointF(cx - r * 0.85, cy), QPointF(cx + r * 0.85, cy));
    }
    else if (f == QLatin1String("small depression"))
    {
        // 1.13 Небольшая ямка: simple U arc
        QPainterPath path;
        path.moveTo(cx - r * 0.75, cy - r * 0.2);
        path.cubicTo(cx - r * 0.75, cy + r * 0.75,
                     cx + r * 0.75, cy + r * 0.75,
                     cx + r * 0.75, cy - r * 0.2);
        painter->drawPath(path);
    }
    else if (f == QLatin1String("broken ground"))
    {
        // 1.15 Изрытая поверхность: two small U arcs side by side
        QPainterPath p1;
        p1.moveTo(cx - r,        cy - r * 0.1);
        p1.cubicTo(cx - r,       cy + r * 0.65,
                   cx - r * 0.1, cy + r * 0.65,
                   cx - r * 0.1, cy - r * 0.1);
        painter->drawPath(p1);
        QPainterPath p2;
        p2.moveTo(cx + r * 0.1, cy - r * 0.1);
        p2.cubicTo(cx + r * 0.1, cy + r * 0.65,
                   cx + r,       cy + r * 0.65,
                   cx + r,       cy - r * 0.1);
        painter->drawPath(p2);
    }
    else if (f == QLatin1String("anthill") || f == QLatin1String("termite mound") ||
             f == QLatin1String("anthill / termite mound"))
    {
        // 1.16 Муравейник: 6-armed asterisk (60° between arms)
        for (int k = 0; k < 6; ++k)
        {
            const qreal angle = k * M_PI / 3.0;
            painter->drawLine(QPointF(cx, cy),
                              QPointF(cx + r * std::cos(angle),
                                      cy + r * std::sin(angle)));
        }
    }

    // ---- Rock (IOF 2.x) ----

    else if (f == QLatin1String("cliff") || f == QLatin1String("rock face"))
    {
        // 2.1 Утес: thick horizontal + 5 even downward ticks
        painter->setPen(QPen(Qt::black, lw * 2.5, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(QPointF(cx - r, cy - r * 0.15), QPointF(cx + r, cy - r * 0.15));
        painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::FlatCap));
        for (int k = 0; k < 5; ++k)
        {
            const qreal tx = cx - r + (2.0 * r * k) / 4.0;
            painter->drawLine(QPointF(tx, cy - r * 0.15), QPointF(tx, cy + r * 0.6));
        }
    }
    else if (f == QLatin1String("cave"))
    {
        // 2.3 Пещера: 4 diagonal lines converging from corners to centre
        const qreal d = r * 0.75;
        painter->drawLine(QPointF(cx - d, cy - d), QPointF(cx, cy));
        painter->drawLine(QPointF(cx + d, cy - d), QPointF(cx, cy));
        painter->drawLine(QPointF(cx - d, cy + d), QPointF(cx, cy));
        painter->drawLine(QPointF(cx + d, cy + d), QPointF(cx, cy));
    }
    else if (f == QLatin1String("boulder"))
    {
        // 2.4 Валун: filled equilateral triangle
        drawTriangle(cx, cy + r * 0.1, r * 0.95, true);
    }
    else if (f == QLatin1String("boulder field"))
    {
        // 2.5 Валунное поле: 3 scattered filled triangles
        drawTriangle(cx,            cy - r * 0.15, r * 0.55, true);
        drawTriangle(cx - r * 0.52, cy + r * 0.42, r * 0.45, true);
        drawTriangle(cx + r * 0.52, cy + r * 0.42, r * 0.45, true);
    }
    else if (f == QLatin1String("boulder cluster") || f == QLatin1String("cluster of boulders"))
    {
        // 2.6 Скопление валунов: big + small triangle
        drawTriangle(cx - r * 0.22, cy + r * 0.05, r * 0.72, true);
        drawTriangle(cx + r * 0.52, cy + r * 0.2,  r * 0.46, true);
    }

    // ---- Water (IOF 3.x) ----

    else if (f == QLatin1String("lake") || f == QLatin1String("pond") ||
             f == QLatin1String("pool") || f == QLatin1String("lake / pond"))
    {
        // 3.1/3.2 Водоём/Пруд: 3 parallel wavy horizontal lines
        for (int row = -1; row <= 1; ++row)
        {
            const qreal wy = cy + row * r * 0.38;
            QPainterPath wave;
            wave.moveTo(cx - r, wy);
            wave.cubicTo(cx - r * 0.5, wy - r * 0.22,
                         cx,           wy + r * 0.22,
                         cx + r * 0.5, wy - r * 0.22);
            wave.cubicTo(cx + r * 0.75, wy - r * 0.3,
                         cx + r,        wy,
                         cx + r,        wy);
            painter->drawPath(wave);
        }
    }
    else if (f == QLatin1String("marsh"))
    {
        // 3.7 Болото: 3 horizontal straight lines (≡)
        for (int row = -1; row <= 1; ++row)
            painter->drawLine(QPointF(cx - r * 0.85, cy + row * r * 0.38),
                              QPointF(cx + r * 0.85, cy + row * r * 0.38));
    }
    else if (f == QLatin1String("narrow marsh"))
    {
        // 3.6 Узкое болото: two horizontal dotted lines
        painter->setPen(QPen(Qt::black, lw * 1.5, Qt::DotLine, Qt::RoundCap));
        painter->drawLine(QPointF(cx - r * 0.85, cy - r * 0.22),
                          QPointF(cx + r * 0.85, cy - r * 0.22));
        painter->drawLine(QPointF(cx - r * 0.85, cy + r * 0.22),
                          QPointF(cx + r * 0.85, cy + r * 0.22));
    }
    else if (f == QLatin1String("firm ground in marsh"))
    {
        // 3.8 Сухое место: solid + dashed + solid alternating lines
        painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(QPointF(cx - r * 0.85, cy - r * 0.38),
                          QPointF(cx + r * 0.85, cy - r * 0.38));
        painter->setPen(QPen(Qt::black, lw, Qt::DashLine, Qt::FlatCap));
        painter->drawLine(QPointF(cx - r * 0.85, cy),
                          QPointF(cx + r * 0.85, cy));
        painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(QPointF(cx - r * 0.85, cy + r * 0.38),
                          QPointF(cx + r * 0.85, cy + r * 0.38));
    }
    else if (f == QLatin1String("well") || f == QLatin1String("water tank") ||
             f == QLatin1String("cistern") || f == QLatin1String("well / water tank"))
    {
        // 3.9/3.11 Колодец/Резервуар: circle outline + wavy tail below
        painter->drawEllipse(QPointF(cx, cy - r * 0.22), r * 0.44, r * 0.44);
        QPainterPath wave;
        wave.moveTo(cx, cy + r * 0.22);
        wave.cubicTo(cx - r * 0.28, cy + r * 0.45,
                     cx + r * 0.28, cy + r * 0.65,
                     cx,            cy + r * 0.88);
        painter->drawPath(wave);
    }
    else if (f == QLatin1String("river") || f == QLatin1String("stream") ||
             f == QLatin1String("major river") || f == QLatin1String("wide stream") ||
             f == QLatin1String("river / stream"))
    {
        // 3.4 Речка: double-period zigzag wave
        const qreal amp = r * 0.38;
        QPainterPath wave;
        wave.moveTo(cx - r, cy);
        wave.cubicTo(cx - r * 0.75, cy - amp, cx - r * 0.25, cy + amp, cx, cy);
        wave.cubicTo(cx + r * 0.25, cy - amp, cx + r * 0.75, cy + amp, cx + r, cy);
        painter->drawPath(wave);
    }
    else if (f == QLatin1String("ditch") || f == QLatin1String("channel") ||
             f == QLatin1String("trench") || f == QLatin1String("ditch / channel"))
    {
        // Two parallel horizontal lines
        const qreal gap = r * 0.3;
        painter->drawLine(QPointF(cx - r, cy - gap), QPointF(cx + r, cy - gap));
        painter->drawLine(QPointF(cx - r, cy + gap), QPointF(cx + r, cy + gap));
    }
    else if (f == QLatin1String("spring") || f == QLatin1String("source") ||
             f == QLatin1String("source / spring"))
    {
        // 3.10 Родник: S-curve (source flowing outward)
        QPainterPath path;
        path.moveTo(cx - r, cy);
        path.cubicTo(cx - r * 0.5, cy - r * 0.6,
                     cx - r * 0.2, cy - r * 0.6,
                     cx - r * 0.2, cy);
        path.cubicTo(cx - r * 0.2, cy + r * 0.5,
                     cx + r,       cy + r * 0.5,
                     cx + r,       cy);
        painter->drawPath(path);
    }

    // ---- Vegetation (IOF 4.x) ----

    else if (f == QLatin1String("open land") || f == QLatin1String("field") ||
             f == QLatin1String("cultivated land"))
    {
        // 4.1 Открытое пространство: diamond outline (◇)
        QPolygonF diamond;
        diamond << QPointF(cx,     cy - r * 0.9)
                << QPointF(cx + r, cy)
                << QPointF(cx,     cy + r * 0.9)
                << QPointF(cx - r, cy);
        painter->drawPolygon(diamond);
    }
    else if (f == QLatin1String("forest corner"))
    {
        // 4.3 Угол леса: two lines forming acute angle pointing left (◁)
        painter->drawLine(QPointF(cx + r * 0.7, cy - r * 0.7), QPointF(cx - r * 0.6, cy));
        painter->drawLine(QPointF(cx - r * 0.6, cy),           QPointF(cx + r * 0.7, cy + r * 0.7));
    }
    else if (f == QLatin1String("clearing") || f == QLatin1String("small clearing"))
    {
        // 4.4 Прогал: ring of 7 dots
        painter->setBrush(Qt::black);
        const qreal dr = r * 0.16;
        for (int k = 0; k < 7; ++k)
        {
            const qreal angle = k * 2.0 * M_PI / 7.0;
            painter->drawEllipse(QPointF(cx + r * 0.68 * std::cos(angle),
                                         cy + r * 0.68 * std::sin(angle)), dr, dr);
        }
    }
    else if (f == QLatin1String("linear thicket") || f == QLatin1String("hedge"))
    {
        // 4.6 Зеленая изгородь: wavy horizontal line
        QPainterPath wave;
        wave.moveTo(cx - r, cy);
        wave.cubicTo(cx - r * 0.55, cy - r * 0.45,
                     cx - r * 0.15, cy + r * 0.45,
                     cx + r * 0.25, cy - r * 0.45);
        wave.cubicTo(cx + r * 0.65, cy - r * 0.75,
                     cx + r,        cy,
                     cx + r,        cy);
        painter->drawPath(wave);
    }
    else if (f == QLatin1String("copse") || f == QLatin1String("grove"))
    {
        // 4.8 Околок: triangle outline + 3 dots outside
        drawTriangle(cx, cy - r * 0.05, r * 0.7, false);
        painter->setBrush(Qt::black);
        const qreal dr = r * 0.14;
        const qreal pr = r * 0.92;
        painter->drawEllipse(QPointF(cx,                cy - pr),             dr, dr);
        painter->drawEllipse(QPointF(cx + pr * 0.866,   cy + pr * 0.5),       dr, dr);
        painter->drawEllipse(QPointF(cx - pr * 0.866,   cy + pr * 0.5),       dr, dr);
    }
    else if (f == QLatin1String("distinct tree") || f == QLatin1String("distinctive tree"))
    {
        // 4.9 Выделяющееся дерево: simple triangle outline
        drawTriangle(cx, cy, r * 0.9, false);
    }
    else if (f == QLatin1String("charcoal burning ground"))
    {
        // 5.19 Земля для сжигания угля: circle with inscribed triangle
        painter->drawEllipse(QPointF(cx, cy), r * 0.82, r * 0.82);
        drawTriangle(cx, cy + r * 0.12, r * 0.62, false);
    }

    // ---- Man-made (IOF 5.x) ----

    else if (f == QLatin1String("earth wall"))
    {
        // 1.6 Земляной вал: 5 crosses (+) in a row
        const qreal arm = r * 0.28;
        for (int k = 0; k < 5; ++k)
        {
            const qreal tx = cx - r + (2.0 * r * k) / 4.0;
            painter->drawLine(QPointF(tx - arm, cy), QPointF(tx + arm, cy));
            painter->drawLine(QPointF(tx, cy - arm), QPointF(tx, cy + arm));
        }
    }
    else if (f == QLatin1String("building"))
    {
        // 5.11 Здание: filled square
        painter->setBrush(Qt::black);
        const qreal s = r * 0.72;
        painter->drawRect(QRectF(cx - s, cy - s, 2 * s, 2 * s));
    }
    else if (f == QLatin1String("ruin") || f == QLatin1String("ruined building"))
    {
        // 5.13 Руины: dashed square outline
        painter->setPen(QPen(Qt::black, lw, Qt::DashLine, Qt::FlatCap));
        const qreal s = r * 0.72;
        painter->drawRect(QRectF(cx - s, cy - s, 2 * s, 2 * s));
    }
    else if (f == QLatin1String("wall"))
    {
        // 5.8 Стена: very thick horizontal line
        painter->setPen(QPen(Qt::black, lw * 3.5, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
    }
    else if (f == QLatin1String("fence"))
    {
        // 5.9 Ограда: horizontal line + ticks pointing upward only
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
        const qreal tick = r * 0.45;
        for (int k = 0; k <= 4; ++k)
        {
            const qreal tx = cx - r + (2.0 * r * k) / 4.0;
            painter->drawLine(QPointF(tx, cy), QPointF(tx, cy - tick));
        }
    }
    else if (f == QLatin1String("crossing point"))
    {
        // 5.10 Проход: two vertical bars + gap in middle
        const qreal bar_h = r * 0.55;
        const qreal gap   = r * 0.35;
        painter->drawLine(QPointF(cx - r, cy - bar_h), QPointF(cx - r, cy + bar_h));
        painter->drawLine(QPointF(cx + r, cy - bar_h), QPointF(cx + r, cy + bar_h));
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx - gap, cy));
        painter->drawLine(QPointF(cx + gap, cy), QPointF(cx + r, cy));
    }
    else if (f == QLatin1String("path") || f == QLatin1String("track") ||
             f == QLatin1String("footpath") || f == QLatin1String("narrow ride") ||
             f == QLatin1String("path / track"))
    {
        // 5.2 Тропа: diagonal solid line (~30° from horizontal)
        painter->drawLine(QPointF(cx - r, cy + r * 0.5), QPointF(cx + r, cy - r * 0.5));
    }
    else if (f == QLatin1String("paved area") || f == QLatin1String("road"))
    {
        // 5.12 Парковка: square outline + diagonal hatching
        const qreal s = r * 0.72;
        painter->drawRect(QRectF(cx - s, cy - s, 2 * s, 2 * s));
        painter->setClipRect(QRectF(cx - s + lw, cy - s + lw, 2 * s - 2 * lw, 2 * s - 2 * lw));
        for (int k = -3; k <= 3; ++k)
            painter->drawLine(QPointF(cx + k * s * 0.65 - s, cy - s),
                              QPointF(cx + k * s * 0.65 + s, cy + s));
        painter->setClipping(false);
    }
    else if (f == QLatin1String("bridge"))
    {
        // 5.4 Мост: horizontal line + perpendiculars only at ends
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
        const qreal cap = r * 0.4;
        painter->drawLine(QPointF(cx - r, cy - cap), QPointF(cx - r, cy + cap));
        painter->drawLine(QPointF(cx + r, cy - cap), QPointF(cx + r, cy + cap));
    }
    else if (f == QLatin1String("tower"))
    {
        // 5.15 Башня/Пилон: T-shape (vertical + horizontal crossbar at top)
        painter->drawLine(QPointF(cx, cy - r * 0.85), QPointF(cx, cy + r * 0.5));
        painter->drawLine(QPointF(cx - r * 0.65, cy - r * 0.85),
                          QPointF(cx + r * 0.65, cy - r * 0.85));
    }
    else if (f == QLatin1String("high-voltage line pylon"))
    {
        // 5.6 Опора ЛЭП: circle + X + 4 rays at 45°
        const qreal cr = r * 0.34;
        painter->drawEllipse(QPointF(cx, cy), cr, cr);
        const qreal xi = cr * 0.65;
        painter->drawLine(QPointF(cx - xi, cy - xi), QPointF(cx + xi, cy + xi));
        painter->drawLine(QPointF(cx + xi, cy - xi), QPointF(cx - xi, cy + xi));
        for (int k = 0; k < 4; ++k)
        {
            const qreal a = k * M_PI / 2.0 + M_PI / 4.0;
            painter->drawLine(QPointF(cx + cr * std::cos(a), cy + cr * std::sin(a)),
                              QPointF(cx + r  * std::cos(a), cy + r  * std::sin(a)));
        }
    }
    else if (f == QLatin1String("boundary stone / cairn"))
    {
        // 5.17 Пограничный камень: circle outline + centre filled dot
        painter->drawEllipse(QPointF(cx, cy), r * 0.65, r * 0.65);
        painter->setBrush(Qt::black);
        painter->drawEllipse(QPointF(cx, cy), r * 0.2, r * 0.2);
    }
    else if (f == QLatin1String("monument / statue"))
    {
        // 5.20 Монумент: triangle outline
        drawTriangle(cx, cy + r * 0.1, r * 0.9, false);
    }
    else if (f == QLatin1String("fodder rack"))
    {
        // 5.18 Кормушка: T with base (inverted T with foot rail)
        painter->drawLine(QPointF(cx, cy - r * 0.55), QPointF(cx, cy + r * 0.35));
        painter->drawLine(QPointF(cx - r * 0.65, cy - r * 0.55),
                          QPointF(cx + r * 0.65, cy - r * 0.55));
        painter->drawLine(QPointF(cx - r * 0.65, cy + r * 0.35),
                          QPointF(cx + r * 0.65, cy + r * 0.35));
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
// ISCD column C — part of feature  (IOF 0.1–0.5)
// ============================================================

void CourseOverlay::drawISCDPartSymbol(QPainter* painter,
                                        const QString& part,
                                        const QRectF& rect)
{
    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal r  = std::min(rect.width(), rect.height()) * 0.44;
    const qreal lw = std::max(1.0, r * 0.16);

    if (tryDrawCustomPath(painter, part.toLower().trimmed(), rect, lw)) return;

    painter->save();
    painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);

    const QString p = part.toLower().trimmed();

    if (p == QLatin1String("upper"))
    {
        // 0.3 Upper: two horizontal lines; filled dot at CENTRE of the TOP line
        const qreal y_top = cy - r * 0.26;
        const qreal y_bot = cy + r * 0.26;
        const qreal dot   = r * 0.09;
        painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(QPointF(cx - r * 0.55, y_top), QPointF(cx + r * 0.55, y_top));
        painter->drawLine(QPointF(cx - r * 0.55, y_bot), QPointF(cx + r * 0.55, y_bot));
        painter->setBrush(Qt::black);
        painter->drawEllipse(QPointF(cx, y_top), dot, dot);
        painter->setBrush(Qt::NoBrush);
    }
    else if (p == QLatin1String("lower"))
    {
        // 0.4 Lower: two horizontal lines; filled dot at CENTRE of the BOTTOM line
        const qreal y_top = cy - r * 0.26;
        const qreal y_bot = cy + r * 0.26;
        const qreal dot   = r * 0.09;
        painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(QPointF(cx - r * 0.55, y_top), QPointF(cx + r * 0.55, y_top));
        painter->drawLine(QPointF(cx - r * 0.55, y_bot), QPointF(cx + r * 0.55, y_bot));
        painter->setBrush(Qt::black);
        painter->drawEllipse(QPointF(cx, y_bot), dot, dot);
        painter->setBrush(Qt::NoBrush);
    }
    else if (p == QLatin1String("middle"))
    {
        // 0.5 Middle: three vertical lines; filled dot at CENTRE of the middle line
        const qreal vht = r * 0.55;
        const qreal sp  = r * 0.32;
        const qreal dot = r * 0.09;
        for (int k = -1; k <= 1; ++k)
            painter->drawLine(QPointF(cx + k * sp, cy - vht),
                              QPointF(cx + k * sp, cy + vht));
        painter->setBrush(Qt::black);
        painter->drawEllipse(QPointF(cx, cy), dot, dot);
        painter->setBrush(Qt::NoBrush);
    }
    else
    {
        // Cardinal/intercardinal: draw arrow pointing in the named direction
        qreal dx = 0.0, dy = 0.0;
        if      (p == QLatin1String("northern") || p == QLatin1String("n"))  { dx =  0; dy = -1; }
        else if (p == QLatin1String("ne"))                                    { dx =  1; dy = -1; }
        else if (p == QLatin1String("eastern")  || p == QLatin1String("e"))  { dx =  1; dy =  0; }
        else if (p == QLatin1String("se"))                                    { dx =  1; dy =  1; }
        else if (p == QLatin1String("southern") || p == QLatin1String("s"))  { dx =  0; dy =  1; }
        else if (p == QLatin1String("sw"))                                    { dx = -1; dy =  1; }
        else if (p == QLatin1String("western")  || p == QLatin1String("w"))  { dx = -1; dy =  0; }
        else if (p == QLatin1String("nw"))                                    { dx = -1; dy = -1; }

        if (dx != 0.0 || dy != 0.0)
        {
            const qreal len = std::sqrt(dx * dx + dy * dy);
            dx /= len;  dy /= len;
            const QPointF tip(cx + dx * r * 0.75, cy + dy * r * 0.75);
            painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawLine(QPointF(cx, cy), tip);
            // Arrowhead: two lines at ±150° from the direction
            const qreal head  = r * 0.32;
            const qreal angle = std::atan2(dy, dx);
            for (int s = -1; s <= 1; s += 2)
            {
                const qreal a = angle + s * (5.0 * M_PI / 6.0);
                painter->drawLine(tip,
                    QPointF(tip.x() + head * std::cos(a),
                            tip.y() + head * std::sin(a)));
            }
        }
        else
        {
            // Fallback text for unrecognised values
            QFont f;
            f.setPixelSize(std::max(7, static_cast<int>(r * 1.5)));
            f.setBold(true);
            painter->setFont(f);
            painter->setPen(Qt::black);
            painter->drawText(rect.adjusted(-rect.width()*0.12, -rect.height()*0.12,
                                            rect.width()*0.12,  rect.height()*0.12),
                              Qt::AlignCenter | Qt::TextSingleLine,
                              part.toUpper());
        }
    }

    painter->restore();
}


// ============================================================
// ISCD column E — appearance / approach  (IOF 8.x)
// ============================================================

void CourseOverlay::drawISCDApproachSymbol(QPainter* painter,
                                            const QString& approach,
                                            const QRectF& rect)
{
    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal r  = std::min(rect.width(), rect.height()) * 0.44;
    const qreal lw = std::max(1.0, r * 0.16);

    if (tryDrawCustomPath(painter, approach.toLower().trimmed(), rect, lw)) return;

    painter->save();
    painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);

    const QString a = approach.toLower().trimmed();

    // Helper: small filled triangle pointing up
    auto smallTri = [&](qreal cx_t, qreal cy_t, qreal h_t) {
        const qreal hb = h_t / std::sqrt(3.0);
        QPolygonF tri;
        tri << QPointF(cx_t,      cy_t - h_t * 2.0 / 3.0)
            << QPointF(cx_t + hb, cy_t + h_t / 3.0)
            << QPointF(cx_t - hb, cy_t + h_t / 3.0);
        painter->setBrush(Qt::black);
        painter->drawPolygon(tri);
        painter->setBrush(Qt::NoBrush);
    };

    if (a == QLatin1String("shallow"))
    {
        // 8.2 Мелкий: very shallow U arc (opening upward, small height)
        QPainterPath path;
        path.moveTo(cx - r, cy + r * 0.1);
        path.cubicTo(cx - r, cy + r * 0.42,
                     cx + r, cy + r * 0.42,
                     cx + r, cy + r * 0.1);
        painter->drawPath(path);
    }
    else if (a == QLatin1String("deep"))
    {
        // 8.3 Глубокий: deep U curve
        QPainterPath path;
        path.moveTo(cx - r, cy - r * 0.42);
        path.cubicTo(cx - r, cy + r * 0.82,
                     cx + r, cy + r * 0.82,
                     cx + r, cy - r * 0.42);
        painter->drawPath(path);
    }
    else if (a == QLatin1String("overgrown"))
    {
        // 8.4 Заросший: 2×2 crosshatch grid (#)
        const qreal hw = r * 0.62;
        painter->drawLine(QPointF(cx - hw, cy - r * 0.28), QPointF(cx + hw, cy - r * 0.28));
        painter->drawLine(QPointF(cx - hw, cy + r * 0.28), QPointF(cx + hw, cy + r * 0.28));
        painter->drawLine(QPointF(cx - r * 0.28, cy - hw), QPointF(cx - r * 0.28, cy + hw));
        painter->drawLine(QPointF(cx + r * 0.28, cy - hw), QPointF(cx + r * 0.28, cy + hw));
    }
    else if (a == QLatin1String("open"))
    {
        // 8.5 Открытый: scattered dots (6 points)
        painter->setBrush(Qt::black);
        const qreal dr = r * 0.16;
        const QPointF pts[] = {
            {cx - r * 0.55, cy - r * 0.45},
            {cx + r * 0.12, cy - r * 0.55},
            {cx + r * 0.55, cy - r * 0.18},
            {cx - r * 0.42, cy + r * 0.25},
            {cx + r * 0.18, cy + r * 0.22},
            {cx + r * 0.52, cy + r * 0.52}
        };
        for (const auto& pt : pts)
            painter->drawEllipse(pt, dr, dr);
    }
    else if (a == QLatin1String("rocky"))
    {
        // 8.6 Каменистый: 3 small filled triangles in a row
        smallTri(cx - r * 0.45, cy + r * 0.12, r * 0.52);
        smallTri(cx,             cy - r * 0.18, r * 0.52);
        smallTri(cx + r * 0.45, cy + r * 0.12, r * 0.52);
    }
    else if (a == QLatin1String("marshy"))
    {
        // 8.7 Заболоченный: 3 horizontal lines (≡)
        for (int row = -1; row <= 1; ++row)
            painter->drawLine(QPointF(cx - r * 0.82, cy + row * r * 0.38),
                              QPointF(cx + r * 0.82, cy + row * r * 0.38));
    }
    else if (a == QLatin1String("sandy"))
    {
        // 8.8 Песчаный: 4×3 grid of small dots
        painter->setBrush(Qt::black);
        const qreal dr  = r * 0.10;
        const qreal spx = r * 0.48;
        const qreal spy = r * 0.42;
        for (int row = -1; row <= 1; ++row)
            for (int col = 0; col < 4; ++col)
                painter->drawEllipse(QPointF(cx + (col - 1.5) * spx,
                                             cy + row * spy), dr, dr);
    }
    else if (a == QLatin1String("ruined"))
    {
        // 8.11 Разрушенный: Г-shaped folded line (horizontal then drops down)
        painter->drawLine(QPointF(cx - r * 0.68, cy - r * 0.3), QPointF(cx + r * 0.52, cy - r * 0.3));
        painter->drawLine(QPointF(cx + r * 0.52, cy - r * 0.3), QPointF(cx + r * 0.52, cy + r * 0.62));
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
// ISCD column G — location detail  (IOF 12.x)
// ============================================================

void CourseOverlay::drawISCDLocationSymbol(QPainter* painter,
                                            const QString& location,
                                            const QRectF& rect)
{
    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal r  = std::min(rect.width(), rect.height()) * 0.44;
    const qreal lw = std::max(1.0, r * 0.16);
    const qreal cr = r * 0.34;  // radius of feature-outline circle
    const qreal dr = r * 0.17;  // radius of location dot

    if (tryDrawCustomPath(painter, location.toLower().trimmed(), rect, lw)) return;

    painter->save();
    painter->setPen(QPen(Qt::black, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);

    const QString loc = location.toLower().trimmed();

    // Draw circle (feature outline) + filled dot at offset (dx,dy)*dist from centre
    auto circleWithDot = [&](qreal dx, qreal dy, qreal dist) {
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), cr, cr);
        painter->setBrush(Qt::black);
        painter->drawEllipse(QPointF(cx + dx * dist, cy + dy * dist), dr, dr);
        painter->setBrush(Qt::NoBrush);
    };

    // Parse "<dir> <type>" where dir is compass (n/ne/e/…) and type is foot/edge/tip/end
    auto parseDir = [](const QString& s, qreal& dx, qreal& dy) -> bool {
        dx = dy = 0;
        if      (s == QLatin1String("n"))  { dx =  0; dy = -1; }
        else if (s == QLatin1String("ne")) { dx =  1; dy = -1; }
        else if (s == QLatin1String("e"))  { dx =  1; dy =  0; }
        else if (s == QLatin1String("se")) { dx =  1; dy =  1; }
        else if (s == QLatin1String("s"))  { dx =  0; dy =  1; }
        else if (s == QLatin1String("sw")) { dx = -1; dy =  1; }
        else if (s == QLatin1String("w"))  { dx = -1; dy =  0; }
        else if (s == QLatin1String("nw")) { dx = -1; dy = -1; }
        else                               { return false; }
        const qreal len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.001) { dx /= len; dy /= len; }
        return true;
    };

    if (loc == QLatin1String("top"))
    {
        // 12.10 Верх: Π — two verticals + crossbar at top
        const qreal vx = r * 0.44;
        painter->drawLine(QPointF(cx - vx, cy - r * 0.55), QPointF(cx - vx, cy + r * 0.55));
        painter->drawLine(QPointF(cx + vx, cy - r * 0.55), QPointF(cx + vx, cy + r * 0.55));
        painter->drawLine(QPointF(cx - vx, cy - r * 0.55), QPointF(cx + vx, cy - r * 0.55));
    }
    else if (loc == QLatin1String("upper part"))
    {
        // 12.8 Верхняя часть: |‾| — verticals + crossbar in upper third; bars extend above
        const qreal vx = r * 0.44;
        painter->drawLine(QPointF(cx - vx, cy - r * 0.55), QPointF(cx - vx, cy + r * 0.55));
        painter->drawLine(QPointF(cx + vx, cy - r * 0.55), QPointF(cx + vx, cy + r * 0.55));
        painter->drawLine(QPointF(cx - vx, cy - r * 0.15), QPointF(cx + vx, cy - r * 0.15));
    }
    else if (loc == QLatin1String("lower part"))
    {
        // 12.9 Нижняя часть: |_| — verticals + crossbar in lower third
        const qreal vx = r * 0.44;
        painter->drawLine(QPointF(cx - vx, cy - r * 0.55), QPointF(cx - vx, cy + r * 0.55));
        painter->drawLine(QPointF(cx + vx, cy - r * 0.55), QPointF(cx + vx, cy + r * 0.55));
        painter->drawLine(QPointF(cx - vx, cy + r * 0.15), QPointF(cx + vx, cy + r * 0.15));
    }
    else if (loc == QLatin1String("foot"))
    {
        // 12.11 Подножие: ⌊ L-shape (horizontal + vertical down on left end)
        painter->drawLine(QPointF(cx - r * 0.6, cy - r * 0.25),
                          QPointF(cx + r * 0.6, cy - r * 0.25));
        painter->drawLine(QPointF(cx - r * 0.6, cy - r * 0.25),
                          QPointF(cx - r * 0.6, cy + r * 0.55));
    }
    else if (loc == QLatin1String("side"))
    {
        // Generic side: circle + dot at eastern edge
        circleWithDot(1.0, 0.0, cr);
    }
    else if (loc == QLatin1String("corner (inside)"))
    {
        // 12.4 Угол внутри: > chevron pointing right
        painter->drawLine(QPointF(cx - r * 0.5, cy - r * 0.6), QPointF(cx + r * 0.55, cy));
        painter->drawLine(QPointF(cx + r * 0.55, cy),           QPointF(cx - r * 0.5, cy + r * 0.6));
    }
    else if (loc == QLatin1String("corner (outside)"))
    {
        // 12.5 Угол снаружи: ∨ V pointing down
        painter->drawLine(QPointF(cx - r * 0.6, cy - r * 0.4), QPointF(cx, cy + r * 0.55));
        painter->drawLine(QPointF(cx, cy + r * 0.55),           QPointF(cx + r * 0.6, cy - r * 0.4));
    }
    else if (loc == QLatin1String("junction"))
    {
        // + cross
        painter->drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
        painter->drawLine(QPointF(cx, cy - r), QPointF(cx, cy + r));
    }
    else if (loc == QLatin1String("between"))
    {
        // 12.14 Между: —•— dot flanked by horizontal bars
        painter->setBrush(Qt::black);
        painter->drawEllipse(QPointF(cx, cy), dr, dr);
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(cx - r,        cy), QPointF(cx - dr * 1.6, cy));
        painter->drawLine(QPointF(cx + dr * 1.6, cy), QPointF(cx + r,        cy));
    }
    else
    {
        // Parse directional variants: "<dir> foot|edge|tip|end"
        const int spaceIdx = loc.indexOf(QLatin1Char(' '));
        const QString dirStr  = spaceIdx >= 0 ? loc.left(spaceIdx) : loc;
        const QString typeStr = spaceIdx >= 0 ? loc.mid(spaceIdx + 1)
                                              : QLatin1String("foot");

        qreal dx = 0, dy = 0;
        if (parseDir(dirStr, dx, dy))
        {
            if (typeStr == QLatin1String("foot"))
            {
                // 12.12: circle + dot beyond edge in direction
                circleWithDot(dx, dy, cr + dr * 2.4);
            }
            else if (typeStr == QLatin1String("edge"))
            {
                // 12.1/12.2: circle + dot on the edge
                circleWithDot(dx, dy, cr);
            }
            else if (typeStr == QLatin1String("tip") || typeStr == QLatin1String("end"))
            {
                // 12.6/12.7: directional arrow (no circle)
                const QPointF tip(cx + dx * r * 0.75, cy + dy * r * 0.75);
                painter->drawLine(QPointF(cx, cy), tip);
                const qreal head  = r * 0.3;
                const qreal angle = std::atan2(dy, dx);
                for (int s = -1; s <= 1; s += 2)
                {
                    const qreal a = angle + s * (5.0 * M_PI / 6.0);
                    painter->drawLine(tip,
                        QPointF(tip.x() + head * std::cos(a),
                                tip.y() + head * std::sin(a)));
                }
            }
            else
            {
                circleWithDot(dx, dy, cr + dr * 2.4);
            }
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
    }

    painter->restore();
}


}  // namespace OpenOrienteering
