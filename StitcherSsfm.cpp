#include "StitcherSsfm.h"
#include "SphericalSfm/SphericalSfm.h"
#include "MLConfigManager.h"

using namespace std;
using namespace cv;
using namespace cv::detail;

int StitcherSsfm::warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks, 
    std::vector<cv::Mat>& warped, std::vector<cv::Rect>& windows, cv::Mat& stitched)
{
    int ret;
    std::vector<cv::Mat> images_warped;
    std::vector<cv::Mat> masks_warped;
    std::vector<cv::Size> sizes;
    std::vector<cv::Point> corners;
    //if (blend_scheme == "seam")
        //ret = warp_and_compositebyseam(frames, masks, images_warped, masks_warped, sizes, corners, stitched);
    //else
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

int StitcherSsfm::warp_points(const int frameidx, std::vector<cv::Point>& inoutpoints) const
{
    if (warper == nullptr)
        return false;
    
    Mat K;
    cameras_[frameidx].K().convertTo(K, CV_32F);
    for (auto& point : inoutpoints)
    {
        cv::Point warp = warper->warpPoint(point, K, cameras_[frameidx].R);
        point = warp;
    }
    return true;
}

int StitcherSsfm::stitch(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks)
{
    Ssfm::SphericalSfm ssfm;
    ssfm.set_intrinsics(0.9 * frames[0].cols, 0.5 * frames[0].cols, 0.5 * frames[0].rows);
    ssfm.set_allow_translation(false);
    ssfm.set_optimize_translation(false);
    ssfm.set_visualize_root(MLConfigManager::get().get_stitch_cache().toStdString());
    ssfm.set_progress_observer(progress_reporter);

    const auto& stitcher_cfg = MLConfigManager::get().stitcher_cfg;
    ssfm.qualitylevel = stitcher_cfg.stitcher_detect_qualitylevel;
    ssfm.cellsize = stitcher_cfg.stitcher_detect_cellH;
    ssfm.min_rot = stitcher_cfg.stitcher_track_min_rot;
    ssfm.inlier_threshold = stitcher_cfg.stitcher_track_inlier_threshold;
    ssfm.trackwin = stitcher_cfg.stitcher_track_win;
    ssfm.set_verbose(stitcher_cfg.stitcher_verbose);

    ssfm.runSfM(frames, masks);
    Ssfm::SparseModel model;
    ssfm.track_all_frames(frames, masks, model);
    cameras_ = model.poses;
    return 0;
}

/// <summary>
/// require: cameras
/// write to: images_warped, masks_warped, sizes, corners
/// </summary>
int StitcherSsfm::warp_and_compositebyseam(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& inmasks,
    std::vector<cv::Mat>& images_warped, std::vector<cv::Mat>& masks_warped,
    std::vector<cv::Size>& sizes, std::vector<cv::Point>& corners, cv::Mat& stitch_result)
{
    const int num_images = frames.size();
    images_warped.resize(num_images);
    masks_warped.resize(num_images);
    sizes.resize(num_images);
    corners.resize(num_images);

    auto warp_type = config_->warp_type_;
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
    float warped_image_scale = cameras_[0].focal;
    warper = warper_creator->create(warped_image_scale * seam_scale);

    /***************
    * find seam in seam_scale and compute compensate of exposure
    *****************/
    Ptr<ExposureCompensator> compensator = ExposureCompensator::createDefault(config_->expos_comp_type_);
    vector<UMat> seammasks_warp(num_images);
    {
        vector<UMat> images_warp_4seam(num_images);
        vector<UMat> images_warped_f(num_images);
        for (int i = 0; i < num_images; ++i)
        {
            Mat_<float> K;
            cameras_[i].K().convertTo(K, CV_32F);
            K(0, 0) *= seam_scale; K(0, 2) *= seam_scale;
            K(1, 1) *= seam_scale; K(1, 2) *= seam_scale;

            cv::Mat image, mask;
            resize(frames[i], image, Size(), seam_scale, seam_scale, INTER_LINEAR_EXACT);
            resize(inmasks[i], mask, Size(), seam_scale, seam_scale, INTER_NEAREST);
            corners[i] = warper->warp(image, K, cameras_[i].R, INTER_LINEAR, BORDER_REFLECT, images_warp_4seam[i]);
            sizes[i] = images_warp_4seam[i].size();
            warper->warp(mask, K, cameras_[i].R, INTER_NEAREST, BORDER_CONSTANT, seammasks_warp[i]);
        }
        for (int i = 0; i < num_images; ++i)
            images_warp_4seam[i].convertTo(images_warped_f[i], CV_32F);

        //LOGLN("Compensating exposure...");

        if (dynamic_cast<GainCompensator*>(compensator.get()))
        {
            GainCompensator* gcompensator = dynamic_cast<GainCompensator*>(compensator.get());
            gcompensator->setNrFeeds(config_->expos_comp_nr_feeds);
        }
        if (dynamic_cast<ChannelsCompensator*>(compensator.get()))
        {
            ChannelsCompensator* ccompensator = dynamic_cast<ChannelsCompensator*>(compensator.get());
            ccompensator->setNrFeeds(config_->expos_comp_nr_feeds);
        }
        if (dynamic_cast<BlocksCompensator*>(compensator.get()))
        {
            BlocksCompensator* bcompensator = dynamic_cast<BlocksCompensator*>(compensator.get());
            bcompensator->setNrFeeds(config_->expos_comp_nr_feeds);
            bcompensator->setNrGainsFilteringIterations(config_->expos_comp_nr_filtering);
            bcompensator->setBlockSize(config_->expos_comp_block_size, config_->expos_comp_block_size);
        }
        compensator->feed(corners, images_warp_4seam, seammasks_warp);
        //LOGLN("Compensating exposure, time: " << ((getTickCount() - t) / getTickFrequency()) << " sec");

        //LOGLN("Finding seams...");
        std::string seam_find_type = "gc_colorgrad";
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
    images_warped.resize(num_images);
    masks_warped.resize(num_images);
    //double compose_seam_aspect = 1;
    bool is_compose_scale_set = false;
    for (int img_idx = 0; img_idx < num_images; ++img_idx)
    {
        //LOGLN("Compositing image #" << indices[img_idx] + 1);
        // Read image and resize it if necessary
        full_img = frames[img_idx];
        if (!is_compose_scale_set)
        {
            is_compose_scale_set = true;
            // Compute relative scales
            //compose_seam_aspect = compose_scale / seam_scale;
            // Update warped image scale
            warper = warper_creator->create(warped_image_scale);
            // Update corners and sizes
            for (int i = 0; i < num_images; ++i)
            {
                // Update corner and size
                Size sz = full_img.size();
                Mat K;
                cameras_[i].K().convertTo(K, CV_32F);
                Rect roi = warper->warpRoi(sz, K, cameras_[i].R);
                corners[i] = roi.tl();
                sizes[i] = roi.size();
            }
        }

        Size img_size = full_img.size();
        full_mask = inmasks[img_idx];
        //full_mask = cv::Mat(img_size, CV_8UC1, cv::Scalar(255));
        Mat K;
        cameras_[img_idx].K().convertTo(K, CV_32F);
        Mat img_warped, mask_warped;
        // Warp the current image
        warper->warp(full_img, K, cameras_[img_idx].R, INTER_LINEAR, BORDER_REFLECT, img_warped);
        // Warp the current image mask
        warper->warp(full_mask, K, cameras_[img_idx].R, INTER_NEAREST, BORDER_CONSTANT, mask_warped);
        // Compensate exposure
        compensator->apply(img_idx, corners[img_idx], img_warped, mask_warped);
        img_warped.convertTo(img_warped_s, CV_16S);
        images_warped[img_idx] = img_warped;
        masks_warped[img_idx] = mask_warped;

        dilate(seammasks_warp[img_idx], dilated_mask, Mat());
        resize(dilated_mask, seam_mask, mask_warped.size(), 0, 0, INTER_LINEAR_EXACT);
        //resize(seammasks_warp[img_idx], seam_mask, mask_warped.size(), 0, 0, INTER_LINEAR_EXACT);
        bitwise_and(seam_mask, mask_warped, seam_mask);
        if (!blender)
        {
            const auto blend_type = config_->blend_type_;
            blender = Blender::createDefault(blend_type);
            Size dst_sz = resultRoi(corners, sizes).size();
            float blend_width = sqrt(static_cast<float>(dst_sz.area())) * config_->blend_strength_ / 100.f;
            if (blend_width < 1.f)
                blender = Blender::createDefault(Blender::NO);
            else if (config_->blend_type_ == Blender::MULTI_BAND)
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



/// <summary>
/// require: cameras
/// write to: images_warped, masks_warped, sizes, corners
/// </summary>
int StitcherSsfm::warp_and_compositebyblend(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& inmasks,
    std::vector<cv::Mat>& images_warped, std::vector<cv::Mat>& masks_warped,
    std::vector<cv::Size>& sizes, std::vector<cv::Point>& corners, cv::Mat& stitch_result)
{
    if (progress_reporter) progress_reporter->beginStage("Warping and Composite");
    const int num_images = frames.size();
    images_warped.resize(num_images);
    masks_warped.resize(num_images);
    sizes.resize(num_images);
    corners.resize(num_images);

    auto warp_type = config_->warp_type_;
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
    float warped_image_scale = cameras_[0].focal;
    warper = warper_creator->create(warped_image_scale * seam_scale);

    /***************
    * find seam in seam_scale and compute compensate of exposure
    *****************/
    Ptr<ExposureCompensator> compensator = ExposureCompensator::createDefault(config_->expos_comp_type_);
    {
        vector<UMat> images_warp_4seam(num_images);
        vector<UMat> images_warped_f(num_images);
        vector<UMat> seammasks_warp(num_images);
        for (int i = 0; i < num_images; ++i)
        {
            if (progress_reporter) progress_reporter->setValue(0.15 * i / num_images);
            Mat_<float> K;
            cameras_[i].K().convertTo(K, CV_32F);
            K(0, 0) *= seam_scale; K(0, 2) *= seam_scale;
            K(1, 1) *= seam_scale; K(1, 2) *= seam_scale;

            cv::Mat image, mask;
            resize(frames[i], image, Size(), seam_scale, seam_scale, INTER_LINEAR_EXACT);
            resize(inmasks[i], mask, Size(), seam_scale, seam_scale, INTER_NEAREST);
            corners[i] = warper->warp(image, K, cameras_[i].R, INTER_LINEAR, BORDER_REFLECT, images_warp_4seam[i]);
            sizes[i] = images_warp_4seam[i].size();
            warper->warp(mask, K, cameras_[i].R, INTER_NEAREST, BORDER_CONSTANT, seammasks_warp[i]);
        }
        for (int i = 0; i < num_images; ++i)
            images_warp_4seam[i].convertTo(images_warped_f[i], CV_32F);

        //LOGLN("Compensating exposure...");

        if (dynamic_cast<GainCompensator*>(compensator.get()))
        {
            GainCompensator* gcompensator = dynamic_cast<GainCompensator*>(compensator.get());
            gcompensator->setNrFeeds(config_->expos_comp_nr_feeds);
        }
        if (dynamic_cast<ChannelsCompensator*>(compensator.get()))
        {
            ChannelsCompensator* ccompensator = dynamic_cast<ChannelsCompensator*>(compensator.get());
            ccompensator->setNrFeeds(config_->expos_comp_nr_feeds);
        }
        if (dynamic_cast<BlocksCompensator*>(compensator.get()))
        {
            BlocksCompensator* bcompensator = dynamic_cast<BlocksCompensator*>(compensator.get());
            bcompensator->setNrFeeds(config_->expos_comp_nr_feeds);
            bcompensator->setNrGainsFilteringIterations(config_->expos_comp_nr_filtering);
            bcompensator->setBlockSize(config_->expos_comp_block_size, config_->expos_comp_block_size);
        }
        compensator->feed(corners, images_warp_4seam, seammasks_warp);
        if (progress_reporter) progress_reporter->setValue(0.2);
    }
    
    //LOGLN("Compositing...");
    Mat full_img, full_mask;
    Mat img_warped_s;
    Ptr<Blender> blender;
    images_warped.resize(num_images);
    masks_warped.resize(num_images);
    for (int img_idx = 0; img_idx < num_images; ++img_idx)
    {
        if (progress_reporter) progress_reporter->setValue(0.2 + 0.7 * img_idx / num_images);
        //LOGLN("Compositing image #" << indices[img_idx] + 1);
        // Read image and resize it if necessary
        full_img = frames[img_idx];
        full_mask = inmasks[img_idx];
        if (img_idx == 0)
        {
            // Compute relative scales
            //compose_seam_aspect = compose_scale / seam_scale;
            // Update warped image scale
            warper = warper_creator->create(warped_image_scale);
            // Update corners and sizes
            for (int i = 0; i < num_images; ++i)
            {
                // Update corner and size
                Size sz = full_img.size();
                Mat K;
                cameras_[i].K().convertTo(K, CV_32F);
                Rect roi = warper->warpRoi(sz, K, cameras_[i].R);
                corners[i] = roi.tl();
                sizes[i] = roi.size();
            }
        }

        Size img_size = full_img.size();
        Mat K;
        cameras_[img_idx].K().convertTo(K, CV_32F);
        Mat img_warped, blend_mask_warped, valid_mask_warped;
        valid_mask_warped = Mat(img_size, CV_8UC1, cv::Scalar(255));
        // Warp the current image
        warper->warp(full_img, K, cameras_[img_idx].R, INTER_LINEAR, BORDER_REFLECT, img_warped);
        // Warp the current image mask
        warper->warp(full_mask, K, cameras_[img_idx].R, INTER_NEAREST, BORDER_CONSTANT, blend_mask_warped);
        warper->warp(valid_mask_warped, K, cameras_[img_idx].R, INTER_NEAREST, BORDER_CONSTANT, masks_warped[img_idx]);

        // Compensate exposure
        compensator->apply(img_idx, corners[img_idx], img_warped, blend_mask_warped);
        img_warped.convertTo(img_warped_s, CV_16S);
        images_warped[img_idx] = img_warped;

        if (!blender)
        {
            auto blend_type = config_->blend_type_;
            blender = Blender::createDefault(blend_type);
            Size dst_sz = resultRoi(corners, sizes).size();
            float blend_width = sqrt(static_cast<float>(dst_sz.area())) * config_->blend_strength_ / 100.f;
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
