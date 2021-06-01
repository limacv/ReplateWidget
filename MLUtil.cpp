#include "MLUtil.h"
#include <qdebug.h>
#include <opencv2/imgproc.hpp>
#include <qpixmap.h>

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

    cv::Mat qimage_to_mat_ref(QImage& img, int format)
    {
        return cv::Mat(img.height(), img.width(),
            format, img.bits(), img.bytesPerLine());
    }

    cv::Mat qimage_to_mat_cpy(const QImage& img, int format)
    {
        return cv::Mat(img.height(), img.width(), format,
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
}
