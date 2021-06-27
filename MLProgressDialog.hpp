#pragma once
#include "SphericalSfm/MLProgressObserverBase.h"
#include <qwidget.h>
#include <qprogressdialog.h>
#include "MLUtil.h"

class MLProgressDialog :
    public MLProgressObserverBase
{
public:
    MLProgressDialog(QWidget* parent = Q_NULLPTR)
        :dialog("Processing", "Cancel", 0, 100, parent)
    {
        dialog.setWindowModality(Qt::WindowModal);
        dialog.setAutoClose(false);
    }
    virtual ~MLProgressDialog() { }

    virtual void beginStage(const std::string& name) 
    { 
        dialog.setLabelText(QString::fromStdString(name)); 
        dialog.setWindowModality(Qt::WindowModal);
        dialog.setValue(0);
        dialog.show();
    }
    virtual void setValue(float value) { dialog.setValue(100 * value); }
    virtual bool wasCanceled() 
    { 
        if (dialog.wasCanceled()) throw MLUtil::UserCancelException(); 
        return dialog.wasCanceled();
    }
    
private:
    QProgressDialog dialog;
};

