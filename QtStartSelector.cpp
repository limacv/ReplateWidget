#include "QtStartSelector.h"
#include <qfiledialog.h>
#include <qfile.h>
#include <qglobal.h>

QtStartSelector::QtStartSelector(QWidget *parent)
	: QDialog(parent),
	isProjFile(true),
	filepath()
{
	ui.setupUi(this);
	connect(ui.pushButtonLoadVideo, SIGNAL(clicked()), this, SLOT(selectVideo()));
	connect(ui.pushButtonLoadProject, SIGNAL(clicked()), this, SLOT(selectProject()));
}

QtStartSelector::~QtStartSelector()
{
}

void QtStartSelector::selectVideo()
{
	filepath = QFileDialog::getOpenFileName(
		this, tr("Open Video"), "D:/MSI_NB/source/data/",
		tr("Videos (*.mp4 *.avi)")
	);
	if (QFile::exists(filepath))
		accept();
	else
	{
		qFatal(qPrintable(QString("The opened file %1 seems not exists").arg(filepath)));
		exit(1);
	}
	isProjFile = false;
}

void QtStartSelector::selectProject()
{
	filepath = QFileDialog::getOpenFileName(
		this, tr("Open Project File"), "D:/MSI_NB/source/data/",
		tr("Project File (*.rprj)")
	);
	if (QFile::exists(filepath))
		accept();
	else
	{
		qFatal(qPrintable(QString("The opened file %1 seems not exists").arg(filepath)));
		exit(1);
	}
	isProjFile = true;
}
