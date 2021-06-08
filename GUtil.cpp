#include "GUtil.h"
#include "intrin.h"
#include <cmath>

//QPoint GeImageUtility::bgr2uv(const QRgb *rgb)
//{
//    QPointF uv;
//    const unsigned char* c = (const unsigned char*)rgb;
//    //uv.setY( -0.14713*c[0]-0.28886*c[1]+0.436*c[2] );
//    //uv.setX( 0.615*c[0]-0.51499*c[1]-0.10001*c[2] );
//    uv.setX( clamp(-0.148*c[0]-0.291*c[1]+0.439*c[2], -128, 127) );
//    uv.setY( clamp(0.439*c[0]-0.368*c[1]-0.071*c[2], -128, 127) );
//    uv /= 2;
//    return uv.toPoint();
//}

//trianglePath

//int GUtil::anchor_length = 0;

static const QRgb PreColor[] = {
    qRgb(0, 255, 255), // cyan
    qRgb(255, 255, 0), // yellow
    qRgb(255,0,255), // Magenta
    qRgb(0,128,128), // Teal
    qRgb(178,34,34), // firebrick
    qRgb(128,128,0), // Olive
    qRgb(128,128,128), // Gray
    qRgb(128,0,128), // Purple
    qRgb(0,128,0), //Green
    qRgb(255, 255, 128),
    qRgb(0, 255, 255), // cyan
    qRgb(255, 255, 0), // yellow
    qRgb(255,0,255), // Magenta
    qRgb(0,128,128), // Teal
    qRgb(178,34,34), // firebrick
    qRgb(128,128,0), // Olive
    qRgb(128,128,128), // Gray
    qRgb(128,0,128), // Purple
    qRgb(0,128,0), //Green
    qRgb(255, 255, 128)
};

static const int FadeDuration[] = {
    0,
    5,
    10,
    15,
    20
};


//int GUtil::getAnchor_length()
//{
//    return anchor_length;
//}

//void GUtil::setAnchor_length(int value)
//{
//    anchor_length = value;
//}

QRect GUtil::cvtRect(cv::Rect rect)
{
    return QRect(rect.x, rect.y, rect.width, rect.height);
}

cv::Rect GUtil::cvtRect(QRect rect)
{
    return cv::Rect(rect.x(), rect.y(), rect.width(), rect.height());
}

QSize GUtil::cvtSize(cv::Size rect)
{
    return QSize(rect.width, rect.height);
}

cv::Size GUtil::cvtSize(QSize rect)
{
    return cv::Size(rect.width(), rect.height());
}

QRgb GUtil::getPreDefinedColor(int i)
{
    return PreColor[i];
}

QPoint GUtil::bgr2uv(const cv::Vec3b *bgr)
{
    QPointF uv;
    const unsigned char* c = (const unsigned char*)bgr;
    qreal u = -0.14713*c[2]-0.28886*c[1]+0.436*c[0];
    qreal v = 0.615*c[2]-0.51499*c[1]-0.10001*c[0];
    u /= -2;
    v /= -2;
    uv.setX( u + 128 );
    uv.setY( v + 128 );
    //uv.setX( -0.148*c[2]-0.291*c[1]+0.439*c[0] + 128 );
    //uv.setY( 0.439*c[2]-0.368*c[1]-0.071*c[0] + 128 );
    return uv.toPoint();
}

void GUtil::writeImgToFile(const QImage &img, const QString &filename)
{
    QFile file(filename);
    file.open(QIODevice::Append);
    QTextStream out(&file);

    for (int r = 0; r < img.height(); ++r)
    {
        QRgb *row = (QRgb*)img.scanLine(r);
        for (int c = 0; c < img.width(); ++c)
        {
            out << row[c] << ' ';
        }
        out << endl;
    }

    out << "****************************" << endl << endl;
    file.close();
}

bool GUtil::bgr2uv(const uchar *bgr, uchar *uv)
{
    if (!uv)
        return false;

    qreal u = -0.14713*bgr[2]-0.28886*bgr[1]+0.436*bgr[0];
    qreal v = 0.615*bgr[2]-0.51499*bgr[1]-0.10001*bgr[0];
    u /= -2;
    v /= -2;
    uv[0] = qRound(u) + 128;
    uv[1] = qRound(v) + 128;
    return true;
}

void GUtil::drawGradientLine(QPainter &painter, const QPoint *points,
                                      int pointSize, int startH, int range)
{
    for ( int i = 0; i < pointSize - 1; ++i)
    {
        QColor gradient = QColor::fromHsv( ((i % range) + startH) % 360, 255, 255);
        //QColor gradient = QColor::fromHsv( i % 360, 255, 255);
        painter.setPen(QPen(gradient, 2, Qt::SolidLine));
        //painter.setPen(QPen(Qt::red, 2, Qt::SolidLine));
        painter.drawLine(points[i], points[i+1]);
    }
}

float GUtil::clamp(float x, float a, float b)
{
    return x < a ? a : (x > b ? b : x);
}

double GUtil::getAngle(QPointF src, QPointF p1, QPointF p2)
{
    QPointF v1 = p1 - src;
    QPointF v2 = p2 - src;
    double prod = v1.x() * v2.x() + v1.y() * v2.y();
    double v1_len = sqrt(v1.x() * v1.x() + v1.y() * v1.y());
    double v2_len = sqrt(v2.x() * v2.x() + v2.y() * v2.y());
    double cos_value = prod / (v1_len * v2_len);
    if (cos_value < -1 && cos_value > -2)
        cos_value = -1;
    else if (cos_value > 1 && cos_value < 2)
        cos_value = 1;

    return acos(cos_value) ;
}

void GUtil::resizeImage( const cv::Mat &src, cv::Mat &tar, const cv::Size &tarSize, bool keepRatio )
{
    cv::Size oriSize(src.cols, src.rows);
    if (keepRatio)
    {
        float origAspect = oriSize.width / (float) oriSize.height;
        float newAspect = tarSize.width / (float) tarSize.height;
        float scale;
        if (origAspect > newAspect)
            scale = tarSize.width / (float)oriSize.width;
        else
            scale = tarSize.height / (float)oriSize.height;
        cv::Size newSize(oriSize.width * scale, oriSize.height * scale);
        cv::resize(src, tar, newSize);
    }
    else
        cv::resize(src, tar, tarSize);
}

QImage GUtil::mat2qimage(const cv::Mat& src, QImage::Format format)
{
    QImage ret;
    // TODO: add more format
    if (src.type() == CV_8UC3)
    {
        if ( format == QImage::Format_RGB888)
        {
            ret = QImage((const uchar*) src.data,
                         src.cols, src.rows, src.step, format);
        }
        else if (format == QImage::Format_ARGB32_Premultiplied)
        {
            ret = QImage((const uchar*) src.data, src.cols, src.rows,
                         src.step, QImage::Format_RGB888).convertToFormat(format);
        }
        else
            qDebug() << "Undefined format conversion! " << src.type() << format;
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
            ret = QImage((const uchar*) bgr.data, bgr.cols, bgr.rows, bgr.step,
                         QImage::Format_ARGB32).convertToFormat(format);
        }
        else
            qDebug() << "Undefined format conversion! " << src.type() << format;
    }
    else
        qDebug() << "Undefined format conversion! " << src.type() << format;
    if(0)
    {
        imwrite("mat2qimagecv.png", src);
        ret.save("mat2qimage.png");
    }
    return ret;
}

cv::Mat GUtil::qimage_to_mat_ref(QImage &img, int format)
{
    return cv::Mat(img.height(), img.width(),
                   format, img.bits(), img.bytesPerLine());
}

cv::Mat GUtil::qimage_to_mat_cpy(const QImage &img, int format)
{
    return cv::Mat(img.height(), img.width(), format,
                   const_cast<uchar*>(img.bits()),
                   img.bytesPerLine()).clone();
}

void GUtil::averageExtractedImages(const QVector<cv::Mat4b> &matarray, cv::Mat4b &out)
{
    if (matarray.size() == 0)
        return;
    cv::Mat4i sum(matarray[0].rows, matarray[0].cols, cv::Vec4i(0));
    for (int i = 0; i < matarray.size(); ++i)
    {
        const cv::Mat4b &mat = matarray[i];
        for (int y = 0; y < sum.rows; ++y)
        {
            cv::Vec4i *rowSum = sum[y];
            const cv::Vec4b *rowMat = mat[y];
            for (int x = 0; x < sum.cols; ++x)
            {
                if (cv::norm(rowMat[x]) != 0 )
                    rowSum[x] += cv::Vec4i(rowMat[x][0], rowMat[x][1], rowMat[x][2], 1);
            }
        }
        //imwrite(QString("%1.png").arg(i).toStdString(), mat);
    }
    // subdivide
    out.create(sum.rows, sum.cols);
    for (int y = 0; y < sum.rows; ++y)
    {
        const cv::Vec4i *rowSum = sum[y];
        cv::Vec4b *rowOut = out[y];
        for (int x = 0; x < sum.cols; ++x)
        {
            const cv::Vec4i &v = rowSum[x];
            if (v[3] != 0)
                rowOut[x] = cv::Vec4b(v[0]/v[3], v[1]/v[3], v[2]/v[3], 255);
            else
                rowOut[x] = cv::Vec4b(0,0,0,0);
        }
    }
    //imwrite(QString("foreground.png").toStdString(), out);
}

void GUtil::averageImages(const std::vector<cv::Mat4b> &matarray, cv::Mat4b &out)
{
    int len = matarray.size();
    if (len == 0)
        return;
    cv::Mat4i sum(matarray[0].rows, matarray[0].cols, cv::Vec4i(0));
    for (size_t i = 0; i < matarray.size(); ++i)
    {
        const cv::Mat4b &mat = matarray[i];
        for (int y = 0; y < sum.rows; ++y)
        {
            cv::Vec4i *rowSum = sum[y];
            const cv::Vec4b *rowMat = mat[y];
            for (int x = 0; x < sum.cols; ++x)
                rowSum[x] += cv::Vec4i(rowMat[x][0], rowMat[x][1], rowMat[x][2], rowMat[x][3]);
        }
        //imwrite(QString("%1.png").arg(i).toStdString(), mat);
    }
    // subdivide
    out.create(sum.rows, sum.cols);
    for (int y = 0; y < sum.rows; ++y)
    {
        const cv::Vec4i *rowSum = sum[y];
        cv::Vec4b *rowOut = out[y];
        for (int x = 0; x < sum.cols; ++x)
        {
            const cv::Vec4i &v = rowSum[x];
            //if (v[3] != 0)
            rowOut[x] = cv::Vec4b(v[0]/len, v[1]/len, v[2]/len, v[3]/len);
            //else
            //    rowOut[x] = cv::Vec4b(0,0,0,0);
        }
    }
    //imwrite(QString("foreground.png").toStdString(), out);
}

void GUtil::adaptiveImages(const std::vector<cv::Mat> &matarray,
                           std::vector<cv::Mat> &out)
{
    int n = matarray.size();
    if (n == 0) {
        qDebug() << "weighted Images: zero input";
        return;
    }
    out.resize(n);

    int rows = matarray[0].rows;
    int cols = matarray[0].cols;

    std::vector<cv::Mat4i> matarray4i(n);
    cv::Mat4i sumOne(rows, cols, cv::Vec4i(0));
    for (int i = 0; i < n; ++i) {
        matarray[i].convertTo(matarray4i[i], CV_32SC4);
        sumOne += matarray4i[i];
//        cv::imwrite(QString("4i_%1.png").arg(i).toStdString(), matarray4i[i]);
    }
    sumOne /= n;

    // I did a simple composition here instead of the original method
    for (int i = 0; i < n; ++i) {
        cv::Mat4i avg = (matarray4i[i] + sumOne * 2) / 3;
        avg.convertTo(out[i], CV_8UC4);
//        cv::imwrite(QString("%1.png").arg(i).toStdString(), out[i]);
    }

//    cv::Mat4i sumOne(rows, cols, cv::Vec4i(0));
//    for (size_t i = 0; i < matarray.size(); ++i)
//    {
//        tmp4iArray[i].create(rows, cols);
//        cv::Mat4i &mat = tmp4iArray[i];
//        const cv::Mat4b &src = matarray[i];
//        for (int y = 0; y < mat.rows; ++y)
//        {
//            cv::Vec4i *rowMat = mat[y];
//            cv::Vec4i *rowSum = sumOne[y];
//            const cv::Vec4b *rowSrc = src[y];
//            for (int x = 0; x < mat.cols; ++x)
//            {
//                rowMat[x] = cv::Vec4i(rowSrc[x][0], rowSrc[x][1], rowSrc[x][2], rowSrc[x][3]);
//                rowSum[x] += rowMat[x];
//            }
//        }
//    }

//    // calculate the base mat
//    cv::Mat4i tmpArrayTar(rows, cols, cv::Vec4i(0));
//    for (size_t i = 0; i < matarray.size(); ++i)
//        tmpArrayTar += (maxN - i) * tmp4iArray[i];


//    //cv::Mat4i off = sumOne.clone();
//    int len = (matarray.size() * (matarray.size()+1)) / 2;
//    int addlen = matarray.size();
//    for (size_t i = 0; i < matarray.size(); ++i)
//    {
//        if (i != 0)
//        {
//            sumOne -= 2*tmp4iArray[i-1];
//            tmpArrayTar += sumOne;
//            addlen -= 2;
//            len += addlen;
//        }
////        qDebug() << i << len << addlen;

//        // subdivide
//        out[i].create(rows, cols);
//        for (int y = 0; y < rows; ++y)
//        {
//            const cv::Vec4i *rowTmpTar = tmpArrayTar[y];
//            cv::Vec4b *rowOut = out[i][y];
//            for (int x = 0; x < cols; ++x)
//            {
//                const cv::Vec4i &v = rowTmpTar[x];
//                rowOut[x] = cv::Vec4b(v[0]/len, v[1]/len, v[2]/len, v[3]/len);
//            }
//        }
//        //imwrite(QString("%1_.png").arg(i).toStdString(),out[i]);
//    }
}

QRect GUtil::scaleQRect(const QRect &src, float scale)
{
    return QRect(src.x() * scale, src.y() * scale,
                 src.width() * scale, src.height() * scale);
}

void GUtil::boundRectF(QRectF &rectF)
{
    QPointF offset(0, 0);
    if (rectF.left() < 0)
        offset.setX(-rectF.left());
    else if (rectF.right() > 1)
        offset.setX(1 - rectF.right());
    if (rectF.top() < 0)
        offset.setY(-rectF.top());
    else if (rectF.bottom() > 1)
        offset.setY(1 - rectF.bottom());
    rectF.translate(offset);
}

double GUtil::calScaleFactor(const QSize &src, const QSize &tar)
{
    double scale;
    double srcAspect = src.width() / (double) src.height();
    double tarAspect = tar.width() / (double) tar.height();
    if (srcAspect > tarAspect)
        scale = tar.width() / (double) src.width();
    else
        scale = tar.height() / (double) src.height();
    return scale;
}

QImage GUtil::getTrashImage(const QImage &bg, QSize size, QRect rect)
{
    QImage img = (size.isValid()? bg.scaled(size, Qt::KeepAspectRatio).copy(rect)
                                : bg.copy(rect));

    for (int y = 0; y < img.height(); ++y) {
        QRgb l = img.pixel(0, y);
        QRgb r = img.pixel(img.width()-1, y);
        QRgb v = qRgb((qRed(l)+qRed(r))/2, (qGreen(l)+qGreen(r))/2, (qBlue(l)+qBlue(r))/2);
        for (int x = 0; x < img.width(); ++x)
            img.setPixel(x, y, v);
    }
    return img;
}

std::vector<QPointF> GUtil::smooth(const std::vector<QPointF> &path, int k)
{
    if (k == 0) return path;

    std::vector<QPointF> ret(path.size());
    for (size_t i = 0; i < path.size(); ++i) {
        if (i == 0 || i == path.size() - 1)
            ret[i] = path[i];
        else {
            double angle = getAngle(path[i], path[i-1], path[i+1]);
            double dis0 = (path[i] - path[i-1]).manhattanLength();
            double dis1 = (path[i+1] - path[i]).manhattanLength();
            if (angle >= 3.14 *2.0 / 3.0 || (dis0 < 5 && dis1 < 5))
                ret[i] = ((path[i-1] + path[i+1]) / 2.0 + path[i]) / 2.0;
            else
                ret[i] = path[i];

//            if (angle < 3.14 * 2.0 / 3.0 || )
//                ret[i] = path[i];
//            else
//                ret[i] = ((path[i-1] + path[i+1]) / 2.0 + path[i]) / 2.0;

//            ret[i] = ((path[i-1] + path[i+1]) / 2.0 + path[i]) / 2.0;
        }
        //qDebug() << "Path" << i << ":" << ret[i] << path[i];
    }
    return smoothAngle(ret, k-1);
}

std::vector<QPointF> GUtil::smooth2(const std::vector<QPointF> &path, int k)
{
    if (k == 0) return path;

    std::vector<QPointF> ret(path.size());
    for (size_t i = 0; i < path.size(); ++i) {
        if (i == 0 || i == path.size() - 1)
            ret[i] = path[i];
        else {
            ret[i] = ((path[i-1] + path[i+1]) / 2.0 + path[i]) / 2.0;
        }
    }
    return smoothAngle(ret, k-1);
}

std::vector<QPointF> GUtil::smoothAngle(const std::vector<QPointF> &path, int k)
{
    if (k == 0) return path;

    std::vector<QPointF> ret(path.size());
    for (size_t i = 0; i < path.size(); ++i) {
        if (i == 0 || i == path.size() - 1)
            ret[i] = path[i];
        else {
            double angle = getAngle(path[i], path[i-1], path[i+1]);
            if (angle < 3.14 * 2.0 / 3.0)
                ret[i] = path[i];
            else
                ret[i] = ((path[i-1] + path[i+1]) / 2.0 + path[i]) / 2.0;
        }
        //qDebug() << "Path" << i << ":" << ret[i] << path[i];
    }
    return smooth(ret, k-1);
}

//cv::Mat3b GUtil::alphaBlend(const cv::Mat3b &src, const cv::Mat1b &src_mask, cv::Mat3b &dst, double alpha)
//{
//    cv::Mat3b src_fg;
//    src.copyTo(src_fg, src_mask);
//    cv::Mat3b res = src_fg * alpha + dst * (1.0 - alpha);
//    return res;
//}

void GUtil::alphaBlendBorder(
        const cv::Mat3b &src, const cv::Mat1b &src_mask,
        cv::Mat3b &dst, double alpha)
{
    CV_Assert(src.size() == dst.size());
//    qDebug() << "Alpha" << alpha;
//    GUtil::writeImage("src.png", src, src_mask);
//    GUtil::writeImage("dst.png", dst, src_mask);

//    int w_blur = std::max(std::min(20, src.cols / 10), 1);
//    int h_blur = std::max(std::min(20, src.rows / 10), 1);
    int w_blur = std::max(std::min(50, src.cols / 4), 1);
    int h_blur = std::max(std::min(50, src.rows / 4), 1);

    for (int y = 0; y < src.rows; ++y) {
        const cv::Vec3b *pSrc = src.ptr<cv::Vec3b>(y);
        const uchar *pMask = src_mask.ptr<uchar>(y);
        cv::Vec3b *pDst = dst.ptr<cv::Vec3b>(y);
        for (int x = 0; x < src.cols; ++x) {
            float y_edge = std::min(std::min(y, src.rows - y - 1) / (float) h_blur, 1.0f);
            float x_edge = std::min(std::min(x, src.cols - x - 1) / (float) w_blur, 1.0f);
            float min_edge = std::min(x_edge, y_edge);
            double trans = alpha;
            if (min_edge < 1)
                trans *= min_edge;
            if (pMask[x] > 0) {
                pDst[x] = pSrc[x] * trans + pDst[x] * (1 - trans);
                //pDst[x] = cv::Vec3b(255* trans,0,0);
//                qDebug() << alpha;
//                exit(-1);
            }
        }
    }
//    GUtil::writeImage("dst_after.png", dst, src_mask);
//    exit(-1);
}

void GUtil::writeImage(const std::string name, const cv::Mat3b &image, const cv::Mat1b &mask)
{
    CV_Assert(image.size() == mask.size());
    cv::Mat output;
    image.copyTo(output, mask);
    cv::imwrite(name, output);
}

void GUtil::addTriangle(QPainter &painter, QPoint pos, int color_id, float scale)
{
    painter.setPen(QPen(Qt::black, 1, Qt::SolidLine));
    painter.setBrush(QBrush(PreColor[color_id]));
    QPolygon polygon;
    polygon << pos << pos + QPoint(3, -7) * scale << pos + QPoint(-3, -7) * scale;
    painter.drawPolygon(polygon);
}

void GUtil::addHalo(QPainter &painter, float radius, QPointF center, float alpha, int color_id)
{
    QRadialGradient gr(center, radius);
    gr.setColorAt(0.0, QColor(0, 0, 0, 0));
    QColor c = PreColor[color_id % 10];
    c.setAlphaF(0.2 * alpha);
    gr.setColorAt(0.1, c);
    c.setAlphaF(0.7 * alpha);
    gr.setColorAt(0.5, c);
    c.setAlphaF(0.2 * alpha);
    gr.setColorAt(0.9, c);
    gr.setColorAt(0.95, QColor(0, 0, 0, 0));
    gr.setColorAt(1, QColor(0, 0, 0, 0));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(gr);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(center, radius, radius);

//    painter.drawEllipse(0, 0, radius*2, radius*2);
}

QImage GUtil::addHalo(const QImage &img, float alpha, int color_id)
{
    QImage pic(img);
    QPainter pt(&pic);
    qreal rad = qMin(pic.width(), pic.height()) / 2.0;
//    qDebug() << "Rad2!!" << img.width() << pic.height() << rad;
    QRadialGradient gr(rad, rad, rad, rad, rad);
    gr.setColorAt(0.0, QColor(0, 0, 0, 0));
//    gr.setColorAt(0.1, QColor(150, 150, 200, 63 * alpha));
//    gr.setColorAt(0.5, QColor(255, 255, 127, 140 * alpha));
//    gr.setColorAt(0.9, QColor(150, 150, 200, 63 * alpha));
//    gr.setColorAt(0.1, QColor(0, 174, 174, 53 * alpha));
//    gr.setColorAt(0.5, QColor(0, 227, 227, 100 * alpha));
//    gr.setColorAt(0.9, QColor(0, 174, 174, 53 * alpha));
    int num_color = sizeof(PreColor) / sizeof(PreColor[0]);
    QColor c = PreColor[color_id % num_color];
    c.setAlphaF(0.2 * alpha);
    gr.setColorAt(0.1, c);
    c.setAlphaF(0.7 * alpha);
    gr.setColorAt(0.5, c);
    c.setAlphaF(0.2 * alpha);
    gr.setColorAt(0.9, c);
    gr.setColorAt(0.95, QColor(0, 0, 0, 0));
    gr.setColorAt(1, QColor(0, 0, 0, 0));
    pt.setRenderHint(QPainter::Antialiasing);
    pt.setBrush(gr);
    pt.setPen(Qt::NoPen);
    pt.drawEllipse(0, 0, pic.width(), pic.height());
    return pic;
}

QImage GUtil::addHalo(const QImage &img, const std::set<int> &halo, int id,
                      int color_id)
{
    int radius = 3;
    for (std::set<int>::iterator it = halo.begin(); it != halo.end(); ++it) {
        int offset = abs(*it - id);
        if (offset <= radius) {
            float alpha = (radius-offset+1.0) / (radius+1.0);
            alpha = sqrt(alpha) * 0.7;
            //alpha = 1;
            return addHalo(img, alpha, color_id);
        }
    }
    return img;
}

cv::Rect GUtil::addMarginToRect(const cv::Rect& rect, int margin)
{
    return cv::Rect(rect.x - margin, rect.y - margin, rect.width + 2 * margin, rect.height + 2 * margin);
}

cv::Mat1b GUtil::cvtPainterPath2Mask(const QPainterPath &path)
{
    QRect rect = path.boundingRect().toRect();
    QImage mask(rect.size(), QImage::Format_ARGB32_Premultiplied);
    mask.fill(0);
    QPainter pt(&mask);
    pt.setClipPath(path.translated(-rect.topLeft()));
    pt.fillRect(pt.viewport(), Qt::white);
    return GUtil::qimage_to_mat_cpy(mask.alphaChannel(), CV_8UC1);
}

void GUtil::overlayFeather(const cv::Mat4b& fg, const cv::Mat1b& fg_mask, cv::Mat4b& bg)
{
    // border filter
//    cv::imwrite("fg.png", fg);
//    cv::imwrite("bg.png", bg);
    int w_blur = qMax(qMin(20, fg.cols / 10), 1);
    int h_blur = qMax(qMin(20, fg.rows / 10), 1);
    for (int y = 0; y < fg.rows; ++y) {
        const cv::Vec4b* yFg = fg[y];
        const uchar* yFg_mask = fg_mask[y];
        cv::Vec4b* yBg = bg[y];
        for (int x = 0; x < fg.cols; ++x) {
            if (yFg_mask[x] > 0) {
                qreal dis = abs(yFg[x][0] - yBg[x][0])
                    + abs(yFg[x][1] - yBg[x][1])
                    + abs(yFg[x][2] - yBg[x][2]);

                int alpha = 0;
                if (dis < 17) alpha = qMin(dis * dis, 255.0);

                else alpha = 255;

                float y_edge = qMin(qMin(y, fg.rows - y - 1) / (float)h_blur, 1.0f);
                float x_edge = qMin(qMin(x, fg.cols - x - 1) / (float)w_blur, 1.0f);
                if (x_edge < 1 || y_edge < 1) {
                    alpha *= qMin(x_edge, y_edge);
                }
                float a = alpha / 255.0;
                yBg[x] = yFg[x] * a + yBg[x] * (1.0 - a);
            }
        }
    }

}