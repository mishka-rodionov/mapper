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

#ifndef OPENORIENTEERING_COURSE_FEATURE_H
#define OPENORIENTEERING_COURSE_FEATURE_H

#include <memory>

#include <QObject>
#include <QPointer>

#include "course/course_control.h"

class QAction;
class QWidget;

namespace OpenOrienteering {

class CourseOverlay;
class CoursePanelWidget;
class EditorDockWidget;
class MapEditorController;
class PlaceControlTool;


/**
 * Pluggable feature that adds Course Planning to the MapEditorController.
 *
 * Follows the exact same pattern as MapFindFeature:
 *  - Constructor takes MapEditorController& and wires itself up
 *  - Exposes the panel toggle action for menu integration
 *  - setEnabled() disables all actions during active editing operations
 *
 * Lifecycle (mirrors GPSDisplay / other sensors):
 *  - The CourseOverlay is created in the constructor (MapWidget already exists).
 *  - The CoursePanelWidget dock is created lazily when first shown.
 */
class CourseFeature : public QObject
{
    Q_OBJECT

public:
    explicit CourseFeature(MapEditorController& controller);
    ~CourseFeature() override;

    CourseFeature(const CourseFeature&) = delete;
    CourseFeature& operator=(const CourseFeature&) = delete;

    /** Enables or disables all course actions (called from setEditingInProgress). */
    void setEnabled(bool enabled);

    /**
     * Checkable action to show/hide the Course Planning dock.
     *
     * This is the only action offered in the menu. Placing controls, export
     * and the ISCD symbol reference are buttons in the panel itself.
     */
    QAction* showPanelAction() { return show_panel_act; }

    /** Returns the CourseOverlay owned by this feature. */
    CourseOverlay* overlay() const { return course_overlay.get(); }

private slots:
    void showPanelToggled(bool show);
    void placeControlToggled(bool checked);
    void exportIofFull();
    void exportCompetraMaps();
    void openSymbolBrowser();
    void onControlSelectedInTool(const QString& control_id);
    void onNextControlTypeChangeRequested(ControlType type);

    /**
     * Assigns a course file name to the database as soon as it gains
     * content (a control, a course, or an event name) while it doesn't
     * have one yet, so a freshly-drawn course persists automatically on
     * save without requiring an explicit "Import"/"Save as" step.
     */
    void ensureActiveCourseFileName();

private:
    void createDockWidget();

    MapEditorController& controller;

    std::unique_ptr<CourseOverlay>  course_overlay;
    QPointer<EditorDockWidget>      dock_widget;
    CoursePanelWidget*              panel = nullptr;

    QPointer<PlaceControlTool>      current_tool;
    ControlType                     next_control_type = ControlType::Regular;

    QAction* show_panel_act      = nullptr;
    QAction* place_control_act   = nullptr;
    QAction* export_iof_act      = nullptr;
    QAction* export_competra_maps_act = nullptr;
    QAction* symbol_browser_act  = nullptr;
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_FEATURE_H
