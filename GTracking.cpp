#include "GTracking.h"
#include "GUtil.h"
#include "qdebug.h"
#include "MLDataManager.h"

qreal GTracking::M_ACTIVE_LENGTH_TH = 0.8;
qreal GTracking::M_ACTIVE_RATIO_TH = 0.01;

GTracking::GTracking()
    :kf_(0)
{
}

GTracking::~GTracking()
{
    delete kf_;
    kf_ = 0;
}

QPointF GTracking::predict(const cv::Mat4b& flow_mat, const QRect& image_loc_rect, bool backward,
                              GFrameFlowData& out_flow_data)
{
    size_t region_width = flow_mat.cols;
    size_t region_height = flow_mat.rows;
    size_t num_area_pixels = region_width * region_height;
    QPoint ori_roi_center(region_width/2, region_height/2);

    // find active pixels
    analyzeFlow(flow_mat, backward, out_flow_data);

    // predict using flow center & flow velocity
    QPoint new_roi_center = out_flow_data.flow_center_;
    QPointF flow_offset = out_flow_data.flow_velocity_ * sqrt(
                out_flow_data.flow_confidence_.x() * out_flow_data.flow_confidence_.y());

//    qDebug() << flow_data->flow_velocity_ << flow_data->flow_confidence_;
//    qDebug() << "[[flow_offset" << flow_offset;

    // check confidence for active_boundbox_rect
    int bb_pixels = out_flow_data.active_boundbox_rect_.width() *
            out_flow_data.active_boundbox_rect_.height();
    int cons_pixels = num_area_pixels * out_flow_data.flow_confidence_.x() *
            out_flow_data.flow_confidence_.y();
    // if (0.5) of the pixels are consensus pixels, then use boundbox rect as center rect
    if (bb_pixels < 2 * cons_pixels)
    {
        new_roi_center = out_flow_data.active_boundbox_rect_.center();
        flow_offset = out_flow_data.active_boundbox_rect_.center() - ori_roi_center;
    }
    QPointF center_offset = new_roi_center - ori_roi_center;

    // init kalman filter at first
    if (kf_ == 0)
    {
        kf_ = new GKalman(6,4);
        kf_->begin(image_loc_rect.center().x(),
                   image_loc_rect.center().y(),
                   flow_offset.x(), flow_offset.y());
    }

//    qDebug() << "[[center_offset" << center_offset;
    // adjust by kalman filter
    QPointF center_predict = center_offset + image_loc_rect.center();
    cv::Point center_predict_kf = kf_->update(center_predict.x(),
                                              center_predict.y(),
                                              flow_offset.x(),
                                              flow_offset.y());
    QPointF center_predict_offset = QPointF(center_predict_kf.x,
                                            center_predict_kf.y) - image_loc_rect.center();

    return center_predict_offset;
}

void GTracking::adjust(const cv::Mat4b& flow_mat, const QRect& image_loc_rect,
                          const QRect& image_loc_rect_next, bool backward,
                          GFrameFlowData& out_flow_data)
{
    // find active pixels
    analyzeFlow(flow_mat, backward, out_flow_data);

    QPointF center_offset = image_loc_rect_next.topLeft() - image_loc_rect.topLeft();

    // init kalman filter at first
    if (kf_ == 0)
    {
        kf_ = new GKalman(6,4);
        kf_->begin(image_loc_rect.center().x(),
                   image_loc_rect.center().y(),
                   center_offset.x(), center_offset.y());
    }
    kf_->update(image_loc_rect_next.center().x(),
                image_loc_rect_next.center().y(),
                center_offset.x(),
                center_offset.y());
}

void GTracking::analyzeFlow(const cv::Mat &flow_mat, bool back,
                            GFrameFlowData& flow_data)
{
    size_t region_width = flow_mat.cols;
    size_t region_height = flow_mat.rows;
    size_t num_area_pixels = region_width * region_height;

    std::vector<QPointF> active_point_vec;
    active_point_vec.reserve(num_area_pixels);
    std::vector<QPoint> active_point_pos;
    active_point_pos.reserve(num_area_pixels);

    cv::Mat flow_mat_erode = flow_mat;
    //erodeDilateFilter(flow_mat, flow_mat_erode):

    getActivePoints(flow_mat_erode, back, active_point_pos,
                    active_point_vec);

    // active point ration
    size_t num_total_active_pixels = active_point_vec.size();
    if (num_total_active_pixels < M_ACTIVE_RATIO_TH * num_area_pixels)
    {
        flow_data.flow_center_ = QPoint(region_width/2, region_height/2);
        flow_data.flow_confidence_ = QPointF( num_total_active_pixels
                                               / (qreal)num_area_pixels, 0);
        flow_data.flow_velocity_ = QPointF(0,0);
        return;
    }

    // good active pixels with same direction
    bool *votes_active_vec = new bool[num_total_active_pixels];
    size_t num_good_active_pixels;
    ransac(active_point_vec, 0.99, &num_good_active_pixels, &votes_active_vec);

    // estimate good pixels
    std::vector<QPoint> consensus_pos;
    consensus_pos.reserve(num_good_active_pixels);
    flow_data.flow_velocity_ = QPointF();
    int num_inliers = 0;
    float xMax = std::numeric_limits<float>::min();
    float yMax = std::numeric_limits<float>::min();
    float xMin = std::numeric_limits<float>::max();
    float yMin = std::numeric_limits<float>::max();
    //qDebug() << "[[[xMin" << xMin << "[[[yMin" << yMin;
    QPoint *pPos = active_point_pos.data();
    QPointF *pVec = active_point_vec.data();
    for (size_t i = 0; i < num_total_active_pixels; ++i, ++pPos, ++pVec)
    {
        if (votes_active_vec[i])
        {
            consensus_pos.push_back(*pPos);
            flow_data.flow_velocity_ += *pVec;
            ++num_inliers;

            if (pPos->x() < xMin)
                xMin = pPos->x();
            if (pPos->x() > xMax)
                xMax = pPos->x();
            if (pPos->y() < yMin)
                yMin = pPos->y();
            if (pPos->y() > yMax)
                yMax = pPos->y();
        }
    }
    flow_data.flow_velocity_ /= num_inliers;
    flow_data.active_boundbox_rect_ = QRect(xMin, yMin, xMax - xMin, yMax - yMin);

    // set confidences
    qreal consensus_ratio = (qreal)num_inliers / (qreal) num_total_active_pixels;
    qreal active_ratio = (qreal) num_total_active_pixels / (qreal) num_area_pixels;
    flow_data.flow_confidence_ = QPointF(consensus_ratio, active_ratio);

    // center radius
    //center_radius_for_ransac_ = (region_width + region_height)
    //        * sqrt(active_ratio) * 2 / 3;

    // ransac center
    bool *votes_center = new bool[num_good_active_pixels];
    size_t num_good_center_votes;
    ransac(consensus_pos, 0.99, &num_good_center_votes, &votes_center);

    QPointF flow_center(0,0);
    flow_data.flow_points_.resize(num_good_center_votes);
    QPoint *pIn = flow_data.flow_points_.data();
    QPoint *p = consensus_pos.data();
    for (size_t i = 0; i < num_good_active_pixels; ++i, ++p)
    {
        if (votes_center[i])
        {
            flow_center += *p;
            *pIn = *p;
            ++pIn;
        }
    }
    flow_center /= num_good_center_votes;
    flow_data.flow_center_ = flow_center.toPoint();

    delete [] votes_active_vec;
    delete [] votes_center;
}

void GTracking::getActivePoints(const cv::Mat4b &flow, bool back,
                                std::vector<QPoint> &poses,
                                std::vector<QPointF> &vecs) const
{
    int off = (back?2:0);
    for ( int r = 0; r < flow.rows; ++r)
    {
        const cv::Vec4b *row = flow[r];
        for ( int c = 0; c < flow.cols; ++c )
        {
            const unsigned char *p = (const unsigned char *)(row + c) + off;
            if (!( p[0] == 128 && p[1] == 128))
            {
                QPoint uv(p[0]-128, p[1]-128);
                if ( uv.manhattanLength() > M_ACTIVE_LENGTH_TH )
                {
                    poses.push_back(QPoint(c,r));
                    vecs.push_back(uv);
                    continue;
                    //m_ransacData.push_back(GePointFlow(c,r,uv));
                }
            }
        }
    }
}

template<class T>
void GTracking::ransac(std::vector<T> &data,
                       double desired_prob_for_no_outliers,
                       size_t *num_votes, bool **out_best_votes)
{
    int num_objects = data.size();
    size_t all_tries = num_objects;
    size_t num_tries = all_tries;
    double numerator = log(1.0 - desired_prob_for_no_outliers);
    int max_num_fits = 0;
    T best_vote;
    bool *cur_votes = new bool[num_objects];
    bool *best_votes = new bool[num_objects];

    int n_try = 0;
    for (size_t i = 0; i < num_tries; ++i)
    {
        int select_index = (int)((rand()/(float)RAND_MAX) * num_objects) % num_objects;
        T pRand = data[select_index];

        std::fill(cur_votes, cur_votes + num_objects, false);
        int num_fit = 0;
        for (size_t j = 0; j < data.size(); ++j)
        {
            cur_votes[j] = true;
            ++num_fit;
        }
        if (num_fit > max_num_fits)
        {
            max_num_fits = num_fit;
            best_vote = pRand;
            std::copy(cur_votes, cur_votes + num_objects, best_votes);
        }

        if (num_fit == num_objects)
            i = num_tries;
        else
        {
            double denominator = log(1.0 - num_fit/(double)num_objects);
            num_tries = qMin((size_t)(numerator/denominator+0.5), all_tries);
        }
        ++n_try;
    }

    *num_votes = max_num_fits;
    if (out_best_votes)
        std::copy(best_votes, best_votes + num_objects, *out_best_votes);

    delete [] best_votes;
    delete [] cur_votes;
}