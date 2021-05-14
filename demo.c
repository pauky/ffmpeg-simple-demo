#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libswresample/swresample.h>
// #include "video_encoder.h"

AVFormatContext *m_pAvFormatCtx = NULL; // 流文件解析上下文
AVCodecContext *m_pVidDecodeCtx = NULL; // 视频解码器上下文
uint32_t m_nVidStreamIndex = -1;        // 视频流索引值
AVCodecContext *m_pAudDecodeCtx = NULL; // 音频解码器上下文
uint32_t m_nAudStreamIndex = -1;        // 音频流索引值

int32_t Open(const char *pszFilePath);
int32_t Open(const char *pszFilePath);
int32_t ReadFrame();
int32_t ReadFrame();
int32_t ReadFrame();
int32_t DecodePktToFrame(
    AVCodecContext *pDecoderCtx, // 解码器上下文信息
    AVPacket *pInPacket,         // 输入的数据包
    AVFrame **ppOutFrame);       // 解码后生成的视频帧或者音频帧
void Close();
int32_t VideoConvert(
    const AVFrame *pInFrame,       // 输入视频帧
    enum AVPixelFormat eOutFormat, // 输出视频格式
    int32_t nOutWidth,             // 输出视频宽度
    int32_t nOutHeight,            // 输出视频高度
    AVFrame **ppOutFrame);         // 输出视频帧

int32_t AudioConvert(
    const AVFrame *pInFrame,         // 输入音频帧
    enum AVSampleFormat eOutSmplFmt, // 输出音频格式
    int32_t nOutChannels,            // 输出音频通道数
    int32_t nOutSmplRate,            // 输出音频采样率
    AVFrame **ppOutFrame);           // 输出视频帧

int32_t VidEncoderOpen(
    enum AVPixelFormat ePxlFormat, // 输入图像像素格式, 通常是 AV_PIX_FMT_YUV420P
    int32_t nFrameWidth,           // 输入图像宽度
    int32_t nFrameHeight,          // 输入图像高度
    int32_t nFrameRate,            // 编码的帧率
    float nBitRateFactor);         // 码率因子,通常可以设置为 0.8~8

void VidEncoderClose();

int32_t VidEncoderEncPacket(AVFrame *pInFrame, AVPacket **ppOutPacket);

//
// 打开媒体文件,解析码流信息,并且创建和打开对应的解码器
//
int32_t Open(const char *pszFilePath)
{
  AVCodec *pVidDecoder = NULL;
  AVCodec *pAudDecoder = NULL;
  int res = 0;

  // 打开媒体文件
  res = avformat_open_input(&m_pAvFormatCtx, pszFilePath, NULL, NULL);
  if (m_pAvFormatCtx == NULL)
  {
    printf("<Open> [ERROR] fail avformat_open_input(), res=%d\n", res);
    return res;
  }

  // 查找所有媒体流信息
  res = avformat_find_stream_info(m_pAvFormatCtx, NULL);
  if (res == AVERROR_EOF)
  {
    printf("<Open> reached to file end\n");
    Close();
    return -1;
  }

  // 遍历所有的媒体流信息
  for (unsigned int i = 0; i < m_pAvFormatCtx->nb_streams; i++)
  {
    AVStream *pAvStream = m_pAvFormatCtx->streams[i];
    if (pAvStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      if ((pAvStream->codecpar->width <= 0) || (pAvStream->codecpar->height <= 0))
      {
        printf("<Open> [ERROR] invalid resolution, streamIndex=%d\n", i);
        continue;
      }

      pVidDecoder = avcodec_find_decoder(pAvStream->codecpar->codec_id); // 找到视频解码器
      if (pVidDecoder == NULL)
      {
        printf("<Open> [ERROR] can not find video codec\n");
        continue;
      }

      m_nVidStreamIndex = (uint32_t)i;
      printf("<Open> pxlFmt=%d, frameSize=%d*%d\n",
             (int)pAvStream->codecpar->format,
             pAvStream->codecpar->width,
             pAvStream->codecpar->height);
    }
    else if (pAvStream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      if ((pAvStream->codecpar->channels <= 0) || (pAvStream->codecpar->sample_rate <= 0))
      {
        printf("<Open> [ERROR] invalid resolution, streamIndex=%d\n", i);
        continue;
      }

      pAudDecoder = avcodec_find_decoder(pAvStream->codecpar->codec_id); // 找到音频解码器
      if (pAudDecoder == NULL)
      {
        printf("<Open> [ERROR] can not find Audio codec\n");
        continue;
      }

      m_nAudStreamIndex = (uint32_t)i;
      printf("<Open> sample_fmt=%d, sampleRate=%d, channels=%d, chnl_layout=%d\n",
             (int)pAvStream->codecpar->format,
             pAvStream->codecpar->sample_rate,
             pAvStream->codecpar->channels,
             pAvStream->codecpar->channel_layout);
    }
  }
  if (pVidDecoder == NULL && pAudDecoder == NULL)
  {
    printf("<Open> [ERROR] can not find video or audio stream\n");
    Close();
    return -1;
  }
  // // seek到第0ms开始读取
  // int64_t nSeekTime = 10;
  // res = avformat_seek_file(m_pAvFormatCtx, -1, INT64_MIN, nSeekTime, INT64_MAX, 0);

  // 创建视频解码器并且打开
  if (pVidDecoder != NULL)
  {
    m_pVidDecodeCtx = avcodec_alloc_context3(pVidDecoder);
    if (m_pVidDecodeCtx == NULL)
    {
      printf("<Open> [ERROR] fail to video avcodec_alloc_context3()\n");
      Close();
      return -1;
    }
    res = avcodec_parameters_to_context(m_pVidDecodeCtx, m_pAvFormatCtx->streams[m_nVidStreamIndex]->codecpar);

    res = avcodec_open2(m_pVidDecodeCtx, NULL, NULL);
    if (res != 0)
    {
      printf("<Open> [ERROR] fail to video avcodec_open2(), res=%d\n", res);
      Close();
      return -1;
    }
  }

  // 创建音频解码器并且打开
  if (pAudDecoder != NULL)
  {
    m_pAudDecodeCtx = avcodec_alloc_context3(pAudDecoder);
    if (m_pAudDecodeCtx == NULL)
    {
      printf("<Open> [ERROR] fail to audio avcodec_alloc_context3()\n");
      Close();
      return -1;
    }
    res = avcodec_parameters_to_context(m_pAudDecodeCtx, m_pAvFormatCtx->streams[m_nAudStreamIndex]->codecpar);

    res = avcodec_open2(m_pAudDecodeCtx, NULL, NULL);
    if (res != 0)
    {
      printf("<Open> [ERROR] fail to audio avcodec_open2(), res=%d\n", res);
      Close();
      return -1;
    }
  }

  return 0;
}

//
// 循环不断的读取音视频数据包进行解码处理
//
int32_t ReadFrame()
{
  int res = 0;

  for (;;)
  {
    AVPacket *pPacket = av_packet_alloc();

    // 依次读取数据包
    res = av_read_frame(m_pAvFormatCtx, pPacket);
    if (res == AVERROR_EOF) // 正常读取到文件尾部退出
    {
      printf("<ReadFrame> reached media file end\n");
      break;
    }
    else if (res < 0) // 其他小于0的返回值是数据包读取错误
    {
      printf("<ReadFrame> fail av_read_frame(), res=%d\n", res);
      break;
    }

    if (pPacket->stream_index == m_nVidStreamIndex) // 读取到视频包
    {

      // 这里进行视频包解码操作,详细下一章节讲解
      AVFrame *pVideoFrame = NULL;

      res = DecodePktToFrame(m_pVidDecodeCtx, pPacket, &pVideoFrame);
      if (res == 0 && pVideoFrame != NULL)
      {
        // Enqueue(pVideoFrame); // 解码成功后的视频帧插入队列

        // ff_h264_decode.encode();
        // enum AVPixelFormat eOutFormat = AV_PIX_FMT_YUV420P;
        // AVFrame *pOutVideoFrame = NULL;
        // VideoConvert(pVideoFrame, eOutFormat, 120, 100, &pOutVideoFrame);

        // get_side_data(pVideoFrame);

        // AVPacket *pOutPacket = av_packet_alloc();
        // res = VidEncoderEncPacket(pVideoFrame, &pOutPacket);
        // if (res == 0) {
        //   printf("video frame height, height: %d, size: %d\n", pVideoFrame->height, pOutPacket->size);
        // }
      }
    }
    else if (pPacket->stream_index == m_nAudStreamIndex) // 读取到音频包
    {
      // 这里进行音频包解码操作,详细下一章节讲解
      AVFrame *pAudioFrame = NULL;
      res = DecodePktToFrame(m_pAudDecodeCtx, pPacket, &pAudioFrame);
      if (res == 0 && pAudioFrame != NULL)
      {
        // Enqueue(pAudioFrame); // 解码成功后的音频帧插入队列
        AVFrame *pOutAudioFrame = NULL;
        enum AVSampleFormat eOutFormat = AV_SAMPLE_FMT_S16;
        AudioConvert(pAudioFrame, eOutFormat, 2, 8000, &pOutAudioFrame);
        // printf("audio frame format, ori: %d, convert: %d\n", pAudioFrame->format, pOutAudioFrame->format);
      }
    }

    av_packet_free(&pPacket); // 数据包用完了可以释放了
  }
}

// support in ffmpeg-4.4
// int get_side_data(AVFrame *pVideoFrame)
// {
//   AVFrameSideData *sei_data = av_frame_get_side_data(pVideoFrame, AV_FRAME_DATA_SEI_UNREGISTERED);
//   if (sei_data == NULL)
//   {
//     return 0;
//   }
//   printf("sei_data->size: %d\n", sei_data->size);
//   uint8_t *my_sei_data = sei_data->data[16];
//   printf("my_sei_data: %s\n", my_sei_data);
// }

//
// 功能: 送入一个数据包进行解码,获取解码后的音视频帧
//       注意: 即使正常解码情况下,不是每次都有解码后的帧输出的
//
// 参数: pDecoderCtx ---- 解码器上下文信息
//       pInPacket   ---- 输入的数据包, 可以为NULL来刷新解码缓冲区
//       ppOutFrame  ---- 输出解码后的音视频帧, 即使返回0也可能无输出
//
// 返回: 0:           解码正常;
//       AVERROR_EOF: 解码全部完成;
//       other:       解码出错
//
int32_t DecodePktToFrame(
    AVCodecContext *pDecoderCtx, // 解码器上下文信息
    AVPacket *pInPacket,         // 输入的数据包
    AVFrame **ppOutFrame)        // 解码后生成的视频帧或者音频帧
{
  AVFrame *pOutFrame = NULL;
  int res = 0;

  // 送入要解码的数据包
  res = avcodec_send_packet(pDecoderCtx, pInPacket);
  if (res == AVERROR(EAGAIN)) // 没有数据送入,但是可以继续可以从内部缓冲区读取编码后的视频包
  {
    //    printf("<DecodePktToFrame> avcodec_send_frame() EAGAIN\n");
  }
  else if (res == AVERROR_EOF) // 数据包送入结束不再送入,但是可以继续可以从内部缓冲区读取编码后的视频包
  {
    //    printf("<DecodePktToFrame> avcodec_send_frame() AVERROR_EOF\n");
  }
  else if (res < 0) // 送入输入数据包失败
  {
    printf("<DecodePktToFrame> [ERROR] fail to avcodec_send_frame(), res=%d\n", res);
    return res;
  }

  // 获取解码后的视频帧或者音频帧
  pOutFrame = av_frame_alloc();
  res = avcodec_receive_frame(pDecoderCtx, pOutFrame);
  if (res == AVERROR(EAGAIN)) // 当前这次没有解码后的音视频帧输出,但是后续可以继续读取
  {
    printf("<CAvVideoDecComp::DecodeVideoPkt> no data output\n");
    av_frame_free(&pOutFrame);
    (*ppOutFrame) = NULL;
    return 0;
  }
  else if (res == AVERROR_EOF) // 解码缓冲区已经刷新完成,后续不再有数据输出
  {
    printf("<DecodePktToFrame> avcodec_receive_packet() EOF\n");
    av_frame_free(&pOutFrame);
    (*ppOutFrame) = NULL;
    return AVERROR_EOF;
  }
  else if (res < 0)
  {
    printf("<DecodePktToFrame> [ERROR] fail to avcodec_receive_packet(), res=%d\n", res);
    av_frame_free(&pOutFrame);
    (*ppOutFrame) = NULL;
    return res;
  }

  (*ppOutFrame) = pOutFrame;
  return 0;
}

//
// 关闭媒体文件，关闭对应的解码器
//
void Close()
{
  // 关闭媒体文件解析
  if (m_pAvFormatCtx != NULL)
  {
    avformat_close_input(&m_pAvFormatCtx);
    m_pAvFormatCtx = NULL;
  }

  // 关闭视频解码器
  if (m_pVidDecodeCtx != NULL)
  {
    avcodec_close(m_pVidDecodeCtx);
    avcodec_free_context(&m_pVidDecodeCtx);
    m_pVidDecodeCtx = NULL;
  }

  // 关闭音频解码器
  if (m_pAudDecodeCtx != NULL)
  {
    avcodec_close(m_pAudDecodeCtx);
    avcodec_free_context(&m_pAudDecodeCtx);
    m_pAudDecodeCtx = NULL;
  }
}

//
// 视频帧格式转换
//
int32_t VideoConvert(
    const AVFrame *pInFrame,       // 输入视频帧
    enum AVPixelFormat eOutFormat, // 输出视频格式
    int32_t nOutWidth,             // 输出视频宽度
    int32_t nOutHeight,            // 输出视频高度
    AVFrame **ppOutFrame)          // 输出视频帧
{
  struct SwsContext *pSwsCtx = NULL;
  AVFrame *pOutFrame = NULL;

  // 创建格式转换器, 指定缩放算法,转换过程中不增加任何滤镜特效处理
  pSwsCtx = sws_getContext(pInFrame->width, pInFrame->height, (enum AVPixelFormat)pInFrame->format,
                           nOutWidth, nOutHeight, eOutFormat,
                           SWS_BICUBIC, NULL, NULL, NULL);
  if (pSwsCtx == NULL)
  {
    printf("<VideoConvert> [ERROR] fail to sws_getContext()\n");
    return -1;
  }

  // 创建输出视频帧对象以及分配相应的缓冲区
  uint8_t *data[4] = {NULL};
  int linesize[4] = {0};
  int res = av_image_alloc(data, linesize, nOutWidth, nOutHeight, eOutFormat, 1);
  if (res < 0)
  {
    printf("<VideoConvert> [ERROR] fail to av_image_alloc(), res=%d\n", res);
    sws_freeContext(pSwsCtx);
    return -2;
  }
  pOutFrame = av_frame_alloc();
  pOutFrame->format = eOutFormat;
  pOutFrame->width = nOutWidth;
  pOutFrame->height = nOutHeight;
  pOutFrame->data[0] = data[0];
  pOutFrame->data[1] = data[1];
  pOutFrame->data[2] = data[2];
  pOutFrame->data[3] = data[3];
  pOutFrame->linesize[0] = linesize[0];
  pOutFrame->linesize[1] = linesize[1];
  pOutFrame->linesize[2] = linesize[2];
  pOutFrame->linesize[3] = linesize[3];

  // 进行格式转换处理
  res = sws_scale(pSwsCtx,
                  (const uint8_t *const *)(pInFrame->data),
                  pInFrame->linesize,
                  0,
                  pOutFrame->height,
                  pOutFrame->data,
                  pOutFrame->linesize);
  if (res < 0)
  {
    printf("<VideoConvert> [ERROR] fail to sws_scale(), res=%d\n", res);
    sws_freeContext(pSwsCtx);
    av_frame_free(&pOutFrame);
    return -3;
  }

  (*ppOutFrame) = pOutFrame;
  sws_freeContext(pSwsCtx); // 释放转换器
  return 0;
}

//
// 音频帧格式转换
//
int32_t AudioConvert(
    const AVFrame *pInFrame,         // 输入音频帧
    enum AVSampleFormat eOutSmplFmt, // 输出音频格式
    int32_t nOutChannels,            // 输出音频通道数
    int32_t nOutSmplRate,            // 输出音频采样率
    AVFrame **ppOutFrame)            // 输出视频帧
{
  struct SwrContext *pSwrCtx = NULL;
  AVFrame *pOutFrame = NULL;

  // 创建格式转换器,
  int64_t nInChnlLayout = av_get_default_channel_layout(pInFrame->channels);
  int64_t nOutChnlLayout = (nOutChannels == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;

  pSwrCtx = swr_alloc();
  if (pSwrCtx == NULL)
  {
    printf("<AudioConvert> [ERROR] fail to swr_alloc()\n");
    return -1;
  }
  swr_alloc_set_opts(pSwrCtx,
                     nOutChnlLayout, eOutSmplFmt, nOutSmplRate, nInChnlLayout,
                     (enum AVSampleFormat)(pInFrame->format), pInFrame->sample_rate,
                     0, NULL);

  swr_init(pSwrCtx); // 需要初始化，不然会报错：“Context has not been initialized”

  int isInit = swr_is_initialized(pSwrCtx);
  if (isInit <= 0)
  {

    printf("<AudioConvert> [ERROR] fail to init context\n");
    return -1;
  }

  // 计算重采样转换后的样本数量,从而分配缓冲区大小
  int64_t nCvtBufSamples = av_rescale_rnd(pInFrame->nb_samples, nOutSmplRate, pInFrame->sample_rate, AV_ROUND_UP);

  // 创建输出音频帧
  pOutFrame = av_frame_alloc();
  pOutFrame->format = eOutSmplFmt;
  pOutFrame->nb_samples = (int)nCvtBufSamples;
  pOutFrame->channel_layout = (uint64_t)nOutChnlLayout;
  int res = av_frame_get_buffer(pOutFrame, 0); // 分配缓冲区
  if (res < 0)
  {
    printf("<AudioConvert> [ERROR] fail to av_frame_get_buffer(), res=%d\n", res);
    swr_free(&pSwrCtx);
    av_frame_free(&pOutFrame);
    return -2;
  }

  // 进行重采样转换处理,返回转换后的样本数量
  int nCvtedSamples = swr_convert(pSwrCtx,
                                  (uint8_t **)(pOutFrame->data),
                                  (int)nCvtBufSamples,
                                  (const uint8_t **)(pInFrame->data),
                                  pInFrame->nb_samples);
  if (nCvtedSamples <= 0)
  {
    printf("<AudioConvert> [ERROR] no data for swr_convert()\n");
    swr_free(&pSwrCtx);
    av_frame_free(&pOutFrame);
    return -3;
  }
  pOutFrame->nb_samples = nCvtedSamples;
  pOutFrame->pts = pInFrame->pts; // pts等时间戳沿用
  // pOutFrame->pkt_pts    = pInFrame->pkt_pts;

  (*ppOutFrame) = pOutFrame;
  swr_free(&pSwrCtx); // 释放转换器
  return 0;
}

AVCodecContext *m_pVideoEncCtx = NULL; // 视频编码器上下文

/**
  * @brief 根据图像格式和信息来创建编码器
  * 
  */
int32_t VidEncoderOpen(
    enum AVPixelFormat ePxlFormat, // 输入图像像素格式, 通常是 AV_PIX_FMT_YUV420P
    int32_t nFrameWidth,           // 输入图像宽度
    int32_t nFrameHeight,          // 输入图像高度
    int32_t nFrameRate,            // 编码的帧率
    float nBitRateFactor)          // 码率因子,通常可以设置为 0.8~8
{
  AVCodec *pVideoEncoder = NULL;
  int res = 0;

  pVideoEncoder = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (pVideoEncoder == NULL)
  {
    printf("<VidEncoderOpen> [ERROR] fail to find AV_CODEC_ID_H264\n");
    return -1;
  }

  m_pVideoEncCtx = avcodec_alloc_context3(pVideoEncoder);
  if (m_pVideoEncCtx == NULL)
  {
    printf("<VidEncoderOpen> [ERROR] fail to find FF_avcodec_alloc_context3()\n");
    return -2;
  }

  int64_t nBitRate = (int64_t)((nFrameWidth * nFrameHeight * 3 / 2) * nBitRateFactor); // 计算码率
  m_pVideoEncCtx->codec_id = AV_CODEC_ID_H264;
  m_pVideoEncCtx->pix_fmt = ePxlFormat;
  m_pVideoEncCtx->width = nFrameWidth;
  m_pVideoEncCtx->height = nFrameHeight;
  m_pVideoEncCtx->bit_rate = nBitRate;
  // m_pVideoEncCtx->rc_buffer_size = static_cast<int>(nBitRate);
  m_pVideoEncCtx->framerate.num = nFrameRate; // 帧率
  m_pVideoEncCtx->framerate.den = 1;
  m_pVideoEncCtx->gop_size = nFrameRate; // 每秒1个关键帧
  m_pVideoEncCtx->time_base.num = 1;
  // printf("nFrameRate: %d\n", nFrameRate);
  // m_pVideoEncCtx->time_base.den  = nFrameRate * 1000; // 时间基
  m_pVideoEncCtx->time_base.den = 200; // 时间基
  m_pVideoEncCtx->has_b_frames = 0;
  m_pVideoEncCtx->max_b_frames = 0;
  m_pVideoEncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  res = avcodec_open2(m_pVideoEncCtx, pVideoEncoder, NULL);
  if (res < 0)
  {
    printf("<VidEncoderOpen> [ERROR] fail to find FF_avcodec_open2(), res=%d\n", res);
    avcodec_free_context(&m_pVideoEncCtx);
    return -3;
  }

  return 0;
}

/**
  * @brief 关闭编码器
  * 
  */
void VidEncoderClose()
{
  if (m_pVideoEncCtx != NULL)
  {
    avcodec_free_context(&m_pVideoEncCtx);
    m_pVideoEncCtx = NULL;
  }
}

/**
  * @brief 进行视频帧编码, 注意: 不是每次输入视频帧编码都会有输出数据包的
  * @param pInFrame    输入要进行编码的原始视频帧数据, 当为NULL时表示刷新编码器缓冲区中数据
  *        ppOutPacket 输出编码后的数据包, 即使正常编码无错误,也不是每次都会输出
  * @return 返回错误码， 0 表示有数据包输出;
  *                     EAGAIN 表示当前没有数据输出, 但是编码器正常
  *                     AVERROR_EOF 编码缓冲区刷新完,不再有数据包输出
  *                     other: 出错
  */
int32_t VidEncoderEncPacket(AVFrame *pInFrame, AVPacket **ppOutPacket)
{
  AVPacket *pOutPacket = NULL;
  int res = 0;

  if (m_pVideoEncCtx == NULL)
  {
    printf("<VidEncoderEncPacket> [ERROR] bad status\n");
    return -1;
  }

  // 送入要编码的视频帧
  res = avcodec_send_frame(m_pVideoEncCtx, pInFrame);
  if (res == AVERROR(EAGAIN)) // 没有数据送入,但是可以继续可以从内部缓冲区读取编码后的视频包
  {
    // LOGD("<CAvVideoEncoder::EncodeFrame> avcodec_send_frame() EAGAIN\n");
  }
  else if (res == AVERROR_EOF) // 编码器缓冲区已经刷新,但是可以继续可以从内部缓冲区读取编码后的视频包
  {
    //    LOGD("<CAvVideoEncoder::EncodeFrame> avcodec_send_frame() AVERROR_EOF\n");
  }
  else if (res < 0)
  {
    printf("<VidEncoderEncPacket> [ERROR] fail to avcodec_send_frame(), res=%d\n", res);
    return -2;
  }

  // 读取编码后的数据包
  pOutPacket = av_packet_alloc();
  res = avcodec_receive_packet(m_pVideoEncCtx, pOutPacket);
  if (res == AVERROR(EAGAIN)) // 当前这次没有数据输出,但是后续可以继续读取
  {
    av_packet_free(&pOutPacket);
    return EAGAIN;
  }
  else if (res == AVERROR_EOF) // 编码器缓冲区已经刷新完成,后续不再有数据输出
  {
    printf("<VidEncoderEncPacket> avcodec_receive_packet() EOF\n");
    av_packet_free(&pOutPacket);
    return AVERROR_EOF;
  }
  else if (res < 0)
  {
    printf("<VidEncoderEncPacket> [ERROR] fail to avcodec_receive_packet(), res=%d\n", res);
    av_packet_free(&pOutPacket);
    return res;
  }

  (*ppOutPacket) = pOutPacket;
  return 0;
}

int main(int argc, char *argv[])
{
  Open(argv[1]);
  VidEncoderOpen(AV_PIX_FMT_YUV420P, 120, 100, m_pVidDecodeCtx->framerate.num, 1);
  ReadFrame();
  Close();
  return 0;
}