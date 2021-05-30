#include "GVideoPlayer.h"

GVideoPlayer::GVideoPlayer(QObject *parent)
    :QThread(parent)
    ,stop(true)
    ,m_frameRate(2)
{
}

bool GVideoPlayer::loadVideo(std::string filename)
{
    capture.open(filename);
    if (capture.isOpened())
    {
        //m_frameRate = (int) capture.get(CV_CAP_PROP_FPS);
        return true;
    }
    else
        return false;
}

void GVideoPlayer::Play()
{
    if (!isRunning()) {
        if (isStopped()){
            stop = false;
        }
        start(LowPriority);
    }
    qDebug() << "Player Play.";
}

void GVideoPlayer::run()
{
    while(!stop){
        int delay = (1000/m_frameRate);
        m_curFrameId = (m_curFrameId + 1) % num_images_;
        emit playFrame(m_curFrameId);
        this->msleep(delay);
    }
}

GVideoPlayer::~GVideoPlayer()
{
    mutex.lock();
    stop = true;
    capture.release();
    condition.wakeOne();
    mutex.unlock();
    wait();
}

void GVideoPlayer::initialize(size_t curFrameId, size_t nTotalFrames, size_t frameRate)
{
    m_curFrameId = curFrameId;
    num_images_ = nTotalFrames;
    m_frameRate = frameRate;
}

void GVideoPlayer::Stop()
{
    qDebug() << "Player Stop.";
    stop = true;
}

//void GVideoPlayer::msleep(int ms){
    //struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
    //struct timespec ts;
    //ts.tv_sec = 1;
    //nanosleep(&ts, NULL);
    //nanosleep((struct timespec[]){{0, 500000000}}, NULL);
    //QTest::qSleep(ms);
//}

bool GVideoPlayer::isStopped() const{
    return this->stop;
}
