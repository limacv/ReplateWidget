#pragma once

#include <QWidget>
#include <qtimer.h>
#include <qpalette.h>
#include "MLPlayerHandler.hpp"

namespace Ui { class Step2Widget; };

class MLCacheStitching;
class MLCacheFlow;
class MLCacheTrajectories;
class Step2RenderArea;

class Step2Widget : public QWidget
{
	Q_OBJECT

public:
	Step2Widget(QWidget *parent = Q_NULLPTR);
	virtual ~Step2Widget();
	virtual void showEvent(QShowEvent* event);
	void generateMask(std::vector<cv::Mat>& masks, bool isblend);

private slots:
	// this set of funcs will block the main window and launch progress bar
	void runDetect(); 
	void runTrack();
	void runStitching();
	void runInpainting();
	void forceRunAll();

	// this set of funcs will check necessarities and then launch run***() 
	void tryRunDetect();
	void tryRunTrack();
	void tryRunStitching();
	void tryRunOpticalFlow();

private:
	void runOptflow(); // deprecated

private:
	Ui::Step2Widget *ui;

	MLCacheStitching* stitchdatap;
	MLCacheFlow* flowdatap;
	MLCacheTrajectories* trajp;  // only for convenient

	// for display
	MLPlayerHandler* player_manager;
	QTimer display_timer;
	bool display_showbackground();
	bool display_showwarped();
	bool display_showbox();
	bool display_showtraj();
	bool display_showtrack();
	
	friend class Step2RenderArea;
};


// the class for drawing image and rectangles (since QPainter can only used in paintEvent, 
// so have to use an extra class for painting all the things
class Step2RenderArea : public QWidget
{
	Q_OBJECT

public:
	Step2RenderArea(QWidget* parent = Q_NULLPTR)
		:QWidget(parent), step2widget(nullptr) 
	{
		QPalette pal = palette();
		pal.setColor(QPalette::Background, Qt::black);
		this->setPalette(pal);
	}

	~Step2RenderArea() {};
	void setStep2Widget(Step2Widget* p) { step2widget = p; }

protected:
	virtual void paintEvent(QPaintEvent* event);

private:
	Step2Widget* step2widget;
};