#include "Step4Widget.h"
#include "ui_Step4Widget.h"

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
