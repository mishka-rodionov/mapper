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

namespace OpenOrienteering {

/**
 * Development helper: displays all ISCD symbols in a scrollable grid so that
 * the rendered symbols can be compared visually against the IOF standard.
 */
class ISCDSymbolBrowser : public QDialog
{
    Q_OBJECT
public:
    explicit ISCDSymbolBrowser(QWidget* parent = nullptr);
};

}  // namespace OpenOrienteering
