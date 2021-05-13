#include <memory>
#include <qmessagebox.h>
#include <qpainter.h>
#include "Step2Widget.h"
#include "MLStep1Data.h"
#include "MLDataManager.h"
#include "MLUtil.h"
#include "SphericalSfm/SphericalSfm.h"
#include "StitcherCV2.h"
#include "StitcherSsfm.h"
#include "ui_Step2Widget.h"

Step2Widget::Step2Widget(QWidget *parent)
	: QWidget(parent),
	display_frameidx(0),
	display_showbackground(true),
	display_showwarped(true),
	display_showbox(false),
	display_showtrace(false)
{
	ui = new Ui::Step2Widget();
	ui->setupUi(this);
}

Step2Widget::~Step2Widget()
{
	delete ui;
}

void Step2Widget::initState()
{
	const auto& global_data = MLDataManager::get();
	data.tryLoadStitchingFromFiles();
	ui->frameSlider->setRange(0, global_data.get_framecount() - 1);
	connect(ui->frameSlider, &QSlider::valueChanged, this, &Step2Widget::updateFrameidx);

	connect(ui->buttonStitchBg, &QPushButton::clicked, this, &Step2Widget::runStitching);

	ui->imageWidget->setStep2Widget(this);
}

void Step2Widget::updateFrameidx(int frameidx)
{
	display_frameidx = frameidx;
	ui->imageWidget->update();
}

void Step2Widget::runStitching()
{
	const cv::Size& framesz = MLDataManager::get().raw_frame_size;
	const auto rawframes = MLDataManager::get().raw_frames.toStdVector();
	const auto rawmasks = MLDataManager::get().step1datap->masks.toStdVector();

	if (data.isprepared())
	{
		QMessageBox existwarningbox;
		existwarningbox.setText("Stitching result exists.");
		existwarningbox.setInformativeText("Do you want to overwrite existing stitching results?");
		existwarningbox.setStandardButtons(QMessageBox::Ok | QMessageBox::No);
		existwarningbox.setDefaultButton(QMessageBox::Ok);
		int ret = existwarningbox.exec();
		if (ret == QMessageBox::No)
			return;
	}

	std::unique_ptr<StitcherBase> st;
	if (true)
		st = std::make_unique<StitcherSsfm>();
	else
		st = std::make_unique<StitcherCV2>();

	st->stitch(rawframes, rawmasks);
	st->get_warped_frames(data.warped_frames, data.rois);

	data.background = st->get_stitched_image();

	if (!data.saveToFiles())
		qWarning("Step2Widge::error while save stitch to cache");
}

void Step2RenderArea::paintEvent(QPaintEvent* event)
{
	const auto& raw_frames = MLDataManager::get().raw_frames;
	const auto& data = step2widget->data;
	const int frameidx = step2widget->display_frameidx;

	if (!data.isprepared()) return;

	QPainter paint(this);
	auto viewport = paint.viewport();
	
	if (step2widget->display_showbackground)
	{
		paint.drawImage(viewport, MLUtil::mat2qimage(data.background, QImage::Format_ARGB32_Premultiplied));
	}
	if (step2widget->display_showwarped)
	{
		const auto& frame = data.warped_frames[frameidx];
		
		float top_norm = ((float)data.rois[frameidx].y - data.global_roi.y) / data.global_roi.height,
			left_norm = ((float)data.rois[frameidx].x - data.global_roi.x) / data.global_roi.width,
			wid_norm = (float)data.rois[frameidx].width / data.global_roi.width,
			hei_norm = (float)data.rois[frameidx].height / data.global_roi.height;
		// compute equivalent viewport
		QRectF vp_(left_norm * viewport.width(), top_norm * viewport.height(), wid_norm * viewport.width(), hei_norm * viewport.height());
		paint.drawImage(vp_, MLUtil::mat2qimage(frame, QImage::Format_ARGB32_Premultiplied));
	}
}
