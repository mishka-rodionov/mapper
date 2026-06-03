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

#ifndef OPENORIENTEERING_COURSE_OVERLAY_H
#define OPENORIENTEERING_COURSE_OVERLAY_H

#include <QObject>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

#include "core/map_coord.h"

class QMouseEvent;
class QPainter;

namespace OpenOrienteering {

class Course;
class CourseControl;
class CourseDatabase;
class MapWidget;


/**
 * Renders courses as a purple overlay on a MapWidget.
 *
 * Follows the exact same pattern as GPSDisplay:
 *  - Constructor registers with the widget via setCourseOverlay(this)
 *  - Destructor unregisters via setCourseOverlay(nullptr)
 *  - paint() is called by MapWidget::paintEvent() after GPS display painting
 *
 * Drawing uses screen (viewport) coordinates obtained via
 * MapWidget::mapToViewport(), matching the GPSDisplay approach.
 *
 * The IOF control description table (legend) is draggable: the user can
 * left-click and drag it to any position on the map. The position is stored
 * in map coordinates so it survives pan/zoom and prints at the correct place.
 */
class CourseOverlay : public QObject
{
    Q_OBJECT

public:
    /** Creates the overlay and registers it with the given MapWidget. */
    CourseOverlay(MapWidget* widget, const CourseDatabase& db, QObject* parent = nullptr);

    /** Unregisters from the MapWidget. */
    ~CourseOverlay() override;

    CourseOverlay(const CourseOverlay&) = delete;
    CourseOverlay& operator=(const CourseOverlay&) = delete;

    /**
     * Sets which course is currently visible.
     * Pass nullptr to show all controls without connecting legs.
     */
    void setVisibleCourse(const Course* course);

    /** Returns the currently visible course (may be nullptr). */
    const Course* visibleCourse() const { return visible_course; }

    /**
     * Called by MapWidget::paintEvent() to paint the overlay.
     * The painter is in widget (viewport) coordinate space.
     */
    void paint(QPainter* painter);

    /**
     * Mouse event handlers called by MapWidget before tool dispatch.
     * Return true if the event was consumed (legend was hit/dragged).
     */
    bool mousePressEvent(QMouseEvent* event);
    bool mouseMoveEvent(QMouseEvent* event);
    bool mouseReleaseEvent(QMouseEvent* event);

private slots:
    void onDatabaseChanged();

private:
    // --- Course / control paint helpers ---
    void paintCourse(QPainter* painter, const Course& course) const;
    void paintAllControls(QPainter* painter) const;

    void paintLeg(QPainter* painter, QPointF from, QPointF to) const;
    void paintStart(QPainter* painter, QPointF pos, double rotation_rad) const;
    void paintControl(QPainter* painter, QPointF pos, const QString& number) const;
    void paintFinish(QPainter* painter, QPointF pos) const;
    void paintCrossingPoint(QPainter* painter, QPointF pos) const;
    void paintControlNumber(QPainter* painter, QPointF center, const QString& number) const;

    // --- IOF description table ---
    void paintDescriptionTable(QPainter* painter);

    /** Draws the content of one ISCD cell.  column is 0-based (0=A … 7=H). */
    void paintISCDCell(QPainter* painter, const QString& text,
                       const QRectF& cell, int column) const;

    // ISCD column-specific symbol renderers
    void drawISCDFeatureSymbol(QPainter* painter, const QString& feature, const QRectF& r) const;
    void drawISCDPartSymbol(QPainter* painter, const QString& part, const QRectF& r) const;
    void drawISCDApproachSymbol(QPainter* painter, const QString& approach, const QRectF& r) const;
    void drawISCDLocationSymbol(QPainter* painter, const QString& location, const QRectF& r) const;

    // --- Coordinate conversion ---
    /** Converts a MapCoord (1/1000 mm) to viewport pixel coordinates. */
    QPointF toViewport(const CourseControl& ctrl) const;

    /**
     * Converts millimeters on map paper to viewport pixels at the current zoom.
     * e.g. a 5mm IOF circle at 2× zoom = 2× more pixels.
     */
    qreal mmToViewportPx(qreal mm) const;

    MapWidget* widget;
    const CourseDatabase& db;
    const Course* visible_course = nullptr;

    bool show_description_table = true;

    // --- Legend drag state ---
    MapCoordF legend_anchor;             ///< Top-left of legend in map coords (native units)
    bool legend_anchor_initialized = false;
    bool legend_dragging = false;
    QPointF legend_drag_offset;          ///< Click pos relative to legend top-left (viewport px)
    mutable QRectF legend_bounds_cache;  ///< Updated each paint; used for hit-testing
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_OVERLAY_H
