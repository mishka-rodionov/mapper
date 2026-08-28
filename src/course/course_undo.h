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

#ifndef OPENORIENTEERING_COURSE_UNDO_H
#define OPENORIENTEERING_COURSE_UNDO_H

#include <vector>

#include <QString>

#include "course/course.h"
#include "course/course_control.h"
#include "core/map_coord.h"
#include "undo/undo.h"

namespace OpenOrienteering {

class Map;


// Note: CourseControlAddedType … CoursesChangedType are declared in UndoStep::Type (undo.h).


/**
 * Undo step for when a control was ADDED to the database.
 * undo() removes the control by id.
 */
class AddControlUndoStep : public UndoStep
{
public:
    AddControlUndoStep(Map* map, CourseControl control);
    ~AddControlUndoStep() override = default;

    UndoStep* undo() override;

protected:
    void saveImpl(QXmlStreamWriter& xml) const override;

private:
    CourseControl added_control;
};


/**
 * Undo step for when a control was REMOVED from the database.
 * undo() re-inserts the control (at end, v1 does not restore original index).
 */
class RemoveControlUndoStep : public UndoStep
{
public:
    RemoveControlUndoStep(Map* map, CourseControl control);
    ~RemoveControlUndoStep() override = default;

    UndoStep* undo() override;

protected:
    void saveImpl(QXmlStreamWriter& xml) const override;

private:
    CourseControl removed_control;
};


/**
 * Undo step for when a control was MOVED (position changed).
 * undo() restores the old position.
 */
class MoveControlUndoStep : public UndoStep
{
public:
    MoveControlUndoStep(Map* map, QString control_id, MapCoord old_pos, MapCoord new_pos);
    ~MoveControlUndoStep() override = default;

    UndoStep* undo() override;

protected:
    void saveImpl(QXmlStreamWriter& xml) const override;

private:
    QString   control_id;
    MapCoord  old_pos;
    MapCoord  new_pos;
};


/**
 * Undo step for when a control number label was moved.
 * undo() restores the old label offset.
 */
class MoveControlNumberUndoStep : public UndoStep
{
public:
    MoveControlNumberUndoStep(Map* map, QString control_id, MapCoordF old_offset, MapCoordF new_offset);
    ~MoveControlNumberUndoStep() override = default;

    UndoStep* undo() override;

protected:
    void saveImpl(QXmlStreamWriter& xml) const override;

private:
    QString   control_id;
    MapCoordF old_offset;
    MapCoordF new_offset;
};


/**
 * Undo step for when a control's IOF description was edited.
 * undo() restores the old description.
 */
class ModifyControlDescriptionUndoStep : public UndoStep
{
public:
    ModifyControlDescriptionUndoStep(Map* map, QString control_id,
                                     ControlDescription old_desc,
                                     ControlDescription new_desc);
    ~ModifyControlDescriptionUndoStep() override = default;

    UndoStep* undo() override;

protected:
    void saveImpl(QXmlStreamWriter& xml) const override;

private:
    QString            control_id;
    ControlDescription old_desc;
    ControlDescription new_desc;
};


/**
 * Undo step for any change to the courses list
 * (add course, remove course, rename, reorder entries).
 *
 * Stores a full snapshot of the courses vector before the change.
 * undo() replaces the current courses with the snapshot and returns
 * a step holding the post-change state (for redo).
 */
class CoursesChangedUndoStep : public UndoStep
{
public:
    CoursesChangedUndoStep(Map* map, std::vector<Course> before_snapshot);
    ~CoursesChangedUndoStep() override = default;

    UndoStep* undo() override;

protected:
    void saveImpl(QXmlStreamWriter& xml) const override;

private:
    std::vector<Course> snapshot;
};


/**
 * Undo step for replacing the entire course database in one go
 * (e.g. importing a .courses file onto the map).
 *
 * Stores a full snapshot of controls, courses, event name and legend anchor
 * before the change. undo() clears the database and restores the snapshot,
 * returning a step holding the post-change state (for redo).
 */
class ReplaceCourseDatabaseUndoStep : public UndoStep
{
public:
    ReplaceCourseDatabaseUndoStep(Map* map,
                                  std::vector<CourseControl> controls_snapshot,
                                  std::vector<Course> courses_snapshot,
                                  QString event_name_snapshot,
                                  bool legend_anchor_valid_snapshot,
                                  MapCoordF legend_anchor_snapshot,
                                  QString active_file_snapshot = {});
    ~ReplaceCourseDatabaseUndoStep() override = default;

    UndoStep* undo() override;

protected:
    void saveImpl(QXmlStreamWriter& xml) const override;

private:
    std::vector<CourseControl> controls_snapshot;
    std::vector<Course>        courses_snapshot;
    QString                    event_name_snapshot;
    bool                       legend_anchor_valid_snapshot;
    MapCoordF                  legend_anchor_snapshot;
    QString                    active_file_snapshot;
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_UNDO_H
