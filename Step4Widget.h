#pragma once

#include <QWidget>
#include <qpainter>
#include <qtimer.h>
#include "MLDataStructure.h"
#include "MLPlayerHandler.hpp"

namespace Ui { class Step4Widget; };

class Step4Widget : public QWidget
{
	Q_OBJECT

public:
	Step4Widget(QWidget *parent = Q_NULLPTR);
	virtual ~Step4Widget();
	virtual void showEvent(QShowEvent* event);
	void transformQPainter(QPainter& paint) const;

public slots:
	//bool renderReplates();
	bool render();
	bool exportVideo();

private:
	void updateUIfromcfg();
	void setFilePath(const QString& path);
	void setFps(float fps);
	void setResolutionHeight(int y);
	void setResolutionWidth(int x);
	void setRotation(float rot);
	void setTranslationx(int dx);
	void setTranslationy(int dy);
	void setScalingx(float s);
	void setScalingy(float s);
	void setRepeat(int t);
	void setRenderResultDirty(bool isdirty);


private:
	friend class Step4RenderArea;
	bool render_isdirty;
	Ui::Step4Widget *ui;
	VideoConfig* cfg;
	MLPlayerHandler* player_manager;
};


// the class for drawing image and rectangles (since QPainter can only used in paintEvent, 
// so have to use an extra class for painting all the things
class Step4RenderArea : public QWidget
{
	Q_OBJECT

public:
	Step4RenderArea(QWidget* parent = Q_NULLPTR)
		:QWidget(parent), step4widget(nullptr)
	{}

	~Step4RenderArea() {};
	void setStep4Widget(Step4Widget* p) { step4widget = p; }

protected:
	virtual void paintEvent(QPaintEvent* event);

private:
	friend class Step4Widget;
	Step4Widget* step4widget;
	//VideoConfig* cfg;
	QTimer display_timer;
};