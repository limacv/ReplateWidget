#include <qpainter.h>
#include <qmessagebox.h>
#include <qstatictext.h>
#include <qpoint.h>
#include <qmath.h>
#include "Step1Widget.h"
#include "ui_Step1Widget.h"
#include "MLDataManager.h"
#include "MLConfigManager.h"
#include "MLUtil.h"
#include "MLPythonWarpper.h"

Step1Widget::Step1Widget(QWidget* parent)
	: QWidget(parent),
	display_showbox(false),
	display_showname(false),
	display_showtrace(false),
	display_showmask(false),
	display_frameidx(0),
	trajp(nullptr)
{
	ui = new Ui::Step1Widget();
	ui->setupUi(this);
}

Step1Widget::~Step1Widget()
{
	delete ui;
}

void Step1Widget::initState()
{
	trajp = &MLDataManager::get().trajectories;
	// init the slider
	const auto& raw_frames = MLDataManager::get().raw_frames;
	ui->frameSlider->setRange(0, raw_frames.size() - 1);
	updateFrameidx(0);
	connect(ui->frameSlider, SIGNAL(valueChanged(int)), this, SLOT(updateFrameidx(int)));

	// init the button
	connect(ui->buttonDetect, SIGNAL(clicked()), this, SLOT(runDetect()));
	connect(ui->buttonTrack, SIGNAL(clicked()), this, SLOT(runTrack()));
	connect(ui->buttonGenmask, SIGNAL(clicked()), this, SLOT(runSegmentation()));

	connect(ui->checkShowBox, &QCheckBox::stateChanged, this, [this](int state) {
		this->display_showbox = (state == Qt::Checked);
		this->ui->imageLabel->update();
		});
	connect(ui->checkShowName, &QCheckBox::stateChanged, this, [this](int state) {
		this->display_showname = (state == Qt::Checked);
		this->ui->imageLabel->update();
		});
	connect(ui->checkShowTrace, &QCheckBox::stateChanged, this, [this](int state) {
		this->display_showtrace = (state == Qt::Checked);
		this->ui->imageLabel->update();
		});
	connect(ui->buttonShowMask, &QPushButton::clicked, this, [this] {
		if (this->display_showmask == true)
		{
			this->ui->buttonShowMask->setText("Show Mask");
			display_showmask = false;
			this->ui->imageLabel->update();
		}
		else
		{
			this->ui->buttonShowMask->setText("Show Image");
			display_showmask = true;
			this->ui->imageLabel->update();
		}
		});
	
	// initialize the drawer
	ui->imageLabel->setStep1Widget(this);

	// TODO initialize timer for video playing
	//display_timer.setInterval(33);
	//display_timer.start();
	
	// try loading the existing track file
	trajp->tryLoadDetectionFromFile();
	trajp->tryLoadTrackFromFile();
	MLDataManager::get().reinitMasks();
	runSegmentation();
}

void Step1Widget::runDetect()
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

	int ret = callDetectPy();
	if (ret != 0)
	{
		QMessageBox errorBox;
		errorBox.setText(QString("detection failed with error code %1").arg(QString::number(ret)));
		errorBox.exec();
	}
	if (!trajp->tryLoadDetectionFromFile())
		qWarning("Step1Widget::Failed to load detection file");
	ui->imageLabel->update();
}

void Step1Widget::runTrack()
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

	int ret = callTrackPy();
	if (ret != 0)
	{
		QMessageBox errorBox;
		errorBox.setText(QString("tracking failed with error code %1").arg(QString::number(ret)));
		errorBox.exec();
	}
	if (!trajp->tryLoadTrackFromFile())
		qWarning("Step1Widget::Failed to load track file");
	ui->imageLabel->update();
}


void Step1Widget::runSegmentation()
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();
	const int framecount = globaldata.get_framecount();

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
		auto& mask = globaldata.masks[i];
		for (auto pbox : trajp->frameidx2boxes[i])
			mask(pbox->rect).setTo(0);
	}
	ui->imageLabel->update();
}


void Step1Widget::updateFrameidx(int frameidx)
{
	display_frameidx = frameidx;
	ui->imageLabel->update();
}


void Step1RenderArea::paintEvent(QPaintEvent* event)
{
	const auto& raw_frames = MLDataManager::get().raw_frames;
	const auto& masks = MLDataManager::get().masks;
	
	QPainter paint(this);
	auto viewport = paint.viewport();

	/******************
	* paint image
	********************/
	if (step1widget->display_showmask)
	{
		paint.drawImage(viewport,
			MLUtil::mat2qimage(masks[step1widget->display_frameidx], QImage::Format_RGB888));
	}
	else
	{
		paint.drawImage(viewport, 
			MLUtil::mat2qimage(raw_frames[step1widget->display_frameidx], QImage::Format_RGB888));
		//paint.drawPixmap(paint.viewport(), 
		//	MLUtil::mat2qpixmap(raw_frames[step1widget->display_frameidx], QImage::Format_RGB888));
	}

	const float scalex = (float)viewport.width() / raw_frames[0].cols;
	const float scaley = (float)viewport.height() / raw_frames[0].rows;
	/******************
	* paint box and name
	********************/
	if (step1widget->display_showbox)
	{
		const auto& boxes = step1widget->trajp->frameidx2boxes[step1widget->display_frameidx];
		for (auto it = boxes.constKeyValueBegin(); it != boxes.constKeyValueEnd(); ++it)
		{
			const auto& color = step1widget->trajp->getColor(it->first);
			const auto& box = it->second;

			paint.setPen(QPen(color, 2));
			paint.drawRect(
				(int)(box->rect.x * scalex),
				(int)(box->rect.y * scaley),
				(int)(box->rect.width * scalex),
				(int)(box->rect.height * scaley));
		}
	}

	/******************
	* paint name
	******************/
	if (step1widget->display_showname)
	{
		const auto& boxes = step1widget->trajp->frameidx2boxes[step1widget->display_frameidx];
		for (auto it = boxes.constKeyValueBegin(); it != boxes.constKeyValueEnd(); ++it)
		{
			const auto& color = step1widget->trajp->getColor(it->first);
			const auto& box = it->second;

			QString text = QString("%1_%2").arg(MLCacheTrajectories::classid2name[box->classid], QString::number(box->instanceid));
			paint.fillRect(
				(int)(box->rect.x * scalex) - 1,
				(int)(box->rect.y * scaley) - 10,
				text.size() * 7,
				10, color);
			paint.setPen(QColor(255, 255, 255));
			paint.drawText(
				(int)(box->rect.x * scalex),
				(int)(box->rect.y * scaley) - 1, text);
		}
	}

	/******************
	* paint name
	******************/
	if (step1widget->display_showtrace)
	{
		const auto& trajectories = step1widget->trajp->objectid2boxes;
		const auto& frameidx = step1widget->display_frameidx;
		const auto& framecount = MLDataManager::get().get_framecount();
		for (auto it = trajectories.constKeyValueBegin(); it != trajectories.constKeyValueEnd(); ++it)
		{
			const auto& color = step1widget->trajp->getColor(it->first);
			const auto& boxes = it->second;
			QPoint lastpt(-999, -999);

			for (const auto& pbox : boxes)
			{
				QPoint currpt(scalex * (pbox->rect.x + pbox->rect.width / 2),
					scaley * (pbox->rect.y + pbox->rect.height / 2));

				if (lastpt.x() < 0)
				{
					lastpt = currpt;
					continue;
				}
				
				float width = 4 - qAbs<float>(pbox->frameidx - frameidx) / 5;
				width = MAX(width, 0.2);
				paint.setPen(QPen(color, width));
				paint.drawLine(lastpt, currpt);
				lastpt = currpt;
			}
		}
	}
}