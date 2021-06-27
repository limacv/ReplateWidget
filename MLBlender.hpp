#pragma once
#include <opencv2/stitching/detail/blenders.hpp>

namespace cv
{
    namespace detail
    {
        class MLBlender :
            public Blender
        {
        public:
            enum { Simple = 3 };

            MLBlender() {}
            virtual ~MLBlender() {}

            virtual void blend(InputOutputArray dst, InputOutputArray dst_mask);
            virtual void feed(InputArray img, InputArray mask, Point tl);
            virtual void prepare(Rect dst_roi);

            static Ptr<Blender> createDefault();

        private:
            cv::Mat accimg;
            cv::Mat countimg;
        };
    }
}

inline
void cv::detail::MLBlender::blend(InputOutputArray dst, InputOutputArray dst_mask)
{
    Mat count_img3;
    cv::merge(std::vector<Mat>({ countimg, countimg, countimg }), count_img3);
    accimg /= count_img3;
    accimg.convertTo(accimg, CV_8UC3, 255);
    
    countimg = countimg > 0.5;
    countimg.convertTo(countimg, CV_8UC1, 255);

    dst.assign(accimg);
    dst_mask.assign(countimg);
}

inline 
void cv::detail::MLBlender::feed(InputArray img, InputArray mask, Point tl)
{
    Mat img_ = img.getMat(), mask_ = mask.getMat();
    Mat full_img_f; img_.convertTo(full_img_f, CV_32FC3, 1. / 255);
    Mat full_mask_f; mask_.convertTo(full_mask_f, CV_32FC1, 1. / 255);

    Rect roi(tl - dst_roi_.tl(), full_img_f.size());
    add(accimg(roi), full_img_f, accimg(roi), mask_);
    add(countimg(roi), full_mask_f, countimg(roi));
}

inline 
void cv::detail::MLBlender::prepare(Rect dst_roi)
{
    dst_roi_ = dst_roi;
    accimg = cv::Mat(dst_roi_.size(), CV_32FC3, cv::Scalar(0));
    countimg = cv::Mat(dst_roi_.size(), CV_32FC1, cv::Scalar(0.001));
}

inline cv::Ptr<cv::detail::Blender> cv::detail::MLBlender::createDefault()
{
    return makePtr<MLBlender>();
}
