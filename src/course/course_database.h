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
#include <QStringList>

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


    // --- Course file association ---
    //
    // The database's content may be associated with a *.courses sidecar
    // file living next to the map file. The association is tracked here
    // (rather than derived from a fixed naming convention) so that a map
    // can be detached from its course file without deleting or renaming
    // that file, and so a map can remember previously-used course files
    // for quick re-attachment.
    //
    // Both fields are just names (relative to the map's directory), not
    // file content — resolving/reading/writing is done by callers via
    // CourseSerialization.

    /** Name (relative to the map directory) of the currently associated
     *  course file, or empty if the database is not associated with any
     *  file (e.g. right after "Clear courses from map"). */
    const QString& activeFile() const { return active_file; }

    /**
     * Sets the active file. A non-empty name is also recorded in the
     * recent-files history (moved to the front, deduplicated). Passing
     * an empty name only clears the active pointer — any previous name
     * stays in the history so it won't be picked again for a new file.
     */
    void setActiveFile(const QString& name);

    /** Sets the active file without touching the history. Used when
     *  restoring an exact prior state (undo/redo, loading from XML). */
    void setActiveFileRaw(const QString& name);

    /** Names (relative to the map directory) of course files previously
     *  associated with this map, most-recently-used first. */
    const QStringList& recentFiles() const { return recent_files; }

    /** Sets the recent-files history verbatim. Used when loading from XML. */
    void setRecentFilesRaw(QStringList names);


signals:
    void eventNameChanged();
    void controlAdded(int index);
    void controlChanged(int index);
    void controlRemoved(int index);
    void courseAdded(int index);
    void courseChanged(int index);
    void courseRemoved(int index);
    void activeFileChanged();

private:
    QString event_name;
    std::vector<CourseControl> controls;
    std::vector<Course> courses;
    MapCoordF legend_anchor;
    bool legend_anchor_valid = false;
    QString active_file;
    QStringList recent_files;
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_DATABASE_H
