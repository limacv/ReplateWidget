#pragma once

#include <QWidget>
#include "ui_Step2Widget.h"

class Step2Widget : public QWidget
{
	Q_OBJECT

public:
	Step2Widget(QWidget *parent = Q_NULLPTR);
	~Step2Widget();

private:
	Ui::Step2Widget ui;
};
