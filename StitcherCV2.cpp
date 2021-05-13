#include <iostream>
#include "StitcherCV2.h"

#include <opencv2/opencv_modules.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#ifdef HAVE_OPENCV_XFEATURES2D
#include "opencv2/xfeatures2d.hpp"
#include "opencv2/xfeatures2d/nonfree.hpp"
#endif

using namespace std;
using namespace cv;
using namespace cv::detail;

bool StitcherCV2::get_warped_frames(std::vector<cv::Mat>& frames, std::vector<cv::Rect>& windows)
{
    if (images_warped.empty()
        || images_warped.size() != masks_warped.size()
        || images_warped.size() != sizes.size()
        || images_warped.size() != corners.size())
        return false;
    else
    {
        const int num_img = images_warped.size();
        frames.resize(num_img); 
        windows.resize(num_img);
        for (int i = 0; i < images_warped.size(); ++i)
        {
            cv::merge(vector<UMat>({ images_warped[i], masks_warped[i] }), frames[i]);
            windows[i] = cv::Rect(corners[i], sizes[i]);
        }
    }
    return true;
}


void StitcherCV2::build_feature_matches(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& inmasks)
{
    const int num_images = frames.size();
    double seam_scale = 1;
    bool is_seam_scale_set = false;

    Ptr<Feature2D> finder;
    if (features_type == "orb")
    {
        finder = ORB::create();
    }
    else if (features_type == "akaze")
    {
        finder = AKAZE::create();
    }
#ifdef HAVE_OPENCV_XFEATURES2D
    else if (features_type == "surf")
    {
        finder = xfeatures2d::SURF::create();
    }
#endif
    else if (features_type == "sift")
    {
        finder = SIFT::create();
    }

    Mat full_img, img, full_mask, mask;
    features.resize(num_images);

    for (int i = 0; i < num_images; ++i)
    {
        full_img = frames[i];
        full_mask = inmasks[i];

        if (work_megapix < 0)
        {
            img = full_img;
            mask = full_mask;
            work_scale = 1;
            is_work_scale_set = true;
        }
        else
        {
            if (!is_work_scale_set)
            {
                work_scale = min(1.0, sqrt(work_megapix * 1e6 / full_img.size().area()));
                is_work_scale_set = true;
            }
            resize(full_img, img, Size(), work_scale, work_scale, INTER_LINEAR_EXACT);
            resize(full_mask, mask, Size(), work_scale, work_scale, INTER_NEAREST);
        }
        if (!is_seam_scale_set)
        {
            seam_scale = min(1.0, sqrt(seam_megapix * 1e6 / full_img.size().area()));
            seam_work_aspect = seam_scale / work_scale;
            is_seam_scale_set = true;
        }

        //detectracker.detect(img, )
        computeImageFeatures(finder, img, features[i], mask);
        features[i].img_idx = i;
        //LOGLN("Features in image #" << i + 1 << ": " << features[i].keypoints.size());
        
    }
    full_img.release();
    img.release();
    full_mask.release();
    mask.release();

    Ptr<FeaturesMatcher> matcher;
    pairwise_matches.reserve(num_images * num_images);
    if (matcher_type == "affine")
        matcher = makePtr<AffineBestOf2NearestMatcher>(false, try_cuda, match_conf);
    else if (range_width == -1)
        matcher = makePtr<BestOf2NearestMatcher>(try_cuda, match_conf);
    else
        matcher = makePtr<BestOf2NearestRangeMatcher>(range_width, try_cuda, match_conf);

    for (int i = 0; i < num_images; ++i)
        for (int j = 0; j < num_images; ++j)
        {
            if (j == i + 1 || j + 1 == i)
            {
                MatchesInfo m01;
                (*matcher)(features[i], features[j], m01);
                m01.src_img_idx = i;
                m01.dst_img_idx = j;
                matcher->collectGarbage();
                pairwise_matches.push_back(m01);
            }
            else
            {
                pairwise_matches.emplace_back();
            }
        }
}


int StitcherCV2::bundle_adjustment()
{
    Ptr<Estimator> estimator;
    if (estimator_type == "affine")
        estimator = makePtr<AffineBasedEstimator>();
    else
        estimator = makePtr<HomographyBasedEstimator>();
    if (!(*estimator)(features, pairwise_matches, cameras))
    {
        cout << "Homography estimation failed.\n";
        return -1;
    }
    for (size_t i = 0; i < cameras.size(); ++i)
    {
        Mat R;
        cameras[i].R.convertTo(R, CV_32F);
        cameras[i].R = R;
        //LOGLN("Initial camera intrinsics #" << indices[i] + 1 << ":\nK:\n" << cameras[i].K() << "\nR:\n" << cameras[i].R);
    }

    Ptr<detail::BundleAdjusterBase> adjuster;
    if (ba_cost_func == "reproj") adjuster = makePtr<detail::BundleAdjusterReproj>();
    else if (ba_cost_func == "ray") adjuster = makePtr<detail::BundleAdjusterRay>();
    else if (ba_cost_func == "affine") adjuster = makePtr<detail::BundleAdjusterAffinePartial>();
    else if (ba_cost_func == "no") adjuster = makePtr<NoBundleAdjuster>();
    else
    {
        cout << "Unknown bundle adjustment cost function: '" << ba_cost_func << "'.\n";
        return -1;
    }
    adjuster->setConfThresh(ba_conf_thresh);
    Mat_<uchar> refine_mask = Mat::zeros(3, 3, CV_8U);
    if (ba_refine_mask[0] == 'x') refine_mask(0, 0) = 1;
    if (ba_refine_mask[1] == 'x') refine_mask(0, 1) = 1;
    if (ba_refine_mask[2] == 'x') refine_mask(0, 2) = 1;
    if (ba_refine_mask[3] == 'x') refine_mask(1, 1) = 1;
    if (ba_refine_mask[4] == 'x') refine_mask(1, 2) = 1;
    adjuster->setRefinementMask(refine_mask);
    if (!(*adjuster)(features, pairwise_matches, cameras))
    {
        cout << "Camera parameters adjusting failed.\n";
        return -1;
    }
    // Find median focal length
    vector<double> focals;
    for (size_t i = 0; i < cameras.size(); ++i)
    {
        //LOGLN("Camera #" << indices[i] + 1 << ":\nK:\n" << cameras[i].K() << "\nR:\n" << cameras[i].R);
        focals.push_back(cameras[i].focal);
    }
    sort(focals.begin(), focals.end());
    float ;
    if (focals.size() % 2 == 1)
        warped_image_scale = static_cast<float>(focals[focals.size() / 2]);
    else
        warped_image_scale = static_cast<float>(focals[focals.size() / 2 - 1] + focals[focals.size() / 2]) * 0.5f;
    if (do_wave_correct)
    {
        vector<Mat> rmats;
        for (size_t i = 0; i < cameras.size(); ++i)
            rmats.push_back(cameras[i].R.clone());
        waveCorrect(rmats, wave_correct);
        for (size_t i = 0; i < cameras.size(); ++i)
            cameras[i].R = rmats[i];
    }
    return 0;
}

int StitcherCV2::warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& inmasks)
{
    const int num_images = frames.size();
    images_warped.resize(num_images);
    masks_warped.resize(num_images);
    sizes.resize(num_images);
    corners.resize(num_images);

    // Warp images and their masks
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
    Ptr<RotationWarper> warper = warper_creator->create(static_cast<float>(warped_image_scale * seam_work_aspect));


    /***************
    * find seam in seam_scale and compute compensate of exposure
    *****************/
    Ptr<ExposureCompensator> compensator = ExposureCompensator::createDefault(expos_comp_type);
    vector<UMat> seammasks_warp(num_images);
    {
        vector<UMat> images_warp_4seam(num_images);
        vector<UMat> images_warped_f(num_images);
        double seam_scale = seam_work_aspect * work_scale;
        for (int i = 0; i < num_images; ++i)
        {
            Mat_<float> K;
            cameras[i].K().convertTo(K, CV_32F);
            float swa = (float)seam_work_aspect;
            K(0, 0) *= swa; K(0, 2) *= swa;
            K(1, 1) *= swa; K(1, 2) *= swa;
        
            cv::Mat image, mask;
            resize(frames[i], image, Size(), seam_scale, seam_scale, INTER_LINEAR_EXACT);
            resize(inmasks[i], mask, Size(), seam_scale, seam_scale, INTER_NEAREST);
            corners[i] = warper->warp(image, K, cameras[i].R, INTER_LINEAR, BORDER_REFLECT, images_warp_4seam[i]);
            sizes[i] = images_warp_4seam[i].size();
            warper->warp(mask, K, cameras[i].R, INTER_NEAREST, BORDER_CONSTANT, seammasks_warp[i]);
        }
        for (int i = 0; i < num_images; ++i)
            images_warp_4seam[i].convertTo(images_warped_f[i], CV_32F);

        //LOGLN("Compensating exposure...");
        
        if (dynamic_cast<GainCompensator*>(compensator.get()))
        {
            GainCompensator* gcompensator = dynamic_cast<GainCompensator*>(compensator.get());
            gcompensator->setNrFeeds(expos_comp_nr_feeds);
        }
        if (dynamic_cast<ChannelsCompensator*>(compensator.get()))
        {
            ChannelsCompensator* ccompensator = dynamic_cast<ChannelsCompensator*>(compensator.get());
            ccompensator->setNrFeeds(expos_comp_nr_feeds);
        }
        if (dynamic_cast<BlocksCompensator*>(compensator.get()))
        {
            BlocksCompensator* bcompensator = dynamic_cast<BlocksCompensator*>(compensator.get());
            bcompensator->setNrFeeds(expos_comp_nr_feeds);
            bcompensator->setNrGainsFilteringIterations(expos_comp_nr_filtering);
            bcompensator->setBlockSize(expos_comp_block_size, expos_comp_block_size);
        }
        compensator->feed(corners, images_warp_4seam, seammasks_warp);
        //LOGLN("Compensating exposure, time: " << ((getTickCount() - t) / getTickFrequency()) << " sec");
        
        //LOGLN("Finding seams...");
        Ptr<SeamFinder> seam_finder;
        if (seam_find_type == "no")
            seam_finder = makePtr<detail::NoSeamFinder>();
        else if (seam_find_type == "voronoi")
            seam_finder = makePtr<detail::VoronoiSeamFinder>();
        else if (seam_find_type == "gc_color")
            seam_finder = makePtr<detail::GraphCutSeamFinder>(GraphCutSeamFinderBase::COST_COLOR);
        else if (seam_find_type == "gc_colorgrad")
            seam_finder = makePtr<detail::GraphCutSeamFinder>(GraphCutSeamFinderBase::COST_COLOR_GRAD);
        else if (seam_find_type == "dp_color")
            seam_finder = makePtr<detail::DpSeamFinder>(DpSeamFinder::COLOR);
        else if (seam_find_type == "dp_colorgrad")
            seam_finder = makePtr<detail::DpSeamFinder>(DpSeamFinder::COLOR_GRAD);
        if (!seam_finder)
        {
            cout << "Can't create the following seam finder '" << seam_find_type << "'\n";
            return 1;
        }
        seam_finder->find(images_warped_f, corners, seammasks_warp);
    }

    
    //LOGLN("Compositing...");
    Mat full_img, full_mask;
    Mat img_warped_s;
    Mat dilated_mask, seam_mask;
    Ptr<Blender> blender;

    //double compose_seam_aspect = 1;
    double compose_scale = 1;
    bool is_compose_scale_set = false;
    double compose_work_aspect = 1;
    for (int img_idx = 0; img_idx < num_images; ++img_idx)
    {
        //LOGLN("Compositing image #" << indices[img_idx] + 1);
        // Read image and resize it if necessary
        full_img = frames[img_idx];
        if (!is_compose_scale_set)
        {
            if (compose_megapix > 0)
                compose_scale = min(1.0, sqrt(compose_megapix * 1e6 / full_img.size().area()));
            is_compose_scale_set = true;
            // Compute relative scales
            //compose_seam_aspect = compose_scale / seam_scale;
            compose_work_aspect = compose_scale / work_scale;
            // Update warped image scale
            warped_image_scale *= static_cast<float>(compose_work_aspect);
            warper = warper_creator->create(warped_image_scale);
            // Update corners and sizes
            for (int i = 0; i < num_images; ++i)
            {
                // Update intrinsics
                cameras[i].focal *= compose_work_aspect;
                cameras[i].ppx *= compose_work_aspect;
                cameras[i].ppy *= compose_work_aspect;
                // Update corner and size
                Size sz = full_img.size();
                if (std::abs(compose_scale - 1) > 1e-1)
                {
                    sz.width = cvRound(sz.width * compose_scale);
                    sz.height = cvRound(sz.height * compose_scale);
                }
                Mat K;
                cameras[i].K().convertTo(K, CV_32F);
                Rect roi = warper->warpRoi(sz, K, cameras[i].R);
                corners[i] = roi.tl();
                sizes[i] = roi.size();
            }
        }
        if (abs(compose_scale - 1) > 1e-1)
        {
            resize(full_img, full_img, Size(), compose_scale, compose_scale, INTER_LINEAR_EXACT);
        }

        Size img_size = full_img.size();
        full_mask = cv::Mat(img_size, CV_8UC1, cv::Scalar(255));
        Mat K;
        cameras[img_idx].K().convertTo(K, CV_32F);
        UMat img_warped, mask_warped;
        // Warp the current image
        warper->warp(full_img, K, cameras[img_idx].R, INTER_LINEAR, BORDER_REFLECT, img_warped);
        // Warp the current image mask
        warper->warp(full_mask, K, cameras[img_idx].R, INTER_NEAREST, BORDER_CONSTANT, mask_warped);
        // Compensate exposure
        compensator->apply(img_idx, corners[img_idx], img_warped, mask_warped);
        img_warped.convertTo(img_warped_s, CV_16S);
        images_warped[img_idx] = img_warped;
        masks_warped[img_idx] = mask_warped;

        //dilate(seammasks_warp[img_idx], dilated_mask, Mat());
        //resize(dilated_mask, seam_mask, mask_warped.size(), 0, 0, INTER_LINEAR_EXACT);
        resize(seammasks_warp[img_idx], seam_mask, mask_warped.size(), 0, 0, INTER_LINEAR_EXACT);
        bitwise_and(seam_mask, mask_warped, seam_mask);
        if (!blender)
        {
            blender = Blender::createDefault(blend_type, try_cuda);
            Size dst_sz = resultRoi(corners, sizes).size();
            float blend_width = sqrt(static_cast<float>(dst_sz.area())) * blend_strength / 100.f;
            if (blend_width < 1.f)
                blender = Blender::createDefault(Blender::NO, try_cuda);
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
        blender->feed(img_warped_s, seam_mask, corners[img_idx]);
    }

    Mat result, result_mask;
    blender->blend(result, result_mask);
    result.convertTo(result, CV_8UC3);
    result_mask.convertTo(result_mask, CV_8UC1);
    cv::merge(std::vector<Mat>({ result, result_mask }), stitch_result);
    //LOGLN("Compositing, time: " << ((getTickCount() - t) / getTickFrequency()) << " sec");
    //imwrite(result_name, result);
    return 0;
}
