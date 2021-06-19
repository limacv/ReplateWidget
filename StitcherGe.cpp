#include "StitcherGe.h"
#include <QProgressDialog>
#include <QApplication>
#include <QThread>
#include <QMessageBox>
#include <QDebug>
#include <fstream>
#include "GUtil.h"
#include <ceres/ceres.h>
#include <iomanip>
#include <sstream>
#include "GeCeresBA.hpp"
#include "MLProgressDialog.hpp"

int StitcherGe::stitch(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks)
{
    int num_images_ = frames.size();
    full_img_size = frames[0].size();

    estimateWorkScale(work_scale, config()->work_megapix_, full_img_size);

    featureFinder(frames, m_features);

    qDebug() << "featureFinder";

    pairwiseMatch(m_features, m_pairwise_matches);

    qDebug() << "pairwiseMatch";

    initialIntrinsics(m_features, m_pairwise_matches, config_->cameras_);

    qDebug() << "initialIntrinsics";

    //if (saveBA)
    //    saveCamerasAndMatch(bundle_adjust_file, m_cameras, m_features, m_pairwise_matches);

    bundleAdjustCeres(m_features, m_pairwise_matches, config_->cameras_);
    // bundleAdjust(m_features, m_pairwise_matches, config_->cameras_);

    qDebug() << "bundleAdjustCeres";

    if (config()->wave_correct_ != "no")
    {
        vector<Mat> rmats;
        for (size_t i = 0; i < config_->cameras_.size(); ++i)
            rmats.push_back(config_->cameras_[i].R);
        waveCorrect(rmats, config()->wave_correct_type_);
        for (size_t i = 0; i < config_->cameras_.size(); ++i)
            config_->cameras_[i].R = rmats[i];
    }
    // the cameras_ is the config in the work scale, now we convert back to the original scale
    for (auto& camera_param : config_->cameras_)
    {
        camera_param.focal /= work_scale;
        camera_param.ppx = (float)full_img_size.width / 2;
        camera_param.ppy = (float)full_img_size.height / 2;
    }
    return 0;
}


int StitcherGe::warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks, std::vector<cv::Mat>& warped, std::vector<cv::Rect>& windows, cv::Mat& stitched)
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

void StitcherGe::featureFinder(const vector<Mat>& fullImages, vector<ImageFeatures>& features) const
{
    int num_images_ = fullImages.size();
    features.resize(num_images_);

    Ptr<Feature2D> finder;
    if (config()->features_type_ == "surf")
    {
        finder = xfeatures2d::SURF::create(400.);
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

        resize(fullImages[i], work_img, Size(), work_scale, work_scale);
        computeImageFeatures(finder, work_img, features[i]);
        features[i].img_idx = i;

        if (config()->preview_)
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
}

void StitcherGe::estimateWorkScale(double& work_scale, double megapix_ratio,
    const Size& full_img_size) const
{
    work_scale = min(1.0, sqrt(megapix_ratio * 1e6 / full_img_size.area()));
}

void StitcherGe::estimateSeamScale(double& seam_scale, double megapix_ratio,
    const Size& full_img_size) const
{
    seam_scale = min(1.0, sqrt(megapix_ratio * 1e6 / full_img_size.area()));
}

void StitcherGe::estimateWarpScale(double& warp_scale, double megapix_ratio,
    const Size& full_img_size) const
{
    warp_scale = min(1.0, sqrt(megapix_ratio * 1e6 / full_img_size.area()));
}

void StitcherGe::saveCameras(const string& filename, const vector<CameraParams>& cameras) const
{
    qDebug() << "Saving Cameras...";

    ofstream fout(filename);
    //fout << cameras;
    fout << cameras.size() << endl;
    for (size_t i = 0; i < cameras.size(); ++i)
    {
        fout << cameras[i].K().at<double>(0, 0) << " " << cameras[i].K().at<double>(0, 2) << " "
            << cameras[i].K().at<double>(1, 2) << " ";
        SVD svd;
        svd(cameras[i].R, SVD::FULL_UV);
        Mat R = svd.u * svd.vt;
        if (determinant(R) < 0)
            R *= -1;
        Mat r = R.t();
        for (int j = 0; j < 9; ++j)
            fout << r.at<float>(j / 3, j % 3) << " ";
        fout << endl;
    }
    fout.close();

    qDebug() << "Save Cameras";
}

void StitcherGe::saveCamerasAndMatch(const string& filename, const vector<CameraParams>& cameras,
    const vector<ImageFeatures>& features, const vector<MatchesInfo>& pairwise_matches) const
{
    qDebug() << "Saving Cameras & Matches...";

    ofstream fout(filename);
    int num = 0;
    for (size_t i = 0; i < pairwise_matches.size(); ++i)
    {
        if (pairwise_matches[i].confidence < config()->conf_thresh_)
            continue;
        num += pairwise_matches[i].num_inliers;
    }
    fout << cameras.size() << " " << num << endl;
    //save cameras
    for (size_t i = 0; i < cameras.size(); ++i)
    {
        // focal ppx ppy
        fout << cameras[i].focal << " " << features[i].img_size.width / 2.0
            << " " << features[i].img_size.height / 2.0 << " ";
        //for (int j = 0; j < 9; ++j)
        //    fout << cameras[i].K().at<double>(j/3, j%3) << " ";
        SVD svd;
        svd(cameras[i].R, SVD::FULL_UV);
        Mat R = svd.u * svd.vt;
        if (determinant(R) < 0)
            R *= -1;
        Mat r = R.t();
        for (int j = 0; j < 9; ++j)
            fout << (double)(r.at<float>(j / 3, j % 3)) << " ";
        //for (int j = 0; j < 3; ++j)
        //    fout << cameras[i].t.at<double>(j, 0) << " ";
        fout << endl;
    }

    //double errSum = 0;
    for (size_t idx = 0; idx < pairwise_matches.size(); ++idx)
    {
        const MatchesInfo& matches_info = pairwise_matches[idx];
        if (matches_info.confidence < config()->conf_thresh_)
            continue;
        int i = matches_info.src_img_idx;
        int j = matches_info.dst_img_idx;
        const ImageFeatures& features1 = features[i];
        const ImageFeatures& features2 = features[j];

        Mat r1;
        cameras[i].R.convertTo(r1, CV_64F);
        SVD svd;
        svd(r1, SVD::FULL_UV);
        Mat R1 = svd.u * svd.vt;
        if (determinant(R1) < 0)
            R1 *= -1;
        Mat rvec;
        Rodrigues(R1, rvec);
        Mat R1_(3, 3, CV_64F);
        Rodrigues(rvec, R1_);
        Mat_<double> H1 = R1_ * cameras[i].K().inv();

        Mat r2;
        cameras[j].R.convertTo(r2, CV_64F);
        svd(r2, SVD::FULL_UV);
        Mat R2 = svd.u * svd.vt;
        if (determinant(R2) < 0)
            R2 *= -1;
        Mat rvec2;
        Rodrigues(R2, rvec2);
        Mat R2_(3, 3, CV_64F);
        Rodrigues(rvec2, R2_);
        Mat_<double> H2 = R2_ * cameras[j].K().inv();

        for (size_t k = 0; k < matches_info.matches.size(); ++k)
        {
            if (matches_info.inliers_mask[k] <= 0)
                continue;
            const DMatch& m = matches_info.matches[k];
            Point2f p1 = features1.keypoints[m.queryIdx].pt;
            Point2f p2 = features2.keypoints[m.trainIdx].pt;
            fout << i << " " << j << " " << p1.x << " " << p1.y << " "\
                << p2.x << " " << p2.y << endl;/**/

                /*double x1 = H1(0,0)*p1.x + H1(0,1)*p1.y + H1(0,2);
                double y1 = H1(1,0)*p1.x + H1(1,1)*p1.y + H1(1,2);
                double z1 = H1(2,0)*p1.x + H1(2,1)*p1.y + H1(2,2);
                double len = sqrt(x1*x1 + y1*y1 + z1*z1);
                x1 /= len; y1 /= len; z1 /= len;

                double x2 = H2(0,0)*p2.x + H2(0,1)*p2.y + H2(0,2);
                double y2 = H2(1,0)*p2.x + H2(1,1)*p2.y + H2(1,2);
                double z2 = H2(2,0)*p2.x + H2(2,1)*p2.y + H2(2,2);
                len = sqrt(x2*x2 + y2*y2 + z2*z2);
                x2 /= len; y2 /= len; z2 /= len;
                double len2 = sqrt(x2*x2 + y2*y2 + z2*z2);
                x2 /= len2; y2 /= len2; z2 /= len2;

                double err = (x1 - x2)*(x1 - x2) + (y1 - y2)*(y1 - y2) + (y1 - y2)*(y1 - y2);
                fout << i << " " << j << " " << err << " ";
                fout << setprecision(1) << std::fixed << p1.x << " " << p1.y << " " << p2.x << " " << p2.y << "    ";
                fout << setprecision(4) << std::fixed << x1 << " " << y1 << " " << z1 << " " \
                    << x2 << " " << y2 << " " << z2 << endl;*/
                    //errSum += err;
        }
    }
    //cout << errSum << endl;

    fout.flush();
    fout.close();

    qDebug() << "Save Cameras & Matches";
}

void StitcherGe::_loadCameras(const string& filename, vector<CameraParams>& cameras) const
{
    ifstream fin(filename);
    if (!fin)
    {
        cout << "Can't load file " << filename << endl;
        return;
    }

    int nCameras;
    fin >> nCameras;
    cameras.resize(nCameras);
    for (int i = 0; i < nCameras; ++i)
    {
        double k[9], R[9], t[3];
        for (int j = 0; j < 9; ++j)
            fin >> k[j];
        for (int j = 0; j < 9; ++j)
            fin >> R[j];
        for (int j = 0; j < 3; ++j)
            fin >> t[j];
        cameras[i].focal = k[0];
        cameras[i].ppx = k[2];
        cameras[i].ppy = k[5];
        cameras[i].R = Mat(3, 3, CV_64F, R).clone();
        //double a = cameras[i].R.at<double>(0, 0);
        cameras[i].t = Mat(3, 1, CV_64F, t).clone();
    }
    fin.close();
}

void StitcherGe::_loadRotCameras(const string& filename, vector<CameraParams>& cameras) const
{
    ifstream fin(filename);
    if (!fin)
    {
        cout << "Can't load file " << filename << endl;
        return;
    }

    int nCameras;
    fin >> nCameras;
    cameras.resize(nCameras);
    for (int i = 0; i < nCameras; ++i)
    {
        double focal, ppx, ppy;
        fin >> focal >> ppx >> ppy;
        double k[9], R[9];
        double t[3] = { 0,0,0 };
        //for (int j = 0; j < 9; ++j)
        //    fin >> k[j];
        for (int j = 0; j < 9; ++j)
            fin >> R[j];
        //for (int j = 0; j < 3; ++j)
        //    fin >>t[j];
        cameras[i].focal = focal;
        cameras[i].ppx = ppx;
        cameras[i].ppy = ppy;
        cameras[i].R = Mat(3, 3, CV_64F, R).t();
        cameras[i].t = Mat(3, 1, CV_64F, t).clone();
    }
    fin.close();
}

void StitcherGe::loadCameras(const string& filename, vector<CameraParams>& cameras) const
{
    qDebug() << "Loading cameras...";

    if (strstr(filename.c_str(), ".cam"))
        _loadCameras(filename, cameras);
    else if (strstr(filename.c_str(), ".rotcam"))
        _loadRotCameras(filename, cameras);
    else if (strstr(filename.c_str(), ".ba"))
        _loadRotCameras(filename, cameras);

    qDebug() << "Load cameras ";

}

void StitcherGe::_pairwiseMatch1(const vector<ImageFeatures>& features, vector<MatchesInfo>& pairwise_matches) const
{
    //vector<MatchesInfo> pairwise_matches;
    //int neighborRange = 4;
    progress_reporter->beginStage("Matching features. Very slow at 50%, be patient");

    BestOf2NearestMatcher matcher(config()->try_gpu_, config()->match_conf_);
    int nFeatures = features.size();
    Mat1b match_mask(nFeatures, nFeatures);
    match_mask.setTo(cv::Scalar::all(0));
    const static int FT_NEIGHBOR = 33;
    const static int Fibonacci[] = { 1,2,3,5,8,13,21,34,55,89 };

    progress_reporter->setValue(0);
    QApplication::processEvents();
    if (progress_reporter->wasCanceled()) {
        qWarning() << "Programe canceled. Detecting Features";
        exit(-1);
    }

    for (size_t i = 0; i < features.size(); ++i)
    {

        for (size_t j = 0; j < 10 && (i + Fibonacci[j]) < features.size(); ++j)
            match_mask[i][i + Fibonacci[j]] = 255;
        //        for( size_t j = 1 ; i + j < features.size(); ++j)
        //            match_mask[i][i + j] = 255;
    }


    matcher(features, pairwise_matches, match_mask.getUMat(cv::ACCESS_READ));

    progress_reporter->setValue(1);
}

void StitcherGe::_pairwiseMatch2(const vector<ImageFeatures>& features, vector<MatchesInfo>& pairwise_matches) const
{
    //vector<MatchesInfo> pairwise_matches;
    //int neighborRange = 4;
    BestOf2NearestMatcher matcher(config()->try_gpu_, config()->match_conf_);
    int nFeatures = features.size();
    Mat1b match_mask(nFeatures, nFeatures);
    match_mask.setTo(cv::Scalar::all(0));
    const static int FT_NEIGHBOR = 5;
    const static int test[] = { 1,2,3,4,5,10,20 };
    for (size_t i = 0; i < features.size(); ++i)
    {
        for (size_t j = 1; j < FT_NEIGHBOR && (i + j) < features.size(); j *= 2)
            match_mask[i][i + j] = 255;
        //        for (size_t j = 0; j < 7 && (i+test[j]) < features.size(); ++j)
        //            match_mask[i][i+test[j]] = 255;

        //        for (size_t j = 1; i + j < features.size(); ++j)
        //            match_mask[i][i+j] = 255;
    }
    matcher(features, pairwise_matches, match_mask.getUMat(cv::ACCESS_READ));
}


void StitcherGe::pairwiseMatch(const vector<ImageFeatures>& features,
    vector<MatchesInfo>& pairwise_matches) const
{

    qDebug() << "Pairwise matchings..."; 

    // _pairwiseMatch1(features, pairwise_matches);
    Ptr<FeaturesMatcher> matcher;
    if (config()->matcher_type == "affine")
        matcher = makePtr<AffineBestOf2NearestMatcher>(false, true, config()->match_conf_);
    else if (config()->match_range_width == -1)
        matcher = makePtr<BestOf2NearestMatcher>(true, config()->match_conf_);
    else
        matcher = makePtr<BestOf2NearestRangeMatcher>(config()->match_range_width, true, config()->match_range_width);
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

void StitcherGe::restrictMatchNumber(MatchesInfo& info) const
{
    int NDST_INLINERS = 500;
    if (info.num_inliers < NDST_INLINERS)
        return;
    std::vector<DMatch> matches;
    for (size_t i = 0; i < info.inliers_mask.size(); ++i)
        if (info.inliers_mask[i])
            matches.push_back(info.matches[i]);
    std::sort(matches.begin(), matches.end());
    std::vector<uchar> mask(NDST_INLINERS, 1);
    info.matches.assign(matches.begin(), matches.begin() + NDST_INLINERS);
    info.inliers_mask.assign(mask.begin(), mask.end());
    info.num_inliers = NDST_INLINERS;
}

void StitcherGe::initialIntrinsics(const vector<ImageFeatures>& features,
    vector<MatchesInfo>& pairwise_matches, vector<CameraParams>& cameras) const
{
    HomographyBasedEstimator estimator;
    if (!estimator(features, pairwise_matches, cameras))
    {
        qCritical() << "Homography estimation failed\n";
        cout << "Homography estimation failed\n" << endl;
        exit(-1);
    }

    for (size_t i = 0; i < cameras.size(); ++i)
    {
        Mat R;
        cameras[i].R.convertTo(R, CV_32F);
        cameras[i].R = R;
        qDebug() << "Initial camera intrinsics #" << i + 1 << ":\nK:\n";
    }

    qDebug() << "Initial intrinsics finished";
}


void StitcherGe::bundleAdjust(const vector<ImageFeatures>& features,
    const vector<MatchesInfo>& pairwise_matches,
    vector<CameraParams>& cameras) const
{
    qDebug() << "Bundle Adjusting... ";
    Ptr<detail::BundleAdjusterBase> adjuster;
    
    adjuster = makePtr<detail::BundleAdjusterReproj>();
    adjuster->setConfThresh(config()->conf_thresh_);

    Mat_<uchar> refine_mask = Mat::zeros(3, 3, CV_8U);
    refine_mask(0, 0) = 1;
    refine_mask(0, 2) = 1;
    refine_mask(1, 2) = 1;
    adjuster->setRefinementMask(refine_mask);
    if (!(*adjuster)(features, pairwise_matches, cameras))
    {
        cout << "camera parameters adjusting failed" << endl;
        qCritical() << "BA Failed" << endl;
        exit(-1);
    }
    qDebug() << "After BA, parameters of # 1: " << endl;
    qDebug() << "Bundle Adjust, time: ";
}


void StitcherGe::bundleAdjustCeres(const vector<ImageFeatures>& features,
    const vector<MatchesInfo>& pairwise_matches,
    vector<CameraParams>& cameras) const
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
        if (matches_info.confidence < config()->conf_thresh_)
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
    }

    ceres::Solver::Options options;
    //    options.linear_solver_type = ceres::SPARSE_SCHUR;
    options.linear_solver_type = ceres::DENSE_SCHUR;
    options.max_num_iterations = 1000;
    options.num_threads = 4;
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
int StitcherGe::warp_and_compositebyblend(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& inmasks,
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
    
    decltype(config()->cameras_) cameras;
    cameras.assign(config()->cameras_.begin(), config()->cameras_.end());

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
    
    estimateWarpScale(seam_scale, config()->seam_megapix_, full_img_size);
    m_warper = warper_creator->create(warped_image_scale * seam_scale);

    /***************
    * find seam in seam_scale and compute compensate of exposure
    *****************/
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

    // Compute relative scales
    estimateWarpScale(compose_scale, config()->compose_megapix_, full_img_size);
    m_warper = warper_creator->create(warped_image_scale * compose_scale);

    //LOGLN("Compositing...");
    UMat full_img, full_mask;
    UMat img_warped_s;
    Ptr<Blender> blender;
    images_warped.resize(num_images);
    masks_warped.resize(num_images);
    for (int img_idx = 0; img_idx < num_images; ++img_idx)
    {
        if (progress_reporter) progress_reporter->setValue(0.2 + 0.7 * img_idx / num_images);
        //LOGLN("Compositing image #" << indices[img_idx] + 1);
        // Read image and resize it if necessary
        full_img = frames[img_idx].getUMat(ACCESS_READ);
        full_mask = inmasks[img_idx].getUMat(ACCESS_READ);

        Size img_size = full_img.size();
        Mat_<float> K;
        cameras[img_idx].K().convertTo(K, CV_32F);
        K(0, 0) *= compose_scale; K(0, 2) *= compose_scale;
        K(1, 1) *= compose_scale; K(1, 2) *= compose_scale;
        UMat img_warped, blend_mask_warped, valid_mask_warped;
        valid_mask_warped = UMat(img_size, CV_8UC1, cv::Scalar(255));
        // Warp the current image
        m_warper->warp(full_img, K, cameras[img_idx].R, INTER_LINEAR, BORDER_REFLECT, img_warped);
        // Warp the current image mask
        m_warper->warp(full_mask, K, cameras[img_idx].R, INTER_NEAREST, BORDER_CONSTANT, blend_mask_warped);
        m_warper->warp(valid_mask_warped, K, cameras[img_idx].R, INTER_NEAREST, BORDER_CONSTANT, masks_warped[img_idx]);

        // Compensate exposure
        compensator->apply(img_idx, corners[img_idx], img_warped, blend_mask_warped);
        img_warped.convertTo(img_warped_s, CV_16S);
        images_warped[img_idx] = img_warped.getMat(ACCESS_RW);
        
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
            blender = Blender::createDefault(blend_type, config_->try_gpu_);
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


int StitcherGe::warp_points(const int frameidx, std::vector<cv::Point>& inoutpoints) const
{
    if (m_warper == nullptr)
        return false;

    Mat K;
    config()->cameras_[frameidx].K().convertTo(K, CV_32F);
    for (auto& point : inoutpoints)
    {
        cv::Point warp = m_warper->warpPoint(point, K, config()->cameras_[frameidx].R);
        point = warp;
    }
    return true;
}

void StitcherGe::drawMatches(const vector<Mat3b>& images, vector<ImageFeatures>& features,
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
            if (mi.confidence < config()->conf_thresh_)
                continue;

            cv::drawMatches(imageWork[i], features[i].keypoints, imageWork[j],
                features[j].keypoints, mi.matches, m);
            char filename[128];
            sprintf_s(filename, "matches/match_%d_%d_%d.jpg", i, j, mi.num_inliers);
            imwrite(filename, m);
        }
    }

    
    qDebug() << "Draw Matches to JPG, time:  ";
}

void StitcherGe::_logoFilter(const vector<Rect>& logoMask, ImageFeatures& features, float scale) const
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

void StitcherGe::_logoFilter(const vector<Rect>& logoMask, vector<ImageFeatures>& features, float scale) const
{
    for (size_t i = 0; i < features.size(); ++i)
    {
        _logoFilter(logoMask, features[i]);
    }
}

void StitcherGe::_simpleBlend(const Mat& src, const Mat& mask, const Point& tl, Mat& dst, Mat& dst_mask)
{
    // codes from opencv
    CV_Assert(src.type() == CV_16SC3);
    CV_Assert(mask.type() == CV_8U);
    int dx = tl.x;
    int dy = tl.y;

    for (int y = 0; y < src.rows; ++y)
    {
        const Point3_<short>* src_row = src.ptr<Point3_<short> >(y);
        Point3_<short>* dst_row = dst.ptr<Point3_<short> >(dy + y);
        const uchar* mask_row = mask.ptr<uchar>(y);
        uchar* dst_mask_row = dst_mask.ptr<uchar>(dy + y);

        for (int x = 0; x < src.cols; ++x)
        {
            if (mask_row[x])
                dst_row[dx + x] = src_row[x];
            dst_mask_row[dx + x] |= mask_row[x];
        }
    }
}

void StitcherGe::seamEstimate(const vector<Mat>& images_warped_f, const std::vector<Point>& corners,
    std::vector<Mat>& masks) const
{
    SeamType seam_type_ = config()->seam_type_;
    qDebug() << "Seam Finding... type: " << G_SEAM_STR[seam_type_ * 2];
    

    Ptr<SeamFinder> seam_finder;
    if (seam_type_ == SeamType::NO)
        seam_finder = new detail::NoSeamFinder();
    else if (seam_type_ == SeamType::VORONOI)
        seam_finder = new detail::VoronoiSeamFinder();
    else if (seam_type_ == SeamType::GC_COLOR)
    {
#ifdef HAVE_OPENCV_GPU
        qDebug() << "GPU!!!!!!!" << config()->try_gpu_ << "--" << gpu::getCudaEnabledDeviceCount();
        if (config()->try_gpu_ && gpu::getCudaEnabledDeviceCount() > 0)
            seam_finder = new detail::GraphCutSeamFinderGpu(GraphCutSeamFinderBase::COST_COLOR);
        else
#endif
            seam_finder = new detail::GraphCutSeamFinder(GraphCutSeamFinderBase::COST_COLOR);
    }
    else if (seam_type_ == SeamType::GC_COLORGRAD)
    {
#ifdef HAVE_OPENCV_GPU
        if (config()->try_gpu_ && gpu::getCudaEnabledDeviceCount() > 0)
            seam_finder = new detail::GraphCutSeamFinderGpu(GraphCutSeamFinderBase::COST_COLOR_GRAD);
        else
#endif
            seam_finder = new detail::GraphCutSeamFinder(GraphCutSeamFinderBase::COST_COLOR_GRAD);
    }
    else if (seam_type_ == SeamType::DP_COLOR)
        seam_finder = new detail::DpSeamFinder(DpSeamFinder::COLOR);
    else if (seam_type_ == SeamType::DP_COLORGRAD)
        seam_finder = new detail::DpSeamFinder(DpSeamFinder::COLOR_GRAD);
    if (seam_finder.empty())
    {
        cout << "Can't create the following seam finder '" << G_SEAM_STR[seam_type_ * 2] << "'\n";
    }

    vector<UMat> images_warped_f_UMat, masks_UMat;
    for (auto mat : images_warped_f)
        images_warped_f_UMat.emplace_back(mat.getUMat(ACCESS_READ));
    seam_finder->find(images_warped_f_UMat, corners, masks_UMat);

    masks.resize(0);
    for (auto mat : masks_UMat)
        masks.emplace_back(mat.getMat(ACCESS_RW));

    
    qDebug() << "Seam Finding, time ";
}

