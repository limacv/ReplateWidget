#pragma once
#include <QWidget>
#include <qpushbutton.h>
#include <qtimer.h>
#include <qslider.h>
#include <functional>
#include "MLDataManager.h"

class MLPlayerHandler
{
public:
	MLPlayerHandler(std::function<int()> getframecount, QSlider* slider, QWidget* display, QTimer* timer, QPushButton* button)
		:m_slider(slider), m_display(display), m_timer(timer), m_playbutton(button), m_display_frameidx(0), f_totalframecount{ std::move(getframecount) }
	{
		QIcon icon;
		icon.addFile(QString::fromUtf8(":/ReplateWidget/images/play_button_icon.png"), QSize(), QIcon::Normal, QIcon::Off);
		icon.addFile(QString::fromUtf8(":/ReplateWidget/images/stop_button_icon.png"), QSize(), QIcon::Normal, QIcon::On);
		m_playbutton->setCheckable(true);
		m_playbutton->setIcon(icon);
		m_playbutton->setFlat(true);
		m_playbutton->setChecked(false);
		m_timer->setInterval(33);
		m_timer->stop();

		QObject::connect(m_slider, &QSlider::valueChanged, [&](int idx) {
			update_slider_range();
			m_display_frameidx = idx;
			m_display->update();
			});
		QObject::connect(m_playbutton, &QPushButton::clicked, [&]() {
			update_slider_range();
			if (m_timer->isActive())
			{
				m_timer->stop();
				m_playbutton->setChecked(false);
			}
			else
			{
				m_timer->start();
				m_playbutton->setChecked(true);
			}
			});
		QObject::connect(m_timer, &QTimer::timeout, [&]() {
			update_slider_range();
			int framecount = f_totalframecount();
			if (framecount > 0)
			{
				m_display_frameidx = (m_display_frameidx + 1) % framecount;
				m_slider->setValue(m_display_frameidx);
			}
			});
	}

	void play() const { m_timer->start(); }
	void stop() const { m_timer->stop(); }
	int display_frameidx() const { return m_display_frameidx; }

private:
	void update_slider_range()
	{
		const int& framecount = f_totalframecount();
		if (framecount > 0 && framecount != m_slider->maximum() - 1)
			m_slider->setRange(0, framecount - 1);
	}

private:
	int m_display_frameidx;
	std::function<int()> f_totalframecount;
	QSlider* m_slider;
	QWidget* m_display;
	QTimer* m_timer;
	QPushButton* m_playbutton;
};

