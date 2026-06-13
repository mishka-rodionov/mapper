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

#ifndef OPENORIENTEERING_COURSE_DATABASE_H
#define OPENORIENTEERING_COURSE_DATABASE_H

#include <vector>

#include <QObject>
#include <QString>

#include "core/map_coord.h"
#include "course/course.h"
#include "course/course_control.h"

namespace OpenOrienteering {


/**
 * Top-level container for all course planning data of a map.
 *
 * Owned by Map as a unique_ptr member. Emits fine-grained signals on any
 * mutation so that the CourseOverlay and CoursePanelWidget can react without
 * polling.
 */
class CourseDatabase : public QObject
{
    Q_OBJECT

public:
    explicit CourseDatabase(QObject* parent = nullptr);
    ~CourseDatabase() override;

    CourseDatabase(const CourseDatabase&) = delete;
    CourseDatabase& operator=(const CourseDatabase&) = delete;

    bool operator==(const CourseDatabase& other) const;
    bool operator!=(const CourseDatabase& other) const { return !(*this == other); }


    // --- Event info ---

    const QString& eventName() const { return event_name; }
    void setEventName(const QString& name);


    // --- Controls ---

    int numControls() const { return static_cast<int>(controls.size()); }

    const CourseControl& control(int i) const { return controls[std::size_t(i)]; }
    CourseControl& control(int i) { return controls[std::size_t(i)]; }

    /** Returns nullptr if id is not found. */
    const CourseControl* findById(const QString& id) const;
    CourseControl* findById(const QString& id);

    /** Appends a control and emits controlAdded(). Returns the new index. */
    int addControl(CourseControl ctrl);

    /** Replaces control at index and emits controlChanged(). */
    void updateControl(int index, CourseControl ctrl);

    /** Removes control at index and emits controlRemoved(). */
    void removeControl(int index);

    /**
     * Generates a unique id for a new control of the given type.
     * Start → "S1"; Finish → "F1"; Regular → next unused integer string.
     */
    QString generateUniqueId(ControlType type) const;


    // --- Courses ---

    int numCourses() const { return static_cast<int>(courses.size()); }

    const Course& course(int i) const { return courses[std::size_t(i)]; }
    Course& course(int i) { return courses[std::size_t(i)]; }

    /** Appends a course and emits courseAdded(). Returns the new index. */
    int addCourse(Course c);

    /** Replaces course at index and emits courseChanged(). */
    void updateCourse(int index, Course c);

    /** Removes course at index and emits courseRemoved(). */
    void removeCourse(int index);


    // --- Legend anchor ---

    bool hasLegendAnchor() const { return legend_anchor_valid; }
    MapCoordF legendAnchor() const { return legend_anchor; }
    void setLegendAnchor(const MapCoordF& anchor);
    void clearLegendAnchor();


signals:
    void eventNameChanged();
    void controlAdded(int index);
    void controlChanged(int index);
    void controlRemoved(int index);
    void courseAdded(int index);
    void courseChanged(int index);
    void courseRemoved(int index);

private:
    QString event_name;
    std::vector<CourseControl> controls;
    std::vector<Course> courses;
    MapCoordF legend_anchor;
    bool legend_anchor_valid = false;
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_DATABASE_H
