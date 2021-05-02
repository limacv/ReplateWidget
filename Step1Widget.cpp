#include <qmessagebox.h>
#include "Step1Widget.h"
#include "ui_Step1Widget.h"
#include "MLDataManager.h"
#include "MLConfigManager.h"
#include "MLUtil.h"
#include "MLPythonWarpper.h"

Step1Widget::Step1Widget(QWidget *parent)
	: QWidget(parent)
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
	// init the slider
	const auto& raw_frames = MLDataManager::get().raw_frames;
	ui->frameSlider->setRange(0, raw_frames.size() - 1);
	updateImage(0);
	connect(ui->frameSlider, SIGNAL(valueChanged(int)), this, SLOT(updateImage(int)));

	// init the button
	connect(ui->buttonDetect, SIGNAL(clicked()), this, SLOT(runDetect()));
	connect(ui->buttonTrack, SIGNAL(clicked()), this, SLOT(runTrack()));

	// initialize timer for video playing
	// TODO
	
	// try loading the existing track file
	data.tryLoadDetectionFromFile();
	data.tryLoadTrackFromFile();
}

void Step1Widget::updateImage(int frameidx) const
{
	const auto& raw_frames = MLDataManager::get().raw_frames;
	ui->imageLabel->setPixmap(MLUtil::mat2qpixmap(raw_frames[frameidx], QImage::Format_RGB888));
	ui->imageLabel->update();
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
	if (!data.tryLoadDetectionFromFile())
		qWarning("Step1Widget::Failed to load detection file");
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
	if (!data.tryLoadTrackFromFile())
		qWarning("Step1Widget::Failed to load track file");
}