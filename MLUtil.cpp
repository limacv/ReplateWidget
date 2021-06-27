#include "MLUtil.h"
#include <qdebug.h>
#include <opencv2/imgproc.hpp>
#include <qpixmap.h>
#include <qpainter.h>

namespace MLUtil
{
    QImage mat2qimage(const cv::Mat& src, const QImage::Format& format)
    {
        QImage ret;
        // TODO: add more format
        if (src.type() == CV_8UC3)
        {
            ret = QImage((const uchar*)src.data, src.cols, src.rows, src.step, QImage::Format_RGB888)
                .convertToFormat(format);
        }
        else if (src.type() == CV_8UC1)
        {
            ret = QImage((const uchar*)src.data, src.cols, src.rows, src.step, QImage::Format_Grayscale8)
                .convertToFormat(format);
        }
        else if (src.type() == CV_8UC4)
        {
            ret = QImage((const uchar*)src.data, src.cols, src.rows, src.step, QImage::Format_RGBA8888)
                .convertToFormat(format);
        }
        else
            qDebug() << "Undefined format conversion! " << src.type() << format;
        return ret;
    }

    QPixmap mat2qpixmap(const cv::Mat& src, const QImage::Format& format)
    {
        return QPixmap::fromImage(mat2qimage(src, format));
    }

    cv::Mat qimage_to_mat_ref(QImage& img, int cv_format)
    {
        return cv::Mat(img.height(), img.width(),
            cv_format, img.bits(), img.bytesPerLine());
    }

    cv::Mat qimage_to_mat_cpy(const QImage& img, int cv_format)
    {
        return cv::Mat(img.height(), img.width(), cv_format,
            const_cast<uchar*>(img.bits()),
            img.bytesPerLine()).clone();
    }

    void cvtrgba2rgb_a(const cv::Mat& rgba, cv::Mat& rgb, cv::Mat& a)
    {
        cv::extractChannel(rgba, a, 3);
        cv::cvtColor(rgba, rgb, cv::COLOR_RGBA2RGB);
    }
    void cvtrgba2gray_a(const cv::Mat& rgba, cv::Mat& gray, cv::Mat& a)
    {
        cv::extractChannel(rgba, a, 3);
        cv::cvtColor(rgba, gray, cv::COLOR_RGBA2GRAY);
    }

    QIcon getIcon(ICON_ID id)
    {
        static QIcon play_icon = QIcon(QPixmap(":/ReplateWidget/images/play_button_icon.png"));
        static QIcon stop_icon = QIcon(QPixmap(":/ReplateWidget/images/stop_button_icon.png"));
        switch (id)
        {
        case ICON_ID::PLAY:
            return play_icon;
        case ICON_ID::STOP:
            return stop_icon;
        default:
            break;
        }
    }

    QRect scaleViewportWithRatio(const QRect& viewport, float wid_hei)
    {
        QRect rect_ori = viewport;
        float ratio_ori = (float)rect_ori.width() / rect_ori.height();
        if (ratio_ori > wid_hei)  // too wide
        {
            const int wid = rect_ori.height() * wid_hei;
            rect_ori.translate((rect_ori.width() - wid) / 2, 0);
            rect_ori.setWidth(wid);
        }
        else if (ratio_ori < wid_hei) // too thin
        {
            const int hei = rect_ori.width() / wid_hei;
            rect_ori.translate(0, (rect_ori.height() - hei) / 2);
            rect_ori.setHeight(hei);
        }
        return rect_ori;
    }


	constexpr float
		CIEDE2000::deg2Rad(
			const float deg)
	{
		return (deg * (M_PI / 180.0));
	}

	constexpr float
		CIEDE2000::rad2Deg(
			const float rad)
	{
		return ((180.0 / M_PI) * rad);
	}

	float
		CIEDE2000::CIEDE2000(
			const LAB& lab1,
			const LAB& lab2)
	{
		/*
		 * "For these and all other numerical/graphical 􏰀delta E00 values
		 * reported in this article, we set the parametric weighting factors
		 * to unity(i.e., k_L = k_C = k_H = 1.0)." (Page 27).
		 */
		const float k_L = 1.0, k_C = 1.0, k_H = 1.0;
		const float deg360InRad = CIEDE2000::deg2Rad(360.0);
		const float deg180InRad = CIEDE2000::deg2Rad(180.0);
		const float pow25To7 = 6103515625.0; /* pow(25, 7) */

		/*
		 * Step 1
		 */
		 /* Equation 2 */
		float C1 = sqrt((lab1.a * lab1.a) + (lab1.b * lab1.b));
		float C2 = sqrt((lab2.a * lab2.a) + (lab2.b * lab2.b));
		/* Equation 3 */
		float barC = (C1 + C2) / 2.0;
		/* Equation 4 */
		float G = 0.5 * (1 - sqrt(pow(barC, 7) / (pow(barC, 7) + pow25To7)));
		/* Equation 5 */
		float a1Prime = (1.0 + G) * lab1.a;
		float a2Prime = (1.0 + G) * lab2.a;
		/* Equation 6 */
		float CPrime1 = sqrt((a1Prime * a1Prime) + (lab1.b * lab1.b));
		float CPrime2 = sqrt((a2Prime * a2Prime) + (lab2.b * lab2.b));
		/* Equation 7 */
		float hPrime1;
		if (lab1.b == 0 && a1Prime == 0)
			hPrime1 = 0.0;
		else {
			hPrime1 = atan2f(lab1.b, a1Prime);
			/*
			 * This must be converted to a hue angle in degrees between 0
			 * and 360 by addition of 2􏰏 to negative hue angles.
			 */
			if (hPrime1 < 0)
				hPrime1 += deg360InRad;
		}
		float hPrime2;
		if (lab2.b == 0 && a2Prime == 0)
			hPrime2 = 0.0;
		else {
			hPrime2 = atan2f(lab2.b, a2Prime);
			/*
			 * This must be converted to a hue angle in degrees between 0
			 * and 360 by addition of 2􏰏 to negative hue angles.
			 */
			if (hPrime2 < 0)
				hPrime2 += deg360InRad;
		}

		/*
		 * Step 2
		 */
		 /* Equation 8 */
		float deltaLPrime = lab2.l - lab1.l;
		/* Equation 9 */
		float deltaCPrime = CPrime2 - CPrime1;
		/* Equation 10 */
		float deltahPrime;
		float CPrimeProduct = CPrime1 * CPrime2;
		if (CPrimeProduct == 0)
			deltahPrime = 0;
		else {
			/* Avoid the fabs() call */
			deltahPrime = hPrime2 - hPrime1;
			if (deltahPrime < -deg180InRad)
				deltahPrime += deg360InRad;
			else if (deltahPrime > deg180InRad)
				deltahPrime -= deg360InRad;
		}
		/* Equation 11 */
		float deltaHPrime = 2.0 * sqrtf(CPrimeProduct) *
			sinf(deltahPrime / 2.0);

		/*
		 * Step 3
		 */
		 /* Equation 12 */
		float barLPrime = (lab1.l + lab2.l) / 2.0;
		/* Equation 13 */
		float barCPrime = (CPrime1 + CPrime2) / 2.0;
		/* Equation 14 */
		float barhPrime, hPrimeSum = hPrime1 + hPrime2;
		if (CPrime1 * CPrime2 == 0) {
			barhPrime = hPrimeSum;
		}
		else {
			if (fabs(hPrime1 - hPrime2) <= deg180InRad)
				barhPrime = hPrimeSum / 2.0;
			else {
				if (hPrimeSum < deg360InRad)
					barhPrime = (hPrimeSum + deg360InRad) / 2.0;
				else
					barhPrime = (hPrimeSum - deg360InRad) / 2.0;
			}
		}
		/* Equation 15 */
		float T = 1.0 - (0.17 * cosf(barhPrime - CIEDE2000::deg2Rad(30.0))) +
			(0.24 * cosf(2.0 * barhPrime)) +
			(0.32 * cosf((3.0 * barhPrime) + CIEDE2000::deg2Rad(6.0))) -
			(0.20 * cosf((4.0 * barhPrime) - CIEDE2000::deg2Rad(63.0)));
		/* Equation 16 */
		float deltaTheta = CIEDE2000::deg2Rad(30.0) *
			exp(-pow((barhPrime - deg2Rad(275.0)) / deg2Rad(25.0), 2.0));
		/* Equation 17 */
		float R_C = 2.0 * sqrt(pow(barCPrime, 7.0) /
			(pow(barCPrime, 7.0) + pow25To7));
		/* Equation 18 */
		float S_L = 1 + ((0.015 * pow(barLPrime - 50.0, 2.0)) /
			sqrt(20 + pow(barLPrime - 50.0, 2.0)));
		/* Equation 19 */
		float S_C = 1 + (0.045 * barCPrime);
		/* Equation 20 */
		float S_H = 1 + (0.015 * barCPrime * T);
		/* Equation 21 */
		float R_T = (-sin(2.0 * deltaTheta)) * R_C;

		/* Equation 22 */
		float deltaE = sqrt(
			pow(deltaLPrime / (k_L * S_L), 2.0) +
			pow(deltaCPrime / (k_C * S_C), 2.0) +
			pow(deltaHPrime / (k_H * S_H), 2.0) +
			(R_T * (deltaCPrime / (k_C * S_C)) * (deltaHPrime / (k_H * S_H))));

		return (deltaE);
	}
}
