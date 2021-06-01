#include "GObjLayer.h"
#include "MLDataManager.h"
#include "MLUtil.h"

GObjLayer::GObjLayer(QWidget *parent)
    : QWidget(parent)
{
    //initialize("Obj");
}

GObjLayer::GObjLayer(GEffectPtr &effect, const std::string &name, QWidget *parent)
    : QWidget(parent)
{
    initialize(effect, name);
}

GObjLayer::~GObjLayer()
{
    destroy();
}

void GObjLayer::initialize(GEffectPtr &effect, const std::string &name)
{
    resize(1260, 100);
    setMaximumHeight(50);
    const auto& global_data = MLDataManager::get();
    // name label
    name_label_ = new QLabel(name.c_str(), this);
    name_label_->setFixedWidth(12);
    name_label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

    // show checkbox
    show_checkbox_ = new QCheckBox(this);
    show_checkbox_->setChecked(true);
    show_checkbox_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

    // picture
    picture_ = new GPictureLabel(this);
    picture_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    int start_frame = effect->startFrame();
    QRectF rectF = effect->path()->frameRoiRect(start_frame);
    const QPixmap pic = QPixmap::fromImage(MLUtil::mat2qimage(global_data.getRoiofFrame(start_frame, rectF), QImage::Format_ARGB32_Premultiplied));
                //GUtil::mat2qimage(video->getImage(start_frame, rectF)));

    picture_->setPixmap(pic);
    picture_->setStyleSheet("QLabel {border-width: 1px;border-color: slategray;border-style: solid;}");
    picture_->setAutoFillBackground(true);

    timeline_ = new GTimelineEx(this);
    timeline_->init(global_data.get_framecount());
    timeline_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    delete_button_ = new QPushButton(this);
    delete_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QPixmap pixmap(":/ReplateWidget/images/delete_button_icon.png");
    QIcon buttonIcon(pixmap);
    delete_button_->resize(40, 40);
    delete_button_->setFlat(true);
    delete_button_->setIcon(buttonIcon);
    delete_button_->setIconSize(delete_button_->size());
    delete_button_->setAutoFillBackground(true);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(show_checkbox_);
    layout->addWidget(picture_);
    layout->addWidget(timeline_);
    layout->addWidget(delete_button_);

    this->addEffect(effect);

    connect(delete_button_, SIGNAL(clicked()), this, SLOT(releaseWidget()));
    connect(show_checkbox_, SIGNAL(toggled(bool)), this, SLOT(toggleShown(bool)));
}


bool GObjLayer::isChecked() const
{
    return timeline_->isChecked();
}

void GObjLayer::setLabelImage(const QPixmap *pic)
{
    picture_->setPixmap(*pic);
}

void GObjLayer::setNumFrames(int num_frames)
{
    timeline_->init(num_frames);
}

void GObjLayer::addEffect(GEffectPtr &efx)
{
    timeline_->addEffect(efx);
    update();
}

void GObjLayer::write(YAML::Emitter &out) const
{
    if (!timeline_ || timeline_->IntervalCount() == 0) return;

    out << YAML::BeginMap;
    out << YAML::Key << "Name" << YAML::Value << name_label_->text().toStdString();
//    out << YAML::Key << "Frame_id" << YAML::Value << ;
//    out << YAML::Key << "Num_Frame" << YAML::Value << num_frame_;
//    out << YAML::Key << "Img_Rect" << YAML::Value << img_rect_;
    out << YAML::Key << "Timeline" << YAML::Value << timeline_;
    out << YAML::EndMap;
}

//const YAML::Node &operator >>(const YAML::Node &node, GObjLayer *obj)
//{
//    std::string name = node["Name"].as<std::string>();
//    int frame_id = node["Frame_id"].as<int>();
//    int num_frame = node["Num_Frame"].as<int>();
//    QRect rect;
//    node["Img_Rect"] >> rect;

//    obj = new GObjLayer(this);
//    obj->initialize();
//}

QSize GObjLayer::sizeHint() const
{
    return QSize(500, 100);
}

QSize GObjLayer::minimumSizeHint() const
{
    return QSize(300, 100);
}

void GObjLayer::releaseEffectWidget(GEffectPtr &efx)
{
    if (timeline_->releaseEffect(efx)) {
//        qDebug() << "Send signal";
//        effectWidgetReleased(efx);
    }
//    if (timeline_->IntervalCount() == 0)
//        hide();
}

void GObjLayer::releaseAllEffectWidgets()
{
//    qDebug() << "Release all widgets:" << name_label_->text();
//    std::vector<GEffectPtr> efx_erase(timeline_->IntervalCount());
//    for (size_t i = 0; i < efx_erase.size(); ++i)
//        efx_erase[i] = timeline_->intervals(i)->effect();
////        efx_erase[i] = timeline_->getIntervals()[i]->effect();
//    for (size_t i = 0; i < efx_erase.size(); ++i)
//        releaseEffectWidget(efx_erase[i]);
//    efx_erase.clear();
}

void GObjLayer::releaseWidget()
{
    releaseObject(this);
}

void GObjLayer::destroy()
{
//    releaseAllEffectWidgets();
    delete timeline_;
    delete name_label_;
    delete picture_;
    delete show_checkbox_;
    delete delete_button_;
//    delete
//    hide();
}

void GObjLayer::toggleShown(bool shown)
{
    timeline_->toggleShown(shown);
}

GTimelineEx *GObjLayer::timeline() const
{
    return timeline_;
}

void GObjLayer::setTimeline(GTimelineEx *timeline)
{
    timeline_ = timeline;
}

QPushButton *GObjLayer::deleteButton() const
{
    return deleteButton();
}


//void GObjLayer::paintMaskRegion(QPainter &painter, const QImage* flow, bool bDirection)
//{
//    //m_timeline->paintMaskRegion(painter, flow, bDirection);
//}
