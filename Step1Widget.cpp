#include <qpainter.h>
#include <qmessagebox.h>
#include <qstatictext.h>
#include <qpoint.h>
#include <qdialog.h>
#include <qfiledialog.h>
#include <qmath.h>
#include <qevent.h>
#include "Step1Widget.h"
#include "ui_Step1Widget.h"
#include "MLDataManager.h"
#include "MLConfigManager.h"
#include "MLUtil.h"

Step1Widget::Step1Widget(QWidget* parent)
	: QWidget(parent),
	display_frameidx(0)
{
	ui = new Ui::Step1Widget();
	ui->setupUi(this);

	// init the slider
	connect(ui->frameSlider, &QSlider::valueChanged, this, &Step1Widget::updateFrameidx);
	ui->frameSlider->setRange(0, 0);

	// init the button
	connect(ui->buttonLoadVideo, &QPushButton::clicked, this, &Step1Widget::selectVideo);
	connect(ui->buttonLoadProject, &QPushButton::clicked, this, &Step1Widget::selectProject);

	// initialize the drawer
	ui->imageLabel->setStep1Widget(this);
}

Step1Widget::~Step1Widget()
{
	delete ui;
}

void Step1Widget::showEvent(QShowEvent* event)
{
	if (event->spontaneous()) return;
	std::cout << "mmp" << std::endl;
}

void Step1Widget::selectVideo()
{
	auto& global_data = MLDataManager::get();
	global_data.set_dirty(MLDataManager::DataIndex::RAW);

	QString filepath = QFileDialog::getOpenFileName(
		this, tr("Open Video"), "D:/MSI_NB/source/data/",
		tr("Videos (*.mp4 *.avi)")
	);

	if (!global_data.load_raw_video(filepath))
	{
		QMessageBox::warning(this, "Warning", "Failed to load video");
		return;
	}

	global_data.set_clean(MLDataManager::DataIndex::RAW);
	ui->frameSlider->setRange(0, global_data.get_framecount() - 1);
}

void Step1Widget::selectProject()
{
	QString filepath = QFileDialog::getOpenFileName(
		this, tr("Open Project File"), "D:/MSI_NB/source/data/",
		tr("Project File (*.rprj)")
	);
	QMessageBox::warning(this, "warning", "select from project is not yet implemented");
}

void Step1Widget::updateFrameidx(int frameidx)
{
	display_frameidx = frameidx;
	ui->imageLabel->update();
}


void Step1RenderArea::paintEvent(QPaintEvent* event)
{
	if (MLDataManager::get().get_framecount() == 0)
		return;

	QPainter paint(this);
	MLDataManager::get().paintRawFrames(paint, step1widget->display_frameidx);
}

QSize Step1RenderArea::sizeHint()
{
	const auto& global_data = MLDataManager::get();
	if (!global_data.raw_video_cfg.isempty())
		return QSize(global_data.raw_video_cfg.size.width, global_data.raw_video_cfg.size.height);
	else
		return QSize(0, 0);

}

inline bool Step1RenderArea::hasHeightForWidth() { return !MLDataManager::get().raw_video_cfg.isempty(); }

inline int Step1RenderArea::heightForWidth(int w) { return w * MLDataManager::get().raw_video_cfg.size.height / MLDataManager::get().raw_video_cfg.size.width; }
