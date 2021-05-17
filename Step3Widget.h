#pragma once

#include <QWidget>
namespace Ui { class Step3Widget; };

class MLCachePlatesConfig;
class MLCacheTrajectories;

class Step3Widget : public QWidget
{
	Q_OBJECT

public:
	Step3Widget(QWidget *parent = Q_NULLPTR);
	~Step3Widget();
	void initState();

public slots:
	void onWidgetShowup();

private:
	MLCacheTrajectories* trajp;
	MLCachePlatesConfig* platesp;
	Ui::Step3Widget *ui;
};


// the class for drawing image and rectangles (since QPainter can only used in paintEvent, 
// so have to use an extra class for painting all the things
class Step3RenderArea : public QWidget
{
	Q_OBJECT

public:
	Step3RenderArea(QWidget* parent = Q_NULLPTR)
		:QWidget(parent), step3widget(nullptr)
	{}

	~Step3RenderArea() {};
	void setStep3Widget(Step3Widget* p) { step3widget = p; }

protected:
	virtual void paintEvent(QPaintEvent* event);

private:
	Step3Widget* step3widget;
};