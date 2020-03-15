#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <thread>
#include "ncnn/net.h"
extern "C"
{
#include<stdio.h>
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
#include<libavutil/avutil.h>
#include<libswscale/swscale.h>
#include<libavutil/avutil.h>
#include<libavutil/imgutils.h>
#include "libavutil/opt.h"
}

struct FaceInfo {
    QRect location_;
    float score_;
    float keypoints_[10];
};

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = 0);
    ~Widget();

signals:
    void touchFrame();

public slots:
    void frameSlot();

protected:
    void paintEvent(QPaintEvent*);

private:
    ncnn::Net* pnet_ = nullptr;
    ncnn::Net* rnet_ = nullptr;
    ncnn::Net* onet_ = nullptr;








    //变量定义*********************************************************************
    AVFormatContext *pFormatCtx;
    int i, videoStream;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame;
    AVFrame *pFrameYUV;
    uint8_t *buffer;
    int numBytes;

    int frameFinished;
    AVPacket packet;

    struct SwsContext *img_convert_ctx;
    int err_code;
    char buf[1024];
    FILE *fp_yuv;
    int y_size;
    std::thread t;




    int pnet_size_;
    int min_face_size_;
    float scale_factor_;
    bool initialized_;
    const float meanVals[3] = { 127.5f, 127.5f, 127.5f };
    const float normVals[3] = { 0.0078125f, 0.0078125f, 0.0078125f };
    const float nms_threshold_[3] = { 0.5f, 0.7f, 0.7f };
    const float threshold_[3] = { 0.8f, 0.8f, 0.6f };




    //output
    struct SwsContext *img_convert_rgb_ctx;

    AVCodec *mEncoder;
        AVCodecContext *mEncCtx;
        AVFormatContext *mFmtCtx;
        AVStream *mOutStm;
        AVFrame *mYUVFrm;

      QImage img;


private:
    int PDetect(const ncnn::Mat& img_in, std::vector<FaceInfo>* first_bboxes);
    int RDetect(const ncnn::Mat& img_in, const std::vector<FaceInfo>& first_bboxes,
        std::vector<FaceInfo>* second_bboxes);
    int ODetect(const ncnn::Mat& img_in,
        const std::vector<FaceInfo>& second_bboxes,
        std::vector<FaceInfo>* third_bboxes);

    int Refine(std::vector<FaceInfo>* bboxes, const QSize max_size);
};

#endif // WIDGET_H
