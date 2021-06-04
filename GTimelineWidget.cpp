#include "GTimelineWidget.h"
#include "MLDataManager.h"
#include "Step3Widget.h"
using namespace std;

GTimelineWidget::GTimelineWidget(Step3Widget* step3p, QWidget *parent)
    : QWidget(parent),
    step3widget(step3p)
{
    setMinimumHeight(200);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    main_layout_ = new QVBoxLayout(this);
    main_layout_->setAlignment(Qt::AlignTop);

    button_group_ = new QButtonGroup(this);
    button_group_->setExclusive(true);
}

void GTimelineWidget::addTimeline(GEffectPtr &effect, const string &name)
{
    GObjLayer *obj = new GObjLayer(effect, name, this);

    button_group_->addButton(obj->timeline());
    objects_.push_back(obj);
    main_layout_->addWidget(obj);
    obj->show();

    obj->timeline()->updateFrameChange(step3widget->cur_frameidx);

    connect(obj->timeline(), SIGNAL(changeCurrentEffect(GEffectPtr&)),
            this, SLOT(changeCurrentEffect(GEffectPtr&)));
    connect(obj, SIGNAL(releaseObject(GObjLayer*)),
            this, SLOT(deleteTimeline(GObjLayer*)));
    update();
}

void GTimelineWidget::updateDuration()
{
    for (std::list<GObjLayer*>::iterator it = objects_.begin();
         it != objects_.end(); ++it)
        (*it)->update();
}

void GTimelineWidget::changeCurrentEffect(GEffectPtr &effect)
{
    step3widget->setCurrentEffect(effect);
    currentEffectChanged();
}

void GTimelineWidget::updateFrameId(int frame_id)
{
    for (std::list<GObjLayer*>::iterator it = objects_.begin();
         it != objects_.end(); ++it)
        (*it)->timeline()->updateFrameChange(frame_id);
}

void GTimelineWidget::deleteTimeline(GObjLayer *obj)
{
    qDebug() << "Delete timeline";
    removeEffectFromManager(obj);
    main_layout_->removeWidget(obj);
    button_group_->removeButton(obj->timeline());
    objects_.remove(obj);
    delete obj;
}

void GTimelineWidget::removeEffectFromManager(GObjLayer *obj)
{
    auto& global_data = MLDataManager::get();
    for (int i = 0; i < obj->timeline()->IntervalCount(); ++i)
        global_data.effect_manager_.popEffect(obj->timeline()->intervals(i)->effect());
}
