#include <memory>
#include <qmessagebox.h>
#include <qpainter.h>
#include <qprogressbar.h>
#include <QtConcurrent/qtconcurrentrun.h>
#include <opencv2/photo.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/tracking.hpp>
#include <qevent.h>
#include <qfuture.h>
#include <qfuturewatcher.h>
#include "MLProgressDialog.hpp"
#include <opencv2/xphoto/inpainting.hpp>
#include "ui_Step2Widget.h"
#include "Step2Widget.h"
#include "MLDataManager.h"
#include "MLConfigManager.h"
#include "MLCacheTrajectories.h"
#include "MLCacheStitching.h"
#include "MLUtil.h"
#include "SphericalSfm/SphericalSfm.h"
#include "StitcherSsfm.h"
#include "StitcherGe.h"
#include "StitcherMl.h"
#include "StitcherNone.h"
#include "MLPythonWarpper.h"

Step2Widget::Step2Widget(QWidget* parent)
	: QWidget(parent),
	stitchdatap(&MLDataManager::get().stitch_cache),
	flowdatap(nullptr /*&MLDataManager::get().flow_cache*/),
	trajp(&MLDataManager::get().trajectories)
{
	ui = new Ui::Step2Widget();
	ui->setupUi(this);
	player_manager = new MLPlayerHandler(
		[]() ->int {return MLDataManager::get().get_framecount(); },
		ui->frameSlider,
		ui->imageWidget,
		&display_timer,
		ui->frameButton
	);

	connect(ui->buttonStitchBg, &QPushButton::clicked, this, &Step2Widget::runStitching);
	connect(ui->buttonInpaintBg, &QPushButton::clicked, this, &Step2Widget::runInpainting);
	connect(ui->buttonDetection, &QPushButton::clicked, this, &Step2Widget::runDetect);
	connect(ui->buttonTracking, &QPushButton::clicked, this, &Step2Widget::runTrack);
	connect(ui->buttonRunall, &QPushButton::clicked, this, &Step2Widget::forceRunAll);
	
	connect(ui->checkShowBg, SIGNAL(stateChanged(int)), this->ui->imageWidget, SLOT(update()));
	connect(ui->checkShowFrames, SIGNAL(stateChanged(int)), this->ui->imageWidget, SLOT(update()));
	connect(ui->checkShowBox, SIGNAL(stateChanged(int)), this->ui->imageWidget, SLOT(update()));
	connect(ui->checkShowTraj, SIGNAL(stateChanged(int)), this->ui->imageWidget, SLOT(update()));
	connect(ui->checkShowTrack, &QCheckBox::stateChanged, this, [this](int state) {
		this->ui->checkShowTraj->setEnabled(this->display_showtrack());
		this->ui->imageWidget->update();
		});

	ui->imageWidget->setStep2Widget(this);
}

Step2Widget::~Step2Widget()
{
	delete ui;
	delete player_manager;
}

void Step2Widget::showEvent(QShowEvent* event)
{
	if (event->spontaneous()) return;
	QWidget::show();
	auto& global_data = MLDataManager::get();

	if (!trajp->isDetectOk())
		tryRunDetect();
	if (!trajp->isTrackOk())
		tryRunTrack();

	if (!trajp->isDetectGlobalBoxOk()
		|| !trajp->isTrackGlobalBoxOk()
		|| !stitchdatap->isPrepared())
	{
		tryRunStitching();
	}
	//if (!flowdatap->tryLoadFlows() || !flowdatap->isprepared())
		//runOptflow();
}

void Step2Widget::forceRunAll()
{
	runDetect();
	runTrack();
	runStitching();
}

inline bool Step2Widget::display_showbackground() { return ui->checkShowBg->isChecked(); }

inline bool Step2Widget::display_showwarped() { return ui->checkShowFrames->isChecked(); }

inline bool Step2Widget::display_showbox() { return ui->checkShowBox->isChecked(); }

inline bool Step2Widget::display_showtraj() { return ui->checkShowTraj->isChecked(); }

inline bool Step2Widget::display_showtrack() { return ui->checkShowTrack->isChecked(); }

// if cache exist, load cache, else run detection
void Step2Widget::tryRunDetect()
{
	if (!trajp->tryLoadDetectionFromFile())
		runDetect();
}

// force to run detection
void Step2Widget::runDetect()
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();
	// check if detect file exist
	bool result_exist = true;
	for (int i = 0; i < globaldata.get_framecount(); ++i)
	{
		auto filepath = pathcfg.get_detect_result_cache(i);
		if (!QFile::exists(filepath))
			result_exist = false;
	}
	// if exist, promote dialog
	if (result_exist)
	{
		QMessageBox existwarningbox;
		existwarningbox.setText("Detection result exists.");
		existwarningbox.setInformativeText("Do you want to overwrite existing detection results?");
		existwarningbox.setStandardButtons(QMessageBox::Ok | QMessageBox::No);
		existwarningbox.setDefaultButton(QMessageBox::Ok);
		int ret = existwarningbox.exec();
		if (ret == QMessageBox::No)
			return;
	}

	QProgressDialog bar("Run Detection", "Cancel", 0, 1000, this);
	QFuture<int> future_ret = QtConcurrent::run(callDetectPy);
	while (!future_ret.isFinished())
	{
		QCoreApplication::processEvents();
		if (bar.wasCanceled())
			return;

		bar.setValue(bar.value() >= 999 ? bar.value() : bar.value() + 1);
		QThread::msleep(10);
	}
	int ret = future_ret.result();
	if (ret != 0)
	{
		QMessageBox errorBox;
		errorBox.setText(QString("detection failed with error code %1").arg(QString::number(ret)));
		errorBox.exec();
	}
	if (!trajp->tryLoadDetectionFromFile())
		qWarning("Step1Widget::Failed to load detection file");
	ui->imageWidget->update();
}

void Step2Widget::tryRunTrack()
{
	if (!trajp->tryLoadTrackFromFile())
		runTrack();
}

void Step2Widget::runTrack()
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();
	// check if detect file exist, if exist, promote dialog
	if (QFile::exists(pathcfg.get_track_result_cache()))
	{
		QMessageBox existwarningbox;
		existwarningbox.setText("Tracking result exists.");
		existwarningbox.setInformativeText("Do you want to overwrite existing tracking results?");
		existwarningbox.setStandardButtons(QMessageBox::Ok | QMessageBox::No);
		existwarningbox.setDefaultButton(QMessageBox::Ok);
		int ret = existwarningbox.exec();
		if (ret == QMessageBox::No)
			return;
	}

	QProgressDialog bar("Run Tracking", "Cancel", 0, 1000, this);
	QFuture<int> future_ret = QtConcurrent::run(callTrackPy);
	while (!future_ret.isFinished())
	{
		QCoreApplication::processEvents();
		if (bar.wasCanceled())
			return;

		bar.setValue(bar.value() >= 999 ? bar.value() : bar.value() + 1);
		QThread::msleep(5);
	}
	int ret = future_ret.result();
	if (ret != 0)
	{
		QMessageBox errorBox;
		errorBox.setText(QString("tracking failed with error code %1").arg(QString::number(ret)));
		errorBox.exec();
	}
	if (!trajp->tryLoadTrackFromFile())
		qWarning("Step1Widget::Failed to load track file");
	ui->imageWidget->update();
}


void Step2Widget::generateMask(std::vector<cv::Mat>& masks)
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();
	const int framecount = globaldata.get_framecount();
	masks.resize(framecount);
	for (int i = 0; i < framecount; ++i)
	{
		masks[i].create(globaldata.raw_video_cfg.size, CV_8UC1);
		masks[i].setTo(255);
	}

	if (!trajp->isDetectOk() && !trajp->isTrackOk())
	{
		QMessageBox notpreparedbox;
		notpreparedbox.setText("The track tracjectories is empty");
		notpreparedbox.setInformativeText("The generated mask will be empty.");
		notpreparedbox.setStandardButtons(QMessageBox::Ok);
		notpreparedbox.setDefaultButton(QMessageBox::Ok);
		int ret = notpreparedbox.exec();
	}

	for (int i = 0; i < framecount; ++i)
	{
		auto& mask = masks[i];
		for (auto pbox : trajp->frameidx2detectboxes[i])
			mask(pbox->rect & cv::Rect(cv::Point(0, 0), mask.size())).setTo(0);
		//for (auto pbox : trajp->frameidx2trackboxes[i])
			//mask(pbox->rect).setTo(0);

		for (const QRectF& rect : globaldata.manual_masks)
		{
			int hei = globaldata.raw_video_cfg.size.height,
				wid = globaldata.raw_video_cfg.size.width;
			cv::Rect rectcv(rect.x() * wid, rect.y() * hei, rect.width() * wid, rect.height() * hei);
			mask(rectcv).setTo(0);
		}
	}
	ui->imageWidget->update();
}

void Step2Widget::tryRunStitching()
{
	MLProgressDialog bar(this);
	if (!trajp->tryLoadGlobalTrackBoxes()
		|| !trajp->tryLoadGlobalDetectBoxes()
		|| !stitchdatap->tryLoadAllFromFiles(&bar))
		runStitching();
}

void Step2Widget::runStitching()
{
	const cv::Size& framesz = MLDataManager::get().raw_video_cfg.size;
	const auto rawframes = MLDataManager::get().raw_frames.toStdVector();
	const auto& global_cfg = MLConfigManager::get();

	if (stitchdatap->isPrepared())
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
	auto st_cfg = &global_cfg.stitcher_cfg;
	if (st_cfg->stitcher_type_ == "Ml")
		st = std::make_unique<StitcherMl>(st_cfg);
	else if (st_cfg->stitcher_type_ == "Ge")
		st = std::make_unique<StitcherGe>(st_cfg);
	else if (st_cfg->stitcher_type_ == "Ssfm")
		st = std::make_unique<StitcherSsfm>(st_cfg);
	else if (st_cfg->stitcher_type_ == "No")
		st = std::make_unique<StitcherNone>(st_cfg);
	else
	{
		qCritical("Stitcher type not recognized");
		return;
	}

	MLProgressDialog bar(this);
	st->set_progress_observer(&bar);

	bool loaded = st->loadCameraParams(global_cfg.get_stitch_cameraparams_path().toStdString());
	std::vector<cv::Mat> rawmasks;
	generateMask(rawmasks);
	if (ui->checkReCameraParameter->isChecked() || !loaded)
	{
		st->stitch(rawframes, rawmasks);
		st->saveCameraParams(global_cfg.get_stitch_cameraparams_path().toStdString());
	}

	st->warp_and_composite(rawframes, rawmasks,
		stitchdatap->warped_frames, stitchdatap->rois, stitchdatap->background);

	// warp rectangles
	for (int frameidx = 0; frameidx < rawframes.size(); ++frameidx)
	{
		{ // warp tracking boxes
			auto& boxes = trajp->frameidx2trackboxes[frameidx];
			std::vector<cv::Point> points; points.reserve(boxes.size() * 2);
			for (auto& box : boxes)
			{
				points.emplace_back(box->rect.tl());
				points.emplace_back(box->rect.br());
			}
			st->warp_points(frameidx, points);

			int i = 0;
			for (auto& it = boxes.begin(); it != boxes.end(); ++it, i+=2)
				it.value()->rect_global = cv::Rect(points[i], points[i + 1]);
		}
		{ // warp detection boxes
			auto& boxes = trajp->frameidx2detectboxes[frameidx];
			std::vector<cv::Point> points; points.reserve(boxes.size() * 2);
			for (auto& box : boxes)
			{
				points.emplace_back(box->rect.tl());
				points.emplace_back(box->rect.br());
			}
			st->warp_points(frameidx, points);

			int i = 0;
			for (auto it = boxes.begin(); it != boxes.end(); ++it, i+=2)
				(*it)->rect_global = cv::Rect(points[i], points[i + 1]);
		}
	}

	if (!stitchdatap->saveAllToFiles(&bar))
		qWarning("Step2Widge::error while save stitch to cache");
	if (!trajp->saveGlobalDetectBoxes())
		qWarning("Step2Widge::error while save global detect boxes");
	if (!trajp->saveGlobalTrackBoxes())
		qWarning("Step2Widge::error while save global track boxes");

	stitchdatap->update_global_roi();
}

void Step2Widget::tryRunOpticalFlow()
{

}

void Step2Widget::runOptflow()
{
	throw std::runtime_error("should not enter here");
	if (!stitchdatap->isPrepared())
		return;
	const auto& global_data = MLDataManager::get();
	const int framecount = global_data.get_framecount();
	const auto& rois = stitchdatap->rois;
	flowdatap->flows.resize(framecount);
	
	QProgressDialog bar("Run Optical Flow", "Cancel", 0, framecount + 1, this);
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
		bar.setValue(i);
	}
	bar.setLabelText("saving...");
	flowdatap->saveFlows();
	bar.setValue(framecount + 1);
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
	const auto& global_data = MLDataManager::get();
	const auto& raw_frames = MLDataManager::get().raw_frames;
	const auto& stitch_cache = MLDataManager::get().stitch_cache;
	auto& trajectories = MLDataManager::get().trajectories;
	const int frameidx = step2widget->player_manager->display_frameidx();

	if (!stitch_cache.isPrepared()) return;
	
	QPainter paint(this);
	auto viewport = paint.viewport();
	//auto topaintroi = [&](const cv::Rect& rectin) -> QRectF {
	//	float top_norm = ((float)rectin.y - stitch_cache.global_roi.y) / stitch_cache.global_roi.height,
	//		left_norm = ((float)rectin.x - stitch_cache.global_roi.x) / stitch_cache.global_roi.width,
	//		wid_norm = (float)rectin.width / stitch_cache.global_roi.width,
	//		hei_norm = (float)rectin.height / stitch_cache.global_roi.height;
	//	return QRectF(left_norm * viewport.width(), top_norm * viewport.height(), wid_norm * viewport.width(), hei_norm * viewport.height());
	//};
	const float scalex = (float)viewport.width() / raw_frames[0].cols;
	const float scaley = (float)viewport.height() / raw_frames[0].rows;
	/******************
	* paint background and foreground
	******************/
	global_data.paintWarpedFrames(paint, frameidx, step2widget->display_showbackground(), step2widget->display_showwarped());

	/******************
	* paint boxes and names
	******************/
	if (step2widget->display_showbox() && step2widget->display_showtrack())
		global_data.paintWorldTrackBoxes(paint, frameidx, true, step2widget->display_showtraj());
	else if (step2widget->display_showbox())
		global_data.paintWorldDetectBoxes(paint, frameidx, true);
}
