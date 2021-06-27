#pragma once
#include <opencv2/stitching/detail/blenders.hpp>

namespace cv
{
    namespace detail
    {
        class MLEdgeBlender :
            public Blender
        {
        public:
            enum { SimpleEdge = 4 };

            MLEdgeBlender() {}
            virtual ~MLEdgeBlender() {}

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
void cv::detail::MLEdgeBlender::blend(InputOutputArray dst, InputOutputArray dst_mask)
{
    accimg.convertTo(accimg, CV_8UC3, 255);

    countimg = countimg > 0.5;
    countimg.convertTo(countimg, CV_8UC1, 255);

    dst.assign(accimg);
    dst_mask.assign(countimg);
}

inline
void cv::detail::MLEdgeBlender::feed(InputArray img, InputArray mask, Point tl)
{
    Mat img_ = img.getMat(), mask_ = mask.getMat();
    Mat full_img_f; img_.convertTo(full_img_f, CV_32FC3, 1. / 255);
    Mat full_mask_f; mask_.convertTo(full_mask_f, CV_32FC1, 1. / 255);

    Rect roi(tl - dst_roi_.tl(), full_img_f.size());

    Mat count_roi = countimg(roi);
    accimg(roi).forEach<Vec3f>([&](Vec3f& pix, const int* pos) {
        float& count = (*count_roi.ptr<float>(pos[0], pos[1]));
        const Vec3f& src_pix = full_img_f.at<Vec3f>(pos[0], pos[1]);
        const float& src_ma = full_mask_f.at<float>(pos[0], pos[1]);
        if (src_ma && count > 0.5)
        {
            pix = src_pix * 0.2 + 0.8 * pix;
        }
        else if (src_ma)
        {
            pix = src_pix;
            count = 1.f;
        }
        });
}

inline
void cv::detail::MLEdgeBlender::prepare(Rect dst_roi)
{
    dst_roi_ = dst_roi;
    accimg = cv::Mat(dst_roi_.size(), CV_32FC3, cv::Scalar(0));
    countimg = cv::Mat(dst_roi_.size(), CV_32FC1, cv::Scalar(0));
}

inline cv::Ptr<cv::detail::Blender> cv::detail::MLEdgeBlender::createDefault()
{
    return makePtr<MLEdgeBlender>();
}
