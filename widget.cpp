#include "widget.h"
#include <thread>
#include <unistd.h>
#include <QPainter>
#include <iostream>

using namespace std;

#ifndef MIN
#  define MIN(a,b)  ((a) > (b) ? (b) : (a))
#endif

#ifndef MAX
#  define MAX(a,b)  ((a) < (b) ? (b) : (a))
#endif

float InterRectArea(const QRect & a, const QRect & b) {
    QPoint left_top = QPoint(MAX(a.x(), b.x()), MAX(a.y(), b.y()));
    QPoint right_bottom = QPoint(MIN(a.right(), b.right()), MIN(a.bottom(), b.bottom()));
    QPoint diff = right_bottom - left_top;
    return (MAX(diff.x() + 1, 0) * MAX(diff.y() + 1, 0));
}

int ComputeIOU(const QRect & rect1,
    const QRect & rect2, float * iou,
    const std::string& type) {

    float inter_area = InterRectArea(rect1, rect2);
    if (type == "UNION") {
        *iou = inter_area / (rect1.width()*rect1.height() + rect2.width()*rect2.height() - inter_area);
    }
    else {
        *iou = inter_area / MIN(rect1.width()*rect1.height(), rect2.width()*rect2.height());
    }

    return 0;
}

template <typename T>
int const NMS(const std::vector<T>& inputs, std::vector<T>* result,
    const float& threshold, const std::string& type = "UNION") {
    result->clear();
    if (inputs.size() == 0)
        return -1;

    std::vector<T> inputs_tmp;
    inputs_tmp.assign(inputs.begin(), inputs.end());
    std::sort(inputs_tmp.begin(), inputs_tmp.end(),
    [](const T& a, const T& b) {
        return a.score_ < b.score_;
    });

    std::vector<int> indexes(inputs_tmp.size());

    for (int i = 0; i < indexes.size(); i++) {
        indexes[i] = i;
    }

    while (indexes.size() > 0) {
        int good_idx = indexes[0];
        result->push_back(inputs_tmp[good_idx]);
        std::vector<int> tmp_indexes = indexes;
        indexes.clear();
        for (int i = 1; i < tmp_indexes.size(); i++) {
            int tmp_i = tmp_indexes[i];
            float iou = 0.0f;
            ComputeIOU(inputs_tmp[good_idx].location_, inputs_tmp[tmp_i].location_, &iou, type);
            if (iou <= threshold) {
                indexes.push_back(tmp_i);
            }
        }
    }
}

Widget::Widget(QWidget *parent)
    : QWidget(parent),
      img(640, 360, QImage::Format_RGB888)
{
    setGeometry(200, 200, 640, 480);
    pnet_ = new ncnn::Net();
    rnet_ = new ncnn::Net();
    onet_ = new ncnn::Net();


    pnet_size_ = 12;
    min_face_size_ = 40;
    scale_factor_ = 0.709f;
    initialized_ = false;







    string root_path = "/home/lmc/Desktop/untitled/models";
    std::string pnet_param = std::string(root_path) + "/pnet.param";
    std::string pnet_bin = std::string(root_path) + "/pnet.bin";
    if (pnet_->load_param(pnet_param.c_str()) == -1 ||
        pnet_->load_model(pnet_bin.c_str()) == -1) {
        std::cout << "Load pnet model failed." << std::endl;
        std::cout << "pnet param: " << pnet_param << std::endl;
        std::cout << "pnet bin: " << pnet_bin << std::endl;
    }
    std::string rnet_param = std::string(root_path) + "/rnet.param";
    std::string rnet_bin = std::string(root_path) + "/rnet.bin";
    if (rnet_->load_param(rnet_param.c_str()) == -1 ||
        rnet_->load_model(rnet_bin.c_str()) == -1) {
        std::cout << "Load rnet model failed." << std::endl;
        std::cout << "rnet param: " << rnet_param << std::endl;
        std::cout << "rnet bin: " << rnet_bin << std::endl;
    }
    std::string onet_param = std::string(root_path) + "/onet.param";
    std::string onet_bin = std::string(root_path) + "/onet.bin";
    if (onet_->load_param(onet_param.c_str()) == -1 ||
        onet_->load_model(onet_bin.c_str()) == -1) {
        std::cout << "Load onet model failed." << std::endl;
        std::cout << "onet param: " << onet_param << std::endl;
        std::cout << "onet bin: " << onet_bin << std::endl;
    }













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






                    memcpy(this->img.bits(), this->pFrameYUV->data[0], 640*360*3);
                    ncnn::Mat img_in = ncnn::Mat::from_pixels(this->pFrameYUV->data[0],
                        ncnn::Mat::PIXEL_RGB, 640, 360);

                    QSize max_size = {640, 360};

                    std::vector<FaceInfo> faces;

                    img_in.substract_mean_normalize(this->meanVals, this->normVals);
                printf("11111111111111111\n");
                    std::vector<FaceInfo> first_bboxes, second_bboxes;
                    std::vector<FaceInfo> first_bboxes_result;
                    this->PDetect(img_in, &first_bboxes);
                    NMS(first_bboxes, &first_bboxes_result, this->nms_threshold_[0]);

                    this->Refine(&first_bboxes_result, max_size);

                    this->RDetect(img_in, first_bboxes_result, &second_bboxes);

                    std::vector<FaceInfo> second_bboxes_result;
                    NMS(second_bboxes, &second_bboxes_result, this->nms_threshold_[1]);

                    this->Refine(&second_bboxes_result, max_size);

                    std::vector<FaceInfo> third_bboxes;
                    this->ODetect(img_in, second_bboxes_result, &third_bboxes);

                    NMS(third_bboxes, &faces, this->nms_threshold_[2], "MIN");

                    this->Refine(&faces, max_size);
                    printf("222222222222222222\n");
                    QPainter pp(&img);

                    for (auto &tmp : faces)
                    {
                        pp.drawRect(tmp.location_);
                    }


                    memcpy(this->pFrameYUV->data[0], img.bits(), 640*360*3);





















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
                  //  usleep(3000);

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












  //  memset(pi.bits(), a, 640*480*3);
    p.drawImage(0, 0, img);
}




int Widget::PDetect(const ncnn::Mat & img_in,
    std::vector<FaceInfo>* first_bboxes) {
    first_bboxes->clear();
    int width = img_in.w;
    int height = img_in.h;
    float min_side = MIN(width, height);
    float curr_scale = float(pnet_size_) / min_face_size_;
    min_side *= curr_scale;
    std::vector<float> scales;
    while (min_side > pnet_size_) {
        scales.push_back(curr_scale);
        min_side *= scale_factor_;
        curr_scale *= scale_factor_;
    }

    // mutiscale resize the image
    for (int i = 0; i < static_cast<size_t>(scales.size()); ++i) {
        int w = static_cast<int>(width * scales[i]);
        int h = static_cast<int>(height * scales[i]);
        ncnn::Mat img_resized;
        ncnn::resize_bilinear(img_in, img_resized, w, h);
        ncnn::Extractor ex = pnet_->create_extractor();
        //ex.set_num_threads(2);
        ex.set_light_mode(true);
        ex.input("data", img_resized);
        ncnn::Mat score_mat, location_mat;
        ex.extract("prob1", score_mat);
        ex.extract("conv4-2", location_mat);
        const int stride = 2;
        const int cell_size = 12;
        for (int h = 0; h < score_mat.h; ++h) {
            for (int w = 0; w < score_mat.w; ++w) {
                int index = h * score_mat.w + w;
                // pnet output: 1x1x2  no-face && face
                // face score: channel(1)
                float score = score_mat.channel(1)[index];
                if (score < threshold_[0]) continue;

                // 1. generated bounding box
                int x1 = round((stride * w + 1) / scales[i]);
                int y1 = round((stride * h + 1) / scales[i]);
                int x2 = round((stride * w + 1 + cell_size) / scales[i]);
                int y2 = round((stride * h + 1 + cell_size) / scales[i]);

                // 2. regression bounding box
                float x1_reg = location_mat.channel(0)[index];
                float y1_reg = location_mat.channel(1)[index];
                float x2_reg = location_mat.channel(2)[index];
                float y2_reg = location_mat.channel(3)[index];

                int bbox_width = x2 - x1 + 1;
                int bbox_height = y2 - y1 + 1;

                FaceInfo face_info;
                face_info.score_ = score;
                face_info.location_.setLeft(x1 + x1_reg * bbox_width);
                face_info.location_.setTop(y1 + y1_reg * bbox_height);
                face_info.location_.setRight(x2 + x2_reg * bbox_width);
                face_info.location_.setBottom(y2 + y2_reg * bbox_height);
\
          //      face_info.location_ = face_info.location_ & cv::Rect(0, 0, width, height);
                first_bboxes->push_back(face_info);
            }
        }
    }
    return 0;
}

int Widget::RDetect(const ncnn::Mat & img_in,
    const std::vector<FaceInfo>& first_bboxes,
    std::vector<FaceInfo>* second_bboxes) {

    second_bboxes->clear();
    for (int i = 0; i < static_cast<int>(first_bboxes.size()); ++i) {

        QRect face = first_bboxes.at(i).location_ & QRect(0, 0, img_in.w, img_in.h);
        ncnn::Mat img_face, img_resized;
        ncnn::copy_cut_border(img_in, img_face, face.y(), img_in.h - face.bottom(), face.x(), img_in.w - face.right());

        ncnn::resize_bilinear(img_face, img_resized, 24, 24);

        ncnn::Extractor ex = rnet_->create_extractor();
        ex.set_light_mode(true);
        ex.set_num_threads(2);
        ex.input("data", img_resized);
        ncnn::Mat score_mat, location_mat;
        ex.extract("prob1", score_mat);
        ex.extract("conv5-2", location_mat);
        float score = score_mat[1];
        if (score < threshold_[1]) continue;
        float x_reg = location_mat[0];
        float y_reg = location_mat[1];
        float w_reg = location_mat[2];
        float h_reg = location_mat[3];

        FaceInfo face_info;
        face_info.score_ = score;
        face_info.location_.setLeft(face.x()+ + x_reg * face.width());
        face_info.location_.setTop(face.y() + y_reg * face.height());
        face_info.location_.setWidth(face.x() + face.width() +
                                     w_reg * face.width() - face_info.location_.x());

        face_info.location_.setHeight(face.y() + face.height() +
                                      h_reg * face.height() - face_info.location_.y());

        second_bboxes->push_back(face_info);
    }
    return 0;
}

int Widget::ODetect(const ncnn::Mat & img_in,
    const std::vector<FaceInfo>& second_bboxes,
    std::vector<FaceInfo>* third_bboxes) {
    third_bboxes->clear();
    for (int i = 0; i < static_cast<int>(second_bboxes.size()); ++i) {
        QRect face = second_bboxes.at(i).location_ & QRect(0, 0, img_in.w, img_in.h);
        ncnn::Mat img_face, img_resized;
        ncnn::copy_cut_border(img_in, img_face, face.y(), img_in.h - face.bottom(), face.x(), img_in.w - face.right());
        ncnn::resize_bilinear(img_face, img_resized, 48, 48);

        ncnn::Extractor ex = onet_->create_extractor();
        ex.set_light_mode(true);
        ex.set_num_threads(2);
        ex.input("data", img_resized);
        ncnn::Mat score_mat, location_mat, keypoints_mat;
        ex.extract("prob1", score_mat);
        ex.extract("conv6-2", location_mat);
        ex.extract("conv6-3", keypoints_mat);
        float score = score_mat[1];
        if (score < threshold_[1]) continue;
        float x_reg = location_mat[0];
        float y_reg = location_mat[1];
        float w_reg = location_mat[2];
        float h_reg = location_mat[3];

        FaceInfo face_info;
        face_info.score_ = score;
        face_info.location_.setLeft(face.x() + x_reg * face.width());
        face_info.location_.setTop(face.y() + y_reg * face.height());
        face_info.location_.setWidth(face.x() + face.width() +
                                     w_reg * face.width() - face_info.location_.x());
        face_info.location_.setHeight(face.y() + face.height() +
                                      h_reg * face.height() - face_info.location_.y());



        for (int num = 0; num < 5; num++) {
            face_info.keypoints_[num] = face.x() + face.width() * keypoints_mat[num];
            face_info.keypoints_[num + 5] = face.y() + face.height() * keypoints_mat[num + 5];
        }

        third_bboxes->push_back(face_info);
    }
    return 0;
}

int Widget::Refine(std::vector<FaceInfo>* bboxes, const QSize max_size) {
    int num_boxes = static_cast<int>(bboxes->size());
    for (int i = 0; i < num_boxes; ++i) {
        FaceInfo face_info = bboxes->at(i);
        int width = face_info.location_.width();
        int height = face_info.location_.height();
        float max_side = MAX(width, height);
        face_info.location_.setLeft(face_info.location_.x() + 0.5 * width - 0.5 * max_side);
        face_info.location_.setTop(face_info.location_.y() + 0.5 * height - 0.5 * max_side);
        face_info.location_.setWidth(max_side);
        face_info.location_.setHeight(max_side);


        face_info.location_ = face_info.location_ & QRect(0, 0, max_size.width(), max_size.height());
        bboxes->at(i) = face_info;
    }

    return 0;
}


