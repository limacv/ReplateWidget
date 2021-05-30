#pragma once
#include <opencv2/core.hpp>
#include <qimage.h>

namespace MLUtil
{
	// you need to guarantee that cvmat is RGB
	QImage mat2qimage(const cv::Mat& src, const QImage::Format& format);
	QPixmap mat2qpixmap(const cv::Mat& src, const QImage::Format& format);
	cv::Mat qimage_to_mat_ref(QImage& img, int format);
	cv::Mat qimage_to_mat_cpy(const QImage& img, int format);
	void cvtrgba2rgb_a(const cv::Mat& rgba, cv::Mat& rgb, cv::Mat& a);
	void cvtrgba2gray_a(const cv::Mat& rgba, cv::Mat& gray, cv::Mat& a);
};

