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

#include <algorithm>
#include <utility>
#include <vector>

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
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

double boundedDescriptionScale(double scale)
{
    if (!(scale > 0.0))
        return 1.0;
    if (scale < 0.25)
        return 0.25;
    if (scale > 2.0)
        return 2.0;
    return scale;
}

int descriptionScaleToPercent(double scale)
{
    return static_cast<int>(boundedDescriptionScale(scale) * 100.0 + 0.5);
}

/**
 * A plain click toggles the clicked row's selection instead of replacing it,
 * so several controls can be picked one-by-one without holding Ctrl.
 * Shift+click still selects a contiguous range as usual.
 */
class ToggleSelectionTreeWidget : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;

protected:
    QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex& index,
                                                          const QEvent* event) const override
    {
        if (event && event->type() == QEvent::MouseButtonPress
            && !(static_cast<const QMouseEvent*>(event)->modifiers() & Qt::ShiftModifier))
        {
            return QItemSelectionModel::Toggle | QItemSelectionModel::Rows;
        }
        return QTreeWidget::selectionCommand(index, event);
    }
};

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

        type_btn_start->setText(tr("Start"));
        type_btn_regular->setText(tr("Control"));
        type_btn_finish->setText(tr("Finish"));
        type_btn_crossing->setText(tr("Crossing"));

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

        controls_tree = new ToggleSelectionTreeWidget;
        controls_tree->setColumnCount(2);
        controls_tree->setHeaderLabels({tr("Code"), tr("Type")});
        controls_tree->setContextMenuPolicy(Qt::CustomContextMenu);
        controls_tree->setRootIsDecorated(false);
        controls_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
        controls_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);

        connect(controls_tree, &QTreeWidget::itemClicked,
                this, &CoursePanelWidget::onControlItemClicked);
        connect(controls_tree, &QTreeWidget::customContextMenuRequested,
                this, &CoursePanelWidget::onControlContextMenu);
        connect(controls_tree->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &CoursePanelWidget::onControlSelectionModelChanged);

        selection_summary_label = new QLabel(tr("No controls selected"));
        selection_summary_label->setWordWrap(true);

        course_target_combo = new QComboBox;
        add_to_course_from_controls_btn = new QPushButton(tr("Add"));
        add_to_course_from_controls_btn->setToolTip(
            tr("Add all selected controls (Start/Finish/Regular/Crossing) to the chosen course"));
        add_to_course_from_controls_btn->setEnabled(false);

        connect(course_target_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &CoursePanelWidget::onCourseTargetComboChanged);
        connect(add_to_course_from_controls_btn, &QPushButton::clicked,
                this, &CoursePanelWidget::addSelectedControlToCourse);

        auto* add_to_course_row = new QHBoxLayout;
        add_to_course_row->addWidget(new QLabel(tr("Add to course:")));
        add_to_course_row->addWidget(course_target_combo, 1);
        add_to_course_row->addWidget(add_to_course_from_controls_btn);

        properties_widget = new ControlPropertiesWidget(map, db);

        auto* layout = new QVBoxLayout;
        layout->addLayout(type_row);
        layout->addWidget(controls_tree, 1);
        layout->addWidget(selection_summary_label);
        layout->addLayout(add_to_course_row);
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
        remove_course_btn = new QPushButton(tr("-"));
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

        // Course type: Linear (fixed order) vs Score (choice, points per control)
        course_type_combo = new QComboBox;
        course_type_combo->addItem(tr("Linear (fixed order)"), static_cast<int>(CourseType::Linear));
        course_type_combo->addItem(tr("Score (choice)"), static_cast<int>(CourseType::Score));
        course_type_combo->setToolTip(
            tr("Linear: controls must be visited in the listed order.\n"
               "Score: controls may be taken in any order, each worth points."));
        course_type_combo->setEnabled(false);

        connect(course_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &CoursePanelWidget::onCourseTypeComboChanged);

        auto* course_type_row = new QHBoxLayout;
        course_type_row->addWidget(new QLabel(tr("Type:")));
        course_type_row->addWidget(course_type_combo, 1);

        // Entries sub-section
        entries_list = new QListWidget;
        entries_list->setSelectionMode(QAbstractItemView::SingleSelection);

        add_entry_btn    = new QPushButton(tr("Add selected"));
        remove_entry_btn = new QPushButton(tr("Remove"));
        entry_up_btn     = new QPushButton(tr("Up"));
        entry_down_btn   = new QPushButton(tr("Down"));
        add_entry_btn->setToolTip(tr("Add the controls selected in the Controls tab to this course"));
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

        // Points for the selected entry (Score courses only)
        entry_points_label   = new QLabel(tr("Points:"));
        entry_points_spinbox = new QSpinBox;
        entry_points_spinbox->setRange(0, 999);
        entry_points_spinbox->setSuffix(tr(" pts"));
        entry_points_spinbox->setToolTip(tr("Score awarded for taking this control (Score courses only)"));
        entry_points_spinbox->setEnabled(false);
        entry_points_label->setEnabled(false);

        connect(entries_list, &QListWidget::currentRowChanged,
                this, &CoursePanelWidget::onEntriesSelectionChanged);
        connect(entry_points_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &CoursePanelWidget::onEntryPointsValueChanged);

        auto* entry_points_row = new QHBoxLayout;
        entry_points_row->addWidget(entry_points_label);
        entry_points_row->addWidget(entry_points_spinbox, 1);

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

        legend_scale_label = new QLabel(tr("Legend scale:"));
        legend_scale_spinbox = new QSpinBox;
        legend_scale_spinbox->setRange(25, 200);
        legend_scale_spinbox->setSingleStep(5);
        legend_scale_spinbox->setSuffix(tr(" %"));
        legend_scale_spinbox->setToolTip(tr("Scale of the control description table drawn on the map"));
        legend_scale_spinbox->setEnabled(false);
        legend_scale_label->setEnabled(false);

        connect(legend_scale_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &CoursePanelWidget::onLegendScaleValueChanged);

        auto* legend_scale_row = new QHBoxLayout;
        legend_scale_row->addWidget(legend_scale_label);
        legend_scale_row->addWidget(legend_scale_spinbox, 1);

        default_points_label   = new QLabel(tr("Default points:"));
        default_points_spinbox = new QSpinBox;
        default_points_spinbox->setRange(1, 999);
        default_points_spinbox->setSuffix(tr(" pts"));
        default_points_spinbox->setToolTip(
            tr("Score assigned automatically to newly added controls in this course"));
        default_points_spinbox->setEnabled(false);
        default_points_label->setEnabled(false);

        connect(default_points_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &CoursePanelWidget::onDefaultPointsValueChanged);

        auto* default_points_row = new QHBoxLayout;
        default_points_row->addWidget(default_points_label);
        default_points_row->addWidget(default_points_spinbox, 1);

        auto* layout = new QVBoxLayout;
        layout->addWidget(new QLabel(tr("Courses:")));
        layout->addWidget(courses_list, 2);
        layout->addLayout(course_btns);
        layout->addLayout(course_type_row);
        layout->addLayout(climb_row);
        layout->addLayout(legend_scale_row);
        layout->addLayout(default_points_row);
        layout->addWidget(new QLabel(tr("Entries:")));
        layout->addWidget(entries_list, 3);
        layout->addLayout(entry_btns);
        layout->addLayout(entry_points_row);
        layout->setContentsMargins(4, 4, 4, 4);
        auto* tab = new QWidget;
        tab->setLayout(layout);
        tabs->addTab(tab, tr("Courses"));
    }

    auto* main_layout = new QVBoxLayout(this);
    main_layout->addWidget(tabs);
    main_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(main_layout);

    if (overlay)
    {
        connect(overlay, &CourseOverlay::visibleCourseDescriptionScaleChangeRequested,
                this, &CoursePanelWidget::onLegendScaleChangeRequested);
    }

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
    controls_tree->clearSelection();

    if (control_id.isEmpty())
        return;

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

    selected_control_order.clear();
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
    updateSelectionSummary();
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

void CoursePanelWidget::updateSelectionSummary()
{
    const auto ids = selectedControlIds();
    if (ids.empty())
    {
        selection_summary_label->setText(tr("No controls selected"));
        add_to_course_from_controls_btn->setEnabled(false);
        return;
    }

    QStringList labels;
    labels.reserve(int(ids.size()));
    for (const auto& id : ids)
    {
        const auto* ctrl = db.findById(id);
        labels << ((ctrl && !ctrl->description.code.isEmpty()) ? ctrl->description.code : id);
    }
    selection_summary_label->setText(
        tr("Selected (%1): %2").arg(labels.size()).arg(labels.join(QLatin1String(", "))));
    add_to_course_from_controls_btn->setEnabled(course_target_combo->count() > 0);
}

void CoursePanelWidget::onControlSelectionModelChanged(const QItemSelection& selected,
                                                        const QItemSelection& deselected)
{
    for (const auto& index : deselected.indexes())
    {
        if (index.column() != 0) continue;
        auto* item = controls_tree->topLevelItem(index.row());
        if (!item) continue;
        const QString id = item->data(0, Qt::UserRole).toString();
        selected_control_order.erase(
            std::remove(selected_control_order.begin(), selected_control_order.end(), id),
            selected_control_order.end());
    }
    for (const auto& index : selected.indexes())
    {
        if (index.column() != 0) continue;
        auto* item = controls_tree->topLevelItem(index.row());
        if (!item) continue;
        const QString id = item->data(0, Qt::UserRole).toString();
        if (std::find(selected_control_order.begin(), selected_control_order.end(), id)
            == selected_control_order.end())
            selected_control_order.push_back(id);
    }
    updateSelectionSummary();
}

void CoursePanelWidget::onCourseTargetComboChanged(int index)
{
    if (rebuilding) return;
    if (index == courses_list->currentRow()) return;
    courses_list->setCurrentRow(index);
}


// ── Courses tab ───────────────────────────────────────────────────────────────

void CoursePanelWidget::rebuildCoursesList()
{
    if (rebuilding) return;
    rebuilding = true;

    const int prev_row = courses_list->currentRow();
    courses_list->clear();
    course_target_combo->clear();
    for (int i = 0; i < db.numCourses(); ++i)
    {
        courses_list->addItem(db.course(i).name);
        course_target_combo->addItem(db.course(i).name);
    }

    // Restore selection if still valid
    if (prev_row >= 0 && prev_row < courses_list->count())
    {
        courses_list->setCurrentRow(prev_row);
        course_target_combo->setCurrentIndex(prev_row);
    }

    rebuilding = false;

    rebuildEntriesList();
    updateSelectionSummary();

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
        if (course.type == CourseType::Score)
            label += QLatin1String(" - ") + tr("%1 pts").arg(entry.points);
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
    if (course_target_combo->currentIndex() != idx)
        course_target_combo->setCurrentIndex(idx);
    climb_spinbox->setEnabled(idx >= 0);
    climb_label->setEnabled(idx >= 0);
    legend_scale_spinbox->setEnabled(idx >= 0);
    legend_scale_label->setEnabled(idx >= 0);
    course_type_combo->setEnabled(idx >= 0);
    const bool is_score = idx >= 0 && db.course(idx).type == CourseType::Score;
    default_points_spinbox->setEnabled(is_score);
    default_points_label->setEnabled(is_score);
    if (idx >= 0)
    {
        const auto& c = db.course(idx);
        climb_spinbox->setValue(c.climb_m);
        legend_scale_spinbox->setValue(descriptionScaleToPercent(c.description_scale));
        default_points_spinbox->setValue(c.default_points);
        const int type_index = course_type_combo->findData(static_cast<int>(c.type));
        if (type_index >= 0 && course_type_combo->currentIndex() != type_index)
            course_type_combo->setCurrentIndex(type_index);
    }
    else
    {
        legend_scale_spinbox->setValue(100);
    }
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

void CoursePanelWidget::onLegendScaleValueChanged(int value)
{
    const int idx = selectedCourseIndex();
    if (idx < 0 || rebuilding)
        return;

    const double scale = boundedDescriptionScale(value / 100.0);
    auto snapshot = coursesSnapshot();
    auto updated = db.course(idx);
    if (updated.description_scale == scale)
        return;

    updated.description_scale = scale;
    db.updateCourse(idx, std::move(updated));
    map.push(new CoursesChangedUndoStep(&map, std::move(snapshot)));
}

void CoursePanelWidget::onLegendScaleChangeRequested(double scale, bool commit)
{
    const int idx = selectedCourseIndex();
    if (idx < 0)
        return;

    const double bounded_scale = boundedDescriptionScale(scale);
    if (!legend_scale_drag_active)
    {
        legend_scale_drag_before = coursesSnapshot();
        legend_scale_drag_active = true;
    }

    auto updated = db.course(idx);
    const bool changed = (updated.description_scale != bounded_scale);
    rebuilding = true;
    if (changed)
    {
        updated.description_scale = bounded_scale;
        db.updateCourse(idx, std::move(updated));
    }
    legend_scale_spinbox->setValue(descriptionScaleToPercent(bounded_scale));
    rebuilding = false;

    if (commit)
    {
        const auto after = coursesSnapshot();
        if (legend_scale_drag_before != after)
            map.push(new CoursesChangedUndoStep(&map, std::move(legend_scale_drag_before)));
        legend_scale_drag_before.clear();
        legend_scale_drag_active = false;
    }
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

void CoursePanelWidget::onCourseTypeComboChanged(int index)
{
    const int idx = selectedCourseIndex();
    if (idx < 0 || rebuilding) return;

    const auto new_type = static_cast<CourseType>(course_type_combo->itemData(index).toInt());
    auto updated = db.course(idx);
    if (updated.type == new_type) return;

    auto snapshot = coursesSnapshot();
    updated.type = new_type;
    if (new_type == CourseType::Score)
    {
        // Entries that never had points assigned (e.g. added while the course
        // was still Linear) default to the course's default score instead of
        // silently being worth 0.
        for (auto& entry : updated.entries)
            if (entry.points == 0)
                entry.points = updated.default_points;
    }
    db.updateCourse(idx, std::move(updated));
    map.push(new CoursesChangedUndoStep(&map, std::move(snapshot)));
}

void CoursePanelWidget::onDefaultPointsValueChanged(int value)
{
    const int idx = selectedCourseIndex();
    if (idx < 0 || rebuilding) return;

    auto updated = db.course(idx);
    const int old_default = updated.default_points;
    if (old_default == value) return;

    auto snapshot = coursesSnapshot();
    updated.default_points = value;
    // Entries still following the default (i.e. never overridden individually)
    // move to the new default along with it.
    for (auto& entry : updated.entries)
        if (entry.points == old_default)
            entry.points = value;
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
    const int idx = selectedCourseIndex();
    if (idx < 0) return;

    const auto control_ids = selectedControlIds();
    if (control_ids.empty()) return;

    auto snapshot = coursesSnapshot();
    auto updated = db.course(idx);
    for (const auto& control_id : control_ids)
    {
        CourseEntry entry;
        entry.control_id = control_id;
        if (updated.type == CourseType::Score)
            entry.points = updated.default_points;
        updated.entries.push_back(std::move(entry));
    }
    db.updateCourse(idx, std::move(updated));
    map.push(new CoursesChangedUndoStep(&map, std::move(snapshot)));
}

void CoursePanelWidget::onEntriesSelectionChanged()
{
    const int course_idx = selectedCourseIndex();
    const int entry_idx  = entries_list->currentRow();
    const bool show = course_idx >= 0 && entry_idx >= 0
                    && db.course(course_idx).type == CourseType::Score;

    rebuilding = true;
    entry_points_spinbox->setEnabled(show);
    entry_points_label->setEnabled(show);
    if (show)
        entry_points_spinbox->setValue(db.course(course_idx).entries[std::size_t(entry_idx)].points);
    rebuilding = false;
}

void CoursePanelWidget::onEntryPointsValueChanged(int value)
{
    if (rebuilding) return;
    const int course_idx = selectedCourseIndex();
    const int entry_idx  = entries_list->currentRow();
    if (course_idx < 0 || entry_idx < 0) return;

    auto updated = db.course(course_idx);
    if (entry_idx >= static_cast<int>(updated.entries.size())) return;
    if (updated.entries[std::size_t(entry_idx)].points == value) return;

    auto snapshot = coursesSnapshot();
    updated.entries[std::size_t(entry_idx)].points = value;
    db.updateCourse(course_idx, std::move(updated));
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

std::vector<QString> CoursePanelWidget::selectedControlIds() const
{
    return selected_control_order;
}

int CoursePanelWidget::selectedCourseIndex() const
{
    return courses_list->currentRow();
}


}  // namespace OpenOrienteering
