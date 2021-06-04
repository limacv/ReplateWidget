#include "GPathTracker.h"
#include "GTracking.h"
#include "MLDataManager.h"
#include <vector>
#include <algorithm>
#include <opencv2/imgproc.hpp>

float compute_IoU(const cv::Rect& r1, const cv::Rect& r2)
{
    return static_cast<float>((r1 & r2).area()) / (r1 | r2).area();
}

GPathPtr GPathTracker::autoTrackPath(int start_frame, const QRectF& start_rectF)
{
    const auto& global_data = MLDataManager::get();
    if (start_frame < 0 || start_frame >= global_data.get_framecount())
        return nullptr;
    cv::Rect rect_world = global_data.toWorldROI(start_rectF);

    // first, try to directly use the tracking results
    std::vector<cv::Rect> rois; rois.resize(global_data.get_framecount());
    int begin_frameidx = -1; 
    int rois_size = 0;
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
            begin_frameidx = traj.beginidx;
            rois_size = traj.length();
            for (int i = begin_frameidx; i < begin_frameidx + rois_size; ++i)
                rois[i] = traj[i]->rect_global;

            GPathPtr path_data = std::make_shared<GPath>(false);
            auto& path = *path_data;
            path.setStartFrame(traj.beginidx);
            path.resize(traj.length());
            for (int i = 0; i < traj.length(); ++i)
            {
                cv::Rect dilatedrect = traj[i + traj.beginidx]->rect_global;
                dilatedrect.x -= 5; dilatedrect.y -= 5;
                dilatedrect.width += 10; dilatedrect.height += 10;
                path.roi_rect_[i] = global_data.toPaintROI(traj[i + traj.beginidx]->rect_global, QRect(), true);
                boundRectF(path.roi_rect_[i]);
            }
            return path_data;
        }
    }

    // second, try to use detection boxes and optical flow to track
    {

    }
    return nullptr;
}

GPathPtr GPathTracker::trackPath(int start_frame, const QRectF& start_rectF,
    int end_frame, const QRectF& end_rectF)
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

    // Track, will update path.roi_rect_;  path.flow_points_;   path.number_flow_points_;
    GTracking track;
    for (int i = start_frame; i < end_frame; ++i) {
        qDebug() << "Tracking frame:" << i;
        int curIdx = (path.isBackward() ? end_frame - i : i - start_frame);
        int nextIdx = (path.isBackward() ? curIdx - 1 : curIdx + 1);
        int cur_frame = curIdx + start_frame;

        // init roi
        if (i == start_frame) {
            // TODO: we don't use backward to init any more
            path.roi_rect_[curIdx] = (path.isBackward() ? end_rectF : start_rectF);
        }

        QRectF current_rectF = path.roi_rect_[curIdx];
        cv::Mat4b flow_roi_mat = global_data.getFlowImage(curIdx + start_frame, current_rectF);

        // Predict next window position
        GFrameFlowData flow_data;
        QPointF image_off = track.predict(flow_roi_mat, global_data.imageRect(current_rectF),
            path.isBackward(), flow_data);
        QPointF offet(image_off.x() / (qreal)global_data.VideoWidth(),
            image_off.y() / (qreal)global_data.VideoHeight());
        path.roi_rect_[nextIdx] = current_rectF.translated(offet);
        boundRectF(path.roi_rect_[nextIdx]);
        //        path.roi_fg_mat_[nextIdx] = video_->getForeground(nextIdx + start_frame,
        //                                                          path.roi_rect_[nextIdx]);

        int number_points = flow_data.flow_points_.size();
        path.flow_points_[curIdx] = new QPointF[number_points];
        QMatrix mat;
        mat.scale(1.0 / (qreal)global_data.VideoWidth(), 1.0 / (qreal)global_data.VideoHeight());
        for (int i = 0; i < number_points; ++i)
            path.flow_points_[curIdx][i] = mat.map(QPointF(flow_data.flow_points_[i]));
        path.number_flow_points_[curIdx] = number_points;
    }

    return path_data;
}

void GPathTracker::updateTrack(GPath* path_data)
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
            //                    getForeground(cur_frame, path.frameRoiRect(cur_frame));
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

void GPathTracker::boundRectF(QRectF& src) const
{
    QPointF off(0, 0);
    if (src.x() < 0)
        off.setX(-src.x());
    if (src.right() >= 1)
        off.setX(1 - src.right());
    if (src.y() < 0)
        off.setY(-src.y());
    if (src.bottom() >= 1)
        off.setY(1 - src.bottom());
    src.translate(off);
}
