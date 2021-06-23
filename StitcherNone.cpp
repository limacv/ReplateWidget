#include "StitcherNone.h"
#include "MLProgressDialog.hpp"

using namespace cv;

int StitcherNone::stitch(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks)
{
    return 0;
}

//int StitcherNone::warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks, std::vector<cv::Mat>& warped, std::vector<cv::Rect>& windows, cv::Mat& stitched)
//{
//    const int num_images = frames.size();
//    warped.resize(num_images);
//    windows.resize(num_images);
//
//    for (int i = 0; i < num_images; ++i)
//    {
//        cvtColor(frames[i], warped[i], COLOR_BGR2BGRA);
//        windows[i] = Rect(Point(0, 0), frames[i].size());
//    }
//
//
//    //LOGLN("Compositing...");
//    Mat full_img, full_mask;
//    Mat img_warped_s;
//    Ptr<Blender> blender;
//    for (int img_idx = 0; img_idx < num_images; ++img_idx)
//    {
//        //LOGLN("Compositing image #" << indices[img_idx] + 1);
//        // Read image and resize it if necessary
//        full_img = frames[img_idx];
//        full_mask = masks[img_idx];
//        Mat img_warped = full_img, img_warped_s, blend_mask_warped = full_mask;
//        Size img_size = full_img.size();
//
//        img_warped.convertTo(img_warped_s, CV_16S);
//        
//        if (img_idx == 0)
//        {
//            const auto& blend_type = config_->blend_type_;
//            blender = Blender::createDefault(blend_type, false);
//            float blend_width = sqrt(static_cast<float>(img_size.area())) * config_->blend_strength_ / 100.f;
//            if (blend_width < 1.f)
//                blender = Blender::createDefault(Blender::NO);
//            else if (blend_type == Blender::MULTI_BAND)
//            {
//                MultiBandBlender* mb = dynamic_cast<MultiBandBlender*>(blender.get());
//                mb->setNumBands(static_cast<int>(ceil(log(blend_width) / log(2.)) - 1.));
//                //LOGLN("Multi-band blender, number of bands: " << mb->numBands());
//            }
//            else if (blend_type == Blender::FEATHER)
//            {
//                FeatherBlender* fb = dynamic_cast<FeatherBlender*>(blender.get());
//                fb->setSharpness(1.f / blend_width);
//                //LOGLN("Feather blender, sharpness: " << fb->sharpness());
//            }
//            blender->prepare(cv::Rect(Point(0, 0), img_size));
//        }
//        blender->feed(img_warped_s, blend_mask_warped, Point(0, 0));
//    }
//
//    Mat result, result_mask;
//    blender->blend(result, result_mask);
//    result.convertTo(result, CV_8UC3);
//    result_mask.convertTo(result_mask, CV_8UC1);
//    cv::merge(std::vector<Mat>({ result, result_mask }), stitched);
//    //LOGLN("Compositing, time: " << ((getTickCount() - t) / getTickFrequency()) << " sec");
//    //imwrite(result_name, result);
//    return 0;
//}

int StitcherNone::warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks, std::vector<cv::Mat>& warped, std::vector<cv::Rect>& windows, cv::Mat& stitched)
{
    const int num_images = frames.size();
    warped.resize(num_images);
    windows.resize(num_images);

    for (int i = 0; i < num_images; ++i)
    {
        cvtColor(frames[i], warped[i], COLOR_BGR2BGRA);
        windows[i] = Rect(Point(0, 0), frames[i].size());
    }

    if (progress_reporter) progress_reporter->beginStage("blending...");
    //LOGLN("Compositing...");
    Mat acc_img(frames[0].size(), CV_64FC3, cv::Scalar(0, 0, 0)), 
        count_img(frames[0].size(), CV_64FC1, cv::Scalar(0.0001));
    for (int img_idx = 0; img_idx < num_images; ++img_idx)
    {
        if (progress_reporter) progress_reporter->setValue((float)img_idx / num_images);
        if (progress_reporter && progress_reporter->wasCanceled())
            return -1;
        //LOGLN("Compositing image #" << indices[img_idx] + 1);
        // Read image and resize it if necessary
        Mat full_img_f; frames[img_idx].convertTo(full_img_f, CV_64FC3, 1. / 255);
        Mat full_mask_f; masks[img_idx].convertTo(full_mask_f, CV_64FC1, 1. / 255);
        add(acc_img, full_img_f, acc_img, masks[img_idx]);
        add(count_img, full_mask_f, count_img);
    }

    if (progress_reporter) progress_reporter->setValue(1.f);
    Mat count_img3;
    cv::merge(std::vector<Mat>({ count_img, count_img, count_img }), count_img3);
    acc_img /= count_img3;
    acc_img.convertTo(acc_img, CV_8UC3, 255);
    count_img = count_img > 0.5;
    count_img.convertTo(count_img, CV_8UC1, 255);
    cv::merge(std::vector<Mat>({ acc_img, count_img }), stitched);
    //LOGLN("Compositing, time: " << ((getTickCount() - t) / getTickFrequency()) << " sec");
    //imwrite(result_name, result);
    return 0;
}



int StitcherNone::warp_points(const int frameidx, std::vector<cv::Point>& inoutpoints) const
{
    return 0;
}
