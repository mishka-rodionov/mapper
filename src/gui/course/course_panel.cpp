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

#include "course_panel.h"

#include <utility>
#include <vector>

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "core/map.h"
#include "course/course.h"
#include "course/course_control.h"
#include "course/course_database.h"
#include "course/course_overlay.h"
#include "course/course_undo.h"
#include "gui/course/control_properties_widget.h"


namespace OpenOrienteering {

namespace {

QString controlTypeLabel(ControlType t)
{
    switch (t)
    {
    case ControlType::Start:        return CoursePanelWidget::tr("Start");
    case ControlType::Finish:       return CoursePanelWidget::tr("Finish");
    case ControlType::CrossingPoint: return CoursePanelWidget::tr("Crossing");
    default:                        return CoursePanelWidget::tr("Control");
    }
}

}  // anonymous namespace


CoursePanelWidget::CoursePanelWidget(Map& map, CourseDatabase& db,
                                     CourseOverlay* overlay, QWidget* parent)
: QWidget(parent)
, map(map)
, db(db)
, overlay(overlay)
{
    tabs = new QTabWidget(this);

    // ── Controls tab ──────────────────────────────────────────────
    {
        // Type selector: four exclusive toggle buttons for Start / Control / Finish / Crossing.
        // Clicking one sets the type that will be assigned to the next placed control.
        type_btn_start    = new QToolButton;
        type_btn_regular  = new QToolButton;
        type_btn_finish   = new QToolButton;
        type_btn_crossing = new QToolButton;

        type_btn_start->setText(tr("△ Start"));
        type_btn_regular->setText(tr("○ Control"));
        type_btn_finish->setText(tr("◎ Finish"));
        type_btn_crossing->setText(tr("✕ Crossing"));

        type_btn_start->setToolTip(tr("Next placed control will be a Start (triangle)"));
        type_btn_regular->setToolTip(tr("Next placed control will be a regular Control (circle)"));
        type_btn_finish->setToolTip(tr("Next placed control will be a Finish (double circle)"));
        type_btn_crossing->setToolTip(tr("Next placed control will be a Crossing Point"));

        for (auto* btn : {type_btn_start, type_btn_regular, type_btn_finish, type_btn_crossing})
        {
            btn->setCheckable(true);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
        type_btn_regular->setChecked(true);  // default

        type_button_group = new QButtonGroup(this);
        type_button_group->addButton(type_btn_start,    static_cast<int>(ControlType::Start));
        type_button_group->addButton(type_btn_regular,  static_cast<int>(ControlType::Regular));
        type_button_group->addButton(type_btn_finish,   static_cast<int>(ControlType::Finish));
        type_button_group->addButton(type_btn_crossing, static_cast<int>(ControlType::CrossingPoint));
        type_button_group->setExclusive(true);

        connect(type_button_group, QOverload<int>::of(&QButtonGroup::buttonClicked),
                this, [this](int id) {
                    emit nextControlTypeChangeRequested(static_cast<ControlType>(id));
                });

        auto* type_row = new QHBoxLayout;
        type_row->setSpacing(2);
        type_row->setContentsMargins(0, 0, 0, 0);
        type_row->addWidget(type_btn_start);
        type_row->addWidget(type_btn_regular);
        type_row->addWidget(type_btn_finish);
        type_row->addWidget(type_btn_crossing);

        controls_tree = new QTreeWidget;
        controls_tree->setColumnCount(2);
        controls_tree->setHeaderLabels({tr("Code"), tr("Type")});
        controls_tree->setContextMenuPolicy(Qt::CustomContextMenu);
        controls_tree->setRootIsDecorated(false);
        controls_tree->setSelectionMode(QAbstractItemView::SingleSelection);

        connect(controls_tree, &QTreeWidget::itemClicked,
                this, &CoursePanelWidget::onControlItemClicked);
        connect(controls_tree, &QTreeWidget::customContextMenuRequested,
                this, &CoursePanelWidget::onControlContextMenu);

        properties_widget = new ControlPropertiesWidget(map, db);

        auto* layout = new QVBoxLayout;
        layout->addLayout(type_row);
        layout->addWidget(controls_tree, 1);
        layout->addWidget(properties_widget);
        layout->setContentsMargins(4, 4, 4, 4);
        auto* tab = new QWidget;
        tab->setLayout(layout);
        tabs->addTab(tab, tr("Controls"));
    }

    // ── Courses tab ───────────────────────────────────────────────
    {
        courses_list = new QListWidget;
        courses_list->setSelectionMode(QAbstractItemView::SingleSelection);
        connect(courses_list, &QListWidget::currentRowChanged,
                this, &CoursePanelWidget::onCourseSelectionChanged);

        add_course_btn    = new QPushButton(tr("+"));
        rename_btn        = new QPushButton(tr("Rename"));
        remove_course_btn = new QPushButton(tr("−"));
        add_course_btn->setToolTip(tr("Add course"));
        rename_btn->setToolTip(tr("Rename selected course"));
        remove_course_btn->setToolTip(tr("Remove selected course"));

        connect(add_course_btn,    &QPushButton::clicked, this, &CoursePanelWidget::addCourse);
        connect(rename_btn,        &QPushButton::clicked, this, &CoursePanelWidget::renameCourse);
        connect(remove_course_btn, &QPushButton::clicked, this, &CoursePanelWidget::removeCourse);

        auto* course_btns = new QHBoxLayout;
        course_btns->addWidget(add_course_btn);
        course_btns->addWidget(rename_btn);
        course_btns->addStretch();
        course_btns->addWidget(remove_course_btn);

        // Entries sub-section
        entries_list = new QListWidget;
        entries_list->setSelectionMode(QAbstractItemView::SingleSelection);

        add_entry_btn    = new QPushButton(tr("Add selected"));
        remove_entry_btn = new QPushButton(tr("Remove"));
        entry_up_btn     = new QPushButton(tr("↑"));
        entry_down_btn   = new QPushButton(tr("↓"));
        add_entry_btn->setToolTip(tr("Add the control selected in the Controls tab to this course"));
        remove_entry_btn->setToolTip(tr("Remove the selected entry from this course"));

        connect(add_entry_btn,    &QPushButton::clicked, this, &CoursePanelWidget::addSelectedControlToCourse);
        connect(remove_entry_btn, &QPushButton::clicked, this, &CoursePanelWidget::removeEntryFromCourse);
        connect(entry_up_btn,     &QPushButton::clicked, this, &CoursePanelWidget::moveEntryUp);
        connect(entry_down_btn,   &QPushButton::clicked, this, &CoursePanelWidget::moveEntryDown);

        auto* entry_btns = new QHBoxLayout;
        entry_btns->addWidget(add_entry_btn);
        entry_btns->addWidget(remove_entry_btn);
        entry_btns->addStretch();
        entry_btns->addWidget(entry_up_btn);
        entry_btns->addWidget(entry_down_btn);

        // Climb (manual entry)
        climb_label   = new QLabel(tr("Climb:"));
        climb_spinbox = new QSpinBox;
        climb_spinbox->setRange(0, 9999);
        climb_spinbox->setSuffix(tr(" m"));
        climb_spinbox->setToolTip(tr("Total climb along the course in meters (enter manually)"));
        climb_spinbox->setEnabled(false);
        climb_label->setEnabled(false);

        connect(climb_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &CoursePanelWidget::onClimbValueChanged);

        auto* climb_row = new QHBoxLayout;
        climb_row->addWidget(climb_label);
        climb_row->addWidget(climb_spinbox, 1);

        auto* layout = new QVBoxLayout;
        layout->addWidget(new QLabel(tr("Courses:")));
        layout->addWidget(courses_list, 2);
        layout->addLayout(course_btns);
        layout->addWidget(new QLabel(tr("Entries:")));
        layout->addWidget(entries_list, 3);
        layout->addLayout(entry_btns);
        layout->addLayout(climb_row);
        layout->setContentsMargins(4, 4, 4, 4);
        auto* tab = new QWidget;
        tab->setLayout(layout);
        tabs->addTab(tab, tr("Courses"));
    }

    auto* main_layout = new QVBoxLayout(this);
    main_layout->addWidget(tabs);
    main_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(main_layout);

    // Connect to database signals
    connect(&db, &CourseDatabase::controlAdded,   this, &CoursePanelWidget::rebuildControlsTree);
    connect(&db, &CourseDatabase::controlChanged, this, &CoursePanelWidget::rebuildControlsTree);
    connect(&db, &CourseDatabase::controlRemoved, this, &CoursePanelWidget::rebuildControlsTree);
    connect(&db, &CourseDatabase::courseAdded,    this, &CoursePanelWidget::rebuildCoursesList);
    connect(&db, &CourseDatabase::courseChanged,  this, &CoursePanelWidget::rebuildCoursesList);
    connect(&db, &CourseDatabase::courseRemoved,  this, &CoursePanelWidget::rebuildCoursesList);

    // Initial population
    rebuildControlsTree();
    rebuildCoursesList();
}


// ── Type selector ─────────────────────────────────────────────────────────────

void CoursePanelWidget::setNextControlType(ControlType type)
{
    if (auto* btn = type_button_group->button(static_cast<int>(type)))
        btn->setChecked(true);
}


// ── Select from outside ───────────────────────────────────────────────────────

void CoursePanelWidget::selectControl(const QString& control_id)
{
    selected_control_id = control_id;
    properties_widget->setControl(control_id);
    // Find and highlight in tree
    for (int i = 0; i < controls_tree->topLevelItemCount(); ++i)
    {
        auto* item = controls_tree->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == control_id)
        {
            controls_tree->setCurrentItem(item);
            break;
        }
    }
}


// ── Controls tab ─────────────────────────────────────────────────────────────

void CoursePanelWidget::rebuildControlsTree()
{
    if (rebuilding) return;
    rebuilding = true;

    controls_tree->clear();
    for (int i = 0; i < db.numControls(); ++i)
    {
        const auto& ctrl = db.control(i);
        auto* item = new QTreeWidgetItem;
        // Column 0: code (or id if code is empty)
        const QString label = ctrl.description.code.isEmpty() ? ctrl.id : ctrl.description.code;
        item->setText(0, label);
        item->setText(1, controlTypeLabel(ctrl.type));
        item->setData(0, Qt::UserRole, ctrl.id);
        controls_tree->addTopLevelItem(item);
    }
    controls_tree->resizeColumnToContents(0);

    rebuilding = false;
}

void CoursePanelWidget::onControlItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;
    const QString id = item->data(0, Qt::UserRole).toString();
    selected_control_id = id;
    properties_widget->setControl(id);
    emit controlSelected(id);
}

void CoursePanelWidget::onControlContextMenu(const QPoint& pos)
{
    auto* item = controls_tree->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    auto* del_act = menu.addAction(tr("Delete control"));
    if (menu.exec(controls_tree->mapToGlobal(pos)) == del_act)
    {
        selected_control_id = item->data(0, Qt::UserRole).toString();
        deleteSelectedControl();
    }
}

void CoursePanelWidget::deleteSelectedControl()
{
    if (selected_control_id.isEmpty()) return;
    for (int i = 0; i < db.numControls(); ++i)
    {
        if (db.control(i).id == selected_control_id)
        {
            map.push(new RemoveControlUndoStep(&map, db.control(i)));
            db.removeControl(i);
            selected_control_id.clear();
            emit controlSelected({});
            return;
        }
    }
}


// ── Courses tab ───────────────────────────────────────────────────────────────

void CoursePanelWidget::rebuildCoursesList()
{
    if (rebuilding) return;
    rebuilding = true;

    const int prev_row = courses_list->currentRow();
    courses_list->clear();
    for (int i = 0; i < db.numCourses(); ++i)
        courses_list->addItem(db.course(i).name);

    // Restore selection if still valid
    if (prev_row >= 0 && prev_row < courses_list->count())
        courses_list->setCurrentRow(prev_row);

    rebuilding = false;

    rebuildEntriesList();

    // Update overlay
    if (overlay)
    {
        const int idx = selectedCourseIndex();
        overlay->setVisibleCourse(idx >= 0 ? &db.course(idx) : nullptr);
    }
}

void CoursePanelWidget::rebuildEntriesList()
{
    if (rebuilding) return;
    entries_list->clear();

    const int idx = selectedCourseIndex();
    if (idx < 0) return;

    const auto& course = db.course(idx);
    for (const auto& entry : course.entries)
    {
        const auto* ctrl = db.findById(entry.control_id);
        QString label = entry.control_id;
        if (ctrl)
        {
            if (!ctrl->description.code.isEmpty())
                label = ctrl->description.code;
            label += QLatin1String(" (") + controlTypeLabel(ctrl->type) + QLatin1Char(')');
        }
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, entry.control_id);
        entries_list->addItem(item);
    }
}

void CoursePanelWidget::onCourseSelectionChanged()
{
    rebuildEntriesList();

    const int idx = selectedCourseIndex();

    // Update climb spinbox
    rebuilding = true;
    climb_spinbox->setEnabled(idx >= 0);
    climb_label->setEnabled(idx >= 0);
    if (idx >= 0)
        climb_spinbox->setValue(db.course(idx).climb_m);
    rebuilding = false;

    if (overlay)
        overlay->setVisibleCourse(idx >= 0 ? &db.course(idx) : nullptr);
}

void CoursePanelWidget::onClimbValueChanged(int value)
{
    const int idx = selectedCourseIndex();
    if (idx < 0 || rebuilding)
        return;
    auto snapshot   = coursesSnapshot();
    auto updated    = db.course(idx);
    updated.climb_m = value;
    db.updateCourse(idx, std::move(updated));
    map.push(new CoursesChangedUndoStep(&map, std::move(snapshot)));
}

void CoursePanelWidget::addCourse()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New course"),
                                               tr("Course name:"),
                                               QLineEdit::Normal, tr("Course"), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    auto snapshot = coursesSnapshot();
    Course c;
    c.name = name.trimmed();
    c.type = CourseType::Linear;
    db.addCourse(std::move(c));
    // courses_list row = last
    courses_list->setCurrentRow(courses_list->count() - 1);

    // Undo step records the before snapshot
    map.push(new CoursesChangedUndoStep(&map, std::move(snapshot)));
}

void CoursePanelWidget::renameCourse()
{
    const int idx = selectedCourseIndex();
    if (idx < 0) return;

    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename course"),
                                               tr("New name:"),
                                               QLineEdit::Normal, db.course(idx).name, &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    auto snapshot = coursesSnapshot();
    auto updated = db.course(idx);
    updated.name = name.trimmed();
    db.updateCourse(idx, std::move(updated));
    map.push(new CoursesChangedUndoStep(&map, std::move(snapshot)));
}

void CoursePanelWidget::removeCourse()
{
    const int idx = selectedCourseIndex();
    if (idx < 0) return;

    auto snapshot = coursesSnapshot();
    db.removeCourse(idx);
    map.push(new CoursesChangedUndoStep(&map, std::move(snapshot)));
}

void CoursePanelWidget::addSelectedControlToCourse()
{
    if (selected_control_id.isEmpty()) return;
    const int idx = selectedCourseIndex();
    if (idx < 0) return;

    auto snapshot = coursesSnapshot();
    auto updated = db.course(idx);
    CourseEntry entry;
    entry.control_id = selected_control_id;
    updated.entries.push_back(std::move(entry));
    db.updateCourse(idx, std::move(updated));
    map.push(new CoursesChangedUndoStep(&map, std::move(snapshot)));
}

void CoursePanelWidget::removeEntryFromCourse()
{
    const int course_idx = selectedCourseIndex();
    if (course_idx < 0) return;
    const int entry_idx = entries_list->currentRow();
    if (entry_idx < 0) return;

    auto snapshot = coursesSnapshot();
    auto updated = db.course(course_idx);
    updated.entries.erase(updated.entries.begin() + entry_idx);
    db.updateCourse(course_idx, std::move(updated));
    map.push(new CoursesChangedUndoStep(&map, std::move(snapshot)));
}

void CoursePanelWidget::moveEntryUp()
{
    const int course_idx = selectedCourseIndex();
    if (course_idx < 0) return;
    const int entry_idx = entries_list->currentRow();
    if (entry_idx <= 0) return;

    auto snapshot = coursesSnapshot();
    auto updated = db.course(course_idx);
    std::swap(updated.entries[std::size_t(entry_idx)],
              updated.entries[std::size_t(entry_idx) - 1]);
    db.updateCourse(course_idx, std::move(updated));
    map.push(new CoursesChangedUndoStep(&map, std::move(snapshot)));
    entries_list->setCurrentRow(entry_idx - 1);
}

void CoursePanelWidget::moveEntryDown()
{
    const int course_idx = selectedCourseIndex();
    if (course_idx < 0) return;
    const int entry_idx = entries_list->currentRow();
    if (entry_idx < 0 || entry_idx >= static_cast<int>(db.course(course_idx).entries.size()) - 1)
        return;

    auto snapshot = coursesSnapshot();
    auto updated = db.course(course_idx);
    std::swap(updated.entries[std::size_t(entry_idx)],
              updated.entries[std::size_t(entry_idx) + 1]);
    db.updateCourse(course_idx, std::move(updated));
    map.push(new CoursesChangedUndoStep(&map, std::move(snapshot)));
    entries_list->setCurrentRow(entry_idx + 1);
}


// ── Private helpers ───────────────────────────────────────────────────────────

std::vector<Course> CoursePanelWidget::coursesSnapshot() const
{
    std::vector<Course> snap;
    snap.reserve(std::size_t(db.numCourses()));
    for (int i = 0; i < db.numCourses(); ++i)
        snap.push_back(db.course(i));
    return snap;
}

int CoursePanelWidget::selectedCourseIndex() const
{
    return courses_list->currentRow();
}


}  // namespace OpenOrienteering
