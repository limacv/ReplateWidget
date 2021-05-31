#pragma once

#include <QWidget>
#include <qtimer.h>
#include "StepWidgetBase.h"

namespace Ui { class Step4Widget; };

class Step4Widget : public StepWidgetBase
{
	Q_OBJECT

public:
	Step4Widget(QWidget *parent = Q_NULLPTR);
	virtual ~Step4Widget();
	virtual void initState();
	virtual void onWidgetShowup();

public slots:
	bool renderReplates();
	bool exportVideo();

private:
	Ui::Step4Widget *ui;

};


// the class for drawing image and rectangles (since QPainter can only used in paintEvent, 
// so have to use an extra class for painting all the things
class Step4RenderArea : public QWidget
{
	Q_OBJECT

public:
	Step4RenderArea(QWidget* parent = Q_NULLPTR)
		:QWidget(parent), step4widget(nullptr),
		display_frameidx(0)
	{}

	~Step4RenderArea() {};
	void setStep4Widget(Step4Widget* p) { step4widget = p; }

protected:
	virtual void paintEvent(QPaintEvent* event);

private:
	friend class Step4Widget;
	Step4Widget* step4widget;
	int display_frameidx;
	QTimer display_timer;
};