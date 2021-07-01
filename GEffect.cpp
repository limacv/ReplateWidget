#include "GEffect.h"
#include <QColorDialog>
#include <typeinfo>
#include <MLDataManager.h>
#include "GPath.h"
#include "GUtil.h"
#include "MLUtil.h"

GEffect::GEffect(const GPathPtr &path)
    : async_offset_(0)
    , render_order_(true)
    , line_color_(qRgba(23, 224, 135, 100))
    , speed_factor_(1)
    , is_shown_(true)
    , draw_line_(false)
    , trail_alpha_(1.0)
    , line_smooth_(10)
    , fade_level_(1)
    , trans_level_(0)
    , marker_mode_(0)
    , reverse_render(false)
{
    setPath(path);
    sync_id_ = this->startFrame();
}

QMatrix GEffect::scale_mat_ = QMatrix();


bool GEffect::adjustLeft(int step)
{
    int new_left = startFrame() + step;
    if (new_left < 0 || new_left >= path_->space())
        return false;
    path_->setStartFrame(new_left);
    return true;
}

bool GEffect::adjustRight(int step)
{
    int new_right = endFrame() + step;
    if (new_right >= path_->space() || new_right < 0 )
        return false;
    path_->setEndFrame(new_right);
    return true;
}

bool GEffect::moveStart(int step)
{
    int new_left = startFrame() + step;
    int new_right = endFrame() + step;
    if (new_left < 0 || new_left >= path_->space())
        return false;
    if (new_right >= path_->space() || new_right < 0)
        return false;
    path_->setStartFrame(new_left);
    path_->setEndFrame(new_right);
    return true;
}

void GEffect::adjustAsync(int offset, int duration)
{
    if (duration <= 0) duration = effectLength();
    async_offset_ = (async_offset_ + offset) % duration;
    if (async_offset_ < 0) async_offset_ += duration;
    qDebug() << "Adjust Offset" << offset << async_offset_;
}

QRect GEffect::renderLocation(int frame_id) const
{
    return scaleMat().mapRect(path()->frameRoiRect(frame_id)).toRect();
}

QImage GEffect::renderImage(int frame_id) const
{
    return path()->frameRoiImage(frame_id);
}

void GEffect::readBasic(const YAML::Node &node)
{
    if (node["Type"]) effect_type_ = (G_EFFECT_ID) node["Type"].as<int>();

    if (node["Priority"]) priority_level_ = node["Priority"].as<int>();

    if (node["StartFrame"])
        path_->setStartFrame(node["StartFrame"].as<int>());

    if (node["EndFrame"])
        path_->setEndFrame(node["EndFrame"].as<int>());

    if (node["Async"])
        async_offset_ = node["Async"].as<int>();
    else
        async_offset_ = 0;

    if (node["Order"])
        render_order_ = node["Order"].as<bool>();
    else
        render_order_ = true;

    if (node["LineColor"])
        line_color_ = node["LineColor"].as<unsigned int>();

    if (node["Speed"])
        speed_factor_ = node["Speed"].as<int>();

    if (node["SyncId"])
        sync_id_ = node["SyncId"].as<int>();

    if (node["Shown"])
        is_shown_ = node["Shown"].as<bool>();

    if (node["Smooth"])
        line_smooth_ = node["Smooth"].as<int>();

    if (node["TrailAlpha"])
        trail_alpha_ = node["TrailAlpha"].as<float>();

    if (node["DrawLine"])
        draw_line_ = node["DrawLine"].as<bool>();
    else if (node["Trail"]) // for old .proj conversion
        draw_line_ = node["Trail"].as<bool>();

    if (node["TransLevel"])
        trans_level_ = node["TransLevel"].as<int>();

    if (node["FadeLevel"])
        fade_level_ = node["FadeLevel"].as<int>();
    else if (node["Fade"]) // for old .proj conversion
        fade_level_ = node["Fade"].as<int>();

    if (node["ReverseRender"])
        reverse_render = node["ReverseRender"].as<bool>();

}

void GEffect::writeBasic(YAML::Emitter &out) const
{
    out << YAML::Key << "Type" << YAML::Value << type();
    out << YAML::Key << "Priority" << YAML::Value << priority_level_;
    out << YAML::Key << "StartFrame" << YAML::Value << startFrame();
    out << YAML::Key << "EndFrame" << YAML::Value << endFrame();

    out << YAML::Key << "Async" << YAML::Value << async_offset_;
    out << YAML::Key << "Order" << YAML::Value << render_order_;
    out << YAML::Key << "LineColor" << YAML::Value << line_color_;
    out << YAML::Key << "Speed" << YAML::Value << speed_factor_;
    out << YAML::Key << "SyncId" << YAML::Value << sync_id_;
    out << YAML::Key << "Shown" << YAML::Value << is_shown_;

    out << YAML::Key << "Smooth" << YAML::Value << line_smooth_;
    out << YAML::Key << "TrailAlpha" << YAML::Value << trail_alpha_;
    out << YAML::Key << "DrawLine" << YAML::Value << draw_line_;

    out << YAML::Key << "TransLevel" << YAML::Value << trans_level_;
    out << YAML::Key << "FadeLevel" << YAML::Value << fade_level_;
    out << YAML::Key << "Marker" << YAML::Value << marker_mode_;

    out << YAML::Key << "ReverseRender" << YAML::Value << reverse_render;
}

YAML::Emitter &operator <<(YAML::Emitter &out, const GEffectPtr &efx)
{
    efx->write(out);
    return out;
}

void operator >> (const YAML::Node& out, GEffectPtr &efx) {
    efx->read(out);
}

void GEffect::release() {qDebug() << "Effect:" << G_EFFECT_STR[type()]
                                  << "released!";}

void GEffect::setPath(const GPathPtr &path)
{
    path_ = path;
}

int GEffect::getSyncLocalIndex(int frame_id, int start_id, int duration) const
{
    int ret = frame_id % duration - start_id % duration;
    if (ret < 0) ret += duration;
    return ret;
}

int GEffect::getSyncLocalIndex2(int frame_id, int start_id, int duration,
                                int sync_id, int slow_factor) const
{
    if (sync_id == -1) sync_id == start_id;
    int slow_length = qMax(duration / slow_factor, 1);
    int sync_diff = frame_id % duration - sync_id % duration;
    int res = ((sync_diff >= 0? (sync_diff + slow_factor - 1) / slow_factor
                               : sync_diff / slow_factor)
            + (sync_id - start_id)) % slow_length;
    if (res < 0) res += slow_length;
    return res;
}

int GEffect::getSyncLocalIndexSlow(int frame_id, int start_id, int duration,
                                   int sync_id, int slow_factor) const
{
    int res = ((sync_id - start_id) * slow_factor
               + frame_id - sync_id) % duration;
    if (res < 0) res += duration;
    return res;
}

int GEffect::getFadeAlpha(int effect_length, int idx, int fade_level) const
{
    int duration = FadeDuration[fade_level];
    int edge0 = effect_length - idx - 1;
    int edge1 = idx;
    int off = qMin(edge0, edge1);
    if (off < duration)
        return qMin(255 * (off + 1) / (duration + 1), 255);
    return 255;
}

int GEffect::getFadeAlphaMotion(int src_len, int range_len, int idx, int fade_level) const
{
    int duration = FadeDuration[fade_level];
    int length = qMin(src_len, range_len + duration);
    if (idx >= length) return 0;
    if (idx >= length - duration)
        idx = length - 1 - idx;
    if (idx < duration)
        return qMin(255 / (duration + 1) * (idx + 1), 255);
    return 255;
}

int GEffect::convertTrans(float a)
{
    if (a < G_TRANS_LEVEL[0]) return 0;
    else if (a < G_TRANS_LEVEL[1]) return 1;
    else if (a < G_TRANS_LEVEL[2]) return 2;
    else return 3;
}


void GEffectTrash::write(YAML::Emitter &out) const
{
    out << YAML::BeginMap;
    writeBasic(out);
    out << YAML::EndMap;
}


void GEffectStill::render(QPainter &painter, int frame_id, int duration, bool video) const
{
    int render_frame = startFrame();
    QRect rect = renderLocation(render_frame);
    //QImage dst = (video? MLDataManager::get().getRoiofFrame(render_frame, rect)
                        //: renderImage(render_frame));
    QImage dst = renderImage(render_frame);
    QPainterPath pp = scaleMat().map(path()->painter_path_);
    if (!pp.isEmpty())
        painter.setClipPath(pp);
    painter.drawImage(rect, dst);
    painter.setClipPath(QPainterPath(), Qt::NoClip);
}

void GEffectStill::write(YAML::Emitter &out) const
{
    out << YAML::BeginMap;
    writeBasic(out);
    out << YAML::EndMap;
}

void GEffectInpaint::render(QPainter& painter, int frame_idx, int duration, bool video) const
{
    int render_frame = startFrame();
    QRect rect = renderLocation(render_frame);
    //QImage dst = (video? MLDataManager::get().getRoiofFrame(render_frame, rect)
                        //: renderImage(render_frame));
    QImage dst = renderImage(render_frame);
    QPainterPath pp = scaleMat().map(path()->painter_path_);
    if (!pp.isEmpty())
        painter.setClipPath(pp);
    painter.drawImage(rect, dst);
    painter.setClipPath(QPainterPath(), Qt::NoClip);
}

void GEffectInpaint::write(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    writeBasic(out);
    out << YAML::EndMap;
}

GEffectBlack::GEffectBlack(const GPathPtr &path): GEffect(path){
    effect_type_ = EFX_ID_BLACK;
    priority_level_ = G_EFX_PRIORITY[effect_type_] * G_EFX_PRIORITY_STEP;
}

void GEffectBlack::render(QPainter &painter, int frame_id, int duration, bool video) const
{
//    renderImage(0).save(QString("%1.png").arg(frame_id));
    painter.drawImage(renderLocation(0), renderImage(0));
}

void GEffectBlack::write(YAML::Emitter &out) const
{
    out << YAML::BeginMap;
    writeBasic(out);
    out << YAML::EndMap;
}

GEffectTrail::GEffectTrail(const GPathPtr &path)
    : GEffect(path)
    , prev_length_(-1)
    , recache(false)
{
    effect_type_ = EFX_ID_TRAIL;
    priority_level_ = G_EFX_PRIORITY[effect_type_] * G_EFX_PRIORITY_STEP;

//    for (int i = 0; i < path->size(); ++i)
//        cv::imwrite(QString("%1.png").arg(i).toStdString(), path->roi_fg_mat_[i]);

    std::vector<cv::Mat> blendImgs;
    GUtil::adaptiveImages(path->roi_fg_mat_, blendImgs);

    d_images.resize(path->space());
    for (size_t i = 0; i < d_images.size(); ++i) {
        d_images[i] = GUtil::mat2qimage(blendImgs[i], QImage::Format_ARGB32_Premultiplied).copy();
        qDebug() << i << d_images[i] << path->roi_fg_mat_[i].rows;
    }
}



void GEffectTrail::preRender(QPainter &painter, int frame_id, int duration)
{
    //if (recache || prev_length_ != effectLength()) {
    //    generateTrail(painter);
    //    recache = false;
    //    prev_length_ = effectLength();
    //}
    setSmooth(line_smooth_); // update trail_
}

void GEffectTrail::generateTrail(QPainter& painter)
{

    cached_effect_.clear();
    
    QRect view_rect = painter.viewport();
    QImage img(view_rect.width(), view_rect.height(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter pt(&img);
    pt.setCompositionMode(QPainter::CompositionMode_SourceOver);
    size_t n = effectLength();

    // weight by motion vector
    std::vector<float> lens;
    getImageAlphaByVec(trail_, lens);

    std::vector<QImage> images(n);

    int efx_alpha = trail_alpha_ * 255;
    for (size_t i = 0; i < n; ++i) {
        size_t curIdx = (render_order_ ? i : n - i - 1);
        curIdx += startFrame();
        QImage dst = d_images[curIdx];
        QImage alpha(dst.size(), QImage::Format_Indexed8);
        int alp = qMin((int)(efx_alpha * lens[curIdx] * 5), 255);
        alpha.fill(alp);
        dst.setAlphaChannel(alpha);  // dst.alpha *= alpha
        QRect rect = renderLocation(curIdx);
        rect.moveCenter(trail_[i].toPoint());
        pt.drawImage(rect, dst);
        //            renderImage(curIdx).save(QString("_%1.png").arg(i));
    }

    cached_effect_.push_back(img);
}

void GEffectTrail::render(QPainter &painter, int frame_id, int duration, bool video) const
{
//    qDebug() << typeid(*this).name() << cached_effect_[0];
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    //QRectF roi = MLDataManager::get().toPaintROI(cached_roi_, painter.viewport());
    //painter.drawImage(painter.viewport(), cached_effect_[0]);

    size_t n = effectLength();
    // weight by motion vector
    std::vector<float> lens;
    getImageAlphaByVec(trail_, lens);
    std::vector<QImage> images(n);

    int efx_alpha = trail_alpha_ * 255;
    for (size_t i = startFrame(); i <= endFrame(); ++i) {
        QImage dst = d_images[i];
        QImage alpha(dst.size(), QImage::Format_Indexed8);
        int alp = qMin((int)(efx_alpha * lens[i - startFrame()] * 5), 255);
        alpha.fill(alp);
        dst.setAlphaChannel(alpha);  // dst.alpha *= alpha
        QRect rect = renderLocation(i);
        rect.moveCenter(trail_[i - startFrame()].toPoint());
        painter.drawImage(rect, dst);
    }
}

void GEffectTrail::getImageAlphaByVec(const std::vector<QPointF> &path, std::vector<float> &out) const
{
    out.resize(path.size());
    std::vector<QPointF> smooth_path = GUtil::smooth(path, 5);
    double sum = 0;
    size_t n = path.size();
    for (size_t j = 0; j < n; ++j) {
        size_t curIdx = j;
        size_t nextIdx = qMin(curIdx + 1, n - 1);
        QPointF st = smooth_path[curIdx];
        QPointF off = smooth_path[nextIdx] - st;
        out[j] = off.manhattanLength();
        sum += out[j];
    }
    double avg = sum / (double) n;
    for (size_t j = 0; j < out.size(); ++j) {
        out[j] /= avg;
        out[j] *= out[j];
    }
}

void GEffectTrail::write(YAML::Emitter &out) const
{
    out << YAML::BeginMap;
    writeBasic(out);
    out << YAML::EndMap;
}

bool GEffectTrail::setSmooth(int s)
{
    
    line_smooth_ = s;
    trail_.resize(this->effectLength());
    for (size_t i = 0; i < this->effectLength(); ++i) {
        trail_[i] = this->renderLocation(i + startFrame()).center();
    }
    trail_ = GUtil::smooth(trail_, line_smooth_);
    //qDebug() << "setSmooth" << line_smooth_;
    return true;
}

int GEffectMotion::max_loop_num = 0;

GEffectMotion::GEffectMotion(const GPathPtr &path)
    : GEffect(path)
{
    effect_type_ = EFX_ID_MOTION;
    priority_level_ = G_EFX_PRIORITY[effect_type_] * G_EFX_PRIORITY_STEP;
    marker_mode_ = 0;
}

void GEffectMotion::renderSlowVideo(QPainter &painter, int frame_id,
                                    int duration) const
{
    qDebug("GEffectMotion::renderSlowVideo function is not implemented!");
    //const auto& global_data = MLDataManager::get();
    //size_t idx = getSyncLocalIndexSlow(
    //            frame_id, startFrame(), duration,
    //            sync_id_, speed_factor_);
    //int new_len = effectLength() * speed_factor_;
    //for (; idx < duration + FadeDuration[fade_level_]; idx += duration) {
    //    int fade = getFadeAlphaMotion(new_len, duration, idx, fade_level_);
    //    if (fade == 0) continue;
    //    QRect rect = renderLocation(idx / speed_factor_ + startFrame());
    //    
    //    QImage dst = video->loadOtherImageFg(idx + speed_factor_ * startFrame(), rect, speed_factor_);
    //    QImage alpha(dst.size(), QImage::Format_Indexed8);
    //    alpha.fill(fade * G_TRANS_LEVEL[trans_level_]);
    //    dst.setAlphaChannel(alpha);
    //    painter.drawImage(rect, dst);
    //}
}

// TODO this function is modified, not sure it's correct
void GEffectMotion::renderSlow(QPainter &painter, int frame_id, int duration, bool video) const
{
    //if (video) {
    //    renderSlowVideo(painter, frame_id, duration);
    //    return;
    //}

    int base_id = getSyncLocalIndex2(
                frame_id, startFrame(), duration,
                sync_id_, speed_factor_);
    base_id = (base_id + async_offset_) % duration;

//    qDebug() << "BB";
//    int length = qMin(duration + FadeDuration[fade_level_], effectLength());
//    for (int idx = base_id; idx < length; idx += duration)
    for (int idx = base_id; idx < this->effectLength(); idx += duration)
    {
//        qDebug() << "CC" << idx << duration << FadeDuration[fade_level_];
        int render_frame = idx + startFrame();
        int fade = getFadeAlpha(this->effectLength(), idx, fade_level_);
        if (fade == 0) continue;
        QRect rect = renderLocation(render_frame);
        QImage dst = renderImage(render_frame);
        QImage alpha(dst.size(), QImage::Format_Indexed8);
        alpha.fill(fade * G_TRANS_LEVEL[trans_level_]);
        dst.setAlphaChannel(alpha);
        painter.drawImage(rect, dst);
    }
//    qDebug() << "DD";
}

void GEffectMotion::preRender(QPainter &painter, int frame_id, int duration)
{
    if (duration <= 0) duration = effectLength();
    int loop_num = std::ceil(this->effectLength() / (float) duration);
    max_loop_num = qMax(loop_num, max_loop_num);

    if (marker_mode_ != 0 || draw_line_)
        setSmooth(line_smooth_);
}

void GEffectMotion::render(QPainter &painter, int frame_id, int duration, bool video) const
{
    const auto& global_data = MLDataManager::get();
    if (duration <= 0)
        duration = this->effectLength();

    if (speed_factor_ > 1) {
        renderSlow(painter, frame_id, duration);
        return;
    }

    int base_id = getSyncLocalIndex(frame_id, startFrame(), duration) + async_offset_;
    base_id %= duration;
    base_id = base_id >= 0 ? base_id : base_id + duration;

    int inv_id = ((effectLength() - 1) / duration) * duration + base_id;
    if (inv_id >= effectLength()) inv_id -= duration;
    int sum_id = inv_id + base_id;

//    int loop_num = std::ceil(effectLength() / (float)duration);
    int trail_length = 0;

    int idx = reverse_render ? base_id + (effectLength() - base_id) / duration * duration : base_id;
    for (; idx < this->effectLength() && idx >= base_id; idx = reverse_render ? idx - duration : idx + duration) {
        int local_id = (render_order_? idx: sum_id - idx);
        int render_frame = local_id + startFrame();
        int anchor_id = (base_id / duration +
                         (render_frame - frame_id) / duration) % max_loop_num;
        if (anchor_id < 0) anchor_id += max_loop_num;

        QRect rect = renderLocation(render_frame);
        QImage dst = renderImage(render_frame);
        //QImage dst = (video? video->loadOriImageFg(render_frame, rect)
                           //: renderImage(render_frame));

        QImage alpha(dst.size(), QImage::Format_Indexed8);
        int fade = getFadeAlpha(this->effectLength(), local_id, fade_level_);
        float trans = G_TRANS_LEVEL[trans_level_];
        alpha.fill(fade * trans);
        dst.setAlphaChannel(alpha);

        painter.drawImage(rect, dst);

        if (marker_mode_ == 2 || (marker_mode_ == 1 && anchor_id == 0)) {
            float rad = qMax(rect.width(), rect.height()) / 2.5;
            int marker_pos = local_id;
            QPointF pos = scaleMat().map(marker_trail_[marker_pos]);
            pos.setY(pos.y() - rad);
            float scale = painter.viewport().height() / 300.0; // TODO: a bit hacking
//            qDebug() << "anchor_id" << anchor_id;
            GUtil::addTriangle(painter, pos.toPoint(), anchor_id, scale);
        }       
        if (anchor_id == 0)
            trail_length = qMax(local_id, trail_length);
    }
    if (draw_line_) {
        int linewidth = painter.viewport().height() / 100;
        QColor pencolor(line_color_);
        pencolor.setAlpha(qAlpha(line_color_));
        painter.setPen(QPen(QColor(pencolor), linewidth, Qt::DotLine));

        std::vector<QPointF> trail(trail_.size());
        for (int i = 0; i < trail.size(); ++i)
            trail[i] = scaleMat().map(trail_[i]);
//        qDebug() << "Draw line" << trail_length << trail_.size() << trail_[0] << trail[0];
//        trail_length -= (startFrame() - path()->startFrame());
        painter.drawPolyline(trail.data(), trail_length + 1);
    }
}

void GEffectMotion::write(YAML::Emitter &out) const
{
    out << YAML::BeginMap;
    writeBasic(out);
    out << YAML::EndMap;
}

bool GEffectMotion::setSmooth(int s)
{
    int n = this->path()->length();
    line_smooth_ = s;
    trail_.resize(n);
    for (size_t i = 0; i < n; ++i) {
        trail_[i] = this->path()->frameRoiRect(i + startFrame()).center();
//        trail_[i] = this->renderLocation(i + startFrame()).center();
    }
    marker_trail_.assign(trail_.begin(), trail_.end());
    trail_ = GUtil::smooth(trail_, line_smooth_);
    marker_trail_ = GUtil::smooth2(trail_, 50);
    //qDebug() << "setSmooth" << line_smooth_;
    return true;
}

bool GEffectMotion::setTrailLine(bool b)
{
    if (!draw_line_ && b)
        line_color_ = QColorDialog().getRgba(line_color_);
    draw_line_ = b;
    return true;
}

bool GEffectMotion::setMarker(int a)
{
    qDebug() << "Set Marker mode" << a;
    marker_mode_ = a;
    return true;
}


//////////////////////////////////////////////////////////////////////////////////////////

int GEffectLoop::max_loop_num = 0;

GEffectLoop::GEffectLoop(const GPathPtr& path)
    : GEffect(path)
{
    effect_type_ = EFX_ID_LOOP;
    priority_level_ = G_EFX_PRIORITY[effect_type_] * G_EFX_PRIORITY_STEP;
}

// TODO this function is modified, not sure it's correct
void GEffectLoop::renderSlow(QPainter& painter, int frame_id, int duration, bool video) const
{
    //if (video) {
    //    renderSlowVideo(painter, frame_id, duration);
    //    return;
    //}

    int base_id = getSyncLocalIndex2(
        frame_id, startFrame(), duration,
        sync_id_, speed_factor_);
    base_id = (base_id + async_offset_) % duration;

    //    qDebug() << "BB";
    //    int length = qMin(duration + FadeDuration[fade_level_], effectLength());
    //    for (int idx = base_id; idx < length; idx += duration)
    for (int idx = base_id; idx < this->effectLength(); idx += duration)
    {
        //        qDebug() << "CC" << idx << duration << FadeDuration[fade_level_];
        int render_frame = idx + startFrame();
        int fade = getFadeAlpha(this->effectLength(), idx, fade_level_);
        if (fade == 0) continue;
        QRect rect = renderLocation(render_frame);
        QImage dst = renderImage(render_frame);
        QImage alpha(dst.size(), QImage::Format_Indexed8);
        alpha.fill(fade * G_TRANS_LEVEL[trans_level_]);
        dst.setAlphaChannel(alpha);
        painter.drawImage(rect, dst);
    }
    //    qDebug() << "DD";
}

void GEffectLoop::preRender(QPainter& painter, int frame_id, int duration)
{
    if (duration <= 0) duration = effectLength();
    int loop_num = std::ceil(this->effectLength() / (float)duration);
    max_loop_num = qMax(loop_num, max_loop_num);

    if (marker_mode_ != 0 || draw_line_)
        setSmooth(line_smooth_);
}

void GEffectLoop::render(QPainter& painter, int frame_id, int duration, bool video) const
{
    const auto& global_data = MLDataManager::get();
    if (duration <= 0)
        duration = this->effectLength();

    if (speed_factor_ > 1) {
        renderSlow(painter, frame_id, duration);
        return;
    }

    int base_id = getSyncLocalIndex(frame_id, startFrame(), duration) + async_offset_;
    base_id %= duration;
    base_id = base_id >= 0 ? base_id : base_id + duration;

    int inv_id = ((effectLength() - 1) / duration) * duration + base_id;
    if (inv_id >= effectLength()) inv_id -= duration;
    int sum_id = inv_id + base_id;

    //    int loop_num = std::ceil(effectLength() / (float)duration);
    int trail_length = 0;
    for (int idx = base_id; idx < this->effectLength(); idx += duration) {
        int local_id = (render_order_ ? idx : sum_id - idx);
        int render_frame = local_id + startFrame();
        int anchor_id = (base_id / duration +
            (render_frame - frame_id) / duration) % max_loop_num;
        if (anchor_id < 0) anchor_id += max_loop_num;

        QRect rect = renderLocation(render_frame);
        QImage dst = renderImage(render_frame);
        //QImage dst = (video? video->loadOriImageFg(render_frame, rect)
                           //: renderImage(render_frame));

        QImage alpha(dst.size(), QImage::Format_Indexed8);
        int fade = getFadeAlpha(this->effectLength(), local_id, fade_level_);
        float trans = G_TRANS_LEVEL[trans_level_];
        alpha.fill(fade * trans);
        dst.setAlphaChannel(alpha);

        painter.drawImage(rect, dst);
    }
}

void GEffectLoop::write(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    writeBasic(out);
    out << YAML::EndMap;
}