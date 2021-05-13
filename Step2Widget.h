#pragma once

#include <QWidget>
#include "MLStep2Data.h"
#include <qtimer.h>

namespace Ui { class Step2Widget; };

class Step2RenderArea;

class Step2Widget : public QWidget
{
	Q_OBJECT

public:
	Step2Widget(QWidget *parent = Q_NULLPTR);
	~Step2Widget();
	void initState();

private slots:
	void runStitching();
	void updateFrameidx(int frameidx);

private:
	Ui::Step2Widget *ui;

	MLStep2Data data;

	// for display
	QTimer display_timer;
	bool display_showbackground;
	bool display_showwarped;
	bool display_showbox;
	bool display_showtrace;
	int display_frameidx;
	friend class Step2RenderArea;
};


// the class for drawing image and rectangles (since QPainter can only used in paintEvent, 
// so have to use an extra class for painting all the things
class Step2RenderArea : public QWidget
{
	Q_OBJECT

public:
	Step2RenderArea(QWidget* parent = Q_NULLPTR)
		:QWidget(parent), step2widget(nullptr) {}
	~Step2RenderArea() {};
	void setStep2Widget(Step2Widget* p) { step2widget = p; }

protected:
	virtual void paintEvent(QPaintEvent* event);

private:
	Step2Widget* step2widget;
};