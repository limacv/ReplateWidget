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
	display_showbox(false),
	display_showname(false),
	display_showtrace(false),
	display_showmask(false),
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

	//const float scalex = (float)viewport.width() / raw_frames[0].cols;
	//const float scaley = (float)viewport.height() / raw_frames[0].rows;
	///******************
	//* paint box and name
	//********************/
	//if (step1widget->display_showbox)
	//{
	//	const auto& boxes = step1widget->trajp->frameidx2boxes[step1widget->display_frameidx];
	//	for (auto it = boxes.constKeyValueBegin(); it != boxes.constKeyValueEnd(); ++it)
	//	{
	//		const auto& color = step1widget->trajp->getColor(it->first);
	//		const auto& box = it->second;

	//		paint.setPen(QPen(color, 2));
	//		paint.drawRect(
	//			(int)(box->rect.x * scalex),
	//			(int)(box->rect.y * scaley),
	//			(int)(box->rect.width * scalex),
	//			(int)(box->rect.height * scaley));
	//	}
	//}

	///******************
	//* paint name
	//******************/
	//if (step1widget->display_showname)
	//{
	//	const auto& boxes = step1widget->trajp->frameidx2boxes[step1widget->display_frameidx];
	//	for (auto it = boxes.constKeyValueBegin(); it != boxes.constKeyValueEnd(); ++it)
	//	{
	//		const auto& color = step1widget->trajp->getColor(it->first);
	//		const auto& box = it->second;

	//		QString text = QString("%1_%2").arg(MLCacheTrajectories::classid2name[box->classid], QString::number(box->instanceid));
	//		paint.fillRect(
	//			(int)(box->rect.x * scalex) - 1,
	//			(int)(box->rect.y * scaley) - 10,
	//			text.size() * 7,
	//			10, color);
	//		paint.setPen(QColor(255, 255, 255));
	//		paint.drawText(
	//			(int)(box->rect.x * scalex),
	//			(int)(box->rect.y * scaley) - 1, text);
	//	}
	//}

	///******************
	//* paint name
	//******************/
	//if (step1widget->display_showtrace)
	//{
	//	const auto& trajectories = step1widget->trajp->objid2trajectories;
	//	const auto& frameidx = step1widget->display_frameidx;
	//	const auto& framecount = MLDataManager::get().get_framecount();
	//	for (auto it = trajectories.constKeyValueBegin(); it != trajectories.constKeyValueEnd(); ++it)
	//	{
	//		const auto& color = step1widget->trajp->getColor(it->first);
	//		const auto& boxes = it->second.boxes;
	//		QPoint lastpt(-999, -999);
	//		bool skip_flag = true;
	//		for (const auto& pbox : boxes)
	//		{
	//			if (pbox == nullptr || pbox->empty())
	//			{
	//				skip_flag = true;
	//				continue;
	//			}

	//			QPoint currpt(scalex * (pbox->rect.x + pbox->rect.width / 2),
	//				scaley * (pbox->rect.y + pbox->rect.height / 2));

	//			if (lastpt.x() < 0)
	//			{
	//				lastpt = currpt;
	//				skip_flag = false;
	//				continue;
	//			}
	//			float width = 4 - qAbs<float>(pbox->frameidx - frameidx) / 5;
	//			width = MAX(width, 0.2);
	//			auto style = skip_flag ? Qt::PenStyle::DotLine : Qt::PenStyle::SolidLine;
	//			paint.setPen(QPen(color, width, style));
	//			paint.drawLine(lastpt, currpt);
	//			lastpt = currpt;
	//			skip_flag = false;
	//		}
	//	}
	//}
}