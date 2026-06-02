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

#include "course_undo.h"

#include <utility>

#include <QXmlStreamWriter>

#include "core/map.h"
#include "course/course_database.h"
#include "undo/undo.h"


namespace OpenOrienteering {


// ============================================================
// AddControlUndoStep
// ============================================================

AddControlUndoStep::AddControlUndoStep(Map* map, CourseControl control)
: UndoStep(CourseControlAddedType, map)
, added_control(std::move(control))
{}

UndoStep* AddControlUndoStep::undo()
{
    CourseDatabase& db = map->courseDatabase();

    // Find the control by id so we can capture its current state before removing
    for (int i = 0; i < db.numControls(); ++i)
    {
        if (db.control(i).id == added_control.id)
        {
            auto* redo = new RemoveControlUndoStep(map, db.control(i));
            db.removeControl(i);
            return redo;
        }
    }
    // Control no longer exists — return a no-op
    return new NoOpUndoStep(map, true);
}

void AddControlUndoStep::saveImpl(QXmlStreamWriter& xml) const
{
    UndoStep::saveImpl(xml);
    // Not persisted in v1 — loaded back as a valid no-op via getUndoStepForType().
}


// ============================================================
// RemoveControlUndoStep
// ============================================================

RemoveControlUndoStep::RemoveControlUndoStep(Map* map, CourseControl control)
: UndoStep(CourseControlRemovedType, map)
, removed_control(std::move(control))
{}

UndoStep* RemoveControlUndoStep::undo()
{
    CourseDatabase& db = map->courseDatabase();
    auto* redo = new AddControlUndoStep(map, removed_control);
    db.addControl(removed_control);
    return redo;
}

void RemoveControlUndoStep::saveImpl(QXmlStreamWriter& xml) const
{
    UndoStep::saveImpl(xml);
}


// ============================================================
// MoveControlUndoStep
// ============================================================

MoveControlUndoStep::MoveControlUndoStep(Map* map, QString control_id,
                                         MapCoord old_pos, MapCoord new_pos)
: UndoStep(CourseControlMovedType, map)
, control_id(std::move(control_id))
, old_pos(old_pos)
, new_pos(new_pos)
{}

UndoStep* MoveControlUndoStep::undo()
{
    CourseDatabase& db = map->courseDatabase();

    auto* ctrl = db.findById(control_id);
    if (!ctrl)
        return new NoOpUndoStep(map, true);

    // Capture current position so redo can restore it
    MapCoord current_pos = ctrl->position;
    auto updated = *ctrl;
    updated.position = old_pos;
    int index = -1;
    for (int i = 0; i < db.numControls(); ++i)
    {
        if (db.control(i).id == control_id)
        {
            index = i;
            break;
        }
    }
    db.updateControl(index, std::move(updated));

    // Redo step moves it from old_pos back to new_pos (i.e., current_pos)
    return new MoveControlUndoStep(map, control_id, current_pos, old_pos);
}

void MoveControlUndoStep::saveImpl(QXmlStreamWriter& xml) const
{
    UndoStep::saveImpl(xml);
}


// ============================================================
// ModifyControlDescriptionUndoStep
// ============================================================

ModifyControlDescriptionUndoStep::ModifyControlDescriptionUndoStep(
        Map* map,
        QString control_id,
        ControlDescription old_desc,
        ControlDescription new_desc)
: UndoStep(CourseControlDescEditType, map)
, control_id(std::move(control_id))
, old_desc(std::move(old_desc))
, new_desc(std::move(new_desc))
{}

UndoStep* ModifyControlDescriptionUndoStep::undo()
{
    CourseDatabase& db = map->courseDatabase();
    auto* ctrl = db.findById(control_id);
    if (!ctrl)
        return new NoOpUndoStep(map, true);

    int index = -1;
    for (int i = 0; i < db.numControls(); ++i)
    {
        if (db.control(i).id == control_id)
        {
            index = i;
            break;
        }
    }
    auto updated = *ctrl;
    updated.description = old_desc;
    db.updateControl(index, std::move(updated));

    // Redo: swap old and new
    return new ModifyControlDescriptionUndoStep(map, control_id, new_desc, old_desc);
}

void ModifyControlDescriptionUndoStep::saveImpl(QXmlStreamWriter& xml) const
{
    UndoStep::saveImpl(xml);
}


// ============================================================
// CoursesChangedUndoStep
// ============================================================

CoursesChangedUndoStep::CoursesChangedUndoStep(Map* map, std::vector<Course> before_snapshot)
: UndoStep(CoursesChangedType, map)
, snapshot(std::move(before_snapshot))
{}

UndoStep* CoursesChangedUndoStep::undo()
{
    CourseDatabase& db = map->courseDatabase();

    // Capture current state for the redo step
    std::vector<Course> current;
    current.reserve(std::size_t(db.numCourses()));
    for (int i = 0; i < db.numCourses(); ++i)
        current.push_back(db.course(i));

    auto* redo = new CoursesChangedUndoStep(map, std::move(current));

    // Restore courses from snapshot:
    // Remove all current courses (in reverse to keep indices stable)
    while (db.numCourses() > 0)
        db.removeCourse(db.numCourses() - 1);

    // Re-add from snapshot
    for (auto& c : snapshot)
        db.addCourse(c);

    return redo;
}

void CoursesChangedUndoStep::saveImpl(QXmlStreamWriter& xml) const
{
    UndoStep::saveImpl(xml);
}


}  // namespace OpenOrienteering
