#include "Step4Widget.h"
#include "MLUtil.h"
#include "MLDataManager.h"
#include "ui_Step4Widget.h"
#include <qfiledialog.h>
#include <qpixmap.h>
#include <qmessagebox.h>
#include <qpainter.h>
#include <opencv2/videoio.hpp>
#include <opencv2/stitching/detail/blenders.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

Step4Widget::Step4Widget(QWidget *parent)
	: QWidget(parent)
{
	ui = new Ui::Step4Widget();
	ui->setupUi(this);
}

Step4Widget::~Step4Widget()
{
	delete ui;
}

void Step4Widget::initState()
{
	const auto& globaldata = MLDataManager::get();
	ui->imageSlider->setRange(0, globaldata.replate_video.size() - 1);

	connect(ui->buttonRender, &QPushButton::clicked, this, &Step4Widget::renderReplates);
	connect(ui->buttonExport, &QPushButton::clicked, this, &Step4Widget::exportVideo);
	connect(ui->imageSlider, &QSlider::valueChanged, this, [this](int idx) {
		ui->imageWidget->display_frameidx = idx;
		ui->imageWidget->update();
		});

	connect(ui->buttonPlay, &QPushButton::clicked, this, [this]() {
		auto& timer = ui->imageWidget->display_timer;
		timer.isActive() ? timer.stop() : timer.start();
		});
	connect(&ui->imageWidget->display_timer, &QTimer::timeout, this, [&]() {
		const int framecount = globaldata.replate_video.size();
		ui->imageWidget->display_frameidx = (ui->imageWidget->display_frameidx + 1) % framecount;
		ui->imageSlider->setValue(ui->imageWidget->display_frameidx);
		});
	ui->imageWidget->setStep4Widget(this);
}

void Step4Widget::onWidgetShowup()
{
	auto& video_cfg = MLDataManager::get().out_video_cfg;
	video_cfg.size = MLDataManager::get().stitch_cache.background.size();
	video_cfg.framecount = MLDataManager::get().plates_cache.framecount;
	ui->imageWidget->display_timer.setInterval(1000.f / video_cfg.fps);
}


bool Step4Widget::renderReplates()
{
	const auto& trajectories = MLDataManager::get().trajectories;
	auto& stitch_cache = MLDataManager::get().stitch_cache;
	auto& allplates = MLDataManager::get().plates_cache;
	auto& video = MLDataManager::get().replate_video;
	auto& video_cfg = MLDataManager::get().out_video_cfg;

	video.resize(video_cfg.framecount);
	for (int frameidx = 0; frameidx < video_cfg.framecount; ++frameidx)
	{
		auto blender = cv::detail::Blender::createDefault(cv::detail::Blender::MULTI_BAND);
		blender->prepare(cv::Rect(cv::Point(0, 0), video_cfg.size));
		dynamic_cast<cv::detail::MultiBandBlender*>(blender.get())->setNumBands(2);
		// blend each roi into the background
		cv::Mat canvas, mask;
		cv::cvtColor(stitch_cache.background, canvas, cv::COLOR_RGBA2RGB);
		canvas.convertTo(canvas, CV_32FC3);
		mask = cv::Mat(canvas.size(), CV_32FC1, cv::Scalar(1.));
		
		QVector<BBox*> plates;
		allplates.collectPlate((float)frameidx / video_cfg.framecount, plates);
		
		for (const auto& boxp : plates)
		{
			cv::Rect roi_warpedimg = boxp->rect_global - stitch_cache.rois[boxp->frameidx].tl(),
				roi_global = boxp->rect_global - stitch_cache.global_roi.tl();
			cv::Mat boximg = stitch_cache.warped_frames[boxp->frameidx](roi_warpedimg).clone();

			cv::cvtColor(boximg, boximg, cv::COLOR_RGBA2RGB);
			boximg.convertTo(boximg, CV_32FC3);
			cv::Mat boxmask = cv::Mat(boximg.size(), CV_32FC1, cv::Scalar(1.));

			//cv::Mat mat1 = canvas(roi_global).mul(mask(roi_global) - boxmask);
			//cv::Mat mat2 = boximg.mul(boxmask);
			//boximg = mat1 + mat2;
			//boximg.copyTo(canvas(roi_global));
			
			cv::Mat dst(boximg.size(), CV_32FC3);
			dst.forEach<cv::Vec3f>([&](cv::Vec3f& pix, const int* pos) {
				const cv::Vec3f& canvaspix = canvas(roi_global).at<cv::Vec3f>(pos[0], pos[1]),
					boxpix = boximg.at<cv::Vec3f>(pos[0], pos[1]);
				const float& canvasweight = mask(roi_global).at<float>(pos[0], pos[1]),
					boxweight = boxmask.at<float>(pos[0], pos[1]);
				pix = canvaspix * (1. - boxweight) + boxpix * boxweight;
				});
			dst.copyTo(canvas(roi_global));
		}
		canvas.convertTo(video[frameidx], CV_8UC3);
	}

	ui->imageSlider->setRange(0, video.size() - 1);
	ui->imageWidget->update();
	return true;
}

bool Step4Widget::exportVideo()
{
	const auto& video = MLDataManager::get().replate_video;
	auto& videocfg = MLDataManager::get().out_video_cfg;
	if (video.empty())
	{
		QMessageBox warningbox;
		warningbox.setText("The rendering result is empty");
		warningbox.setInformativeText("Please rerun the rendering");
		warningbox.setStandardButtons(QMessageBox::Ok);
		warningbox.exec();
	}
	QString outfile = QFileDialog::getSaveFileName(
		this, tr("Select Save File"), "D:/MSI_NB/source/data/",
		tr("Videos (*.mp4 *.avi)")
		);

	videocfg.filepath = outfile;
	cv::VideoWriter writer(outfile.toStdString(), (int)videocfg.fourcc, videocfg.fps, videocfg.size);
	
	if (!writer.isOpened())
	{
		qWarning("Step4Widget::failed to open the writer");
		return false;
	}
	for (const auto& frame : video)
	{
		cv::Mat rgb; cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
		writer.write(rgb);
	}
	writer.release();
	return true;
}

void Step4RenderArea::paintEvent(QPaintEvent* event)
{
	const auto& video = MLDataManager::get().replate_video;
	if (display_frameidx < 0 || display_frameidx >= video.size())
		return;
	QPainter paint(this);
	auto viewport = paint.viewport();
	paint.drawImage(viewport, MLUtil::mat2qimage(video[display_frameidx], QImage::Format_RGB888));
}