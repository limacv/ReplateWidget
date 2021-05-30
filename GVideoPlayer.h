#ifndef GVIDEOPLAYER_H
#define GVIDEOPLAYER_H

// code modified from http://codingexodus.blogspot.hk/2012/12/working-with-video-using-opencv-and-qt.html

#include <QMutex>
#include <QThread>
#include <QImage>
#include <QWaitCondition>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <QDebug>
#include <string>

class GVideoPlayer : public QThread
{
    Q_OBJECT

private:
    bool stop;
    QMutex mutex;
    QWaitCondition condition;
    cv::Mat frame;
    cv::VideoCapture capture;
    cv::Mat RGBframe;
    QImage img;
    int m_frameRate;
    size_t m_curFrameId;
    size_t num_images_;
signals:
    //Signal to output frame to be displayed
    //void processedImage(const QImage &image);
    void processedImage(const cv::Mat &image);
    void playFrame(int iFrame);

protected:
    void run();
    //void msleep(int ms);

public:
    //Constructor
    explicit GVideoPlayer(QObject *parent = 0);
    //Destructor
    ~GVideoPlayer();
    // initialize
    void initialize(size_t curFrameId, size_t nTotalFrames, size_t frameRate);
    //Load a video from memory
    bool loadVideo(std::string filename);
    //check if the player has been stopped
    bool isStopped() const;
    void setFrameRate(int rate) { m_frameRate = rate; }
    int frameRate() const {return m_frameRate;}

public slots:
    //Play the video
    void Play();
    //Stop the video
    void Stop();
};

#endif // GVIDEOPLAYER_H
