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

private slots:
    void onDatabaseChanged();

private:
    // --- Paint helpers ---
    void paintCourse(QPainter* painter, const Course& course) const;
    void paintAllControls(QPainter* painter) const;

    void paintLeg(QPainter* painter, QPointF from, QPointF to) const;
    void paintStart(QPainter* painter, QPointF pos, double rotation_rad) const;
    void paintControl(QPainter* painter, QPointF pos, const QString& number) const;
    void paintFinish(QPainter* painter, QPointF pos) const;
    void paintCrossingPoint(QPainter* painter, QPointF pos) const;
    void paintControlNumber(QPainter* painter, QPointF center, const QString& number) const;

    /** Converts a MapCoord (1/1000 mm) to viewport pixel coordinates. */
    QPointF toViewport(const CourseControl& ctrl) const;

    /**
     * Converts millimeters on map paper to viewport pixels at the current zoom.
     * e.g. a 5mm IOF circle at 2× zoom = 2× more pixels.
     */
    qreal mmToViewportPx(qreal mm) const;

    /** Draws the IOF control description table for the visible course. */
    void paintDescriptionTable(QPainter* painter) const;

    MapWidget* widget;
    const CourseDatabase& db;
    const Course* visible_course = nullptr;

    bool show_description_table = true;
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_OVERLAY_H
