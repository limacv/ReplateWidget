#include "GTimelineEx.h"
//#include "GDataManager.h"
#include "MLDataManager.h"

YAML::Emitter &operator <<(YAML::Emitter &out, const GTimelineEx *timeline)
{
    out << timeline->intervals_;
    return out;
}

YAML::Emitter &operator <<(YAML::Emitter &out, const std::vector<GInterval *> &intervals)
{
    out << YAML::BeginSeq;
    for (size_t i = 0; i < intervals.size(); ++i)
        out << intervals[i];
    out << YAML::EndSeq;
    return out;
}

YAML::Emitter &operator <<(YAML::Emitter &out, const GInterval *interval)
{
    out << YAML::BeginMap;
    out << YAML::Key << "Type" << YAML::Value << interval->effect()->type();
    out << YAML::Key << "Step" << YAML::Value << interval->step_length_;
    out << YAML::Key << "Effect" << YAML::Value
        << GEffectManager::effect_map_write_tmp[interval->effect_];
    out << YAML::EndMap;
    return out;
}

const YAML::Node& operator >>(const YAML::Node &node, GTimelineEx* timeline)
{
    if (!timeline) {
        qDebug() << "obj->timeline is NULL";
    }
    for (size_t i = 0; i < node.size(); ++i) {
        GEffectPtr efx = GEffectManager::effect_map_read_tmp[node[i]["Effect"].as<int>()];
        G_EFFECT_ID type = (G_EFFECT_ID)(node[i]["Type"].as<int>());
        Q_ASSERT(efx->type() == type);
        timeline->addEffect(efx);
    }
    return node;
}

//const YAML::Node& operator >>(const YAML::Node &node,
//                               std::vector<GInterval*> &intervals)
//{
//    qDebug() << "Interval size" << node.size();
//    intervals.resize(node.size(), 0);
//    for (size_t i = 0; i < intervals.size(); ++i)
//        node[i] >> intervals[i];
//    return node;
//}

//const YAML::Node& operator >>(const YAML::Node &node, GInterval *interval)
//{
//    addEffect(GEffectManager::effect_map_read_tmp[node["Effect"].as<int>()],
//            (G_EFFECT_ID)node["Type"].as<int>());
////    if (!interval) interval = new GInterval((G_EFFECT_ID)node["Type"].as<int>(),
////            node["Step"].as<int>(), GEffectManager::effect_map_read_tmp[node["Effect"].as<int>()]);
////    return node;
//    return node;
//}

GTimelineEx::GTimelineEx(QWidget *parent)
    : QPushButton(parent)
    , select_started_(false)
    , align_started_(false)
    , move_stated_(false)
    , cur_frame_id_ (-1)
    , mouse_select_start_frame_(-1)
    , mouse_select_end_frame_(-1)
{
    //resize()
    setMaximumHeight(50);
    setMinimumHeight(40);

    // set black background
    setStyleSheet("QPushButton { border-width: 1px;border-color: slategray;border-style: solid;"
                  //"background-color: gainsboro;"
                  "background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.5 stop: 0 lightgray, stop: 0.2 gainsboro, stop: 1 lightgray);"
                  "padding: 3px;min-width: 9ex; min-height: 2.5ex;}"
                  "*:checked{border-width: 1px;"
                  //"background-color: lightsteelblue;"
                  //"background-color: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 steelblue, stop: 1 lightsteelblue);"
                  "background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.5 stop: 0 lightsteelblue, stop: 0.2 #b0d0de, stop: 1 lightsteelblue);"
                  //"background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 0.5 stop: 0 #f0f8ff, stop: 0.2 #f0ffff, stop: 1 #f0f8ff);"
                  "border-color: slategray;border-style: solid; padding: 3px;min-width: 9ex; min-height: 2.5ex;}");

    setCheckable(true);
    setChecked(true);
    createActions();
}

GTimelineEx::~GTimelineEx()
{
    release();
}

void GTimelineEx::release()
{
    for (size_t i = 0; i < intervals_.size(); ++i)
        delete intervals_[i];
    intervals_.clear();
}

void GTimelineEx::init(int num_frames)
{
    num_frames_ = num_frames;
}

size_t GTimelineEx::IntervalCount() const
{
    return intervals_.size();
}

bool GTimelineEx::releaseEffect(GEffectPtr &efx)
{
    for (std::vector<GInterval*>::iterator it = intervals_.begin();
         it != intervals_.end(); ++it) {
        if ((*it)->effect() == efx) {
            delete (*it);
            intervals_.erase(it);
            return true;
        }
    }
    return false;
}

//const std::vector<GInterval *> &GTimelineEx::getIntervals() const
//{
//    return m_pIntervals;
//}

GInterval *GTimelineEx::intervals(int i) const
{
    return intervals_[i];
}

QSize GTimelineEx::sizeHint() const
{
    return QSize(100, 100);
}

QSize GTimelineEx::minimumSizeHint() const
{
    return QSize(300, 100);
}

void GTimelineEx::paintEvent(QPaintEvent *event)
{
    QPushButton::paintEvent(event);

    // second
    second_hand_step_length_ = width()/(((qreal)num_frames_)*10);
    qreal scale = 20/second_hand_step_length_; // 20 px is a nearly a base length
    scale = ((scale+2.5)/5)*5; // choose a nearby multiplier of 5;
    second_hand_step_length_ *= scale;

    qreal fiveSecondStepLength = 5 * second_hand_step_length_;
    qreal fifthSecondStepLength = second_hand_step_length_ / 5.0;

    QPainter painter(this);

    QPen FiveSecondColor(Qt::black,1);
    QPen SecondColor(QColor(0, 127, 127),1);
    QPen TenthSecondColor(QColor(50,205,50),1);

    int startOffset = 1;
    painter.setPen(TenthSecondColor);
    for (int i = 0; i * fifthSecondStepLength <= width(); ++i) {
        painter.drawLine(i * fifthSecondStepLength + startOffset, 0,
                         i * fifthSecondStepLength + startOffset, 5);
    }
    painter.setPen(SecondColor);
    for (int i = 0; i * second_hand_step_length_ <= width(); ++i) {
        painter.drawLine(i * second_hand_step_length_ + startOffset, 0,
                         i * second_hand_step_length_ + startOffset, 10);
    }
    painter.setPen(FiveSecondColor);
    for (int i = 0; i * fiveSecondStepLength <= width(); ++i) {
        painter.drawLine(i * fiveSecondStepLength + startOffset, 0,
                         i * fiveSecondStepLength + startOffset, 20);
    }

//    // draw pressed range
//    painter.setBrush(QBrush(QColor(33, 64, 95, 180))); // default
//    //painter.setBrush(QBrush(QColor(77, 166, 146, 140)));
//    //painter.setBrush(QBrush(QColor(47, 79, 79, 140)));
//    //painter.setPen(QPen(Qt::black,1,Qt::SolidLine));
//    painter.setPen(Qt::NoPen);
//    if (mouse_select_start_frame_ != -1 &&
//            mouse_select_end_frame_ != -1) {
////        qDebug() << "mouse_select_start_frame_" << mouse_select_start_frame_
////             << mouse_select_end_frame_;
//        int start_x = convertFrameToPosition(mouse_select_start_frame_);
//        int end_x = convertFrameToPosition(mouse_select_end_frame_);
//        QRect curRect = QRect(start_x, 1 * height()/3, end_x-start_x,
//                              2 * height() / 3 - 2).normalized();
//        painter.drawRoundRect(curRect, 5, 5);
//    }

    // draw sync line
    if ((intervals(0)->effect()->speed() != 1) &&
            (intervals(0)->effect()->syncId() != intervals(0)->effect()->startFrame())) {
        int id = intervals(0)->effect()->syncId();
        int id_x = convertFrameToPosition(id);
        painter.setPen(QPen(Qt::magenta, 3, Qt::SolidLine));
        painter.drawLine(id_x, 0, id_x, height());
    }

    // draw mouse click line
    int mouse_click_line_x = convertFrameToPosition(cur_frame_id_);
    painter.setPen(QPen(Qt::red, 1, Qt::SolidLine));
    painter.drawLine(mouse_click_line_x, 0, mouse_click_line_x, height());

    // draw duration line
    const int duration = MLDataManager::get().plates_cache.replate_duration;
    if (duration > 0) {
        int num = std::ceil(num_frames_/(float)duration);
        for (int i = 0; i < num; ++i) {
            int x = convertFrameToPosition(i * duration);
            if (intervals_.size() > 0)
                x += intervals_[0]->offset() * frameStep();
            painter.setPen(QPen(Qt::blue, 3, Qt::SolidLine));
            painter.drawLine(x, 0, x, height());
        }
    }
}

void GTimelineEx::mousePressEvent(QMouseEvent *event)
{
    if (!isChecked()) {
        setChecked(true);
        toggled(true);
        if (!intervals_.empty())
            onPressEffect(intervals(0)->effect());
        QPushButton::mousePressEvent(event);
    }

//    if (event->button() == Qt::LeftButton) {
//        select_started_ = true;
//        mouse_select_start_frame_ = mouse_select_end_frame_ =
//                getFrameId(event->pos().x());
//        updateFrameChange(mouse_select_end_frame_);
//    }
    else if (event->button() == Qt::LeftButton) {
        align_started_ = true;
        align_start_pos_ = event->pos().x();
    }
    update();
}

void GTimelineEx::mouseMoveEvent(QMouseEvent *event)
{
    QPushButton::mouseMoveEvent(event);
//    if (select_started_) {
//        mouse_select_end_frame_ = getFrameId(event->pos().x());
//        updateFrameChange(mouse_select_end_frame_);
//    }
    if (align_started_) {
        int pos = event->pos().x();
        bool forward = (pos > align_start_pos_);
        int abs_off = abs(pos - align_start_pos_) / frameStep() + 0.5;
        int off = forward ? abs_off: -abs_off;
        align_start_pos_ += off * frameStep();
        // TODO: add duration access
//        intervals_[0]->adjustOffset(off);
    }
    update();
}

void GTimelineEx::mouseReleaseEvent(QMouseEvent *event)
{
    QPushButton::mouseReleaseEvent(event);
//    if (select_started_) {
//        mouse_select_end_frame_ = getFrameId(event->pos().x());
//        select_started_ = false;
//        updateFrameChange(mouse_select_end_frame_);
//    }
    if (align_started_) {
        align_started_ = false;
    }
}

void GTimelineEx::contextMenuEvent(QContextMenuEvent *event)
{
    right_click_pos_ = event->pos();
//    int frame = getFrameId(right_click_pos_.x());
//    updateFrameChange(frame);
    QPushButton::contextMenuEvent(event);
    QMenu menu(this);
//    menu.addAction(act_loop_);
//    menu.addAction(act_blur_);
//    menu.addAction(act_still_);
//    menu.addAction(act_motion_);

//    menu.addAction(act_set_opaque);
//    menu.addAction(act_delete_opaque);
//    menu.addAction(act_add_keyPt_);
//    menu.addAction(act_delete_keyPt_);
    menu.addAction(act_set_syncPt_);
    menu.addSeparator();
//    menu.addAction(act_delete_);
    menu.exec(event->globalPos());
    update();
}

void GTimelineEx::resizeEvent(QResizeEvent *)
{
    qDebug() << "Timeline Resized" << this->size();
//    for (size_t i = 0; i < m_pIntervals.size(); ++i) {
//        QPoint p = m_pIntervals[i]->pos();
//        QSize s = m_pIntervals[i]->size();
//        p.setY(height()/3);
//        s.setHeight(height()*2/3-2);
//        m_pIntervals[i]->move(p);
//        m_pIntervals[i]->resize(s);
//    }

    std::vector<GEffectPtr> efx_list;
    for (size_t i = 0; i < intervals_.size(); ++i) {
        efx_list.push_back(intervals_[i]->effect());
        delete intervals_[i];
    }
    intervals_.clear();
    for (int i = 0; i < efx_list.size(); ++i) {
        addEffect(efx_list[i]);
    }
}

void GTimelineEx::addEffect(GEffectPtr &efx)
{
    if (efx->type() == EFX_ID_STILL || efx->type() == EFX_ID_INPAINT) {
        addIntervalEx(efx, efx->startFrame());
    }
    else {
        int start = efx->startFrame();
        int end = efx->endFrame();
        if (start != end)
            addIntervalEx(efx, start, end);
        else
            qDebug() << "Interval region too small!: " << start << " -- " << end;
    }
}

void GTimelineEx::deleteInterval()
{
    int index = intervalAt(right_click_pos_);
    if (index != -1) {
        GInterval* p = intervals_[index];
        intervals_.erase(intervals_.begin()+index);
        delete p;
    }
    else {
        qDebug() << "Invalid action. No interval selected.";
        for (size_t i = 0; i < intervals_.size(); ++i) {
            qDebug() << "Interval" << i<< intervals_[i]->pos() <<intervals_[i]->size();
            intervals_[i]->show();
        }
    }
    update();
}

void GTimelineEx::setSync()
{
    GInterval *p = NULL;
    int frame_id;
    if (getClickedInterval(p, frame_id))
        p->effect()->setSync(frame_id);
}

//void GTimelineEx::addKeypoint()
//{
//    GInterval *p = NULL;
//    int frame_id;
//    if (getClickedInterval(p, frame_id))
//        p->addKeyPoint(frame_id);
//    update();
//}

//void GTimelineEx::deleteKeypoint()
//{
//    GInterval *p = NULL;
//    int frame_id;
//    if (getClickedInterval(p, frame_id))
//        p->deleteKeyPoint(frame_id);
//    update();
//}

//void GTimelineEx::addOpaque()
//{
//    GInterval *p = NULL;
//    int frame_id;
//    if (getClickedInterval(p, frame_id, false))
//        p->addOpaque(frame_id);
//    update();
//}

//void GTimelineEx::deleteOpaque()
//{
//    GInterval *p = NULL;
//    int frame_id;
//    if (getClickedInterval(p, frame_id, false))
//        p->deleteOpaque(frame_id);
//    update();
//}

void GTimelineEx::createActions()
{
    act_set_syncPt_ = new QAction(tr("Sync Point"), this);
    connect(act_set_syncPt_, SIGNAL(triggered()), this, SLOT(setSync()));

//    act_loop_ = new QAction(tr("Loop"), this);
//    connect(act_loop_, SIGNAL(triggered()), this, SLOT(addLoop()));

//    act_blur_ = new QAction(tr("Blur"), this);
//    connect(act_blur_, SIGNAL(triggered()), this, SLOT(addBlur()));

//    act_still_ = new QAction(tr("Still"), this);
//    connect(act_still_, SIGNAL(triggered()), this, SLOT(addStill()));

//    act_motion_ = new QAction(tr("Motion"), this);
//    connect(act_motion_, SIGNAL(triggered()), this, SLOT(addMotion()));

//    act_add_keyPt_ = new QAction(tr("Add Key"), this);
//    connect(act_add_keyPt_, SIGNAL(triggered()), this, SLOT(addKeypoint()));

//    act_delete_keyPt_ = new QAction(tr("Del Key"), this);
//    connect(act_delete_keyPt_, SIGNAL(triggered()), this, SLOT(deleteKeypoint()));

//    act_set_opaque = new QAction(tr("Set Opaque"), this);
//    connect(act_set_opaque, SIGNAL(triggered()), this, SLOT(addOpaque()));

//    act_delete_opaque = new QAction(tr("Del Opaque"), this);
//    connect(act_delete_opaque, SIGNAL(triggered()), this, SLOT(deleteOpaque()));

//    act_delete_ = new QAction(tr("Delete"), this);
//    connect(act_delete_, SIGNAL(triggered()), this, SLOT(deleteInterval()));
}

int GTimelineEx::getFrameId(int pos_x, bool round)
{
    pos_x = qMin(width(), qMax(pos_x, 0));
    //return (num_frames_-1) * pos_x / (double) width() + 0.5;
    return pos_x / frameStep() + (round ? 0.5 : 0);
}

int GTimelineEx::intervalAt(const QPoint &pos)
{
    for (int i = intervals_.size() - 1; i >= 0; --i) {
        const GInterval *item = intervals_[i];
        if (item->rect().translated(item->pos()).contains(pos))
            return i;
    }
    return -1;
}

bool GTimelineEx::getClickedInterval(GInterval *&in, int &frame_id, bool round)
{
    int index = intervalAt(right_click_pos_);
    if (index != -1) {
        in = intervals_[index];
        frame_id = getFrameId(right_click_pos_.x(), round);
        return true;
    }
    else {
        qDebug() << "Invalid action. No interval selected.";
        for (size_t i = 0; i < intervals_.size(); ++i) {
            qDebug() << "Interval" << i<< intervals_[i]->pos() <<intervals_[i]->size();
            intervals_[i]->show();
        }
    }
    return false;
}

void GTimelineEx::addIntervalWidget(GEffectPtr &efx, int start_pos, int end_pos)
{
//    qDebug() <<"width" << width() << "num" << num_frames_;
    QRect rect = QRect(start_pos, height()/3, end_pos,
                       2 * height() / 3 - 2).normalized();
//    qDebug() << rect << type << frameStep() << efx->effectLength();
    GInterval *in = new GInterval(efx, frameStep(), this);
    in->show();
    in->move(rect.topLeft());
    in->resize(rect.size());
    intervals_.push_back(in);
    connect(in, SIGNAL(onPressInvervalofEffect(GEffectPtr&)), this, SIGNAL(onPressEffect(GEffectPtr&)));
    emit intervalAdded(intervals_.back());
    clearCurrent();
}

void GTimelineEx::addIntervalEx(GEffectPtr &efx, int start_frame, int end_frame)
{
    int start_x = convertFrameToPosition(start_frame);
    int end_x = convertFrameToPosition(end_frame);
    addIntervalWidget(efx, start_x, end_x - start_x);
}

void GTimelineEx::addIntervalEx(GEffectPtr &efx, int start_frame)
{
    int start_x = convertFrameToPosition(start_frame) - 1;
    //int end_x = start_x + 4;
//    qDebug() << "Add Interval Still" << start_x;
    addIntervalWidget(efx, start_x, 4);
}

//void GTimelineEx::addInterval(GInterval::IntervalType type,
//                              int start_frame, int end_frame)
//{
//    int start_x = convertFrameToPosition(start_frame);
//    int end_x = convertFrameToPosition(end_frame);
//    QRect rect = QRect(start_x, height()/3, end_x - start_x, 2 * height() / 3 - 2 ).normalized();
//    GInterval *in = new GInterval(type, frameStep(), this);
//    in->id = m_pIntervals.size();
//    in->show();
//    in->move(rect.topLeft());
//    in->resize(rect.size());
//    m_pIntervals.push_back(in);
//    emit intervalAdded(m_pIntervals.back());
//    clearCurrent();
//}

//void GTimelineEx::addInterval(GInterval::IntervalType type, int start_frame)
//{
//    int start_x = convertFrameToPosition(start_frame);
//    QRect rect = QRect(start_x - 1, height()/3, start_x + 3, 2 * height() / 3 - 2 ).normalized();
//    GInterval *in = new GInterval(type, frameStep(), this);
//    in->show();
//    in->move(rect.topLeft());
//    in->resize(rect.size());
//    m_pIntervals.push_back(in);
//    emit intervalAdded(m_pIntervals.back());
//    clearCurrent();
//}

//void GTimelineEx::addIntervalWidget(GInterval::IntervalType type,
//                                    G_EFX *efx,
//                                    int start_pos, int end_pos)
//{
//    QRect rect = QRect(start_pos, height()/3, end_pos,
//                       2 * height() / 3 - 2 ).normalized();
//    GInterval *in = new GInterval(type, frameStep(), efx, this);
//    //in->id = m_pIntervals.size();
//    in->show();
//    in->move(rect.topLeft());
//    in->resize(rect.size());
//    m_pIntervals.push_back(in);
//    connect(in, SIGNAL(onPressInvervalofEffect(GEffectPtr)), this,
//            SLOT(receiveCurrentEffect(GEffectPtr)));
//    emit intervalAdded(m_pIntervals.back());
//    clearCurrent();
//}

//void GTimelineEx::addIntervalEx(GInterval::IntervalType type,
//                                G_EFX *efx,
//                                int start_frame, int end_frame)
//{
//    int start_x = convertFrameToPosition(start_frame);
//    int end_x = convertFrameToPosition(end_frame);
//    addIntervalWidget(type, efx, start_x, end_x - start_x);
//}

//void GTimelineEx::addIntervalEx(GInterval::IntervalType type,
//                                G_EFX *efx, int start_frame)
//{
//    int start_x = convertFrameToPosition(start_frame) - 1;
//    //int end_x = start_x + 4;
//    qDebug() << "Add still" << start_x;
//    addIntervalWidget(type, efx, start_x, 4);
//}

void GTimelineEx::clearCurrent()
{
    mouse_select_end_frame_ = mouse_select_start_frame_ = -1;
}

bool GTimelineEx::updateFrameChange(int iFrame)
{
    if (iFrame != cur_frame_id_) {
//        qDebug() << "checkFrameChange to" << iFrame << "from" << cur_frame_id_;
        cur_frame_id_ = iFrame;
        frameChanged(iFrame);
        update();
        return true;
    }
    return false;
}

void GTimelineEx::toggleShown(bool checked)
{
    for (int i = 0; i < intervals_.size(); ++i) {
        intervals_[i]->toggleShown(checked);
    }
}

int GTimelineEx::convertFrameToPosition(int iFrame)
{
    if (iFrame == 0) return 1;
    //return width() * iFrame / (num_frames_ - 1);
    return frameStep() * iFrame;
    update();
}

double GTimelineEx::frameStep() const
{
    //qDebug() << "frameStep()" << width() << num_frames_ << width() / (double)(num_frames_ - 1);
    return width() / (double)(num_frames_ - 1);
}
