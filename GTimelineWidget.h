#ifndef GTIMELINEWIDGET_H
#define GTIMELINEWIDGET_H

#include <QWidget>
#include <QBoxLayout>
#include <vector>
#include <string>

#include "GObjLayer.h"

class Step3Widget;

class GTimelineWidget : public QWidget
{
    Q_OBJECT

public:
    GTimelineWidget(Step3Widget* step3p, QWidget *parent = 0);
    void addTimeline(GEffectPtr &effect, const std::string &name = "");
    void updateDuration();
    void clear()
    {
        std::vector<GObjLayer*> tmp(objects_.begin(), objects_.end());
        for (auto p : tmp)
            deleteTimeline(p);
    }

public slots:
    void changeCurrentEffectTo(GEffectPtr &effect);
    void updateFrameId(int frame_id);
    void deleteTimeline(GObjLayer *obj);

protected:
    void removeEffectFromManager(GObjLayer* obj);

    //GDataManager *data_manager_;
    std::list<GObjLayer*> objects_;
    
    Step3Widget* step3widget;
    QVBoxLayout *main_layout_;
    QButtonGroup *button_group_;
};

#endif // GTIMELINEWIDGET_H
