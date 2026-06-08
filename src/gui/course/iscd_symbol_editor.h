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

#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>

class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QToolButton;

namespace OpenOrienteering {

class EditorCanvas;

/**
 * Dialog for manually drawing ISCD symbols.
 *
 * Each symbol is drawn on a unit-space canvas ([-1,1]×[-1,1]) using
 * Line, 3-point Arc, Oval, Circle, Square and Point tools.  The result is stored in a JSON file
 * (iscd_symbols.json) next to the application binary and is loaded by
 * CourseOverlay at runtime to replace the built-in geometry.
 */
class ISCDSymbolEditor : public QDialog
{
    Q_OBJECT
public:
    explicit ISCDSymbolEditor(QWidget* parent = nullptr);

private slots:
    void onSymbolSelected(int row);
    void onUndo();
    void onClear();
    void onSave();
    void onStrokesChanged();

private:
    void populateSymbolList();
    void loadFromDisk();

    QListWidget*    m_list         = nullptr;
    EditorCanvas*   m_canvas       = nullptr;
    QToolButton*    m_btnLine      = nullptr;
    QToolButton*    m_btnArc       = nullptr;
    QToolButton*    m_btnOval      = nullptr;
    QToolButton*    m_btnCircle    = nullptr;
    QToolButton*    m_btnSquare    = nullptr;
    QToolButton*    m_btnPoint     = nullptr;
    QDoubleSpinBox* m_pointDiameterSpin = nullptr;
    QToolButton*    m_btnDelete    = nullptr;
    QLabel*         m_statusLabel  = nullptr;
    QLabel*         m_pathLabel    = nullptr;

    QJsonObject   m_paths;       // key = lowercase English name → QJsonArray of strokes
    QString       m_currentKey;  // key of the symbol being edited
    QString       m_jsonPath;    // file path for save/load
};

}  // namespace OpenOrienteering
