#pragma once

#include <QImage>
#include <QFile>
#include <QTextStream>
#include <QPainter>
#include <QDebug>
#include <QPainterPath>
#include <opencv2/opencv.hpp>

namespace GUtil
{
    QRect cvtRect(cv::Rect rect);
    cv::Rect cvtRect(QRect rect);
    QSize cvtSize(cv::Size rect);
    cv::Size cvtSize(QSize rect);

    QRgb getPreDefinedColor(int i);
    //QPoint bgr2uv(const QRgb *rgb);
    QPoint bgr2uv(const cv::Vec3b *bgr);
    void writeImgToFile(const QImage &img, const QString &filename);

    bool bgr2uv(const uchar *bgr, uchar *uv);

    // [startH, startH+range)
    void drawGradientLine(QPainter &painter, const QPoint* points,
                                 int pointSize, int startH = 0, int range = 360);
    inline float clamp(float x, float a, float b);

    double getAngle(QPointF src, QPointF p1, QPointF p2);

    void resizeImage(const cv::Mat &src, cv::Mat &tar, const cv::Size &size, bool keepRatio = true);

    QImage mat2qimage(const cv::Mat &src, QImage::Format format = QImage::Format_RGB888);

    // found from http://qtandopencv.blogspot.hk/2013/08/how-to-convert-between-cvmat-and-qimage.html
    cv::Mat qimage_to_mat_ref(QImage &img, int format);

    cv::Mat qimage_to_mat_cpy(QImage const &img, int format);
    void averageExtractedImages(const QVector<cv::Mat4b> &matarray, cv::Mat4b &out);

    void averageImages(const std::vector<cv::Mat4b> &matarray, cv::Mat4b &out);

    void adaptiveImages(const std::vector<cv::Mat> &matarray, std::vector<cv::Mat> &out);

    QRect scaleQRect(const QRect &src, float scale);

    void boundRectF(QRectF &rectF);

    double calScaleFactor(const QSize &src, const QSize &tar);

    QImage getTrashImage(const QImage &bg, QSize size, QRect rect);

    std::vector<QPointF> smooth(const std::vector<QPointF> &path, int k = 1);

    std::vector<QPointF> smooth2(const std::vector<QPointF> &path, int k = 1);

    std::vector<QPointF> smoothAngle(const std::vector<QPointF> &path, int k = 1);

//    cv::Mat3b alphaBlend(const cv::Mat3b &src, const cv::Mat1b &src_mask,
//                                cv::Mat3b &dst, double alpha);

    void alphaBlendBorder(const cv::Mat3b &src, const cv::Mat1b &src_mask,
                                      cv::Mat3b &dst, double alpha);

    void writeImage(const std::string name, const cv::Mat3b &image, const cv::Mat1b &mask);

    void addTriangle(QPainter &painter, QPoint pos, int color_id, float scale = 1.0);

    void addHalo(QPainter &painter, float radius,
                          QPointF center, float alpha, int color_id);

    QImage addHalo(const QImage &img, float alpha, int color_id);

    QImage addHalo(const QImage &img, const std::set<int> &halo,
                          int id, int color_id = 1);

    cv::Rect addMarginToRect(const cv::Rect& rect, int margin);

//    cv::Mat1b cvtPainterPath2Mask(
//            QSize window_size, const QPainterPath &path, QSize ori_size = QSize());
    cv::Mat1b cvtPainterPath2Mask(const QPainterPath &path);
    void overlayFeather(const cv::Mat4b& fg, const cv::Mat1b& fg_mask, cv::Mat4b& bg);
};