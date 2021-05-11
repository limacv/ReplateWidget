#pragma once

#include <QWidget>
#include <qtimer.h>
#include "MLStep1Data.h"

namespace Ui { class Step1Widget; };


class Step1RenderArea;

class Step1Widget : public QWidget
{
	Q_OBJECT

public:
	Step1Widget(QWidget *parent = Q_NULLPTR);
	~Step1Widget();
	void initState();
	
private slots:
	void runDetect();
	void runTrack();
	void runSegmentation();
	void updateFrameidx(int frameidx);

private:
	Ui::Step1Widget *ui;

	MLStep1Data data;
	
	// for display
	QTimer display_timer;
	bool display_showbox;
	bool display_showname;
	bool display_showtrace;
	bool display_showmask;
	int display_frameidx;
	friend class Step1RenderArea;
};


// the class for drawing image and rectangles (since QPainter can only used in paintEvent, 
// so have to use an extra class for painting all the things
class Step1RenderArea : public QWidget
{
	Q_OBJECT

public:
	Step1RenderArea(QWidget* parent = Q_NULLPTR)
		:QWidget(parent), step1widget(nullptr) {}
	~Step1RenderArea() {};
	void setStep1Widget(Step1Widget* p) { step1widget = p; }

protected:
	virtual void paintEvent(QPaintEvent* event);

private:
	QColor getColor(const ObjID& id);

private:
	Step1Widget* step1widget;
	QHash<ObjID, QColor> colormap;
};