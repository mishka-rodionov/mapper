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

#include <QJsonObject>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

#include "core/map_coord.h"

class QMouseEvent;
class QPainter;
class QTransform;

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
    CourseOverlay(MapWidget* widget, CourseDatabase& db, QObject* parent = nullptr);

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

    /** Shows/hides the interactive course overlay on the map widget. */
    void setPlanningActive(bool active);

    /**
     * Called by MapWidget::paintEvent() to paint the overlay.
     * The painter is in widget (viewport) coordinate space.
     */
    void paint(QPainter* painter);

    /**
     * Called by MapPrinter to paint the overlay in page/device coordinates.
     */
    void paintForPrint(QPainter* painter, const QTransform& map_to_painter,
                       const QSizeF& page_size, qreal pixels_per_mm);

    /**
     * Mouse event handlers called by MapWidget before tool dispatch.
     * Return true if the event was consumed by a course overlay item.
     */
    bool mousePressEvent(QMouseEvent* event);
    bool mouseMoveEvent(QMouseEvent* event);
    bool mouseReleaseEvent(QMouseEvent* event);

signals:
    void visibleCourseDescriptionScaleChangeRequested(double scale, bool commit);

public:
    // ISCD column-specific symbol renderers (static: no instance state required)
    static void drawISCDFeatureSymbol(QPainter* painter, const QString& feature, const QRectF& r);
    static void drawISCDPartSymbol(QPainter* painter, const QString& part, const QRectF& r);
    static void drawISCDApproachSymbol(QPainter* painter, const QString& approach, const QRectF& r);
    static void drawISCDLocationSymbol(QPainter* painter, const QString& location, const QRectF& r);

    // Custom symbol paths loaded from / saved to JSON (edited in ISCDSymbolEditor)
    static void        loadCustomSymbolPaths(const QString& filePath);
    static void        saveCustomSymbolPaths(const QString& filePath);
    static QJsonObject customSymbolPaths();
    static void        setCustomSymbolPaths(const QJsonObject& data);

private slots:
    void onDatabaseChanged();

private:
    struct PaintContext
    {
        const QTransform* map_to_viewport = nullptr;
        QSizeF viewport_size;
        qreal pixels_per_mm = 0.0;
        bool interactive = true;
    };

    struct NumberHit
    {
        QString control_id;
        QRectF bounds;
    };

    void paint(QPainter* painter, const PaintContext& context);

    // --- Course / control paint helpers ---
    void paintCourse(QPainter* painter, const Course& course, const PaintContext& context) const;
    void paintAllControls(QPainter* painter, const PaintContext& context) const;

    void paintLeg(QPainter* painter, QPointF from, QPointF to, const PaintContext& context) const;
    void paintStart(QPainter* painter, QPointF pos, double rotation_rad, const PaintContext& context) const;
    void paintControl(QPainter* painter, QPointF pos, const CourseControl& ctrl, const QString& number, const PaintContext& context) const;
    void paintFinish(QPainter* painter, QPointF pos, const PaintContext& context) const;
    void paintCrossingPoint(QPainter* painter, QPointF pos, const PaintContext& context) const;
    void paintControlNumber(QPainter* painter, QPointF center, const CourseControl& ctrl, const QString& number, const PaintContext& context) const;
    QPointF numberOffsetToViewport(const CourseControl& ctrl, const PaintContext& context) const;
    QPointF numberTextPosition(QPointF center, const CourseControl& ctrl, const PaintContext& context) const;
    void rememberNumberHit(QPainter* painter, const CourseControl& ctrl, const QString& number, QPointF text_pos, const PaintContext& context) const;
    int controlIndex(const QString& id) const;
    const NumberHit* numberHitAt(const QPoint& pos) const;

    // --- IOF description table ---
    void paintDescriptionTable(QPainter* painter, const PaintContext& context);

    /** Draws the content of one ISCD cell.  column is 0-based (0=A … 7=H). */
    void paintISCDCell(QPainter* painter, const QString& text,
                       const QRectF& cell, int column) const;

    /** Draws a miniature start triangle filling the given cell. */
    void paintStartCell(QPainter* painter, const QRectF& cell) const;

    /** Draws miniature finish concentric circles filling the given cell. */
    void paintFinishCell(QPainter* painter, const QRectF& cell) const;

    /** Returns the total course distance in meters (0 if < 2 resolved controls). */
    qreal computeCourseDistanceM(const Course& course) const;

    // --- Coordinate conversion ---
    /** Converts a MapCoord (1/1000 mm) to viewport pixel coordinates. */
    QPointF toViewport(const CourseControl& ctrl, const PaintContext& context) const;
    QPointF toViewport(const MapCoordF& coord, const PaintContext& context) const;
    MapCoordF viewportToMap(const QPointF& point, const PaintContext& context) const;
    QSizeF viewportSize(const PaintContext& context) const;

    /**
     * Converts millimeters on map paper to viewport pixels at the current zoom.
     * e.g. a 5mm IOF circle at 2× zoom = 2× more pixels.
     */
    qreal mmToViewportPx(qreal mm, const PaintContext& context) const;

    static QJsonObject s_custom;

    MapWidget* widget;
    CourseDatabase& db;
    const Course* visible_course = nullptr;
    bool planning_active = false;

    bool show_description_table = true;

    // --- Legend drag state ---
    MapCoordF legend_anchor;             ///< Top-left of legend in map coords (native units)
    bool legend_anchor_initialized = false;
    bool legend_dragging = false;
    bool legend_resizing = false;
    QPointF legend_drag_offset;          ///< Click pos relative to legend top-left (viewport px)
    QRectF legend_resize_start_bounds;
    double legend_resize_start_scale = 1.0;
    double legend_resize_current_scale = 1.0;
    mutable QRectF legend_bounds_cache;  ///< Updated each paint; used for hit-testing
    mutable QRectF legend_resize_handle_cache;

    // --- Control number drag state ---
    mutable QVector<NumberHit> number_hit_cache;
    QString number_drag_control_id;
    MapCoordF number_drag_start_map;
    MapCoordF number_drag_start_offset;
    bool number_dragging = false;
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_OVERLAY_H
