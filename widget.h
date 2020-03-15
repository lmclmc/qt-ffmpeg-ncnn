#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <thread>

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









    //output
    struct SwsContext *img_convert_rgb_ctx;

    AVCodec *mEncoder;
        AVCodecContext *mEncCtx;
        AVFormatContext *mFmtCtx;
        AVStream *mOutStm;
        AVFrame *mYUVFrm;
};

#endif // WIDGET_H
