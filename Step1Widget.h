#pragma once

#include <QWidget>
#include <qtimer.h>
#include "MLPlayerHandler.hpp"
#include "GBaseWidget.h"

namespace Ui { class Step1Widget; };

class Step1RenderArea;

class Step1Widget: public QWidget
{
	Q_OBJECT

public:
	Step1Widget(QWidget *parent = Q_NULLPTR);
	virtual ~Step1Widget();
	virtual void showEvent(QShowEvent* event);

	void resizerect0(int l, int t, int r, int b);

public slots:
	void selectVideo();
	void selectProject();
	void addmask();

signals:
	void videoloaded();

private:
	Ui::Step1Widget *ui;
	
	// for display
	QTimer timer;
	MLPlayerHandler* player_manager;

	friend class Step1RenderArea;
};


// the class for drawing image and rectangles (since QPainter can only used in paintEvent, 
// so have to use an extra class for painting all the things
class Step1RenderArea : public GBaseWidget
{
	Q_OBJECT

public:
	Step1RenderArea(QWidget* parent = Q_NULLPTR)
		:GBaseWidget(parent), step1widget(nullptr) {}
	~Step1RenderArea() {};
	void setStep1Widget(Step1Widget* p) { step1widget = p; }

protected:
	virtual void paintEvent(QPaintEvent* event);
	virtual QSize sizeHint();
	virtual bool hasHeightForWidth();
	virtual int heightForWidth(int w);

private:
	Step1Widget* step1widget;
};