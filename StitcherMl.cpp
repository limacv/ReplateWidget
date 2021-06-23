#include "StitcherMl.h"
#include <QProgressDialog>
#include <QApplication>
#include <QThread>
#include <QMessageBox>
#include <QDebug>
#include <fstream>
#include <iostream>
#include <qelapsedtimer.h>
#include "GUtil.h"
#include <ceres/ceres.h>
#include <iomanip>
#include <sstream>
#include "GeCeresBA.hpp"
#include "MLProgressDialog.hpp"
#include "MLDataManager.h"

#include "MLBlender.hpp"

int StitcherMl::stitch(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks)
{
    int num_images_ = frames.size();
    full_img_size = frames[0].size();

    QElapsedTimer timer; 
    timer.start();
    work_scale = estimateScale(config()->work_megapix_, full_img_size);

    featureFinder(frames, m_features, masks);
    qDebug() << "featureFinder";

    progress_reporter->beginStage("Analying camera parameters");
    auto& cameras_out = cameras_;
    cameras_out.resize(num_images_);
    const int skip_frame = config_->stitch_skip_frame_ < 1 ? 1 : config_->stitch_skip_frame_;

    // level 0
    vector<int> level0_idx;
    for (int i = 0; i < num_images_; i += skip_frame)
        level0_idx.push_back(i);
    if (level0_idx[level0_idx.size() - 1] != num_images_ - 1) 
        level0_idx.push_back(num_images_ - 1);

    vector<ImageFeatures> features_l0; features_l0.reserve(level0_idx.size());
    vector<MatchesInfo> matches_l0;

    for (int idx: level0_idx)
        features_l0.push_back(m_features[idx]);

    pairwiseMatch(features_l0, matches_l0);
    if (progress_reporter->wasCanceled()) return -1;
    progress_reporter->setValue(0.5f / skip_frame);

    if (config_->stitcher_verbose)
    {
        vector<cv::Mat> img;
        for (int idx: level0_idx)
            img.push_back(frames[idx]);
        drawMatches(img, features_l0, matches_l0);
    }

    vector<CameraParams> cameras_l0;
    initialIntrinsics(features_l0, matches_l0, cameras_l0);
    bundleAdjustCeres(features_l0, matches_l0, cameras_l0);
    if (progress_reporter->wasCanceled()) return -1;
    progress_reporter->setValue(1.f / skip_frame);

    for (int i = 0; i < level0_idx.size(); ++i)
        cameras_out[level0_idx[i]] = cameras_l0[i];

    // level 1
    for (int i = 0; i < level0_idx.size() - 1; ++i)
    {
        int idx0 = level0_idx[i];
        int idx1 = std::min(level0_idx[i + 1], num_images_);
        vector<int> fixed_camera({ 0 });
        vector<int> fixed_camera_globally({ idx0 });

        vector<ImageFeatures> features_l1; features_l1.reserve(idx1 - idx0 + 3);
        features_l1.insert(features_l1.begin(), m_features.begin() + idx0, m_features.begin() + idx1);

        features_l1.push_back(m_features[idx1]);
        fixed_camera.push_back(features_l1.size() - 1);
        fixed_camera_globally.push_back(idx1);

        if (i >= 1)
        {
            features_l1.push_back(m_features[level0_idx[i - 1]]);
            fixed_camera.push_back(features_l1.size() - 1);
            fixed_camera_globally.push_back(level0_idx[i - 1]);
        }
        if (i < level0_idx.size() - 2)
        {
            features_l1.push_back(m_features[level0_idx[i + 2]]);
            fixed_camera.push_back(features_l1.size() - 1);
            fixed_camera_globally.push_back(level0_idx[i + 2]);
        }

        vector<MatchesInfo> matches_l1;
        pairwiseMatch(features_l1, matches_l1);

        vector<CameraParams> cameras_l1;
        initialIntrinsics(features_l1, matches_l1, cameras_l1, cameras_out[idx0].R);
        for (int j = 0; j < fixed_camera.size(); ++j)
            cameras_l1[fixed_camera[j]] = cameras_out[fixed_camera_globally[j]];

        bundleAdjustCeres(features_l1, matches_l1, cameras_l1, fixed_camera);

        for (int j = idx0; j < idx1; ++j)
            cameras_out[j] = cameras_l1[j - idx0];

        if (progress_reporter->wasCanceled()) return -1;
        progress_reporter->setValue(1.f / skip_frame + (1.f - 1.f / skip_frame) * (float)i / (level0_idx.size() - 1));
    }

    if (config()->wave_correct_ != "no")
    {
        vector<Mat> rmats;
        for (size_t i = 0; i < cameras_.size(); ++i)
            rmats.push_back(cameras_[i].R);
        waveCorrect(rmats, config()->wave_correct_type_);
        for (size_t i = 0; i < cameras_.size(); ++i)
            cameras_[i].R = rmats[i];
    }

    qInfo() << "Finish analying camera parameters: " << (double)timer.elapsed() / 1000 << " s";
    // the cameras_ is the config in the work scale, now we convert back to the original scale
    for (auto& camera_param : cameras_)
    {
        camera_param.focal /= work_scale;
        camera_param.ppx = (float)full_img_size.width / 2;
        camera_param.ppy = (float)full_img_size.height / 2;
    }

    progress_reporter->setValue(1.f);
    return 0;
}

int StitcherMl::warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks, std::vector<cv::Mat>& warped, std::vector<cv::Rect>& windows, cv::Mat& stitched)
{
    int ret;
    std::vector<cv::Mat> images_warped;
    std::vector<cv::Mat> masks_warped;
    std::vector<cv::Size> sizes;
    std::vector<cv::Point> corners;
    ret = warp_and_compositebyblend(frames, masks, images_warped, masks_warped, sizes, corners, stitched);

    if (ret != 0) return ret;

    const int num_img = images_warped.size();
    warped.resize(num_img);
    windows.resize(num_img);
    for (int i = 0; i < images_warped.size(); ++i)
    {
        cv::merge(std::vector<cv::Mat>({ images_warped[i], masks_warped[i] }), warped[i]);
        windows[i] = cv::Rect(corners[i], sizes[i]);
    }
    return 0;
}

void StitcherMl::featureFinder(const vector<Mat>& fullImages, vector<ImageFeatures>& features, const vector<Mat>& fullMasks) const
{
    int num_images_ = fullImages.size();
    features.resize(num_images_);

    Ptr<Feature2D> finder;
    if (config()->features_type_ == "surf")
    {
        finder = xfeatures2d::SURF::create(config_->features_thres_);
    }
    else if (config()->features_type_ == "orb")
    {
        finder = ORB::create();
    }
    else
    {
        cout << "Unknown 2D features type: '" << config()->features_type_ << "'.\n";
        return;
    }

    progress_reporter->beginStage("Detecting Features");
    Mat work_img;
    Mat work_mask;

    QMatrix mat;
    mat.scale(full_img_size.width, full_img_size.height);

    for (int i = 0; i < num_images_; ++i)
    {
        progress_reporter->setValue((float)i / num_images_);
        QApplication::processEvents();
        if (progress_reporter->wasCanceled()) {
            qWarning() << "Programe canceled. Detecting Features";
            exit(-1);
        }

        resize(fullMasks[i], work_mask, Size(), work_scale, work_scale, cv::INTER_NEAREST);
        resize(fullImages[i], work_img, Size(), work_scale, work_scale);
        computeImageFeatures(finder, work_img, features[i], work_mask);
        features[i].img_idx = i;

        if (config_->preview_)
        {
            for (size_t j = 0; j < features[i].keypoints.size(); j++)
                circle(work_img, features[i].keypoints[j].pt, 3, Scalar(255, 0, 0), -1, 8);

            imshow("FeatureFinder", work_img);
            char c = (char)waitKey(250);
            if (c == 'p') {
                c = 0;
                while (c != 'p' && c != 27) {
                    c = waitKey(10);
                }
            }
            if (c == 27)
                break;
        }

        qDebug() << "Features in image #" << i + 1 << ": " << features[i].keypoints.size();
    }
    //    finder->collectGarbage();
    work_img.release();

    progress_reporter->setValue(1.);
    qDebug() << "Finding features";
    
    //// filter logos
    //if (!MLDataManager::get().manual_masks.isEmpty())
    //{
    //    vector<Rect> logos;
    //    const auto& qlogos = MLDataManager::get().manual_masks;
    //    for (const auto& qlogo : qlogos)
    //    {
    //        QRectF rect = mat.mapRect(qlogo);
    //        logos.push_back(Rect(rect.x(), rect.y(), rect.width(), rect.height()));
    //    }
    //    _logoFilter(logos, features, work_scale);
    //}

}

double StitcherMl::estimateScale(double megapix_ratio, const Size& full_img_size) const
{
    return min(1.0, sqrt(megapix_ratio * 1e6 / full_img_size.area()));
}

void StitcherMl::pairwiseMatch(const vector<ImageFeatures>& features,
    vector<MatchesInfo>& pairwise_matches) const
{

    qDebug() << "Pairwise matchings..."; 

    // _pairwiseMatch1(features, pairwise_matches);
    Ptr<FeaturesMatcher> matcher = makePtr<BestOf2NearestMatcher>(true, config()->match_conf_);
    //else
        //matcher = makePtr<BestOf2NearestRangeMatcher>(config()->match_range_width, true, config()->match_conf_);
    (*matcher)(features, pairwise_matches);
    matcher->collectGarbage();
     
    qDebug() << "Fix match confidence... ";

    // 
    // qDebug() << "Limit the number of match numbers..." << _timer.elapsedSecondsF() << " sec";

    // for (size_t i = 0; i < pairwise_matches.size(); ++i) {
    //     restrictMatchNumber(pairwise_matches[i]);
    // }
     
    qDebug() << "Pairwise matching, time:  ";
    qDebug() << "# Pairwise matching: " << pairwise_matches.size();
}

void StitcherMl::initialIntrinsics(const vector<ImageFeatures>& features,
    vector<MatchesInfo>& pairwise_matches, vector<CameraParams>& cameras, const Mat& iniR) const
{
    HomographyBasedEstimator estimator;
    if (!estimator(features, pairwise_matches, cameras))
    {
        qCritical() << "Homography estimation failed\n";
        cout << "Homography estimation failed\n" << endl;
        exit(-1);
    }
    
    Mat init_rot;
    
    for (size_t i = 0; i < cameras.size(); ++i)
    {
        Mat R;
        cameras[i].R.convertTo(R, CV_32F);
        cameras[i].R = R;

        if (i == 0)
        {
            if (iniR.empty()) init_rot = Mat::eye(3, 3, CV_32F);
            else init_rot = cameras[0].R.inv() * iniR;
            //else init_rot = iniR * cameras[0].R.inv();
        }
        cameras[i].R = cameras[i].R * init_rot;
        //cameras[i].R = init_rot * cameras[i].R;
        qDebug() << "Initial camera intrinsics #" << i + 1 << ":\nK:\n";
    }

    qDebug() << "Initial intrinsics finished";
}

void StitcherMl::bundleAdjustCeres(const vector<ImageFeatures>& features,
    const vector<MatchesInfo>& pairwise_matches, vector<CameraParams>& cameras, 
    const vector<int>& fixed_camera_idx) const
{
    qDebug() << "Bundle Adjusting... ";
    ceres::Problem problem;

    vector<double> camera_inout;
    vector<double> camera_ppxy;
    for (size_t i = 0; i < cameras.size(); ++i)
    {
        Mat r1;
        cameras[i].R.convertTo(r1, CV_64F);
        SVD svd;
        svd(r1, SVD::FULL_UV);
        Mat R1 = svd.u * svd.vt;
        if (determinant(R1) < 0)
            R1 *= -1;

        CV_Assert(R1.type() == CV_64F);

        // convert to ceres rotation matrix, which is the transpose matrix of cv::Mat
        Mat rT = R1.t();
        double r[9];
        for (int j = 0; j < 9; ++j)
            r[j] = rT.at<double>(j / 3, j % 3);
        double rvec[3];
        ceres::RotationMatrixToAngleAxis(r, rvec);

        //Mat rvec2;
        //Rodrigues(R1, rvec2);
        //cout << rvec[0] << " " << rvec[1] << " " << rvec[2] << " " << rvec2 << endl;

        camera_inout.push_back(cameras[i].focal);
        for (int j = 0; j < 3; ++j)
            camera_inout.push_back(rvec[j]);
        camera_ppxy.push_back(cameras[i].ppx);
        camera_ppxy.push_back(cameras[i].ppy);
    }

    vector<double> tmp;
    vector<int> tmpId;
    for (size_t match_idx = 0; match_idx < pairwise_matches.size(); ++match_idx)
    {
        const MatchesInfo& matches_info = pairwise_matches[match_idx];
        if (matches_info.num_inliers < 5)
            continue;
        int i = matches_info.src_img_idx;
        int j = matches_info.dst_img_idx;
        if (i >= j)
            continue;
        const ImageFeatures& features1 = features[i];
        const ImageFeatures& features2 = features[j];

        for (size_t k = 0; k < matches_info.matches.size(); ++k)
        {
            if (matches_info.inliers_mask[k] <= 0)
                continue;
            const DMatch& m = matches_info.matches[k];
            Point2f p1 = features1.keypoints[m.queryIdx].pt;
            Point2f p2 = features2.keypoints[m.trainIdx].pt;
            //double p[2][2];
            //p[0][0] = p1.x;
            //p[0][1] = p1.y;
            //p[1][0] = p2.x;
            //p[1][1] = p2.y;
            tmp.push_back(p1.x);
            tmp.push_back(p1.y);
            tmp.push_back(p2.x);
            tmp.push_back(p2.y);
            tmpId.push_back(i);
            tmpId.push_back(j);
        }
    }

    for (size_t idx = 0; idx < tmp.size() / 4; ++idx)
    {
        int i = tmpId[2 * idx];
        int j = tmpId[2 * idx + 1];
        ceres::CostFunction* cost_function =
            RayBundleError::Create(tmp.data() + idx * 4, camera_ppxy.data() + 2 * i,
                tmp.data() + idx * 4 + 2, camera_ppxy.data() + 2 * j);
        problem.AddResidualBlock(cost_function,
            NULL /* squared loss */,
            camera_inout.data() + 4 * i,
            camera_inout.data() + 4 * j
        );

        auto i_idx = std::find(fixed_camera_idx.begin(), fixed_camera_idx.end(), i),
            j_idx = std::find(fixed_camera_idx.begin(), fixed_camera_idx.end(), j);

        if (i_idx != fixed_camera_idx.end())
            problem.SetParameterBlockConstant(camera_inout.data() + 4 * (*i_idx));
        if (j_idx != fixed_camera_idx.end())
            problem.SetParameterBlockConstant(camera_inout.data() + 4 * (*j_idx));
    }

    ceres::Solver::Options options;
    //    options.linear_solver_type = ceres::SPARSE_SCHUR;
    options.linear_solver_type = ceres::DENSE_SCHUR;
    options.max_num_iterations = 1000;
    options.num_threads = 8;
    //IterErrorCallback *cb = new IterErrorCallback(&pano_problem);
    //options.callbacks.push_back(cb);
    //options.update_state_every_iteration = true;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    std::cout << summary.FullReport() << "\n";

    for (size_t i = 0; i < cameras.size(); ++i)
    {
        cameras[i].focal = camera_inout[4 * i];
        double r[9];
        ceres::AngleAxisToRotationMatrix(camera_inout.data() + 4 * i + 1, r);
        Mat R = Mat(3, 3, CV_64F, r).t();
        Mat R32;
        R.convertTo(R32, CV_32F);
        cameras[i].R = R32;
    }

    //if (!wave_correct_.empty())
    //{
    //    cout << "HERE" << endl;
    //    vector<Mat> rmats;
    //    for (size_t i = 0; i < cameras.size(); ++i)
    //        rmats.push_back(cameras[i].R);
    //    waveCorrect(rmats, wave_correct_type_);
    //    for (size_t i = 0; i < cameras.size(); ++i)
    //        cameras[i].R = rmats[i];
    //}
    
    qDebug() << "Bundle Adjust ";
}

/// <summary>
/// require: cameras
/// write to: images_warped, masks_warped, sizes, corners
/// </summary>
int StitcherMl::warp_and_compositebyblend(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& inmasks,
    std::vector<cv::Mat>& images_warped, std::vector<cv::Mat>& masks_warped,
    std::vector<cv::Size>& sizes, std::vector<cv::Point>& corners, cv::Mat& stitch_result)
{
    // Warp images and their masks
    auto& warp_type = config_->warp_type_;
    if (progress_reporter) progress_reporter->beginStage("Warping... Warp Type: " + warp_type);
    const int num_images = frames.size();
    images_warped.resize(num_images);
    masks_warped.resize(num_images);
    sizes.resize(num_images);
    corners.resize(num_images);

    vector<double> focals;
    
    decltype(cameras_) cameras;
    cameras.assign(cameras_.begin(), cameras_.end());

    for (size_t i = 0; i < cameras.size(); ++i)
    {
        //G_LOG("Camera #" << newIndices[i]+1 << ":");// << cameras[i].K());
        focals.push_back(cameras[i].focal);
    }

    sort(focals.begin(), focals.end());
    double warped_image_scale = 0;
    if (focals.size() % 2 == 1)
        warped_image_scale = static_cast<float>(focals[focals.size() / 2]);
    else
        warped_image_scale = static_cast<float>(focals[focals.size() / 2 - 1] + focals[focals.size() / 2]) * 0.5f;


    Ptr<WarperCreator> warper_creator;
    if (warp_type == "plane")
        warper_creator = makePtr<cv::PlaneWarper>();
    else if (warp_type == "affine")
        warper_creator = makePtr<cv::AffineWarper>();
    else if (warp_type == "cylindrical")
        warper_creator = makePtr<cv::CylindricalWarper>();
    else if (warp_type == "spherical")
        warper_creator = makePtr<cv::SphericalWarper>();
    else if (warp_type == "fisheye")
        warper_creator = makePtr<cv::FisheyeWarper>();
    else if (warp_type == "stereographic")
        warper_creator = makePtr<cv::StereographicWarper>();
    else if (warp_type == "compressedPlaneA2B1")
        warper_creator = makePtr<cv::CompressedRectilinearWarper>(2.0f, 1.0f);
    else if (warp_type == "compressedPlaneA1.5B1")
        warper_creator = makePtr<cv::CompressedRectilinearWarper>(1.5f, 1.0f);
    else if (warp_type == "compressedPlanePortraitA2B1")
        warper_creator = makePtr<cv::CompressedRectilinearPortraitWarper>(2.0f, 1.0f);
    else if (warp_type == "compressedPlanePortraitA1.5B1")
        warper_creator = makePtr<cv::CompressedRectilinearPortraitWarper>(1.5f, 1.0f);
    else if (warp_type == "paniniA2B1")
        warper_creator = makePtr<cv::PaniniWarper>(2.0f, 1.0f);
    else if (warp_type == "paniniA1.5B1")
        warper_creator = makePtr<cv::PaniniWarper>(1.5f, 1.0f);
    else if (warp_type == "paniniPortraitA2B1")
        warper_creator = makePtr<cv::PaniniPortraitWarper>(2.0f, 1.0f);
    else if (warp_type == "paniniPortraitA1.5B1")
        warper_creator = makePtr<cv::PaniniPortraitWarper>(1.5f, 1.0f);
    else if (warp_type == "mercator")
        warper_creator = makePtr<cv::MercatorWarper>();
    else if (warp_type == "transverseMercator")
        warper_creator = makePtr<cv::TransverseMercatorWarper>();
    else
    {
        cout << "Can't create the following warper '" << warp_type << "'\n";
        return 1;
    }
    
    seam_scale = estimateScale(config()->seam_megapix_, full_img_size);
    m_warper = warper_creator->create(warped_image_scale * seam_scale);

    /***************
    * find seam in seam_scale and compute compensate of exposure
    *****************/
    QElapsedTimer timer; timer.start();
    Ptr<ExposureCompensator> compensator = ExposureCompensator::createDefault(config()->expos_comp_type_);
    {
        vector<UMat> images_warp_4seam(num_images);
        vector<UMat> seammasks_warp(num_images);
        for (int i = 0; i < num_images; ++i)
        {
            if (progress_reporter) progress_reporter->setValue(0.15 * i / num_images);
            Mat_<float> K;
            cameras[i].K().convertTo(K, CV_32F);
            K(0, 0) *= seam_scale; K(0, 2) *= seam_scale;
            K(1, 1) *= seam_scale; K(1, 2) *= seam_scale;

            cv::Mat image, mask;
            resize(frames[i], image, Size(), seam_scale, seam_scale, INTER_LINEAR_EXACT);
            resize(inmasks[i], mask, Size(), seam_scale, seam_scale, INTER_NEAREST);
            corners[i] = m_warper->warp(image, K, cameras[i].R, INTER_LINEAR, BORDER_REFLECT, images_warp_4seam[i]);
            sizes[i] = images_warp_4seam[i].size();
            m_warper->warp(mask, K, cameras[i].R, INTER_NEAREST, BORDER_CONSTANT, seammasks_warp[i]);
        }
        //LOGLN("Compensating exposure...");

        if (dynamic_cast<GainCompensator*>(compensator.get()))
        {
            GainCompensator* gcompensator = dynamic_cast<GainCompensator*>(compensator.get());
            gcompensator->setNrFeeds(config()->expos_comp_nr_feeds);
        }
        if (dynamic_cast<ChannelsCompensator*>(compensator.get()))
        {
            ChannelsCompensator* ccompensator = dynamic_cast<ChannelsCompensator*>(compensator.get());
            ccompensator->setNrFeeds(config()->expos_comp_nr_feeds);
        }
        if (dynamic_cast<BlocksCompensator*>(compensator.get()))
        {
            BlocksCompensator* bcompensator = dynamic_cast<BlocksCompensator*>(compensator.get());
            bcompensator->setNrFeeds(config()->expos_comp_nr_feeds);
            bcompensator->setNrGainsFilteringIterations(config()->expos_comp_nr_filtering);
            bcompensator->setBlockSize(config()->expos_comp_block_size, config()->expos_comp_block_size);
        }
        compensator->feed(corners, images_warp_4seam, seammasks_warp);
        if (progress_reporter) progress_reporter->setValue(0.2);
    }

    qInfo() << "Exposure Compensate finished: " << timer.elapsed() << "ms" << endl;
    // Compute relative scales
    compose_scale = estimateScale(config()->compose_megapix_, full_img_size);
    m_warper = warper_creator->create(warped_image_scale * compose_scale);

    //LOGLN("Compositing...");
    Mat full_img, full_mask;
    Mat img_warped_s;
    Ptr<Blender> blender;
    images_warped.resize(num_images);
    masks_warped.resize(num_images);
    timer.restart();
    for (int img_idx = 0; img_idx < num_images; ++img_idx)
    {
        if (progress_reporter) progress_reporter->setValue(0.2 + 0.7 * img_idx / num_images);
        //LOGLN("Compositing image #" << indices[img_idx] + 1);
        // Read image and resize it if necessary
        full_img = frames[img_idx];
        full_mask = inmasks[img_idx];

        Size img_size = full_img.size();
        Mat_<float> K;
        cameras[img_idx].K().convertTo(K, CV_32F);
        K(0, 0) *= compose_scale; K(0, 2) *= compose_scale;
        K(1, 1) *= compose_scale; K(1, 2) *= compose_scale;
        Mat img_warped, blend_mask_warped, valid_mask_warped;
        valid_mask_warped = Mat(img_size, CV_8UC1, cv::Scalar(255));
        // Warp the current image
        m_warper->warp(full_img, K, cameras[img_idx].R, INTER_LINEAR, BORDER_REFLECT, img_warped);
        // Warp the current image mask
        m_warper->warp(full_mask, K, cameras[img_idx].R, INTER_NEAREST, BORDER_CONSTANT, blend_mask_warped);
        m_warper->warp(valid_mask_warped, K, cameras[img_idx].R, INTER_NEAREST, BORDER_CONSTANT, masks_warped[img_idx]);

        // Compensate exposure
        compensator->apply(img_idx, corners[img_idx], img_warped, blend_mask_warped);
        //if (blend_mask_warped.empty())
            //blend_mask_warped = img_warped;
        img_warped.convertTo(img_warped_s, CV_16S);
        images_warped[img_idx] = img_warped;
        
        if (img_idx == 0)
        {
            // Update corners and sizes
            for (int i = 0; i < num_images; ++i)
            {
                // Update corner and size
                Size sz = full_img.size();
                Mat_<float> K;
                cameras[i].K().convertTo(K, CV_32F);
                K(0, 0) *= compose_scale; K(0, 2) *= compose_scale;
                K(1, 1) *= compose_scale; K(1, 2) *= compose_scale;
                Rect roi = m_warper->warpRoi(sz, K, cameras[i].R);
                corners[i] = roi.tl();
                sizes[i] = roi.size();
            }
        //}
        //if (!blender)
        //{
            const auto& blend_type = config()->blend_type_;
            if (blend_type >= 3)
                blender = MLBlender::createDefault();
            else
                blender = Blender::createDefault(blend_type, false);
            Size dst_sz = resultRoi(corners, sizes).size();
            float blend_width = sqrt(static_cast<float>(dst_sz.area())) * config()->blend_strength_ / 100.f;
            if (blend_width < 1.f)
                blender = Blender::createDefault(Blender::NO);
            else if (blend_type == Blender::MULTI_BAND)
            {
                MultiBandBlender* mb = dynamic_cast<MultiBandBlender*>(blender.get());
                mb->setNumBands(static_cast<int>(ceil(log(blend_width) / log(2.)) - 1.));
                //LOGLN("Multi-band blender, number of bands: " << mb->numBands());
            }
            else if (blend_type == Blender::FEATHER)
            {
                FeatherBlender* fb = dynamic_cast<FeatherBlender*>(blender.get());
                fb->setSharpness(1.f / blend_width);
                //LOGLN("Feather blender, sharpness: " << fb->sharpness());
            }
            blender->prepare(corners, sizes);
        }
        blender->feed(img_warped_s, blend_mask_warped, corners[img_idx]);
    }

    qInfo() << "Blending finished: " << timer.elapsed() << "ms " << endl;
    Mat result, result_mask;
    blender->blend(result, result_mask);
    result.convertTo(result, CV_8UC3);
    result_mask.convertTo(result_mask, CV_8UC1);
    cv::merge(std::vector<Mat>({ result, result_mask }), stitch_result);
    if (progress_reporter) progress_reporter->setValue(1.f);
    //LOGLN("Compositing, time: " << ((getTickCount() - t) / getTickFrequency()) << " sec");
    //imwrite(result_name, result);
    return 0;
}

void StitcherMl::_boxesFilter(const vector<Rect>& logoMask, ImageFeatures& features, float scale) const
{
    if (features.keypoints.size() == 0 || logoMask.size() == 0)
        return;

    vector<KeyPoint> newKeyPoint;
    vector<uchar> mask;
    for (size_t i = 0; i < features.keypoints.size(); ++i)
    {
        const KeyPoint& kp = features.keypoints[i];
        bool in = false;
        for (size_t j = 0; j < logoMask.size(); ++j)
        {
            if (logoMask[j].contains(kp.pt * (1.0 / scale)))
            {
                in = true;
                break;
            }
        }
        mask.push_back(!in);
        if (!in)
            newKeyPoint.push_back(kp);
    }
    if (newKeyPoint.size() < features.keypoints.size())
    {
        Mat newDesc = Mat(newKeyPoint.size(), features.descriptors.cols, features.descriptors.type());
        for (size_t i = 0, k = 0; i < mask.size(); ++i)
        {
            if (mask[i])
                features.descriptors.row(i).copyTo(newDesc.row(k++));
        }
        features.keypoints.assign(newKeyPoint.begin(), newKeyPoint.end());
        features.descriptors.release();
        features.descriptors = newDesc.clone().getUMat(ACCESS_RW);
    }
}


void StitcherMl::_detectionFilter(vector<ImageFeatures>& features) const
{
    const auto& allboxes = MLDataManager::get().trajectories.frameidx2detectboxes;
    for (int i = 0; i < features.size(); ++i)
    {
        vector<Rect> boxes; boxes.reserve(allboxes[i].size());
        for (const auto& box : allboxes[i])
            if (!box->empty()) boxes.push_back(box->rect);
        _boxesFilter(boxes, features[i], work_scale);
    }
}

void StitcherMl::_logoFilter(const vector<Rect>& logoMask, vector<ImageFeatures>& features, float scale) const
{
    for (size_t i = 0; i < features.size(); ++i)
    {
        _boxesFilter(logoMask, features[i], scale);
    }
}


int StitcherMl::warp_points(const int frameidx, std::vector<cv::Point>& inoutpoints) const
{
    if (m_warper == nullptr)
        return false;

    Mat K;
    cameras_[frameidx].K().convertTo(K, CV_32F);
    for (auto& point : inoutpoints)
    {
        cv::Point warp = m_warper->warpPoint(point, K, cameras_[frameidx].R);
        point = warp;
    }
    return true;
}

void StitcherMl::drawMatches(const vector<Mat>& images, vector<ImageFeatures>& features,
    vector<MatchesInfo>& pairwise_matches) const
{
    qDebug() << "Drawing Matches to JPG";
    
    int num_images_ = images.size();
    vector<Mat3b> imageWork(num_images_);
    for (int i = 0; i < num_images_; ++i)
        resize(images[i], imageWork[i], Size(), work_scale, work_scale);

    for (int i = 0; i < num_images_; ++i)
    {
        for (int j = 0; j < num_images_; ++j)
        {
            cv::Mat m;
            MatchesInfo& mi = pairwise_matches[i * num_images_ + j];

            cv::drawMatches(imageWork[i], features[i].keypoints, imageWork[j],
                features[j].keypoints, mi.matches, m);
            char filename[128];
            sprintf_s(filename, "matches/match_%d_%d_%d.jpg", i, j, mi.num_inliers);
            imwrite(filename, m);
        }
    }

    
    qDebug() << "Draw Matches to JPG, time:  ";
}