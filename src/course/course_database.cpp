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

#include "course_database.h"

#include <algorithm>

#include <QString>

namespace OpenOrienteering {


CourseDatabase::CourseDatabase(QObject* parent)
: QObject(parent)
{}

CourseDatabase::~CourseDatabase() = default;


bool CourseDatabase::operator==(const CourseDatabase& other) const
{
    return event_name == other.event_name
        && controls   == other.controls
        && courses    == other.courses;
}


void CourseDatabase::setEventName(const QString& name)
{
    if (event_name != name)
    {
        event_name = name;
        emit eventNameChanged();
    }
}


// --- Controls ---

const CourseControl* CourseDatabase::findById(const QString& id) const
{
    auto it = std::find_if(controls.begin(), controls.end(),
                           [&id](const CourseControl& c) { return c.id == id; });
    return it != controls.end() ? &*it : nullptr;
}

CourseControl* CourseDatabase::findById(const QString& id)
{
    auto it = std::find_if(controls.begin(), controls.end(),
                           [&id](const CourseControl& c) { return c.id == id; });
    return it != controls.end() ? &*it : nullptr;
}

int CourseDatabase::addControl(CourseControl ctrl)
{
    controls.push_back(std::move(ctrl));
    int index = static_cast<int>(controls.size()) - 1;
    emit controlAdded(index);
    return index;
}

void CourseDatabase::updateControl(int index, CourseControl ctrl)
{
    controls[std::size_t(index)] = std::move(ctrl);
    emit controlChanged(index);
}

void CourseDatabase::removeControl(int index)
{
    controls.erase(controls.begin() + index);
    emit controlRemoved(index);
}

QString CourseDatabase::generateUniqueId(ControlType type) const
{
    if (type == ControlType::Start)
    {
        if (!findById(QStringLiteral("S1")))
            return QStringLiteral("S1");
        // Multiple starts: S2, S3, ...
        for (int n = 2; ; ++n)
        {
            QString id = QStringLiteral("S") + QString::number(n);
            if (!findById(id))
                return id;
        }
    }

    if (type == ControlType::Finish)
    {
        if (!findById(QStringLiteral("F1")))
            return QStringLiteral("F1");
        for (int n = 2; ; ++n)
        {
            QString id = QStringLiteral("F") + QString::number(n);
            if (!findById(id))
                return id;
        }
    }

    // Regular / CrossingPoint: find next free integer starting from 101
    for (int n = 101; ; ++n)
    {
        QString id = QString::number(n);
        if (!findById(id))
            return id;
    }
}


// --- Courses ---

int CourseDatabase::addCourse(Course c)
{
    courses.push_back(std::move(c));
    int index = static_cast<int>(courses.size()) - 1;
    emit courseAdded(index);
    return index;
}

void CourseDatabase::updateCourse(int index, Course c)
{
    courses[std::size_t(index)] = std::move(c);
    emit courseChanged(index);
}

void CourseDatabase::removeCourse(int index)
{
    courses.erase(courses.begin() + index);
    emit courseRemoved(index);
}


}  // namespace OpenOrienteering
