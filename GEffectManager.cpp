#include "GEffectManager.h"
#include <QDebug>
//#include "GDataManager.h"
#include "MLDataManager.h"
#include "InpainterCV.h"
#include "GUtil.h"
#include <fstream>
#include "GPath.h"

//qreal GEffectManager::M_STREAK_LENGTH = 3;
qreal GEffectManager::M_BLEND_ALPHA = 0.5;
int GEffectManager::M_TRAIL_ALPHA = 40;
int GEffectManager::M_TRAIL_STEP = 1;
int GEffectManager::M_LOOP_COUNT = 2;
bool GEffectManager::M_BLUR_ORDER = false;
std::map<GEffectPtr, int> GEffectManager::effect_map_write_tmp;
std::vector<GEffectPtr> GEffectManager::effect_map_read_tmp;


GEffectManager::GEffectManager()
    : show_text(true)
    //    , delete_buffer_(0)
{
    //    m_effects.assign(EFX_NUMBER, std::vector<G_EFX*>());
}

void GEffectManager::applyTrash(const GPathPtr &path)
{
    QRectF rectF = path->frameRoiRect(path->startFrame());
    //video->inpaintBackground(rectF, path->painter_path_);
    qDebug("TODO::applyIntraining is not implemented");
    // TODO: add changebackground
}

void GEffectManager::applyInpaint(const GPathPtr& path)
{
    const auto& data = MLDataManager::get();
    QRectF rectF = path->frameRoiRect(path->startFrame());
    cv::Rect rect = data.toCropROI(rectF);
    cv::Mat mask;
    if (!path->painter_path_.isEmpty())
    {
        QPainterPath painter_path = data.imageScale().map(path->painter_path_);
        mask = GUtil::cvtPainterPath2Mask(painter_path);
        rect.width = mask.cols;
        rect.height = mask.rows;
    }
    else
        mask = cv::Mat(rect.size(), CV_8UC1, 255);
    
    // add context
    InpainterCV inpainter(10);
    cv::Rect dilate_rect = GUtil::addMarginToRect(rect, 10);
    cv::Rect roi = dilate_rect & cv::Rect(0, 0, data.VideoWidth(), data.VideoHeight());
    cv::Mat context = data.getRoiofBackground(roi);
    cv::Mat context_mask(context.size(), CV_8UC1, cv::Scalar(0));
    //cv::Mat context_mask;  cv::extractChannel(context, context_mask, 3);
    //cv::Mat context = data.getRoiofFrame(path->startFrame(), roi);
    //cv::Mat context_mask;  cv::extractChannel(context, context_mask, 3);
    //cv::bitwise_not(context_mask, context_mask);
    mask.copyTo(context_mask(rect - roi.tl()));

    cv::Mat inpainted;
    inpainter.inpaint(context, context_mask, inpainted);
    cv::merge(std::vector<cv::Mat>({ inpainted(rect - roi.tl()), mask }), inpainted);

    path->roi_fg_mat_[0] = inpainted.clone();
    path->roi_fg_qimg_[0] = GUtil::mat2qimage(path->roi_fg_mat_[0],
        QImage::Format_ARGB32_Premultiplied).copy();
}

void GEffectManager::applyStill(const GPathPtr &path)
{
//    qDebug() << path->size() << path->painter_path_;
    QRectF rectF = path->frameRoiRect(path->startFrame());
    cv::Mat4b fg = MLDataManager::get().getRoiofFrame(path->startFrame(), rectF);
    path->roi_fg_mat_[0] = fg.clone();
    path->roi_fg_qimg_[0] = GUtil::mat2qimage(path->roi_fg_mat_[0],
            QImage::Format_ARGB32_Premultiplied).copy();
}

void GEffectManager::applyBlack(const GPathPtr &path)
{
    const auto& global_data = MLDataManager::get();
    QRectF rectF = path->frameRoiRect(path->startFrame());
    const QMatrix mat = global_data.imageScale();

    QRect rect = mat.mapRect(rectF).toRect();
    QPainterPath painterpath = mat.map(path->painter_path_);
    cv::Mat1b mask = GUtil::cvtPainterPath2Mask(painterpath);
    cv::Mat4b black(GUtil::cvtSize(rect.size()), cv::Vec4b(0,0,0,255));
    path->roi_fg_mat_[0].create(mask.size(), CV_8UC4);
    path->roi_fg_mat_[0] = cv::Vec4b(0,0,0,0);
    black.copyTo(path->roi_fg_mat_[0], mask);
    path->roi_fg_qimg_[0] = GUtil::mat2qimage(path->roi_fg_mat_[0],
        QImage::Format_ARGB32_Premultiplied).copy();
}

void GEffectManager::refreshAllPathImage()
{
    for (auto it = prioritied_effects_.begin(); it != prioritied_effects_.end(); ++it)
    {
        for (auto iit = it->second.begin(); iit != it->second.end(); ++iit)
        {
            auto& path = (*iit)->path();
            path->forceUpdateImages();
        }
    }
}


GEffectPtr GEffectManager::addEffect(const GPathPtr &path, G_EFFECT_ID type)
{
    if (!path) return NULL;
    GEffectPtr efx = createEffect(path, type);

    if (type == EFX_ID_STILL)
        applyStill(path);
    else if (type == EFX_ID_TRASH)
        applyTrash(path);
    else if (type == EFX_ID_BLACK)
        applyBlack(path);
    else if (type == EFX_ID_INPAINT)
        applyInpaint(path);
    else
        qWarning("GEffectManager::addEffect: unrecognized effect type");

    pushEffect(efx);
    return efx;
}

GEffectPtr GEffectManager::addPathEffect(GPathPtr &path, G_EFFECT_ID type, const YAML::Node &node)
{
    if (!path) return NULL;

    if (type == EFX_ID_MOTION || type == EFX_ID_TRAIL) {
        path->updateimages();
    }

    GEffectPtr efx = createEffect(path, type);
    efx->read(node);

    if (type == EFX_ID_STILL)
        applyStill(path);
    else if (type == EFX_ID_TRASH)
        applyTrash(path);
    else if (type == EFX_ID_BLACK)
        applyBlack(path);
    else if (type == EFX_ID_INPAINT)
        applyInpaint(path);

    pushEffect(efx);
    return efx;
}

void GEffectManager::reset()
{
    prioritied_effects_.clear();
    recent_effects_.clear();
}

bool GEffectManager::undo()
{
    if (!recent_effects_.empty()) {
        qDebug() << "Undo" << G_EFFECT_STR[recent_effects_.back()->type()];
        popEffect(recent_effects_.back());
    }
    return false;
}

void GEffectManager::read(const QString& file)
{
    YAML::Node doc;
    try
    {
        doc = YAML::LoadFile(file.toStdString());
    }
    catch (const YAML::ParserException& e)
    {
        qWarning() << e.what();
    }
    catch (const YAML::BadFile& e)
    {
        qDebug() << "cannot find file" << file;
    }
    YAML::Node path_nodes = doc["Path"];
    std::vector<GPathPtr> paths(path_nodes.size());
    for (size_t i = 0; i < path_nodes.size(); ++i) {
        qDebug() << "Load path" << i;
        GPathPtr p(new GPath);
        path_nodes[i][i] >> p;
        paths[i] = p;
    }

    for (size_t i = 0; i < G_EFX_NUMBER; ++i) {
        YAML::Node efx_node = doc["Effect"][G_EFFECT_STR[i]];
//        qDebug() << "Load effect" << G_EFFECT_STR[i];
        for (YAML::const_iterator it = efx_node.begin(); it != efx_node.end(); ++it) {
//            if ((G_EFFECT_ID)i != EFX_ID_TRASH || (G_EFFECT_ID)i != EFX_ID_STILL) continue;
            int path_id = (*it)["PathId"].as<int>();
            GPathPtr path = paths[path_id];
            GEffectPtr efx = addPathEffect(path, (G_EFFECT_ID)i, (*it)["Property"]);
            effect_map_read_tmp.push_back(efx);
        }
    }
}

void GEffectManager::readOld(const QString& file)
{
    YAML::Node doc = YAML::LoadFile(file.toStdString());
    YAML::Node path_nodes = doc["Path"];
    std::vector<GPathPtr> paths(path_nodes.size());
    for (size_t i = 0; i < path_nodes.size(); ++i) {
        qDebug() << "Load path" << i;
        GPathPtr p(new GPath);
//        path_nodes[i][i] >> p;
        loadOldPathData(path_nodes[i][i], p);
        paths[i] = p;
    }

    for (size_t i = 0; i < G_EFX_NUMBER; ++i) {
        YAML::Node efx_node = doc["Effect"][G_EFFECT_STR[i]];
//        qDebug() << "Load effect" << G_EFFECT_STR[i];
        for (YAML::const_iterator it = efx_node.begin(); it != efx_node.end(); ++it) {
//            if ((G_EFFECT_ID)i != EFX_ID_TRASH || (G_EFFECT_ID)i != EFX_ID_STILL) continue;
            int path_id = (*it)["PathId"].as<int>();
            GPathPtr path = paths[path_id];
            G_EFFECT_ID id = (G_EFFECT_ID)i;
            if (id == EFX_ID_MULTIPLE) id = EFX_ID_MOTION;
            GEffectPtr efx = addPathEffect(path, id, (*it)["Property"]);
            effect_map_read_tmp.push_back(efx);
        }
    }
}

void GEffectManager::write(const QString& file)
{
    YAML::Emitter out;
    out << YAML::BeginMap;
    // write effects first
    out << YAML::Key << "Effect" << YAML::Value;
    std::vector<std::vector<GEffectPtr>> effects(G_EFX_NUMBER, std::vector<GEffectPtr>());
    //    for (OrderedEffectMap::const_iterator it = ordered_effects_.begin();
    //         it != ordered_effects_.end(); ++it) {
    //        effects[it->second->type()].push_back(it->second);
    //    }
    for (PriorityEffectMap::iterator it = prioritied_effects_.begin();
         it != prioritied_effects_.end(); ++it) {
        std::list<GEffectPtr> &list = it->second;
        for (std::list<GEffectPtr>::iterator iter = list.begin();
             iter != list.end(); ++iter) {
            effects[(*iter)->type()].push_back(*iter);
        }
    }

    out << YAML::BeginMap; // effect map
    std::map<GPathPtr, int> path2id;
    std::vector<GPathPtr> id2path;
    for (size_t i = 0; i < G_EFX_NUMBER; ++i) {
        out << YAML::Key << G_EFFECT_STR[i] << YAML::Value;
        out << YAML::BeginSeq;
        for (size_t j = 0; j < effects[i].size(); ++j) {
            out << YAML::BeginMap;
            const GEffectPtr &efx = effects[i][j];
            int pathid;
            if (path2id.count(efx->path())) pathid = path2id[efx->path()];
            else {
                pathid = id2path.size();
                id2path.push_back(efx->path());
                path2id.insert(std::make_pair(efx->path(), pathid));
            }

            out << YAML::Key << "PathId" << YAML::Value << pathid;
            out << YAML::Key << "Property" << YAML::Value << efx;
            //            out << efx;
            effect_map_write_tmp[efx] = effect_map_write_tmp.size();
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
    }
    out << YAML::EndMap; // effect map

    // write paths
    out << YAML::Key << "Path" << YAML::Value;
    out << YAML::BeginSeq;
    for (int i = 0; i < id2path.size(); ++i) {
        out << YAML::BeginMap;
        out << YAML::Key << i << YAML::Value << id2path[i];
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::EndMap;
    std::ofstream outfile(file.toStdString());
    if (!outfile.is_open())
    {
        qWarning("Failed to save to file");
        return;
    }
    outfile << out.c_str();
}

void loadPainterPath(const YAML::Node &node, QPainterPath &painter_path)
{
    if (!node || node.size() == 0) return;

    int x, y;
    x = node[0].as<int>();
    y = node[1].as<int>();
    painter_path = QPainterPath(QPoint(x, y));
    int n = node.size() / 2 - 1;
    for (int i = 0; i < n; ++i) {
        x = node[2 * i + 2].as<int>();
        y = node[2 * i + 3].as<int>();
        painter_path.lineTo(x, y);
    }
}

void GEffectManager::loadOldPathData(const YAML::Node &node, GPathPtr &path)
{
    int size = node["Length"].as<int>();
    path->resize(size);
    path->setBackward(node["Backward"].as<bool>());
    path->setStartFrame( node["Start_id"].as<int>() );
    QSize window_size;
    node["Wnd_size"] >> window_size;
    QMatrix mat;
    mat.scale(1.0/(qreal)window_size.width(), 1.0/(qreal)window_size.height());

    std::vector<QRect> tmpRect(size);
    node["Roi_Rects"] >> tmpRect;
    std::vector<QRectF> rectF(size);
    for (int i = 0; i < tmpRect.size(); ++i) {
        rectF[i] = mat.mapRect(QRectF(tmpRect[i]));
    }
    path->roi_rect_.assign(rectF.begin(), rectF.end());
    QPainterPath painterpath;
    loadPainterPath(node["Painter_Path"], painterpath);
    path->painter_path_ = mat.map(painterpath);
    node["Manual"] >> path->manual_adjust_;
}

YAML::Emitter& operator <<(YAML::Emitter &out, const GPathPtr &path)
{
    out << YAML::BeginMap;
    out << YAML::Key << "Space" << YAML::Value << path->space();
//    out << YAML::Key << "Type" << YAML::Value << path->type_;
    out << YAML::Key << "Backward" << YAML::Value << path->isBackward();
    out << YAML::Key << "Start_id" << YAML::Value << path->startFrame();
    out << YAML::Key << "End_id" << YAML::Value << path->endFrame();
    out << YAML::Key << "World_offset" << YAML::Value << path->world_offset_;
//    out << YAML::Key << "Wnd_size" << YAML::Value << path->window_size_;
    out << YAML::Key << "Roi_Rects"  << YAML::Value << path->roi_rect_;
    //    out << YAML::Key << "BB_Rects" << YAML::Value << path->window_boundbox_rect_;
    out << YAML::Key << "Painter_Path" << YAML::Value << path->painter_path_;
    out << YAML::Key << "Manual" << YAML::Value << path->manual_adjust_;
    out << YAML::Key << "Reshape" << YAML::Value << path->roi_reshape_;
    out << YAML::EndMap;
    return out;
}

void operator >> (const YAML::Node &node, GPathPtr &path)
{
    int size = node["Space"].as<int>();
    path->resize(size);
    path->setBackward(node["Backward"].as<bool>());
    path->setStartFrame( node["Start_id"].as<int>() );
    path->setEndFrame(node["End_id"].as<int>());
    path->world_offset_ = node["World_offset"].as<int>();
    
    node["Roi_Rects"] >> path->roi_rect_;
    node["Painter_Path"] >> path->painter_path_;
    node["Manual"] >> path->manual_adjust_;
    node["Reshape"] >> path->roi_reshape_;
}

YAML::Emitter &operator <<(
        YAML::Emitter &out, const QPainterPath &painter_path)
{
    out << YAML::Flow;
    out << YAML::BeginSeq;
    for (int i = 0; i < painter_path.elementCount(); ++i) {
        QPainterPath::Element p = painter_path.elementAt(i);
        out << p.x << p.y;
    }
    out << YAML::EndSeq;
    return out;
}

void operator >>(const YAML::Node &node, QPainterPath &painter_path)
{
    if (!node || node.size() == 0) return;

    qreal x, y;
    x = node[0].as<qreal>();
    y = node[1].as<qreal>();
    painter_path = QPainterPath(QPointF(x, y));
    int n = node.size() / 2 - 1;
    for (int i = 0; i < n; ++i) {
        x = node[2 * i + 2].as<qreal>();
        y = node[2 * i + 3].as<qreal>();
        painter_path.lineTo(x, y);
    }
}

YAML::Emitter& operator <<(YAML::Emitter &out,
                           const std::vector<GRoiPtr> &rois)
{
    out << YAML::BeginSeq;
    for (int i = 0; i < rois.size(); ++i)
        out << rois[i];
    out << YAML::EndSeq;
    return out;
}

void operator >>(const YAML::Node &node, std::vector<GRoiPtr> &rois)
{
    for (int i = 0; i < rois.size(); ++i)
        node[i] >> rois[i];
}


YAML::Emitter& operator << (YAML::Emitter& out, const std::vector<QRect> &rects)
{
    out << YAML::BeginSeq;
    for (size_t i = 0; i < rects.size(); ++i)
        out << rects[i];
    out << YAML::EndSeq;
    return out;
}

void operator >> (const YAML::Node& node, std::vector<QRect> &rects)
{
    //rects.resize(out.size());
    for (size_t i = 0; i < rects.size(); ++i)
        node[i] >> rects[i];
}

YAML::Emitter& operator << (YAML::Emitter& out, const std::vector<QRectF> &rects)
{
    out << YAML::BeginSeq;
    for (size_t i = 0; i < rects.size(); ++i)
        out << rects[i];
    out << YAML::EndSeq;
    return out;
}

void operator >> (const YAML::Node& node, std::vector<QRectF> &rects)
{
    if (rects.size() == 0) rects.resize(node.size());
    for (size_t i = 0; i < rects.size(); ++i)
        node[i] >> rects[i];
}


YAML::Emitter& operator << (YAML::Emitter& out, const std::vector<bool> &arr)
{
    out << YAML::Flow;
    out << YAML::BeginSeq;
    for (size_t i = 0; i < arr.size(); ++i)
        out << arr[i];
    out << YAML::EndSeq;
    return out;
}

void operator >> (const YAML::Node& node, std::vector<bool> &arr)
{
    arr.resize(node.size());
    for (size_t i = 0; i < arr.size(); ++i)
        arr[i] = node[i].as<bool>();
}

YAML::Emitter &operator <<(YAML::Emitter &out, const GRoiPtr &roi)
{
    out << YAML::BeginMap;
    if (roi) {
        if (roi->isRect())
            out << YAML::Key << "Rect" << roi->rectF_;
        else
            out << YAML::Key << "PPath" << roi->painter_path_;
    }
    out << YAML::EndMap;
    return out;
}

void operator >>(const YAML::Node &node, GRoiPtr &roi)
{
    if (node["Rect"]) {
        roi = GRoiPtr(new GRoi());
        node["Rect"] >> roi->rectF_;
    }
    else if (node["PPath"]) {
        roi = GRoiPtr(new GRoi());
        node["PPath"] >> roi->painter_path_;
    }
    else {
        roi = 0;
    }
}

YAML::Emitter& operator << (YAML::Emitter& out, const QRect &rect)
{
    out << YAML::Flow;
    out << YAML::BeginSeq;
    out << rect.x() << rect.y() << rect.width() << rect.height();
    out << YAML::EndSeq;
    return out;
}

void operator >>(const YAML::Node &node, QRect &rect)
{
    rect = QRect(node[0].as<int>(), node[1].as<int>(),
            node[2].as<int>(), node[3].as<int>());
}

YAML::Emitter& operator << (YAML::Emitter& out, const QRectF &rectF)
{
    out << YAML::Flow;
    out << YAML::BeginSeq;
    out << rectF.x() << rectF.y() << rectF.width() << rectF.height();
    out << YAML::EndSeq;
    return out;
}

void operator >>(const YAML::Node &node, QRectF &rectF)
{
    rectF = QRectF(node[0].as<qreal>(), node[1].as<qreal>(),
            node[2].as<qreal>(), node[3].as<qreal>());
}


YAML::Emitter& operator << (YAML::Emitter& out, const QSize &size)
{
    out << YAML::Flow;
    out << YAML::BeginSeq << size.width() << size.height() << YAML::EndSeq;
    return out;
}

void operator >> (const YAML::Node &node, QSize &size)
{
    size = QSize(node[0].as<int>(), node[1].as<int>());
}

//void GEffectManager::restoreForegroundImage(GPathPtr &path, GVideoContent *video_)
//{
//    qDebug() << path->length();
//    for (size_t i = 0; i < path->length(); ++i) {
//        qDebug() << path->roi_rect_[i];
//        int iFrame = i + path->startFrame();
//        path->roi_fg_mat_[i] = video_->getForeground(iFrame, path->roi_rect_[i]);
//    }
//}

//void GEffectManager::blendEffects(cv::Mat3b &background, cv::Mat1b &background_mask,
//                                  cv::Mat1b &background_mask2, int frame_id, GVideoContent *video) const
//{
////    std::vector<std::list<GEffectPtr>> aa;
////    for (PriorityEffectMap::const_iterator it = prioritied_effects_.begin();
////         it != prioritied_effects_.end(); ++it) {
////        const std::list<GEffectPtr> &t = it->second;
////        aa.push_back(t);
////    }
////    for (PriorityEffectMap::const_iterator it = prioritied_effects_.begin();
////         it != prioritied_effects_.end(); ++it) {
////        const std::list<GEffectPtr> &list = it->second;
////        for (std::list<GEffectPtr>::const_iterator iter = list.begin();
////             iter != list.end(); ++iter) {
////            const GEffectPtr &effect = (*iter);
////            effect->blend(background, background_mask, background_mask2, frame_id, GUtil::getAnchor_length(),
////                          window_image_scale_, video);
////        }
////    }
//    for (PriorityEffectMap::const_reverse_iterator it = prioritied_effects_.rbegin();
//         it != prioritied_effects_.rend(); ++it) {
//        const std::list<GEffectPtr> &list = it->second;
//        for (std::list<GEffectPtr>::const_iterator iter = list.begin();
//             iter != list.end(); ++iter) {
//            const GEffectPtr &effect = (*iter);
//            effect->blend(background, background_mask, background_mask2, frame_id, GUtil::getAnchor_length(),
//                          window_image_scale_, video);
//        }
//    }
//}

void GEffectManager::preDrawEffects(QPainter &painter, int frame_id) const
{
    for (auto it = prioritied_effects_.cbegin();
         it != prioritied_effects_.end(); ++it) {
        const std::list<GEffectPtr> &list = it->second;
        for (auto iter = list.cbegin();
             iter != list.end(); ++iter) {
            (*iter)->preRender(painter, frame_id, MLDataManager::get().plates_cache.replate_duration);
        }
    }
}

void GEffectManager::drawEffects(QPainter &painter, int frame_id) const
{
    GEffect::setScalar(painter.viewport().size());
    preDrawEffects(painter, frame_id);

    // TODO:
    for (PriorityEffectMap::const_iterator it = prioritied_effects_.begin();
         it != prioritied_effects_.end(); ++it) {
        const std::list<GEffectPtr> &list = it->second;
        for (std::list<GEffectPtr>::const_iterator iter = list.begin();
             iter != list.end(); ++iter) {
            const GEffectPtr &effect = (*iter);
            if (effect->shown() == false)
                continue;
            effect->render(painter, frame_id, MLDataManager::get().plates_cache.replate_duration);
        }
    }
}

void GEffectManager::renderEffects(QPainter &painter, int frame_id) const
{
    painter.drawImage(painter.viewport(), MLDataManager::get().getBackgroundQImg());

    GEffect::setScalar(painter.viewport().size());

    for (PriorityEffectMap::const_iterator it = prioritied_effects_.begin();
         it != prioritied_effects_.end(); ++it) {
        const std::list<GEffectPtr> &list = it->second;
        for (std::list<GEffectPtr>::const_iterator iter = list.begin();
             iter != list.end(); ++iter) {
            const GEffectPtr &effect = (*iter);
            effect->render(painter, frame_id, MLDataManager::get().plates_cache.replate_duration);
        }
    }
}


void GEffectManager::printDrawingOrder() const
{
    int cnt = 0;
    for (PriorityEffectMap::const_iterator it = prioritied_effects_.begin();
         it != prioritied_effects_.end(); ++it) {
        const std::list<GEffectPtr> &list = it->second;
        for (std::list<GEffectPtr>::const_iterator iter = list.begin();
             iter != list.end(); ++iter) {
            const GEffectPtr &effect = (*iter);
            qDebug() << cnt++ << ":" << effect->priority() << G_EFFECT_STR[effect->type()]
                     << effect->startFrame();
        }
    }
    //    int cnt = 0;
    //    for (OrderedEffectMap::const_iterator it = ordered_effects_.begin();
    //         it != ordered_effects_.end(); ++it) {
    //        const GEffectPtr &effect = it->second;
    //        qDebug() << cnt++ << ":" << it->first << effect->priority() << G_EFFECT_STR[effect->type()];
    //    }
    //    int cnt = 0;
    //    for (DrawOrderMap::const_iterator it = draw_order_map_.begin();
    //         it != draw_order_map_.end(); ++it) {
    //        const G_EFX *effect = it->second;
    //        qDebug() << cnt++ << ":" << it->first << effect->priority() << G_EFFECT_STR[effect->type()];
    //    }
}

//void GEffectManager::drawTrash(QPainter &painter) const
//{
//    for (int i = 0; i < m_trashes.size(); ++i) {
//        m_trashes[i]->draw(painter, 0);
//    }
//}

//void GEffectManager::drawStills(QPainter &painter) const
//{
//    for (int i = 0; i < m_stills.size(); +i)
//        m_stills[i]->draw(painter, 0);
//}

//void GEffectManager::drawMultiStills(QPainter &painter) const
//{
//    for (int i = 0; i < m_mulStills.size(); ++i) {
//        const G_EFX_Multiple *ms = m_mulStills[i];
//        painter.setPen(QPen(QColor(23, 224, 135, 100), 5, Qt::DotLine));
//        painter.drawPolyline(ms->trail.data(), ms->size());
//    }
//}

//void GEffectManager::drawBlurStreaks(QPainter &painter) const
//{
//    for (int i = 0; i < m_blurstreaks.size(); ++i)
//    {
//        G_EFX_BlurStreak *blst = m_blurstreaks[i];
//        int efxAlpha = blst->d_alpha * 255;
//        //int b = a/3; // small
//        QImage alpha (blst->image(0).size(), QImage::Format_Indexed8);
//        //alpha.fill(a);
//        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

//        size_t dst_size = qMin((int)blst->size(), blst->d_maxlen);

//        // weight by motion vector
//        std::vector<QPointF> path_center(dst_size);
//        for (int j = 0; j < dst_size; ++j)
//            path_center[j] = blst->location(j).center();
//        std::vector<QPointF> smooth_path = GUtil::smooth(path_center);
//        std::vector<float> lens;
//        double sum = 0;
//        for (int j = 0; j < dst_size; ++j) {
//            size_t curIdx = (blst->d_accending? j : dst_size - 1 - j);
//            size_t nextIdx = qMin(curIdx + 1, dst_size - 1);
//            QPointF st = smooth_path[curIdx];
//            QPointF off = smooth_path[nextIdx] - st;
//            lens.push_back(off.manhattanLength());
//            sum += lens.back();
//            //qDebug() << ">>len" << j<< ":" << lens.back();
//        }
//        double avg = sum / (double)lens.size();
//        for (int j = 0; j < lens.size(); ++j) {
//            lens[j] /= avg;
//            lens[j] *= lens[j];
//            //qDebug() << ">>len" << j<< ":" << lens[j];
//        }

//        for (int j = 0; j < dst_size; ++j) {
//            size_t curIdx = (blst->d_accending? j : dst_size - 1 - j);
//            //int l = curIdx - (blst->size() - 1 - dstSize);
//            int alp = qMin((int)(efxAlpha * lens[j]), 255);
//            //qDebug() << alp << curIdx << a << b << maxSize << blst->size() << lens[j];
//            alpha.fill(alp); // TODO: change the operation to precalculated
//            QImage img = blst->image(curIdx);
//            //img.save(QString("%1.png").arg(j));
//            img.setAlphaChannel(alpha);
//            //img.save(QString("%1_.png").arg(j));
//            painter.drawImage(blst->location(curIdx), img);
////            if ( m_bDrawDir )
////            {
////                QPoint st = blst->location(curIdx).center();
////                QPoint of = blst->location(curIdx+1).center() - st;
////                //qDebug() << "streak" << i << j << img << of << GeOptPath::M_STREAK_LENGTH;
////                painter.setPen(QPen(QBrush(Qt::yellow), 2, Qt::SolidLine));
////                //painter.drawLine(st, st + of * GeOptPath::M_STREAK_LENGTH);
////                painter.drawLine(st, st + of * GEffectManager::M_STREAK_LENGTH);
////            }
//        }
//        //drawTextBalloon(painter, blst->location(0), blst->efx_id_);
//    }
//}

//void GEffectManager::drawMotions(QPainter &painter, int frame_id) const
//{
//    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
//    for (int i = 0; i < m_motions.size(); ++i) {
//        G_EFX_Motion *mt = m_motions[i];
//        int anchor_length= anchor_length_;
//        if (anchor_length <= 0) anchor_length = mt->size();

//        int start_frame = mt->startFrameId() % anchor_length;
//        if (frame_id < start_frame) continue;
//        int idx = ((frame_id - start_frame) / mt->slow_motion_) % anchor_length;
//        if (idx < mt->size()) {
//            QImage dst(mt->image(idx));
//            // fade in & out
//            if (mt->fade_in_out_){
//                QImage alpha(mt->image(idx).size(), QImage::Format_Indexed8);
//                int fade = getFadeAlpha(anchor_length, idx);
//                alpha.fill(fade);
//                //qDebug() << fade << anchor_length << idx;
//                dst.setAlphaChannel(alpha);
//            }
//            painter.drawImage(mt->location(idx), dst);
////            drawTextBalloon(painter, mt->trail_[idx].toPoint(), mt->efx_id_);
//        }
//    }
//}

//void GEffectManager::drawLoops(QPainter &painter, int frame_id) const
//{
//    // render loops
//    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
//    //std::vector<G_EFX_Loop*> &loops = m_manager->m_loops;
//    for (int i = 0; i < m_loops.size(); ++i)
//    {
//        const G_EFX_Loop *lp = m_loops[i];
//        int start_frame = lp->startFrameId() % anchor_length_;
//        if (frame_id < start_frame)
//            continue;
//        size_t idx = (frame_id - lp->startFrameId()) % anchor_length_;
//        for (int j = idx; j < lp->size(); j += anchor_length_)
//        {
//            painter.drawImage(lp->location(j), lp->image(j));
////            drawTextBalloon(painter, lp->trail_[j].toPoint(), lp->efx_id_);
//        }
//    }
//}

//void GEffectManager::drawBubbles(QPainter &painter) const
//{
//    if (show_text) {
//        painter.setPen(Qt::cyan);
//        for(int i = 0; i < m_bubbles.size(); ++i) {
//            QString tx(QString::number(i));
//            painter.drawRect(QRect(m_bubbles[i], QSize(15, 15)).translated(
//                                 -8 + 3 * tx.size(), -12));
//            painter.drawText(m_bubbles[i], tx);
//        }
//    }
//}

//int GEffectManager::getFadeAlpha(int length, int i) const
//{
//    int fade = length / 5;
//    if (i < fade)
//        return getFadeInAlpha(length, i);
//    else if (i > length - fade - 1)
//        return getFadeOutAlpha(length, i);
//    else return 255;
//}

//int GEffectManager::getFadeInAlpha(int length, int i) const
//{
//    int fade = length / 5;
//    return qMin((255 / (fade+1)) * (i+1), 255);
//}

//int GEffectManager::getFadeOutAlpha(int length, int i) const
//{
//    return getFadeInAlpha(length, length-i-1);
//}

//G_EFX_Loop *GEffectManager::getFirstLoop()
//{
//    // TODO
////    if (m_effects[EFX_ID_LOOP].empty())
////        return NULL;
////    return (G_EFX_Loop *)m_effects[EFX_ID_LOOP][0];
//    return 0;
//}

//void GEffectManager::addEffects(G_EFX *efx)
//{
//    GEffectPtr efx_ptr(efx);
//    addEffects(efx_ptr);
//}

//void GEffectManager::eraseEffect(G_EFX *efx)
//{
//    GEffectPtr efx_ptr(efx);
//    eraseEffect(efx_ptr);
//}

void GEffectManager::pushEffect(const GEffectPtr &efx)
{
    qDebug() << "Manager add" << G_EFFECT_STR[efx->type()];
    //    ordered_effects_.insert(std::make_pair(efx->priority(), efx));
    if (prioritied_effects_.count(efx->priority())) {
        prioritied_effects_[efx->priority()].push_back(efx);
    }
    else
        prioritied_effects_[efx->priority()] = std::list<GEffectPtr>(1, efx);
    recent_effects_.push_back(efx);
}

bool GEffectManager::popEffect(const GEffectPtr &efx)
{
    bool res = false;
    qDebug() << "Erase " << G_EFFECT_STR[efx->type()] << efx.use_count() << efx->priority();

    // erase from effect map
    std::list<GEffectPtr> &list = prioritied_effects_[efx->priority()];
    for (std::list<GEffectPtr>::iterator it = list.begin(); it != list.end(); ++it) {
        qDebug() << (*it).get() << efx.get();
        if (*it == efx) {
            list.erase(it);
            if (list.empty())
                prioritied_effects_.erase(efx->priority());
            qDebug() << "Erase Success!" << G_EFFECT_STR[efx->type()] << efx.use_count() << efx->priority();
            res = true;
            break;
        }
    }

    // erase from recent items
    for (std::vector<GEffectPtr>::iterator iter = recent_effects_.begin();
         iter < recent_effects_.end(); ++iter) {
        if (*iter == efx) {
            recent_effects_.erase(iter);
            break;
        }
    }
    return res;
}

GEffectPtr GEffectManager::createEffect(const GPathPtr &path, G_EFFECT_ID type)
{
    if (!path) return NULL;

    GEffectPtr efx;
    switch (type) {
    case EFX_ID_MOTION:
        efx = GEffectPtr(new GEffectMotion(path));
        break;
    case EFX_ID_TRAIL:
        efx = GEffectPtr(new GEffectTrail(path));
        break;
    case EFX_ID_STILL:
        efx = GEffectPtr(new GEffectStill(path));
        break;
    case EFX_ID_TRASH:
        efx = GEffectPtr(new GEffectTrash(path));
        break;
    case EFX_ID_BLACK:
        efx = GEffectPtr(new GEffectBlack(path));
        break;
    case EFX_ID_INPAINT:
        efx = GEffectPtr(new GEffectInpaint(path));
    default:
        qDebug() << "Create effect failed!" << G_EFFECT_STR[type];
        break;
    }

    return efx;
}
