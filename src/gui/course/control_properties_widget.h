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

#ifndef OPENORIENTEERING_CONTROL_PROPERTIES_WIDGET_H
#define OPENORIENTEERING_CONTROL_PROPERTIES_WIDGET_H

#include <QString>
#include <QWidget>

class QComboBox;
class QLineEdit;

namespace OpenOrienteering {

class CourseDatabase;
class Map;


/**
 * Form widget for editing the IOF control description (columns B–H)
 * of a single selected CourseControl.
 *
 * Changes are committed immediately on focus-out / combo selection and
 * wrapped in ModifyControlDescriptionUndoStep.
 */
class ControlPropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    ControlPropertiesWidget(Map& map, CourseDatabase& db, QWidget* parent = nullptr);
    ~ControlPropertiesWidget() override = default;

    /** Load fields from the control with the given id. Pass empty string to clear. */
    void setControl(const QString& control_id);

    /** Returns the id of the control currently being edited. */
    const QString& controlId() const { return control_id; }

private slots:
    void onCodeEdited();
    void onFeaturePartChanged();
    void onFeatureChanged();
    void onApproachChanged();
    void onDimensionsEdited();
    void onLocationDetailChanged();
    void onOtherInfoEdited();

    void onDatabaseChanged(int index);

private:
    void commit();

    bool loading = false;   ///< Guard to suppress change signals while loading

    Map&            map;
    CourseDatabase& db;
    QString         control_id;

    QLineEdit*  code_edit          = nullptr;  ///< Column B
    QComboBox*  feature_part_combo = nullptr;  ///< Column C
    QComboBox*  feature_combo      = nullptr;  ///< Column D
    QComboBox*  approach_combo     = nullptr;  ///< Column E
    QLineEdit*  dimensions_edit    = nullptr;  ///< Column F
    QComboBox*  location_combo     = nullptr;  ///< Column G
    QLineEdit*  other_edit         = nullptr;  ///< Column H
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_CONTROL_PROPERTIES_WIDGET_H
