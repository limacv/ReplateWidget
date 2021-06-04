#pragma once

#include <QWidget>
#include <qpainter>
#include <qtimer.h>
#include "StepWidgetBase.h"
#include "MLDataStructure.h"

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
	//bool renderReplates();
	bool render();
	bool exportVideo();

private:
	void updateUIfromcfg() const;
	void setSizeHeight(int y);
	void setSizeWidth(int x);
	void setRotation(float rot);
	void setTranslationx(int dx);
	void setTranslationy(int dy);
	void setScalingx(float s);
	void setScalingy(float s);

private:
	Ui::Step4Widget *ui;
	VideoConfig* cfg;
};


// the class for drawing image and rectangles (since QPainter can only used in paintEvent, 
// so have to use an extra class for painting all the things
class Step4RenderArea : public QWidget
{
	Q_OBJECT

public:
	Step4RenderArea(QWidget* parent = Q_NULLPTR)
		:QWidget(parent), cfg(nullptr),
		display_frameidx(0)
	{}

	~Step4RenderArea() {};
	void setVideoConfig(VideoConfig* p) { cfg = p; }

protected:
	virtual void paintEvent(QPaintEvent* event);

private:
	friend class Step4Widget;
	//Step4Widget* step4widget;
	VideoConfig* cfg;
	int display_frameidx;
	QTimer display_timer;
};