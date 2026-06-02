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

#include "course_t.h"

#include <QtTest>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "course/course.h"
#include "course/course_control.h"
#include "course/course_database.h"
#include "course/course_serialization.h"
#include "core/map_coord.h"


namespace OpenOrienteering {


void CourseTest::roundtrip_data()
{
    QTest::addColumn<QString>("event_name");
    QTest::addColumn<int>("num_controls");
    QTest::addColumn<int>("num_courses");

    QTest::newRow("empty")    << QString()                    << 0 << 0;
    QTest::newRow("one_ctrl") << QStringLiteral("Test")       << 1 << 0;
    QTest::newRow("full")     << QStringLiteral("Sprint 2026") << 3 << 2;
}

void CourseTest::roundtrip()
{
    QFETCH(QString, event_name);
    QFETCH(int, num_controls);
    QFETCH(int, num_courses);

    // ── Build a CourseDatabase ──────────────────────────────────────────────

    CourseDatabase original;
    original.setEventName(event_name);

    if (num_controls >= 1)
    {
        CourseControl start;
        start.id       = QStringLiteral("S1");
        start.type     = ControlType::Start;
        start.position = MapCoord::fromNative(10000, -20000);
        original.addControl(start);
    }

    if (num_controls >= 2)
    {
        CourseControl ctrl;
        ctrl.id                    = QStringLiteral("101");
        ctrl.type                  = ControlType::Regular;
        ctrl.position              = MapCoord::fromNative(15000, -22000);
        ctrl.description.code      = QStringLiteral("101");
        ctrl.description.feature_part = QStringLiteral("N");
        ctrl.description.feature   = QStringLiteral("boulder");
        ctrl.description.location_detail = QStringLiteral("foot");
        ctrl.description.dimensions = QStringLiteral("2x1");
        original.addControl(ctrl);
    }

    if (num_controls >= 3)
    {
        CourseControl finish;
        finish.id       = QStringLiteral("F1");
        finish.type     = ControlType::Finish;
        finish.position = MapCoord::fromNative(20000, -25000);
        original.addControl(finish);
    }

    if (num_courses >= 1)
    {
        Course c;
        c.name = QStringLiteral("Short");
        c.type = CourseType::Linear;
        if (num_controls >= 1) c.entries.push_back(CourseEntry{QStringLiteral("S1")});
        if (num_controls >= 2) c.entries.push_back(CourseEntry{QStringLiteral("101")});
        if (num_controls >= 3) c.entries.push_back(CourseEntry{QStringLiteral("F1")});
        original.addCourse(c);
    }

    if (num_courses >= 2)
    {
        Course c;
        c.name = QStringLiteral("Long");
        c.type = CourseType::Linear;
        if (num_controls >= 3) c.entries.push_back(CourseEntry{QStringLiteral("S1")});
        if (num_controls >= 2) c.entries.push_back(CourseEntry{QStringLiteral("101")});
        if (num_controls >= 3) c.entries.push_back(CourseEntry{QStringLiteral("F1")});
        original.addCourse(c);
    }

    // ── Serialise to XML in memory ─────────────────────────────────────────

    QByteArray buffer;
    {
        QXmlStreamWriter writer(&buffer);
        writer.setAutoFormatting(true);
        writer.writeStartDocument();
        CourseSerialization::save(writer, original);
        writer.writeEndDocument();
    }

    QVERIFY(!buffer.isEmpty());

    // ── Deserialise ────────────────────────────────────────────────────────

    CourseDatabase loaded;
    {
        QXmlStreamReader reader(buffer);
        // advance to <courses> start element
        while (!reader.atEnd() && !(reader.isStartElement() && reader.name() == QLatin1String("courses")))
            reader.readNext();
        QVERIFY(!reader.hasError());
        QVERIFY(reader.isStartElement());
        CourseSerialization::load(reader, loaded);
    }

    // ── Compare ────────────────────────────────────────────────────────────

    QCOMPARE(loaded.eventName(),   original.eventName());
    QCOMPARE(loaded.numControls(), original.numControls());
    QCOMPARE(loaded.numCourses(),  original.numCourses());

    for (int i = 0; i < original.numControls(); ++i)
    {
        const auto& o = original.control(i);
        const auto& l = loaded.control(i);
        QCOMPARE(l.id,           o.id);
        QCOMPARE(l.type,         o.type);
        QCOMPARE(l.position,     o.position);

        QCOMPARE(l.description.code,            o.description.code);
        QCOMPARE(l.description.feature_part,    o.description.feature_part);
        QCOMPARE(l.description.feature,         o.description.feature);
        QCOMPARE(l.description.location_detail, o.description.location_detail);
        QCOMPARE(l.description.dimensions,      o.description.dimensions);
    }

    for (int i = 0; i < original.numCourses(); ++i)
    {
        const auto& o = original.course(i);
        const auto& l = loaded.course(i);
        QCOMPARE(l.name,              o.name);
        QCOMPARE(l.type,              o.type);
        QCOMPARE((int)l.entries.size(), (int)o.entries.size());
        for (std::size_t j = 0; j < o.entries.size(); ++j)
            QCOMPARE(l.entries[j].control_id, o.entries[j].control_id);
    }
}


void CourseTest::generateUniqueId()
{
    CourseDatabase db;

    QCOMPARE(db.generateUniqueId(ControlType::Start),   QStringLiteral("S1"));
    QCOMPARE(db.generateUniqueId(ControlType::Finish),  QStringLiteral("F1"));
    QCOMPARE(db.generateUniqueId(ControlType::Regular), QStringLiteral("101"));

    CourseControl s1;
    s1.id   = QStringLiteral("S1");
    s1.type = ControlType::Start;
    db.addControl(s1);

    QCOMPARE(db.generateUniqueId(ControlType::Start),   QStringLiteral("S2"));
    QCOMPARE(db.generateUniqueId(ControlType::Regular), QStringLiteral("101"));
}


void CourseTest::databaseSignals()
{
    CourseDatabase db;

    int added_idx   = -1;
    int changed_idx = -1;
    int removed_idx = -1;

    connect(&db, &CourseDatabase::controlAdded,
            [&](int i) { added_idx = i; });
    connect(&db, &CourseDatabase::controlChanged,
            [&](int i) { changed_idx = i; });
    connect(&db, &CourseDatabase::controlRemoved,
            [&](int i) { removed_idx = i; });

    CourseControl ctrl;
    ctrl.id   = QStringLiteral("101");
    ctrl.type = ControlType::Regular;
    ctrl.position = MapCoord::fromNative(0, 0);

    const int idx = db.addControl(ctrl);
    QCOMPARE(added_idx, idx);

    ctrl.description.code = QStringLiteral("101");
    db.updateControl(idx, ctrl);
    QCOMPARE(changed_idx, idx);

    db.removeControl(idx);
    QCOMPARE(removed_idx, idx);
    QCOMPARE(db.numControls(), 0);
}


}  // namespace OpenOrienteering

QTEST_GUILESS_MAIN(OpenOrienteering::CourseTest)
