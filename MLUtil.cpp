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
            if (format == QImage::Format_ARGB32_Premultiplied)
            {
                //            cv::Mat argb(src.rows, src.cols, CV_8UC4);
                //            int from_to[] = {0,1, 1,2, 2,3, 3,0};
                //            cv::mixChannels( &src, 1, &argb, 1, from_to, 4 );
                //            ret = QImage((const uchar*) argb.data, argb.cols, argb.rows,
                //                         QImage::Format_ARGB32_Premultiplied).copy();
                            // TODO: dont' know why it has to change format
                cv::Mat bgr;
                cv::cvtColor(src, bgr, cv::COLOR_RGBA2BGRA);
                ret = QImage((const uchar*)bgr.data, bgr.cols, bgr.rows, bgr.step,
                    QImage::Format_ARGB32).convertToFormat(format);
            }
            else
                qDebug() << "Undefined format conversion! " << src.type() << format;
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
}
