#include "Step3Widget.h"
#include "MLDataManager.h"
#include "ui_Step3Widget.h"

Step3Widget::Step3Widget(QWidget *parent)
	: QWidget(parent)
{
	ui = new Ui::Step3Widget();
	ui->setupUi(this);
}

Step3Widget::~Step3Widget()
{
	delete ui;
}

void Step3Widget::initState()
{
	auto& globaldata = MLDataManager::get();
	trajp = &globaldata.trajectories;
	platesp = &globaldata.plates_cache;
}

void Step3Widget::onWidgetShowup()
{
	platesp->initialize_cut(45);
}

void Step3RenderArea::paintEvent(QPaintEvent* event)
{

}