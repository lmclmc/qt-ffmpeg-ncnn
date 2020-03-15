#include "widget.h"
#include <thread>
#include <unistd.h>
#include <QPainter>
#include <iostream>

using namespace std;

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    setGeometry(200, 200, 640, 480);
    connect(this, SIGNAL(touchFrame()), this, SLOT(frameSlot()));
    av_register_all();
    QString filename = "/home/lmc/Desktop/test.mp4";
    //1、打开视频文件*************************************************
    pFormatCtx = avformat_alloc_context();

    err_code = avformat_open_input(&pFormatCtx, filename.toStdString().c_str(), NULL, NULL);
    if (err_code != 0)
    {//打开文件失败
        av_strerror(err_code, buf, 1024);
        printf("coundn't open the file!,error code = %d(%s)\n", err_code, buf);
    }
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        printf("Couldn't find stream information.\n");
    }
    //2、找到第一个视频流****************************

    videoStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;//得到视频流的索引
            break;
        }
    }
    if (videoStream == -1)
    {
        printf("Didn't find a video stream.\n");
    }

    /* 3、从视频流中得到一个编解码上下文,里面包含了编解码器的所有信息和一个
    指向真正的编解码器     ,然后我们找到这个解码器*/
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL)
    {
        fprintf(stderr, "Unsupported codec !\n");
    }
    //4、打开该编解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
    {
        printf("cann't open the codec!\n");
    }
    //5、分配两个视频帧，一个保存得到的原始视频帧，一个保存为指定格式的视频帧（该帧通过原始帧转换得来）
    pFrame = av_frame_alloc();

    if (pFrame == NULL)
    {
        printf("pFrame alloc fail!\n");
    }
    pFrameYUV = av_frame_alloc();
    if (pFrameYUV == NULL)
    {
        printf("pFrameYUV alloc fail!\n");
    }

    //6、得到一帧视频截图的内存大小并分配内存，并将YUV数据填充进去
    numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
            pCodecCtx->height,1);
    buffer = (uint8_t*) av_mallocz(numBytes * sizeof(uint8_t));
    if (!buffer)
    {
        printf("numBytes :%d , buffer malloc 's mem \n", numBytes);
    }
    //打印信息
    printf("--------------- File Information ----------------\n");
    av_dump_format(pFormatCtx, 0, filename.toStdString().c_str(), 0);
    printf("-------------------------------------------------\n");
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize,buffer,
            AV_PIX_FMT_RGB24,pCodecCtx->width, pCodecCtx->height,1);



    //7、得到指定转换格式的上下文**********************************
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
            pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
            AV_PIX_FMT_RGB24,
            SWS_BICUBIC,
            NULL, NULL, NULL);
    printf("%d, %d\n", pCodecCtx->width, pCodecCtx->height);
    if (img_convert_ctx == NULL)
    {
        fprintf(stderr, "Cannot initialize the conversion context!\n");
    }














    img_convert_rgb_ctx = sws_getContext(
            640, 360, AV_PIX_FMT_RGB24,
            640, 360, AV_PIX_FMT_YUV420P,
            SWS_BICUBIC,
            NULL, NULL, NULL
            );
    mYUVFrm = av_frame_alloc();
        mYUVFrm->format = AV_PIX_FMT_YUV420P;
        mYUVFrm->width = 640;
        mYUVFrm->height = 360;
        //alloc frame buffer
        int ret = av_frame_get_buffer(mYUVFrm,32);

     mEncoder = avcodec_find_encoder(AV_CODEC_ID_H264);
     if (!mEncoder)
         {
             cout << " avcodec_find_encoder AV_CODEC_ID_H264 failed!" << endl;
         }
         //get encoder contex
         mEncCtx = avcodec_alloc_context3(mEncoder);
         if (!mEncCtx)
         {
             cout << " avcodec_alloc_context3 for encoder contx failed!" << endl;
         }
         //set encoder params
         //bit rate
         mEncCtx->bit_rate = 400000000;

         mEncCtx->width = 640;
         mEncCtx->height = 360;
         mEncCtx->time_base = { 1,25 };

         //set gop size, in another way I frame gap
         mEncCtx->gop_size = 50;

         mEncCtx->max_b_frames = 0;
         mEncCtx->pix_fmt = AV_PIX_FMT_YUV420P;
         mEncCtx->codec_id = AV_CODEC_ID_H264;
         mEncCtx->thread_count = 4;
         mEncCtx->qmin = 10;
         mEncCtx->qmax = 51;
         mEncCtx->qcompress  = 0.6;

         av_opt_set(mEncCtx->priv_data, "tune", "zerolatency", 0);

             //global header info
             mEncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

             //open encoder
              ret = avcodec_open2(mEncCtx,mEncoder,NULL);
             if (ret < 0)
             {
                 cout << " avcodec_open2 encoder failed!" << endl;
             }
             cout << "avcodec_open2 encoder success!" << endl;

             //2 create out context
             avformat_alloc_output_context2(&mFmtCtx, 0, 0, "/home/lmc/Desktop/lmc.mp4");

             //3 add video stream
             mOutStm = avformat_new_stream(mFmtCtx,NULL);
             mOutStm->id = 0;
             mOutStm->codecpar->codec_tag = 0;
             avcodec_parameters_from_context(mOutStm->codecpar,mEncCtx);

             cout << "===============================================" << endl;
             av_dump_format(mFmtCtx, 0, "/home/lmc/Desktop/lmc.mp4", 1);
             cout << "===============================================" << endl;

             //alloc output yuv frame
             mYUVFrm = av_frame_alloc();
             mYUVFrm->format = AV_PIX_FMT_YUV420P;
             mYUVFrm->width = 640;
             mYUVFrm->height = 360;
             //alloc frame buffer
             ret = av_frame_get_buffer(mYUVFrm,32);

             if (ret < 0)
             {
                 av_frame_free(&mYUVFrm);
                 mYUVFrm = NULL;
                 cout << " av_frame_get_buffer  failed!" << endl;
             }

             //5 write mp4 head
             ret = avio_open(&mFmtCtx->pb, "/home/lmc/Desktop/lmc.mp4"
                             ,AVIO_FLAG_WRITE);
             if (ret < 0)
             {
                 cout << " avio_open  failed!" << endl;
             }
             ret = avformat_write_header(mFmtCtx, NULL);
             if (ret < 0)
             {
                 cout << " avformat_write_header  failed!" << endl;
             }












    t = std::thread([=]{
        while (av_read_frame(this->pFormatCtx, &this->packet) >= 0)
        {
            //如果读取的包来自视频流
            if (this->packet.stream_index == this->videoStream)
            {
                //从包中得到解码后的帧
                if (avcodec_decode_video2(this->pCodecCtx, this->pFrame, &this->frameFinished,&this->packet) < 0)
                {
                    printf("Decode Error!\n");
                }
                //如果确定完成得到该视频帧
                if (this->frameFinished)
                {
                    //转换帧数据格式
                    sws_scale(this->img_convert_ctx, this->pFrame->data, this->pFrame->linesize, 0,
                            this->pCodecCtx->height,
                            this->pFrameYUV->data,
                            this->pFrameYUV->linesize);




                    static int64_t mPTS = 0;
                    sws_scale(this->img_convert_rgb_ctx, this->pFrameYUV->data, this->pFrameYUV->linesize, 0, 360,
                                this->mYUVFrm->data, this->mYUVFrm->linesize
                                );
                    mYUVFrm->pts = mPTS;
                        mPTS = mPTS + 4000;
                        //mPTS = mPTS + 1000000;
                        //send to encoder
                        int ret = avcodec_send_frame(mEncCtx, mYUVFrm);
                        if (ret != 0)
                        {
                            char err[64] = {0};
                            av_strerror(ret, err, 64);
                            printf("encoder send frame %d err:%s\n",mPTS-4000, err);
                            return -1;
                        }
                        AVPacket pkt;
                        av_init_packet(&pkt);
                        //receive from encoder
                        //note that frame will not be received immediately
                        ret = avcodec_receive_packet(mEncCtx,&pkt);
                        if (ret != 0)
                        {
                            printf("encoder recieve frame %d err\n",mPTS-4000);
                            return -1;
                        }
                        //write encoded frame to file
                        av_interleaved_write_frame(mFmtCtx,&pkt);
                        av_free_packet(&pkt);








                    this->touchFrame();
                    usleep(3000);

                }
            }
            av_free_packet(&this->packet);//释放读出来的包
        }
        printf("qwefqweffffffffffffffffffffffffff\n");
        //**************************************************************************************
//        //10、释放分配的内存或关闭文件等操作
//    #if OUTPUT_YUV420P
//        fclose(fp_yuv);
//    #endif
        sws_freeContext(this->img_convert_ctx);
        av_free(this->buffer);
        av_free(this->pFrame);
        av_free(this->pFrameYUV);
        avcodec_close(this->pCodecCtx);
        avformat_close_input(&this->pFormatCtx);














        av_write_trailer(this->mFmtCtx);

           //关闭视频输出io
           avio_close(this->mFmtCtx->pb);

           //清理封装输出上下文
           avformat_free_context(this->mFmtCtx);

           //关闭编码器
           avcodec_close(this->mEncCtx);

           //清理编码器上下文
           avcodec_free_context(&this->mEncCtx);

           //清理视频重采样上下文
           sws_freeContext(this->img_convert_rgb_ctx);
    });
}

Widget::~Widget()
{

}

void Widget::frameSlot()
{
    update();
}

void Widget::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    QImage pi(640, 360, QImage::Format_RGB888);

    memcpy(pi.bits(), pFrameYUV->data[0], 640*360*3);
  //  memset(pi.bits(), a, 640*480*3);
    p.drawImage(0, 0, pi);
}
