#include "Step3Widget.h"
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

}