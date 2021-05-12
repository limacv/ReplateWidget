#include <memory>
#include "Step2Widget.h"
#include "MLStep1Data.h"
#include "MLDataManager.h"
#include "SphericalSfm/SphericalSfm.h"
#include "StitcherCV2.h"
#include "StitcherSsfm.h"
#include "ui_Step2Widget.h"

Step2Widget::Step2Widget(QWidget *parent)
	: QWidget(parent)
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
	connect(ui->buttonStitchBg, &QPushButton::clicked, this, &Step2Widget::runStitching);
}

void Step2Widget::runStitching()
{
	const cv::Size& framesz = MLDataManager::get().raw_frame_size;
	const auto rawframes = MLDataManager::get().raw_frames.toStdVector();
	const auto rawmasks = MLDataManager::get().step1datap->masks.toStdVector();

	std::unique_ptr<StitcherBase> st;
	if (true)
		st = std::make_unique<StitcherSsfm>();
	else
		st = std::make_unique<StitcherCV2>();

	st->stitch(rawframes, rawmasks);
	st->get_warped_frames(data.warped_frames, data.warped_mask, data.rois);

	data.background = st->get_stitched_image();
}
