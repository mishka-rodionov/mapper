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

#ifndef OPENORIENTEERING_PLACE_CONTROL_TOOL_H
#define OPENORIENTEERING_PLACE_CONTROL_TOOL_H

#include <QString>

#include "core/map_coord.h"
#include "course/course_control.h"
#include "tools/tool_base.h"

class QAction;
class QKeyEvent;
class QPainter;
class QRectF;

namespace OpenOrienteering {

class CourseDatabase;
class MapEditorController;
class MapWidget;


/**
 * Tool for placing and moving course controls on the map.
 *
 * Left-click on empty space  → create a new control and add it to the database
 * Left-click on a control    → select that control (highlight in panel)
 * Drag from a control        → move it; position is committed on dragFinish()
 * Delete key (selected ctrl) → remove the selected control from the database
 * Escape                     → deactivate the tool
 *
 * All mutations push undo steps via map()->push().
 */
class PlaceControlTool : public MapEditorToolBase
{
    Q_OBJECT

public:
    PlaceControlTool(MapEditorController* editor, QAction* tool_action,
                     CourseDatabase& db);
    ~PlaceControlTool() override;

    /** Sets the type assigned to the NEXT newly placed control. */
    void setNextControlType(ControlType type) { next_type = type; }
    ControlType nextControlType() const { return next_type; }

    /** Returns the id of the currently selected control, or empty string. */
    const QString& selectedControlId() const { return selected_id; }

signals:
    /** Emitted when the selected control changes (including deselection). */
    void selectedControlChanged(const QString& control_id);

protected:
    void initImpl() override;

    void mouseMove() override;
    void clickPress() override;
    void clickRelease() override;
    void dragStart() override;
    void dragMove() override;
    void dragFinish() override;
    void dragCanceled() override;

    bool keyPress(QKeyEvent* event) override;

    void drawImpl(QPainter* painter, MapWidget* widget) override;
    int updateDirtyRectImpl(QRectF& rect) override;
    void updateStatusText() override;
    void objectSelectionChangedImpl() override {}

private:
    /** Returns the id of the control under the viewport point, or empty if none. */
    QString controlAtViewportPos(const QPoint& viewport_pos, MapWidget* widget) const;

    /** Finds the index of the control with the given id. Returns -1 if not found. */
    int controlIndex(const QString& id) const;

    void selectControl(const QString& id);
    void deleteSelectedControl();

    // Visual constants
    static constexpr int   hit_radius_px   = 10;   ///< Hit-test radius in pixels
    static constexpr qreal preview_alpha   = 0.6;   ///< Opacity for the drag-preview circle

    CourseDatabase& db;
    ControlType     next_type = ControlType::Regular;

    QString     selected_id;         ///< Currently selected control id
    QString     drag_target_id;      ///< Control being dragged (empty if none)
    MapCoord    drag_old_pos;        ///< Position before drag started
    bool        is_dragging = false; ///< True while a control drag is active
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_PLACE_CONTROL_TOOL_H
