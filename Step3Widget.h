#pragma once

#include <QWidget>
namespace Ui { class Step3Widget; };

class Step3Widget : public QWidget
{
	Q_OBJECT

public:
	Step3Widget(QWidget *parent = Q_NULLPTR);
	~Step3Widget();
	void initState();

private slots:
	

private:
	Ui::Step3Widget *ui;
};
