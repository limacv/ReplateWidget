#pragma once
#include <opencv2/core.hpp>
#include <qimage.h>
#include <qicon.h>
#include <string>
#include <exception>

namespace MLUtil
{
	// you need to guarantee that cvmat is RGB
	QImage mat2qimage(const cv::Mat& src, const QImage::Format& format);
	QPixmap mat2qpixmap(const cv::Mat& src, const QImage::Format& format);
	cv::Mat qimage_to_mat_ref(QImage& img, int format);
	cv::Mat qimage_to_mat_cpy(const QImage& img, int format);
	void cvtrgba2rgb_a(const cv::Mat& rgba, cv::Mat& rgb, cv::Mat& a);
	void cvtrgba2gray_a(const cv::Mat& rgba, cv::Mat& gray, cv::Mat& a);

	enum class ICON_ID
	{
		PLAY = 0, 
		STOP = 1
	};
	QIcon getIcon(ICON_ID id);
	QRect scaleViewportWithRatio(const QRect& viewport, float wid_hei);

	struct UserCancelException : public std::exception
	{
		virtual const char* what() const throw()
		{
			return "Canceled by user";
		}
	};


	/** Namespace containing all necessary objects and methods for CIEDE2000 */
	namespace CIEDE2000
	{
		struct LAB
		{
			/** Lightness */
			float l;
			/** Color-opponent a dimension */
			float a;
			/** Color-opponent b dimension */
			float b;
		};
		/** Convenience definition for struct LAB */
		using LAB = struct LAB;

		float
			CIEDE2000(
				const LAB& lab1,
				const LAB& lab2);

		constexpr float
			deg2Rad(
				const float deg);

		constexpr float
			rad2Deg(
				const float rad);
	}
};

