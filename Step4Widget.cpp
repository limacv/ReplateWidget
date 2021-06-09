#include "Step4Widget.h"
#include "MLUtil.h"
#include "MLDataManager.h"
#include "ui_Step4Widget.h"
#include <qfiledialog.h>
#include <qpixmap.h>
#include <qmessagebox.h>
#include <qpainter.h>
#include <qvalidator.h>
#include <opencv2/videoio.hpp>
#include <opencv2/stitching/detail/blenders.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <qevent.h>

Step4Widget::Step4Widget(QWidget *parent)
	: QWidget(parent),
	render_isdirty(true)
{
	ui = new Ui::Step4Widget();
	ui->setupUi(this);
	cfg = &MLDataManager::get().out_video_cfg;
	ui->imageWidget->setStep4Widget(this);

	connect(ui->buttonRender, &QPushButton::clicked, this, &Step4Widget::render);
	connect(ui->buttonExport, &QPushButton::clicked, this, &Step4Widget::exportVideo);
	connect(ui->imageSlider, &QSlider::valueChanged, [this](int idx) {
		ui->imageWidget->display_frameidx = idx;
		ui->imageWidget->update();
		});
	connect(ui->buttonPlay, &QPushButton::clicked, this, [this]() {
		auto& timer = ui->imageWidget->display_timer;
		timer.isActive() ? timer.stop() : timer.start();
		});
	connect(&ui->imageWidget->display_timer, &QTimer::timeout, this, [&]() {
		int framecount = MLDataManager::get().replate_video.size();
		if (framecount > 0)
		{
			ui->imageWidget->display_frameidx = (ui->imageWidget->display_frameidx + 1) % framecount;
			ui->imageSlider->setValue(ui->imageWidget->display_frameidx);
		}
		});
	
	// UI that change configuration
	connect(ui->pathLineEdit, &QLineEdit::textEdited, [this](const QString& text) {setFilePath(text); });
	connect(ui->pathLineButton, &QPushButton::clicked, [this](bool checked) {
		QString outfile = QFileDialog::getSaveFileName(this, tr("Select Save File"), "D:/MSI_NB/source/data/", tr("Videos (*.mp4 *.avi)"));
		setFilePath(outfile);
		});
	ui->fpsLineEdit->setValidator(new QDoubleValidator(0, 1000, 1, this));
	connect(ui->fpsLineEdit, &QLineEdit::textEdited, [this](const QString& text) {setFps(text.toDouble()); });
	ui->resolutionhLineEdit->setValidator(new QIntValidator(0, 100000, this));
	ui->resolutionwLineEdit->setValidator(ui->resolutionhLineEdit->validator());
	connect(ui->resolutionhLineEdit, &QLineEdit::textEdited, [this](const QString& text) {	setResolutionHeight(text.toInt()); });
	connect(ui->resolutionwLineEdit, &QLineEdit::textEdited, [this](const QString& text) {	setResolutionWidth(text.toInt());	});

	ui->transxLineEdit->setValidator(new QIntValidator(-1000, 1000, this));
	ui->transyLineEdit->setValidator(ui->transxLineEdit->validator());
	connect(ui->transxLineEdit, &QLineEdit::textEdited, [this](const QString& text) {setTranslationx(text.toInt()); });
	connect(ui->transyLineEdit, &QLineEdit::textEdited, [this](const QString& text) {setTranslationy(text.toInt()); });
	ui->transxSlider->setRange(-1000, 1000);
	ui->transySlider->setRange(-1000, 1000);
	connect(ui->transxSlider, &QSlider::sliderMoved, this, &Step4Widget::setTranslationx);
	connect(ui->transySlider, &QSlider::sliderMoved, this, &Step4Widget::setTranslationy);

	ui->scalexLineEdit->setValidator(new QDoubleValidator(0.5, 2, 2, this));
	ui->scaleyLineEdit->setValidator(ui->scalexLineEdit->validator());
	connect(ui->scalexLineEdit, &QLineEdit::textEdited, this, [this](const QString& text) {setScalingx(text.toDouble()); });
	connect(ui->scaleyLineEdit, &QLineEdit::textEdited, this, [this](const QString& text) {setScalingy(text.toDouble()); });
	ui->scalexSlider->setRange(-100, 100);
	ui->scaleySlider->setRange(-100, 100);
	connect(ui->scalexSlider, &QSlider::sliderMoved, [this](int value) {setScalingx(powf(2.f, (float)value / 100)); });
	connect(ui->scaleySlider, &QSlider::sliderMoved, [this](int value) {setScalingy(powf(2.f, (float)value / 100)); });

	ui->rotationLineEdit->setValidator(new QDoubleValidator(-180, 180, 0.1, this));
	connect(ui->rotationLineEdit, &QLineEdit::textEdited, [this](const QString& text) {setRotation(text.toDouble()); });
	ui->rotationSlider->setRange(-1800, 1800);
	connect(ui->rotationSlider, &QSlider::sliderMoved, this, [this](int value) {setRotation((float)value / 10); });

	connect(ui->repeatSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &Step4Widget::setRepeat);
}

Step4Widget::~Step4Widget()
{
	delete ui;
}

void Step4Widget::setFilePath(const QString& path) 
{ 
	cfg->filepath = path; 
	ui->pathLineEdit->setText(path);
}

void Step4Widget::setFps(float fps)
{
	cfg->fps = fps;
	ui->fpsLineEdit->setText(QString::number(fps));
	ui->imageWidget->display_timer.setInterval(1000.f / fps);
}

void Step4Widget::setResolutionHeight(int y)
{
	cfg->size.height = y;
	ui->resolutionhLineEdit->setText(QString::number(y));
	if (ui->resolutionLockButton->isChecked())
	{
		y = (float)y / MLDataManager::get().VideoHeight() * MLDataManager::get().VideoWidth();
		cfg->size.width = y;
		ui->resolutionwLineEdit->setText(QString::number(y));
	}
	ui->imageWidget->update();
}

void Step4Widget::setResolutionWidth(int x)
{
	cfg->size.width = x;
	ui->resolutionwLineEdit->setText(QString::number(x));
	if (ui->resolutionLockButton->isChecked())
	{
		x = (float)x / MLDataManager::get().VideoWidth() * MLDataManager::get().VideoHeight();
		cfg->size.height = x;
		ui->resolutionhLineEdit->setText(QString::number(x));
	}
	ui->imageWidget->update();
}

void Step4Widget::setRotation(float rot)
{
	cfg->rotation = rot;
	ui->rotationSlider->setSliderPosition((int)(rot * 10));
	ui->rotationLineEdit->setText(QString::number(rot));
	ui->imageWidget->update();
}

void Step4Widget::setTranslationx(int dx)
{
	cfg->translation.x = dx;
	ui->transxSlider->setSliderPosition(dx);
	ui->transxLineEdit->setText(QString::number(dx));
	ui->imageWidget->update();
}

void Step4Widget::setTranslationy(int dy)
{
	cfg->translation.y = dy;
	ui->transySlider->setSliderPosition(dy);
	ui->transyLineEdit->setText(QString::number(dy));
	ui->imageWidget->update();
}

void Step4Widget::setScalingx(float x)
{
	x = x <= 0 ? 0.001 : x;
	cfg->scaling[0] = x;
	ui->scalexSlider->setSliderPosition(logf(x) / logf(2.f) * 100);
	ui->scalexLineEdit->setText(QString::number(x));
	if (ui->scaleLockButton->isChecked())
	{
		cfg->scaling[1] = x;
		ui->scaleySlider->setSliderPosition(logf(x) / logf(2.f) * 100);
		ui->scaleyLineEdit->setText(QString::number(x));
	}
	ui->imageWidget->update();
}

void Step4Widget::setScalingy(float y)
{
	y = y <= 0 ? 0.001 : y;
	cfg->scaling[1] = y;
	ui->scaleySlider->setSliderPosition(logf(y) / logf(2.f) * 100);
	ui->scaleyLineEdit->setText(QString::number(y));
	if (ui->scaleLockButton->isChecked())
	{
		cfg->scaling[0] = y;
		ui->scalexSlider->setSliderPosition(logf(y) / logf(2.f) * 100);
		ui->scalexLineEdit->setText(QString::number(y));
	}
	ui->imageWidget->update();
}

void Step4Widget::setRepeat(int s)
{
	s = s < 1 ? 1 : s;
	cfg->repeat = s;
	ui->repeatSpinBox->setValue(s);
	setRenderResultDirty(true);
}

void Step4Widget::setRenderResultDirty(bool isdirty)
{
	render_isdirty = isdirty;
	ui->buttonRender->setText(isdirty ? "Render*" : "Render");
}

void Step4Widget::showEvent(QShowEvent* event)
{
	if (event->spontaneous()) return;
	const auto& globaldata = MLDataManager::get();

	if (cfg->isempty())
	{
		(*cfg) = globaldata.raw_video_cfg;
		QFileInfo info(cfg->filepath);
		
		cfg->size = MLDataManager::get().stitch_cache.background.size();
		cfg->framecount = MLDataManager::get().plates_cache.replate_duration;
	}
	ui->imageSlider->setRange(0, globaldata.plates_cache.replate_duration - 1);
	ui->imageWidget->display_timer.setInterval(1000.f / cfg->fps);
	render();
	updateUIfromcfg();
}


//bool Step4Widget::renderReplates()
//{
//	const auto& trajectories = MLDataManager::get().trajectories;
//	auto& stitch_cache = MLDataManager::get().stitch_cache;
//	auto& allplates = MLDataManager::get().plates_cache;
//	auto& video = MLDataManager::get().replate_video;
//	auto& video_cfg = MLDataManager::get().out_video_cfg;
//
//	video.resize(video_cfg.framecount);
//	for (int frameidx = 0; frameidx < video_cfg.framecount; ++frameidx)
//	{
//		auto blender = cv::detail::Blender::createDefault(cv::detail::Blender::MULTI_BAND);
//		blender->prepare(cv::Rect(cv::Point(0, 0), video_cfg.size));
//		dynamic_cast<cv::detail::MultiBandBlender*>(blender.get())->setNumBands(2);
//		// blend each roi into the background
//		cv::Mat canvas, mask;
//		cv::cvtColor(stitch_cache.background, canvas, cv::COLOR_RGBA2RGB);
//		canvas.convertTo(canvas, CV_32FC3);
//		mask = cv::Mat(canvas.size(), CV_32FC1, cv::Scalar(1.));
//		
//		QVector<BBox*> plates;
//		allplates.collectPlate((float)frameidx / video_cfg.framecount, plates);
//		
//		for (const auto& boxp : plates)
//		{
//			cv::Rect roi_warpedimg = boxp->rect_global - stitch_cache.rois[boxp->frameidx].tl(),
//				roi_global = boxp->rect_global - stitch_cache.global_roi.tl();
//			cv::Mat boximg = stitch_cache.warped_frames[boxp->frameidx](roi_warpedimg).clone();
//
//			cv::cvtColor(boximg, boximg, cv::COLOR_RGBA2RGB);
//			boximg.convertTo(boximg, CV_32FC3);
//			cv::Mat boxmask = cv::Mat(boximg.size(), CV_32FC1, cv::Scalar(1.));
//
//			//cv::Mat mat1 = canvas(roi_global).mul(mask(roi_global) - boxmask);
//			//cv::Mat mat2 = boximg.mul(boxmask);
//			//boximg = mat1 + mat2;
//			//boximg.copyTo(canvas(roi_global));
//			
//			cv::Mat dst(boximg.size(), CV_32FC3);
//			dst.forEach<cv::Vec3f>([&](cv::Vec3f& pix, const int* pos) {
//				const cv::Vec3f& canvaspix = canvas(roi_global).at<cv::Vec3f>(pos[0], pos[1]),
//					boxpix = boximg.at<cv::Vec3f>(pos[0], pos[1]);
//				const float& canvasweight = mask(roi_global).at<float>(pos[0], pos[1]),
//					boxweight = boxmask.at<float>(pos[0], pos[1]);
//				pix = canvaspix * (1. - boxweight) + boxpix * boxweight;
//				});
//			dst.copyTo(canvas(roi_global));
//		}
//		canvas.convertTo(video[frameidx], CV_8UC3);
//	}
//
//	ui->imageSlider->setRange(0, video.size() - 1);
//	ui->imageWidget->update();
//	return true;
//}

bool Step4Widget::render()
{
	if (!render_isdirty) return true;

	const auto& global_data = MLDataManager::get();
	auto& outvideo = MLDataManager::get().replate_video;
	outvideo.resize(cfg->framecount * cfg->repeat);
	for (int frameidx = 0; frameidx < outvideo.size(); frameidx++)
	{
		QImage frame(global_data.VideoWidth(), global_data.VideoHeight(), QImage::Format_ARGB32_Premultiplied);
		QPainter painter(&frame);
		global_data.paintReplateFrame(painter, frameidx);
		outvideo[frameidx] = MLUtil::qimage_to_mat_cpy(frame, CV_8UC4);
		cv::cvtColor(outvideo[frameidx], outvideo[frameidx], cv::COLOR_BGRA2RGBA);

	}
	ui->imageSlider->setRange(0, outvideo.size() - 1);
	setRenderResultDirty(false);
	return true;
}

bool Step4Widget::exportVideo()
{
	const auto& video = MLDataManager::get().replate_video;
	if (video.empty())
	{
		QMessageBox warningbox;
		warningbox.setText("The rendering result is empty");
		warningbox.setInformativeText("Please rerun the rendering");
		warningbox.setStandardButtons(QMessageBox::Ok);
		warningbox.exec();
	}
	if (QFile::exists(cfg->filepath))
	{
		QMessageBox warningbox;
		warningbox.setText("File exists");
		warningbox.setInformativeText("Do you want to overwrite the video?");
		warningbox.setStandardButtons(QMessageBox::Ok | QMessageBox::No);
		warningbox.setDefaultButton(QMessageBox::No);
		auto ret = warningbox.exec();
		if (ret == QMessageBox::No)
			return false;
	}

	cv::VideoWriter writer(cfg->filepath.toStdString(), (int)cfg->fourcc, cfg->fps, cfg->size);
	
	if (!writer.isOpened())
	{
		qWarning("Step4Widget::failed to open the writer");
		return false;
	}
	for (const auto& frame : video)
	{
		QImage out(cfg->size.width, cfg->size.height, QImage::Format_ARGB32_Premultiplied);
		QPainter painter(&out);

		//transformQPainter(painter);
		//painter.drawImage(painter.viewport(), MLUtil::mat2qimage(frame, QImage::Format_ARGB32_Premultiplied));
		//cv::Mat rgb = MLUtil::qimage_to_mat_cpy(out, CV_8UC4);
		//cv::cvtColor(rgb, rgb, cv::COLOR_RGBA2RGB);
		//writer.write(rgb);

		QRect targetrect = MLUtil::scaleViewportWithRatio(painter.viewport(), (float)cfg->size.width / cfg->size.height);
		painter.setWindow(QRect(0, 0, cfg->size.width, cfg->size.height));
		transformQPainter(painter);
		painter.setViewport(targetrect);
		painter.drawImage(QPoint(0, 0), MLUtil::mat2qimage(frame, QImage::Format_ARGB32_Premultiplied));
		cv::Mat rgb = MLUtil::qimage_to_mat_cpy(out, CV_8UC4);
		cv::cvtColor(rgb, rgb, cv::COLOR_RGBA2RGB);
		writer.write(rgb);
	}
	writer.release();
	return true;
}

void Step4Widget::updateUIfromcfg()
{
	setFilePath(cfg->filepath);
	setFps(cfg->fps);
	setResolutionHeight(cfg->size.height);
	setResolutionWidth(cfg->size.width);
	setRepeat(cfg->repeat);
	setTranslationx(cfg->translation.x);
	setTranslationy(cfg->translation.y);
	setScalingx(cfg->scaling[0]);
	setScalingy(cfg->scaling[1]);
	setRotation(cfg->rotation);
}

void Step4Widget::transformQPainter(QPainter& paint) const
{
	auto& viewport = paint.viewport();
	paint.translate(cfg->translation.x, cfg->translation.y);
	paint.scale(cfg->scaling[0], cfg->scaling[1]);
	paint.translate(cfg->size.width / 2, cfg->size.height / 2);
	paint.rotate(cfg->rotation);
	paint.translate(-cfg->size.width / 2, -cfg->size.height / 2);
}

void Step4RenderArea::paintEvent(QPaintEvent* event)
{
	const auto& global_data = MLDataManager::get();
	const auto& video = global_data.replate_video;
	const auto& size = global_data.out_video_cfg.size;
	if (display_frameidx < 0 || display_frameidx >= video.size())
		return;

	/*if (display_map.height() != size.height || display_map.width() != size.width)
		display_map = QPixmap(size.width, size.height);
	
	display_map.fill(Qt::transparent);
	QPainter paint(&display_map);
	step4widget->transformQPainter(paint);
	paint.drawImage(paint.viewport(), MLUtil::mat2qimage(video[display_frameidx], QImage::Format_ARGB32_Premultiplied));

	QPainter painter2(this);
	QRect viewport = MLUtil::scaleViewportWithRatio(painter2, (float)size.width / size.height);
	painter2.drawPixmap(viewport, display_map);*/

	QPainter painter(this);
	QRect targetrect = MLUtil::scaleViewportWithRatio(painter.viewport(), (float)size.width / size.height);
	targetrect = QRect(targetrect.x() + 3, targetrect.y() + 3, targetrect.width() - 6, targetrect.height() - 6);

	painter.setWindow(QRect(0, 0, size.width, size.height));
	step4widget->transformQPainter(painter);
	painter.setViewport(targetrect);
	painter.drawImage(QPoint(0, 0), MLUtil::mat2qimage(video[display_frameidx], QImage::Format_ARGB32_Premultiplied));

	painter.resetTransform();
	painter.drawRect(targetrect);
}