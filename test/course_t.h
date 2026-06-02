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

#ifndef OPENORIENTEERING_COURSE_T_H
#define OPENORIENTEERING_COURSE_T_H

#include <QObject>

namespace OpenOrienteering {


/**
 * Tests for the Course Planning module:
 *  - CourseDatabase XML round-trip
 *  - Unique id generation
 *  - Database change signals
 */
class CourseTest : public QObject
{
    Q_OBJECT

private slots:
    void roundtrip_data();
    void roundtrip();

    void generateUniqueId();
    void databaseSignals();
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_T_H
