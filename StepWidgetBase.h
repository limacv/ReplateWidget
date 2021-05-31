#pragma once
#include <qwidget.h>

class StepWidgetBase: public QWidget
{
	Q_OBJECT

public:
	StepWidgetBase(QWidget* parent)
		:QWidget(parent)
	{}

	virtual ~StepWidgetBase() {}

	virtual void initState() = 0;
	virtual void onWidgetShowup() = 0;
};

