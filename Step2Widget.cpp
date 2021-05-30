#include <memory>
#include <qmessagebox.h>
#include <qpainter.h>
#include <opencv2/photo.hpp>
#include <opencv2/xphoto/inpainting.hpp>
#include "ui_Step2Widget.h"
#include "Step2Widget.h"
#include "MLDataManager.h"
#include "MLCacheTrajectories.h"
#include "MLCacheStitching.h"
#include "MLUtil.h"
#include "SphericalSfm/SphericalSfm.h"
#include "StitcherCV2.h"
#include "StitcherSsfm.h"

Step2Widget::Step2Widget(QWidget *parent)
	: QWidget(parent),
	display_frameidx(0),
	display_showbackground(false),
	display_showwarped(true),
	display_showbox(false)
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
	auto& global_data = MLDataManager::get();
	stitchdatap = &global_data.stitch_cache;
	flowdatap = &global_data.flow_cache;

	stitchdatap->tryLoadAllFromFiles();
	global_data.trajectories.tryLoadGlobalBoxes();
	global_data.flow_cache.tryLoadFlows();

	ui->frameSlider->setRange(0, global_data.get_framecount() - 1);
	connect(ui->frameSlider, &QSlider::valueChanged, this, &Step2Widget::updateFrameidx);

	connect(ui->buttonStitchBg, &QPushButton::clicked, this, &Step2Widget::runStitching);
	connect(ui->buttonInpaintBg, &QPushButton::clicked, this, &Step2Widget::runInpainting);

	connect(ui->checkShowBg, &QCheckBox::stateChanged, this, [this](int state) {
		this->display_showbackground = (state == Qt::Checked);
		this->ui->imageWidget->update();
		});
	connect(ui->checkShowFrames, &QCheckBox::stateChanged, this, [this](int state) {
		this->display_showwarped = (state == Qt::Checked);
		this->ui->imageWidget->update();
		});
	connect(ui->checkShowBox, &QCheckBox::stateChanged, this, [this](int state) {
		this->display_showbox = (state == Qt::Checked);
		this->ui->imageWidget->update();
		});

	ui->imageWidget->setStep2Widget(this);
}

void Step2Widget::onWidgetShowup()
{
	if (!flowdatap->isprepared())
		runOptflow();
}


void Step2Widget::updateFrameidx(int frameidx)
{
	display_frameidx = frameidx;
	ui->imageWidget->update();
}

void Step2Widget::runStitching()
{
	const cv::Size& framesz = MLDataManager::get().raw_video_cfg.size;
	const auto rawframes = MLDataManager::get().raw_frames.toStdVector();
	const auto rawmasks = MLDataManager::get().masks.toStdVector();

	if (stitchdatap->isprepared())
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
	st->get_warped_frames(stitchdatap->warped_frames, stitchdatap->rois);
	stitchdatap->background = st->get_stitched_image();
	// warp rectangles
	auto& trajectories = MLDataManager::get().trajectories;
	for (int frameidx = 0; frameidx < rawframes.size(); ++frameidx)
	{
		auto& boxes = trajectories.frameidx2boxes[frameidx];
		std::vector<cv::Rect> rects; rects.reserve(boxes.size());
		for (auto& box : boxes)
			rects.emplace_back(box->rect);
		st->get_warped_rects(frameidx, rects);

		int i = 0;
		for (auto& it = boxes.begin(); it != boxes.end(); ++it, ++i)
			it.value()->rect_global = rects[i];
	}

	if (!stitchdatap->saveAllToFiles())
		qWarning("Step2Widge::error while save stitch to cache");
	
	stitchdatap->update_global_roi();
}

void Step2Widget::runOptflow()
{
	if (!stitchdatap->isprepared())
		return;
	const auto& global_data = MLDataManager::get();
	const int framecount = global_data.get_framecount();
	const auto& rois = stitchdatap->rois;
	flowdatap->flows.resize(framecount);

	for (int i = 0; i < framecount; ++i)
	{
		cv::Mat flowb, flowf;
		cv::Mat framecur, maskcur;
		MLUtil::cvtrgba2gray_a(stitchdatap->warped_frames[i], framecur, maskcur);
		if (i == 0)
		{
			flowb = cv::Mat(framecur.size(), CV_8UC2, cv::Scalar(128, 128));
		}
		else
		{
			cv::Rect roi_w = (rois[i] | rois[i - 1]) - stitchdatap->global_roi.tl(),
				roi_1 = rois[i] - stitchdatap->global_roi.tl() - roi_w.tl(),
				roi_2 = rois[i - 1] - stitchdatap->global_roi.tl() - roi_w.tl();
			cv::Mat im1, im2;
			cv::cvtColor(stitchdatap->background(roi_w), im1, cv::COLOR_BGRA2GRAY);
			im2 = im1.clone();
			
			framecur.copyTo(im1(roi_1), maskcur);
			cv::Mat frameprev, maskprev;
			MLUtil::cvtrgba2gray_a(stitchdatap->warped_frames[i - 1], frameprev, maskprev);
			frameprev.copyTo(im2(roi_2), maskprev);

			cv::calcOpticalFlowFarneback(im1, im2, flowb, 0.5, 3, 15, 3, 5, 1.2, 0);
			flowb.convertTo(flowb, CV_8UC2, 1, 128);  // 0.5 to round
			flowb = flowb(roi_1);
		}

		if (i == framecount - 1)
		{
			flowf = cv::Mat(framecur.size(), CV_8UC2, cv::Scalar(128, 128));
		}
		else
		{
			cv::Rect roi_w = (rois[i] | rois[i + 1]) - stitchdatap->global_roi.tl(),
				roi_1 = rois[i] - stitchdatap->global_roi.tl() - roi_w.tl(),
				roi_2 = rois[i + 1] - stitchdatap->global_roi.tl() - roi_w.tl();
			cv::Mat im1, im2;
			cv::cvtColor(stitchdatap->background(roi_w), im1, cv::COLOR_BGRA2GRAY);
			im2 = im1.clone();

			framecur.copyTo(im1(roi_1), maskcur);
			cv::Mat framenext, masknext;
			MLUtil::cvtrgba2gray_a(stitchdatap->warped_frames[i + 1], framenext, masknext);
			framenext.copyTo(im2(roi_2), masknext);

			cv::calcOpticalFlowFarneback(im1, im2, flowf, 0.5, 3, 15, 3, 5, 1.2, 0);
			flowf.convertTo(flowf, CV_8UC2, 1, 128);  // 0.5 to round
			flowf = flowf(roi_1);
		}
		cv::merge(std::vector<cv::Mat>{flowf, flowb}, flowdatap->flows[i]);
		flowf.release(); flowb.release();
	}
	flowdatap->saveFlows();
}

void Step2Widget::runInpainting()
{
	//cv::Mat inpainted, corrs, distances;
	//Inpaint::patchMatch(stitchdatap->background, inpainted, cv::noArray(), corrs, distances, 5)
	if (stitchdatap->background.empty())
		return;

	cv::Mat inpainted;
	cv::Mat rgb, mask;
	cv::cvtColor(stitchdatap->background, rgb, cv::COLOR_RGBA2RGB);
	cv::extractChannel(stitchdatap->background, mask, 3);
	cv::bitwise_not(mask, mask);
	cv::inpaint(rgb, mask, inpainted, 10, cv::INPAINT_TELEA);
	//cv::xphoto::inpaint(rgb, mask, inpainted, cv::xphoto::INPAINT_FSR_FAST);
	
	cv::merge(std::vector<cv::Mat>({ inpainted, cv::Mat(inpainted.size(), CV_8UC1, cv::Scalar(255)) }), stitchdatap->background);

	stitchdatap->saveBackground();
}

void Step2RenderArea::paintEvent(QPaintEvent* event)
{
	const auto& raw_frames = MLDataManager::get().raw_frames;
	const auto& stitch_cache = MLDataManager::get().stitch_cache;
	auto& trajectories = MLDataManager::get().trajectories;
	const int frameidx = step2widget->display_frameidx;

	if (!stitch_cache.isprepared()) return;
	
	QPainter paint(this);
	auto viewport = paint.viewport();
	
	auto topaintroi = [&](const cv::Rect& rectin) -> QRectF {
		float top_norm = ((float)rectin.y - stitch_cache.global_roi.y) / stitch_cache.global_roi.height,
			left_norm = ((float)rectin.x - stitch_cache.global_roi.x) / stitch_cache.global_roi.width,
			wid_norm = (float)rectin.width / stitch_cache.global_roi.width,
			hei_norm = (float)rectin.height / stitch_cache.global_roi.height;
		return QRectF(left_norm * viewport.width(), top_norm * viewport.height(), wid_norm * viewport.width(), hei_norm * viewport.height());
	};

	/******************
	* paint background
	******************/
	if (step2widget->display_showbackground)
	{
		paint.drawImage(viewport, MLUtil::mat2qimage(stitch_cache.background, QImage::Format_ARGB32_Premultiplied));
	}

	/******************
	* paint each frame
	******************/
	if (step2widget->display_showwarped)
	{
		const auto& frame = stitch_cache.warped_frames[frameidx];
		paint.drawImage(topaintroi(stitch_cache.rois[frameidx]), MLUtil::mat2qimage(frame, QImage::Format_ARGB32_Premultiplied));
	}

	/******************
	* paint boxes
	******************/
	if (step2widget->display_showbox)
	{
		const auto& boxes = trajectories.frameidx2boxes[frameidx];
		for (auto it = boxes.constKeyValueBegin(); it != boxes.constKeyValueEnd(); ++it)
		{
			const auto& color = trajectories.getColor(it->first);
			const auto& box = it->second;

			paint.setPen(QPen(color, 2));
			paint.drawRect(topaintroi(box->rect_global));
		}
	}
}
