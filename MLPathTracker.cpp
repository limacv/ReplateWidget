#include "MLPathTracker.h"
#include "MLDataManager.h"
#include "GTracking.h"
#include "GUtil.h"
#include <vector>
#include <list>
#include <algorithm>
#include <opencv2/imgproc.hpp>

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
/// return vector of BBox* (successfully tracked path)
/// unsuccessfully tracked path are repeated in the boundary. e.g.  a a a a a b c d e e e e e 
/// </summary>
std::list<BBox*> autoTrackPath(int start_frame, const QRectF& start_rectF)
{
    const auto& global_data = MLDataManager::get();
    std::list<BBox*> tracked;
    if (start_frame < 0 || start_frame >= global_data.get_framecount())
        return tracked;

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
            for (int i = traj.beginidx; i <= traj.endidx; ++i)
                tracked.push_back(traj[i]);
        }
    }
    // tracked may be empty or contains partial bboxes
    // second try to use detect boxes to guess a good trajectories
    {
        if (tracked.empty())
        {
            BBox* pbox_ini = findDetectBox(start_frame, rect_world);
            if (!pbox_ini) return tracked;
            
            tracked.push_back(pbox_ini);
        }
        // track backwards
        for (int frameidx = tracked.front()->frameidx - 1; frameidx >= 0; --frameidx)
        {
            const auto& pbox0 = tracked.front();
            BBox* pbox1 = findDetectBox(frameidx, pbox0->rect_global, pbox0->classid);
            if (pbox1) tracked.push_front(pbox1);
            else break;
        }
        // track forwards
        for (int frameidx = tracked.back()->frameidx + 1; frameidx < global_data.get_framecount(); ++frameidx)
        {
            const auto& pbox0 = tracked.back();
            BBox* pbox1 = findDetectBox(frameidx, pbox0->rect_global, pbox0->classid);
            if (pbox1) tracked.push_back(pbox1);
            else break;
        }
        return tracked;
    }
}

void fillsInvalidPath(GPath& path, int startidx, int endidx)
{
    for (int i = startidx - 1; i >= 0; --i)
        path.roi_rect_[i] = path.roi_rect_[i + 1];
    for (int i = endidx + 1; i < path.space(); ++i)
        path.roi_rect_[i] = path.roi_rect_[i - 1];
}

GPathPtr MLPathTracker::trackPath(int start_frame, const QRectF& start_rectF, int end_frame, const QRectF& end_rectF)
{
    const auto& global_data = MLDataManager::get();
    if (start_frame < 0 || start_frame >= global_data.get_framecount()) return nullptr;
    std::list<BBox*> trackedbox = autoTrackPath(start_frame, start_rectF);
    
    // initiate path
    GPathPtr path_data = std::make_shared<GPath>(false);
    GPath& path = *path_data;
    path.resize(global_data.get_framecount());
    path.manual_adjust_[start_frame] = true;

    if (trackedbox.empty()) // if track failed, use naive solution
    {
        path.roi_rect_[start_frame] = start_rectF;
        path.setStartFrame(start_frame);
        path.setEndFrame(start_frame);
        fillsInvalidPath(path, start_frame, start_frame);
        return path_data;
    }

    for (const auto pbox : trackedbox) // copy rects
    {
        cv::Rect dilated = GUtil::addMarginToRect(pbox->rect_global, RECT_MARGIN);
        path.roi_rect_[pbox->frameidx] = global_data.toPaintROI(dilated, QRect(), true);
        boundRectF(path.roi_rect_[pbox->frameidx]);
    }
    fillsInvalidPath(path, trackedbox.front()->frameidx, trackedbox.back()->frameidx);
    // deciding start frame and end frame
    if (end_frame < 0 || end_frame >= global_data.get_framecount())
    {
        path.setStartFrame(trackedbox.front()->frameidx);
        path.setEndFrame(trackedbox.back()->frameidx);
    }
    else
    {
        path.setStartFrame(std::min(start_frame, end_frame));
        path.setEndFrame(std::max(start_frame, end_frame));
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
    std::vector<int> modified_points;
    for (int i = 0; i < path.space(); ++i)
        if (path.manual_adjust_[i] && path.dirty_[i])
            modified_points.push_back(i);

    for (int modified_idx : modified_points)
    {
        // update backward
        for (int backward_idx = modified_idx - 1;
            backward_idx >= 0 && !path.manual_adjust_[backward_idx];
            --backward_idx)
        {
            auto rect_w = global_data.toWorldROI(path.roi_rect_[backward_idx + 1]);
            BBox* pbox = findDetectBox(backward_idx, rect_w);
            if (pbox)
            {
                cv::Rect dilated = GUtil::addMarginToRect(pbox->rect_global, RECT_MARGIN);
                path.roi_rect_[backward_idx] = global_data.toPaintROI(dilated, QRect(), true);
                boundRectF(path.roi_rect_[backward_idx]);
            }
            else
                path.roi_rect_[backward_idx] = path.roi_rect_[backward_idx + 1];

            path.dirty_[backward_idx] = true;
        }
        // update forward
        for (int forward_idx = modified_idx + 1;
            forward_idx < path.space() && !path.manual_adjust_[forward_idx];
            ++forward_idx)
        {
            auto rect_w = global_data.toWorldROI(path.roi_rect_[forward_idx - 1]);
            BBox* pbox = findDetectBox(forward_idx, rect_w);
            if (pbox)
            {
                cv::Rect dilated = GUtil::addMarginToRect(pbox->rect_global, RECT_MARGIN);
                path.roi_rect_[forward_idx] = global_data.toPaintROI(dilated, QRect(), true);
                boundRectF(path.roi_rect_[forward_idx]);
            }
            else
                path.roi_rect_[forward_idx] = path.roi_rect_[forward_idx - 1];

            path.dirty_[forward_idx] = true;
        }
    }
}
