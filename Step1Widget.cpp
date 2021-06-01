#include <qpainter.h>
#include <qmessagebox.h>
#include <qstatictext.h>
#include <qpoint.h>
#include <qdialog.h>
#include <qfiledialog.h>
#include <qmath.h>
#include "Step1Widget.h"
#include "ui_Step1Widget.h"
#include "MLDataManager.h"
#include "MLConfigManager.h"
#include "MLUtil.h"

Step1Widget::Step1Widget(QWidget* parent)
	: StepWidgetBase(parent),
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

void Step1Widget::initState()
{
	//trajp = &MLDataManager::get().trajectories;
	// init the slider
	
	//updateFrameidx(0);
	
	//connect(ui->buttonDetect, SIGNAL(clicked()), this, SLOT(runDetect()));
	//connect(ui->buttonTrack, SIGNAL(clicked()), this, SLOT(runTrack()));
	//connect(ui->buttonGenmask, SIGNAL(clicked()), this, SLOT(runSegmentation()));

	//connect(ui->checkShowBox, &QCheckBox::stateChanged, this, [this](int state) {
	//	this->display_showbox = (state == Qt::Checked);
	//	this->ui->imageLabel->update();
	//	});
	//connect(ui->checkShowName, &QCheckBox::stateChanged, this, [this](int state) {
	//	this->display_showname = (state == Qt::Checked);
	//	this->ui->imageLabel->update();
	//	});
	//connect(ui->checkShowTrace, &QCheckBox::stateChanged, this, [this](int state) {
	//	this->display_showtrace = (state == Qt::Checked);
	//	this->ui->imageLabel->update();
	//	});
	//connect(ui->buttonShowMask, &QPushButton::clicked, this, [this] {
	//	if (this->display_showmask == true)
	//	{
	//		this->ui->buttonShowMask->setText("Show Mask");
	//		display_showmask = false;
	//		this->ui->imageLabel->update();
	//	}
	//	else
	//	{
	//		this->ui->buttonShowMask->setText("Show Image");
	//		display_showmask = true;
	//		this->ui->imageLabel->update();
	//	}
	//	});
	
	// TODO initialize timer for video playing
	//display_timer.setInterval(33);
	//display_timer.start();
}

void Step1Widget::onWidgetShowup()
{

}

void Step1Widget::selectVideo()
{
	auto& global_data = MLDataManager::get();

	QString filepath = QFileDialog::getOpenFileName(
		this, tr("Open Video"), "D:/MSI_NB/source/data/",
		tr("Videos (*.mp4 *.avi)")
	);

	if (!global_data.load_raw_video(filepath))
	{
		QMessageBox::warning(this, "Warning", "Failed to load video");
		return;
	}

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