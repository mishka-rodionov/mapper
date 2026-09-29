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

#ifdef QT_PRINTSUPPORT_LIB

#include "competra_export_dialog.h"

#include <vector>

#include <Qt>
#include <QClipboard>
#include <QColor>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLatin1Char>
#include <QLatin1String>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPalette>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSize>
#include <QSpinBox>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>

#include "core/georeferencing.h"
#include "core/latlon.h"
#include "core/map.h"
#include "core/map_coord.h"
#include "core/map_printer.h"
#include "course/course.h"
#include "course/course_database.h"
#include "course/course_overlay.h"
#include "course/course_text_substitution.h"


namespace OpenOrienteering {

namespace {

const auto settings_resolution = QStringLiteral("CourseExport/competraResolution");
const auto settings_whole_map  = QStringLiteral("CourseExport/competraWholeMap");

/** Competra downscales map images to 2000 px; this keeps some headroom for zooming. */
constexpr int default_resolution = 200;

/** Replaces characters which are not allowed in file names on some systems. */
QString sanitizedFileName(QString name)
{
	name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]")), QStringLiteral("_"));
	return name.trimmed();
}

}  // namespace



CompetraExportDialog::CompetraExportDialog(Map& map, const MapView* view, CourseOverlay& overlay,
                                           const QString& map_path, QWidget* parent)
: QDialog(parent)
, map(map)
, view(view)
, overlay(overlay)
{
	setWindowTitle(tr("Export course maps for Competra"));

	const auto& db = map.courseDatabase();
	file_prefix = sanitizedFileName(QFileInfo(map_path).completeBaseName());
	if (file_prefix.isEmpty())
		file_prefix = sanitizedFileName(db.eventName());

	auto* intro_label = new QLabel(
	    tr("One image is saved per course. All images cover the same map area, "
	       "so a single set of corner coordinates attaches each of them in Competra.\n"
	       "Tip: write %1 in a text on the map (e.g. \"Classes: %1\") to print the name of each course there.")
	    .arg(CourseTextSubstitution::courseNamePlaceholder()));
	intro_label->setWordWrap(true);

	course_list = new QListWidget;
	for (int i = 0; i < db.numCourses(); ++i)
	{
		auto* item = new QListWidgetItem(db.course(i).name, course_list);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(Qt::Checked);
	}

	QSettings settings;

	print_area_button = new QRadioButton(tr("Print area (as set up in File > Print)"));
	whole_map_button = new QRadioButton(tr("Whole map"));
	if (settings.value(settings_whole_map, false).toBool())
		whole_map_button->setChecked(true);
	else
		print_area_button->setChecked(true);
	auto* area_layout = new QVBoxLayout;
	area_layout->addWidget(print_area_button);
	area_layout->addWidget(whole_map_button);

	resolution_edit = new QSpinBox;
	resolution_edit->setRange(50, 1200);
	resolution_edit->setSingleStep(50);
	resolution_edit->setSuffix(QStringLiteral(" dpi"));
	resolution_edit->setValue(settings.value(settings_resolution, default_resolution).toInt());
	image_size_label = new QLabel;
	auto* resolution_layout = new QHBoxLayout;
	resolution_layout->addWidget(resolution_edit);
	resolution_layout->addWidget(image_size_label, 1);

	folder_edit = new QLineEdit;
	folder_edit->setText(QDir::toNativeSeparators(map_path.isEmpty() ? QDir::homePath()
	                                                                 : QFileInfo(map_path).absolutePath()));
	auto* folder_button = new QPushButton(tr("Choose..."));
	auto* folder_layout = new QHBoxLayout;
	folder_layout->addWidget(folder_edit, 1);
	folder_layout->addWidget(folder_button);

	file_name_label = new QLabel;
	file_name_label->setWordWrap(true);

	warning_label = new QLabel;
	warning_label->setWordWrap(true);
	{
		auto palette = warning_label->palette();
		palette.setColor(QPalette::WindowText, QColor(Qt::red));
		warning_label->setPalette(palette);
	}

	auto* form_layout = new QFormLayout;
	form_layout->addRow(tr("Courses:"), course_list);
	form_layout->addRow(tr("Map area:"), area_layout);
	form_layout->addRow(tr("Resolution:"), resolution_layout);
	form_layout->addRow(tr("Folder:"), folder_layout);
	form_layout->addRow({}, file_name_label);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
	export_button = buttons->addButton(tr("Export"), QDialogButtonBox::AcceptRole);
	export_button->setDefault(true);

	auto* layout = new QVBoxLayout(this);
	layout->addWidget(intro_label);
	layout->addLayout(form_layout);
	layout->addWidget(warning_label);
	layout->addWidget(buttons);

	connect(course_list, &QListWidget::itemChanged, this, &CompetraExportDialog::updateWidgets);
	connect(print_area_button, &QRadioButton::toggled, this, &CompetraExportDialog::updateWidgets);
	connect(resolution_edit, QOverload<int>::of(&QSpinBox::valueChanged), this, &CompetraExportDialog::updateWidgets);
	connect(folder_edit, &QLineEdit::textChanged, this, &CompetraExportDialog::updateWidgets);
	connect(folder_button, &QPushButton::clicked, this, &CompetraExportDialog::chooseFolder);
	connect(buttons, &QDialogButtonBox::accepted, this, &CompetraExportDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &CompetraExportDialog::reject);

	updateWidgets();
}


CompetraExportDialog::~CompetraExportDialog() = default;


// static
QString CompetraExportDialog::cornersText(const Georeferencing& georef, const QRectF& area)
{
	// Exact top-left, top-right and bottom-right corners of the exported
	// raster (not their bounding box): a map drawn to magnetic/grid north is
	// rotated relative to true north, and squeezing it into a north-up box
	// would shift it by tens of metres. Three corners define the affine
	// transform the clients use to overlay the raster on OSM; the fourth is
	// implied (bottomLeft = topLeft + bottomRight - topRight).
	const auto top_left = georef.toGeographicCoords(MapCoordF(area.topLeft()));
	const auto top_right = georef.toGeographicCoords(MapCoordF(area.topRight()));
	const auto bottom_right = georef.toGeographicCoords(MapCoordF(area.bottomRight()));

	return QStringLiteral(
	    "mapTopLeftLat=%1\nmapTopLeftLng=%2\n"
	    "mapTopRightLat=%3\nmapTopRightLng=%4\n"
	    "mapBottomRightLat=%5\nmapBottomRightLng=%6"
	).arg(top_left.latitude(), 0, 'f', 7).arg(top_left.longitude(), 0, 'f', 7)
	 .arg(top_right.latitude(), 0, 'f', 7).arg(top_right.longitude(), 0, 'f', 7)
	 .arg(bottom_right.latitude(), 0, 'f', 7).arg(bottom_right.longitude(), 0, 'f', 7);
}


// static
void CompetraExportDialog::showCorners(QWidget* parent, const QString& message,
                                       const QString& corners_text, const QString& folder)
{
	// Copied right away (most users paste immediately), and the dialog keeps a
	// Copy button in case the clipboard was overwritten before pasting.
	QGuiApplication::clipboard()->setText(corners_text);

	QDialog dialog(parent);
	dialog.setWindowTitle(tr("Competra map bounds"));
	auto* layout = new QVBoxLayout(&dialog);

	auto* hint = new QLabel(message);
	hint->setWordWrap(true);
	layout->addWidget(hint);

	auto* text_view = new QPlainTextEdit(corners_text);
	text_view->setReadOnly(true);
	text_view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	text_view->setMinimumSize(text_view->fontMetrics().boundingRect(QStringLiteral("mapBottomRightLng=-000.0000000")).width() + 40,
	                          text_view->fontMetrics().lineSpacing() * 7 + 16);
	layout->addWidget(text_view);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	if (!folder.isEmpty())
	{
		auto* folder_button = buttons->addButton(tr("Open folder"), QDialogButtonBox::ActionRole);
		connect(folder_button, &QPushButton::clicked, &dialog, [folder]() {
			QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
		});
	}
	auto* copy_button = buttons->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
	copy_button->setDefault(true);
	connect(copy_button, &QPushButton::clicked, &dialog, [copy_button, corners_text]() {
		QGuiApplication::clipboard()->setText(corners_text);
		copy_button->setText(tr("Copied"));
	});
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);

	dialog.exec();
}


void CompetraExportDialog::updateWidgets()
{
	const auto size = imageSize();
	image_size_label->setText(tr("Image size: %1 x %2 px").arg(size.width()).arg(size.height()));

	QString example_course;
	int num_checked = 0;
	for (int i = 0; i < course_list->count(); ++i)
	{
		if (course_list->item(i)->checkState() != Qt::Checked)
			continue;
		if (num_checked == 0)
			example_course = course_list->item(i)->text();
		++num_checked;
	}
	file_name_label->setText(num_checked == 0 ? QString()
	                         : tr("File names: %1, ...").arg(QFileInfo(imagePath(example_course)).fileName()));

	QString warning;
	if (map.getGeoreferencing().getState() != Georeferencing::Geospatial)
		warning = tr("The map must be georeferenced (Map > Georeferencing) to attach the images in Competra.");
	else if (course_list->count() == 0)
		warning = tr("There are no courses. Create courses in the Course Planning panel first.");
	else if (num_checked == 0)
		warning = tr("Select at least one course.");
	else if (size.isEmpty())
		warning = tr("The map area to export is empty.");
	else if (!QFileInfo(folder_edit->text()).isDir())
		warning = tr("The folder does not exist.");
	warning_label->setText(warning);
	warning_label->setVisible(!warning.isEmpty());
	export_button->setEnabled(warning.isEmpty());
}


void CompetraExportDialog::chooseFolder()
{
	const auto folder = QFileDialog::getExistingDirectory(this, tr("Choose folder"), folder_edit->text());
	if (!folder.isEmpty())
		folder_edit->setText(QDir::toNativeSeparators(folder));
}


QRectF CompetraExportDialog::exportArea() const
{
	if (whole_map_button->isChecked())
		return map.calculateExtent();
	return map.printerConfig().print_area;
}


QSize CompetraExportDialog::imageSize() const
{
	// Same computation as in MapPrinter::getPrintAreaPaperSize() and the
	// image export in PrintWidget.
	const auto& config = map.printerConfig();
	const auto scale_adjustment = map.getScaleDenominator() / qreal(config.options.scale);
	const auto paper_size = exportArea().size() * scale_adjustment;
	const auto pixel_per_mm = resolution_edit->value() / 25.4;
	return { qRound(paper_size.width() * pixel_per_mm), qRound(paper_size.height() * pixel_per_mm) };
}


QString CompetraExportDialog::imagePath(const QString& course_name) const
{
	auto name = sanitizedFileName(course_name);
	if (name.isEmpty())
		name = tr("course");
	if (!file_prefix.isEmpty())
		name = file_prefix + QLatin1Char('_') + name;
	return QDir(QDir::fromNativeSeparators(folder_edit->text())).absoluteFilePath(name + QLatin1String(".png"));
}


void CompetraExportDialog::accept()
{
	QStringList existing_files;
	for (int i = 0; i < course_list->count(); ++i)
	{
		if (course_list->item(i)->checkState() != Qt::Checked)
			continue;
		const auto path = imagePath(map.courseDatabase().course(i).name);
		if (QFileInfo::exists(path))
			existing_files.append(QFileInfo(path).fileName());
	}

	if (!existing_files.isEmpty()
	    && QMessageBox::question(this, windowTitle(),
	                             tr("These files already exist and will be replaced:\n%1\n\nContinue?")
	                             .arg(existing_files.join(QLatin1Char('\n'))),
	                             QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
	{
		return;
	}

	QSettings settings;
	settings.setValue(settings_resolution, resolution_edit->value());
	settings.setValue(settings_whole_map, whole_map_button->isChecked());

	const auto num_saved = exportImages();
	if (num_saved == 0)
		return;

	QDialog::accept();

	const auto folder = QDir::fromNativeSeparators(folder_edit->text());
	showCorners(parentWidget(),
	            tr("%n image(s) saved to %1.\n"
	               "The WGS84 corner coordinates are the same for all images and have been copied to the clipboard. "
	               "On the Competra website, attach each image to its distance and press \"Paste from Mapper\" "
	               "(or paste the text into the coordinates field).", nullptr, num_saved)
	            .arg(QDir::toNativeSeparators(folder)),
	            cornersText(map.getGeoreferencing(), exportArea()),
	            folder);
}


int CompetraExportDialog::exportImages()
{
	const auto& db = map.courseDatabase();
	std::vector<int> courses;
	for (int i = 0; i < course_list->count(); ++i)
	{
		if (course_list->item(i)->checkState() == Qt::Checked)
			courses.push_back(i);
	}

	MapPrinter printer(map, view);
	printer.setTarget(MapPrinter::imageTarget());
	printer.setPrintArea(exportArea());
	printer.setCustomPageSize(printer.getPrintAreaPaperSize());
	printer.setResolution(resolution_edit->value());
	printer.setMode(MapPrinterOptions::Raster);
	printer.setCourseOverlay(&overlay);

	const auto size = imageSize();
	const auto dots_per_meter = qRound(resolution_edit->value() / 0.0254);

	QProgressDialog progress(tr("Exporting course maps..."), tr("Cancel"), 0, int(courses.size()), this);
	progress.setWindowModality(Qt::WindowModal);
	progress.setMinimumDuration(0);

	// Each course is drawn by making it the visible one, like in the editor.
	const auto* const visible_course = overlay.visibleCourse();
	int num_saved = 0;
	for (auto index : courses)
	{
		progress.setValue(num_saved);
		if (progress.wasCanceled())
			break;

		const auto& course = db.course(index);
		overlay.setVisibleCourse(&course);

		QImage image(size, QImage::Format_ARGB32_Premultiplied);
		if (image.isNull())
		{
			QMessageBox::warning(this, tr("Error"), tr("Failed to prepare the image. Not enough memory."));
			break;
		}
		image.setDotsPerMeterX(dots_per_meter);
		image.setDotsPerMeterY(dots_per_meter);
		image.fill(QColor(Qt::white));

		QPainter painter(&image);
		printer.drawPage(&painter, printer.getPrintArea(), &image);
		painter.end();

		const auto path = imagePath(course.name);
		if (!image.save(path))
		{
			QMessageBox::warning(this, tr("Error"),
			                     tr("Failed to save the image:\n%1\nDoes the path exist? Do you have sufficient rights?")
			                     .arg(QDir::toNativeSeparators(path)));
			break;
		}
		++num_saved;
	}
	overlay.setVisibleCourse(visible_course);
	progress.setValue(int(courses.size()));

	return num_saved;
}


}  // namespace OpenOrienteering

#endif  // QT_PRINTSUPPORT_LIB
