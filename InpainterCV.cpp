#include "InpainterCV.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>

void InpainterCV::inpaint(const cv::Mat& rgba, cv::Mat& inpainted)
{
	cv::Mat mask; cv::extractChannel(rgba, mask, 3);
	cv::Mat rgb; cv::cvtColor(rgba, rgb, cv::COLOR_RGBA2RGB);
	inpaint(rgb, mask, inpainted);
}

void InpainterCV::inpaint(const cv::Mat& img, const cv::Mat& mask, cv::Mat& inpainted)
{
	cv::Mat rgb;
	if (img.channels() == 4)
		cv::cvtColor(img, rgb, cv::COLOR_BGRA2BGR);
	else
		rgb = img;
	cv::inpaint(rgb, mask, inpainted, inpaintRadius, cv::INPAINT_TELEA);
}
