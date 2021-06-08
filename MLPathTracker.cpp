#include "MLPathTracker.h"
#include "MLDataManager.h"
#include "GTracking.h"
#include "GUtil.h"
#include <vector>
#include <algorithm>
#include <opencv2/imgproc.hpp>

const int RECT_MARGIN = 15;

BBox* findDetectBox(int frameidx, const cv::Rect& query_rect, int class_id = -1)
{
    const auto& boxes = MLDataManager::get().trajectories.frameidx2detectboxes[frameidx];
    float iou_max = 0; 
    BBox* best = nullptr;
    for (const auto& pbox : boxes)
    {
        if (class_id >= 0 && pbox->classid != class_id) continue;
        float iou = compute_IoU(pbox->rect_global, query_rect);
        if (iou > iou_max)
        {
            best = pbox;
            iou_max = iou;
        }
    }

    float dist_min = 99999;
    if (!best)
    {
        for (const auto& pbox : boxes)
        {
            if (class_id >= 0 && pbox->classid != class_id) continue;
            float dist = compute_Euli(pbox->rect_global, query_rect);
            float ratio = (std::abs(pbox->rect_global.area()) + 0.00001) / (std::abs(query_rect.area()) + 0.00001);
            ratio = ratio < 1 ? 1 / ratio : ratio;
            if (dist < dist_min && ratio < 7.)
            {
                best = pbox;
                dist_min = dist;
            }
        }
        if (dist_min > 30)
            best = nullptr;
    }
    return best;
}

std::vector<BBox*> findDetectBox(int frameidx, int class_id)
{
    const auto& boxes = MLDataManager::get().trajectories.frameidx2detectboxes[frameidx];
    std::vector<BBox*> rects;
    for (const auto& pbox : boxes)
    {
        if (pbox->classid == class_id)
            rects.push_back(pbox);
    }
    return rects;
}

/// <summary>
/// track a path using trajectories
/// </summary>
GPathPtr autoTrackPath(int start_frame, const QRectF& start_rectF)
{
    const auto& global_data = MLDataManager::get();
    if (start_frame < 0 || start_frame >= global_data.get_framecount())
        return nullptr;
    cv::Rect rect_world = global_data.toWorldROI(start_rectF);

    // first, try to directly use the tracking results
    {
        std::vector<const Traject*> candidates;
        const auto& boxes = global_data.trajectories.frameidx2trackboxes[start_frame];
        for (auto itbox = boxes.constKeyValueBegin(); itbox != boxes.constKeyValueEnd(); itbox++)
        {
            const auto& box = itbox->second;
            float iou = compute_IoU(box->rect_global, rect_world);
            if (iou > 0.8)
            {
                const auto& objid2traj = global_data.trajectories.objid2trajectories;
                const auto& trajp = objid2traj.find(ObjID(box->classid, box->instanceid));
                if (trajp != objid2traj.end())
                    candidates.push_back(&(*trajp));
            }
        }
        std::sort(candidates.begin(), candidates.end(), [&](const Traject* l, const Traject* r) -> bool {
            return l->length() > r->length();
            });
        if (candidates.size() > 0)
        {
            const auto& traj = *candidates[0];
            GPathPtr path_data = std::make_shared<GPath>(false);
            auto& path = *path_data;
            path.setStartFrame(traj.beginidx);
            path.resize(traj.length());
            for (int i = 0; i < traj.length(); ++i)
            {
                cv::Rect dilatedrect = GUtil::addMarginToRect(traj[i + traj.beginidx]->rect_global, RECT_MARGIN);
                path.roi_rect_[i] = global_data.toPaintROI(dilatedrect, QRect(), true);
                boundRectF(path.roi_rect_[i]);
            }
            return path_data;
        }
    }

    // second try to use detect boxes to guess a good trajectories
    {
        cv::Rect rect_world0 = global_data.toWorldROI(start_rectF);
        BBox* pbox_ini = findDetectBox(start_frame, rect_world0);
        if (!pbox_ini) return nullptr;
        std::vector<BBox*> reverseboxes; reverseboxes.push_back(pbox_ini);
        for (int frameidx = start_frame - 1; frameidx >= 0; --frameidx)
        {
            const auto& pbox0 = reverseboxes[reverseboxes.size() - 1];
            BBox* pbox1 = findDetectBox(frameidx, pbox0->rect_global, pbox0->classid);
            if (pbox1) reverseboxes.push_back(pbox1);
            else break;
        }
        std::vector<BBox*> orderboxes; orderboxes.push_back(pbox_ini);
        for (int frameidx = start_frame + 1; frameidx < global_data.get_framecount(); ++frameidx)
        {
            const auto& pbox0 = orderboxes[orderboxes.size() - 1];
            BBox* pbox1 = findDetectBox(frameidx, pbox0->rect_global, pbox0->classid);
            if (pbox1) orderboxes.push_back(pbox1);
            else break;
        }

        GPathPtr path_data = std::make_shared<GPath>(false);
        auto& path = *path_data;
        path.setStartFrame(reverseboxes[reverseboxes.size() - 1]->frameidx);
        path.resize(reverseboxes.size() + orderboxes.size() - 1);
        for (int i = 0; i < reverseboxes.size(); ++i)
            path.roi_rect_[reverseboxes.size() - i - 1] = global_data.toPaintROI(
                GUtil::addMarginToRect(reverseboxes[i]->rect_global, RECT_MARGIN), 
                QRect(), true
            );
        for (int i = 1; i < orderboxes.size(); ++i)
            path.roi_rect_[reverseboxes.size() + i - 1] = global_data.toPaintROI(
                GUtil::addMarginToRect(orderboxes[i]->rect_global, RECT_MARGIN),
                QRect(), true
            );
        for (int i = 0; i < path.length(); ++i)
            boundRectF(path.roi_rect_[i]);
        return path_data;
    }
    return nullptr;
}

GPathPtr MLPathTracker::trackPath(int start_frame, const QRectF& start_rectF, int end_frame, const QRectF& end_rectF)
{
    if (start_frame >= 0 && end_frame < 0)
        return autoTrackPath(start_frame, start_rectF);

    const auto& global_data = MLDataManager::get();
    if (start_frame < 0 || end_frame < 0) {
        qCritical() << "Invalid range! " << "A: " << start_frame
            << " B " << end_frame;
        return 0;
    }
    if (start_frame >= end_frame) {
        qCritical() << "Invalid Tracking Range. " << "A: "
            << start_frame << "B: " << end_frame;
        return 0;
    }

    qDebug() << "Track Path." << "A" << start_frame << start_rectF
        << "B" << end_frame << end_rectF;

    // initiate path
    GPathPtr path_data = std::make_shared<GPath>(false);
    GPath& path = *path_data;
    path.setStartFrame(start_frame);
    int number_frames = end_frame - start_frame + 1;
    path.resize(number_frames);

    for (int i = start_frame; i < end_frame; ++i) {
        int curIdx = (path.isBackward() ? end_frame - i : i - start_frame);
        int nextIdx = (path.isBackward() ? curIdx - 1 : curIdx + 1);
        int cur_frameidx = curIdx + start_frame;
        int next_frameidx = nextIdx + start_frame;

        // init roi
        if (i == start_frame) {
            // TODO: we don't use backward to init any more
            path.roi_rect_[curIdx] = (path.isBackward() ? end_rectF : start_rectF);
        }

        QRectF current_rectF = path.roi_rect_[curIdx];
        cv::Rect rect_world0 = global_data.toWorldROI(current_rectF);
        BBox* pbox0 = findDetectBox(cur_frameidx, rect_world0);
        BBox* pbox1 = pbox0 ? findDetectBox(next_frameidx, pbox0->rect_global, pbox0->classid) 
                            : findDetectBox(next_frameidx, rect_world0);

        if (pbox1) // successfully locate new box
        {
            cv::Rect dilated = GUtil::addMarginToRect(pbox1->rect_global, RECT_MARGIN);
            path.roi_rect_[nextIdx] = global_data.toPaintROI(dilated, QRect(), true);
            boundRectF(path.roi_rect_[nextIdx]);
        }
        else
        {
            // TODO track future in case some frames miss
            cv::Rect dilated = GUtil::addMarginToRect(rect_world0, RECT_MARGIN);
            path.roi_rect_[nextIdx] = global_data.toPaintROI(dilated, QRect(), true);
            boundRectF(path.roi_rect_[nextIdx]);
        }
    }
    return path_data;
}


void MLPathTracker::updateTrack(GPath* path_data)
{
    const auto& global_data = MLDataManager::get();
    if (path_data == NULL) {
        qDebug() << "Update track path failed. path_data is NULL";
        return;
    }
    GPath& path = *path_data;
    int start_frame = path.startFrame();
    int end_frame = path.endFrame();

    // Track
    GTracking track;
    for (int i = start_frame; i < end_frame; ++i) {
        qDebug() << "Re-track Frame:" << i;
        int curIdx = (path.isBackward() ? end_frame - i : i - start_frame);
        int nextIdx = (path.isBackward() ? curIdx - 1 : curIdx + 1);
        int cur_frame = curIdx + start_frame;

        path.dirty_[curIdx] = true; // TODO: set true only for non-manual
        // init roi
        if (i == start_frame && path.manual_adjust_[curIdx]) {
            //            path.roi_fg_mat_[curIdx] = video_->
            //                    getForeground(cur_frameidx, path.frameRoiRect(cur_frameidx));
        }

        QRectF current_rectF = path.roi_rect_[curIdx];
        QRectF next_rectF = path.roi_rect_[nextIdx];
        cv::Mat4b flow_roi_mat = global_data.getFlowImage(cur_frame, current_rectF);

        GFrameFlowData flow_data;
        if (path.manual_adjust_[nextIdx]) {
            track.adjust(flow_roi_mat, global_data.imageRect(current_rectF),
                global_data.imageRect(next_rectF),
                path.isBackward(), flow_data);
        }
        else {
            QPointF image_off = track.predict(flow_roi_mat, global_data.imageRect(current_rectF),
                path.isBackward(), flow_data);
            QPointF offet(image_off.x() / (qreal)global_data.VideoWidth(),
                image_off.y() / (qreal)global_data.VideoHeight());
            path.roi_rect_[nextIdx] = current_rectF.translated(offet);
            boundRectF(path.roi_rect_[nextIdx]);
            //            path.roi_fg_mat_[nextIdx] = video_->getForeground(nextIdx + start_frame,
            //                                                              path.roi_rect_[nextIdx]);
        }

        // assign flow points
        int number_points = flow_data.flow_points_.size();
        path.flow_points_[curIdx] = new QPointF[number_points];
        QMatrix mat;
        mat.scale(1.0 / (qreal)global_data.VideoWidth(), 1.0 / (qreal)global_data.VideoHeight());
        for (int i = 0; i < number_points; ++i)
            path.flow_points_[curIdx][i] = mat.map(QPointF(flow_data.flow_points_[i]));
        path.number_flow_points_[curIdx] = number_points;
    }
}
