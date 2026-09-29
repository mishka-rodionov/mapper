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

#ifndef OPENORIENTEERING_COMPETRA_EXPORT_DIALOG_H
#define OPENORIENTEERING_COMPETRA_EXPORT_DIALOG_H

#ifdef QT_PRINTSUPPORT_LIB

#include <QDialog>
#include <QObject>
#include <QRectF>
#include <QSizeF>
#include <QString>

class QImage;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QWidget;

namespace OpenOrienteering {

class CourseOverlay;
class Georeferencing;
class Map;
class MapView;


/**
 * Exports one raster image per course for the Competra app.
 *
 * All images share the same map area, so a single set of WGS84 corner
 * coordinates (see cornersText()) georeferences every one of them. The
 * corners are also embedded in each image (see embedCorners()), so Competra
 * can fill them in from the file alone. Texts on the map may use the
 * "{course}" placeholder to show each course's name.
 */
class CompetraExportDialog : public QDialog
{
	Q_OBJECT

public:
	CompetraExportDialog(Map& map, const MapView* view, CourseOverlay& overlay,
	                     const QString& map_path, QWidget* parent = nullptr);
	~CompetraExportDialog() override;

	/**
	 * Returns the text which Competra's "Paste from Mapper" understands:
	 * the exact top-left, top-right and bottom-right WGS84 corners of a
	 * raster covering the given map area.
	 */
	static QString cornersText(const Georeferencing& georef, const QRectF& area);

	/**
	 * Embeds the corners text in the image's text metadata, one entry per
	 * "key=value" line (e.g. "mapTopLeftLat" -> "55.1234567").
	 *
	 * The values are short enough for Qt to write them as uncompressed PNG
	 * tEXt chunks, which Competra reads when an image is chosen for upload.
	 */
	static void embedCorners(QImage& image, const QString& corners_text);
	
	/**
	 * Copies the corners text to the clipboard and shows it with a Copy button.
	 * If folder is not empty, the dialog also offers to open it.
	 */
	static void showCorners(QWidget* parent, const QString& message,
	                        const QString& corners_text, const QString& folder = {});

	void accept() override;

private:
	void updateWidgets();
	void chooseFolder();

	/** Returns the map area to export, according to the area selection. */
	QRectF exportArea() const;

	/** Returns the image size in pixels for the export area and resolution. */
	QSize imageSize() const;

	/** Returns the absolute image file path for the given course name. */
	QString imagePath(const QString& course_name) const;

	/** Renders and saves the images. Returns the number of saved images. */
	int exportImages();

	Map& map;
	const MapView* view;
	CourseOverlay& overlay;
	QString file_prefix;

	QListWidget*  course_list;
	QRadioButton* print_area_button;
	QRadioButton* whole_map_button;
	QSpinBox*     resolution_edit;
	QLabel*       image_size_label;
	QLineEdit*    folder_edit;
	QLabel*       file_name_label;
	QLabel*       warning_label;
	QPushButton*  export_button;
};


}  // namespace OpenOrienteering

#endif  // QT_PRINTSUPPORT_LIB

#endif  // OPENORIENTEERING_COMPETRA_EXPORT_DIALOG_H
