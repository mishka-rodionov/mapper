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

#ifndef OPENORIENTEERING_COURSE_PANEL_H
#define OPENORIENTEERING_COURSE_PANEL_H

#include <vector>

#include <QString>
#include <QWidget>

#include "course/course.h"
#include "course/course_control.h"

class QAction;
class QButtonGroup;
class QComboBox;
class QLabel;
class QListWidget;
class QMenu;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QItemSelection;

namespace OpenOrienteering {

class ControlPropertiesWidget;
class CourseDatabase;
class CourseOverlay;
class Map;
class MainWindow;


/**
 * Dock widget content for the Course Planning panel.
 *
 * Contains two tabs:
 *  - Controls: flat list of all controls in the database
 *  - Courses:  list of courses + entry list for the selected course
 */
class CoursePanelWidget : public QWidget
{
    Q_OBJECT

public:
    CoursePanelWidget(Map& map, CourseDatabase& db, CourseOverlay* overlay,
                      MainWindow* main_window, QWidget* parent = nullptr);
    ~CoursePanelWidget() override = default;

    /** Highlights the given control (e.g. after PlaceControlTool selects it). */
    void selectControl(const QString& control_id);

    /** Reflects the current next-type selection in the type buttons. */
    void setNextControlType(ControlType type);

signals:
    /** Emitted when the user selects a control in the Controls tab. */
    void controlSelected(const QString& control_id);

    /** Emitted when the user clicks a type button; CourseFeature forwards to the tool. */
    void nextControlTypeChangeRequested(ControlType type);

private slots:
    // Database change reactions
    void rebuildControlsTree();
    void rebuildCoursesList();
    void rebuildEntriesList();

    // Panel-wide actions
    void importCoursesFromFile();
    void clearCoursesFromMap();
    void updateActiveFileLabel();
    void showRecentCoursesMenu();
    void activateRecentCourseFile(const QString& relative_name);

    // Controls tab actions
    void onControlItemClicked(QTreeWidgetItem* item, int column);
    void onControlContextMenu(const QPoint& pos);
    void deleteSelectedControls();
    void updateSelectionSummary();
    void onControlSelectionModelChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void onCourseTargetComboChanged(int index);

    // Courses tab actions
    void addCourse();
    void renameCourse();
    void removeCourse();
    void onCourseSelectionChanged();
    void onCourseTypeComboChanged(int index);
    void onDefaultPointsValueChanged(int value);

    // Entries tab actions
    void addSelectedControlToCourse();
    void onClimbValueChanged(int value);
    void onLegendScaleValueChanged(int value);
    void onLegendScaleChangeRequested(double scale, bool commit);
    void removeEntryFromCourse();
    void moveEntryUp();
    void moveEntryDown();
    void onEntriesSelectionChanged();
    void onEntryPointsValueChanged(int value);

private:
    /** Captures a snapshot of all courses for an undo step. */
    std::vector<Course> coursesSnapshot() const;

    /** Returns selected control ids in the order they were selected (click order). */
    std::vector<QString> selectedControlIds() const;

    /**
     * Replaces the database content with what's in the file at
     * absolute_path (confirming first if the map already has content),
     * pushes an undo step, and sets relative_name as the new active file.
     * Shared by "Import courses file…" and reactivating a recent file.
     */
    void loadCourseFileOntoMap(const QString& absolute_path, const QString& relative_name);

    /** Pushes CoursesChangedUndoStep then commits the new courses. */
    void commitCoursesChange(std::vector<Course> before_snapshot,
                             std::vector<Course> new_courses);

    int selectedCourseIndex() const;

    Map&             map;
    CourseDatabase&  db;
    CourseOverlay*   overlay;  // may be nullptr
    MainWindow*      main_window;  // may be nullptr; used to locate the .courses sidecar

    QTabWidget*   tabs            = nullptr;
    QPushButton*  import_courses_btn = nullptr;
    QPushButton*  clear_courses_btn  = nullptr;
    QToolButton*  recent_courses_btn = nullptr;
    QLabel*       active_file_label  = nullptr;

    // Controls tab — type selector
    QButtonGroup* type_button_group = nullptr;
    QToolButton*  type_btn_start    = nullptr;
    QToolButton*  type_btn_regular  = nullptr;
    QToolButton*  type_btn_finish   = nullptr;
    QToolButton*  type_btn_crossing = nullptr;

    QTreeWidget*              controls_tree     = nullptr;
    QLabel*                   selection_summary_label = nullptr;
    QComboBox*                course_target_combo = nullptr;
    QPushButton*              add_to_course_from_controls_btn = nullptr;
    ControlPropertiesWidget*  properties_widget = nullptr;

    // Courses tab
    QListWidget*  courses_list    = nullptr;
    QPushButton*  add_course_btn  = nullptr;
    QPushButton*  rename_btn      = nullptr;
    QPushButton*  remove_course_btn = nullptr;

    QListWidget*  entries_list    = nullptr;
    QPushButton*  add_entry_btn   = nullptr;
    QPushButton*  remove_entry_btn = nullptr;
    QPushButton*  entry_up_btn    = nullptr;
    QPushButton*  entry_down_btn  = nullptr;
    QLabel*       entry_points_label   = nullptr;
    QSpinBox*     entry_points_spinbox = nullptr;

    QComboBox*    course_type_combo    = nullptr;
    QLabel*       climb_label     = nullptr;
    QSpinBox*     climb_spinbox   = nullptr;
    QLabel*       legend_scale_label   = nullptr;
    QSpinBox*     legend_scale_spinbox = nullptr;
    QLabel*       default_points_label   = nullptr;
    QSpinBox*     default_points_spinbox = nullptr;

    std::vector<Course> legend_scale_drag_before;
    bool legend_scale_drag_active = false;

    QString selected_control_id;  ///< Currently highlighted control
    std::vector<QString> selected_control_order;  ///< Multi-selection, in click order
    bool rebuilding = false;       ///< Guard against recursive rebuild
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_PANEL_H
