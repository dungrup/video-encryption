#include "compml_video.hpp"

#define DEBUG 0
#define IMAGE_SAVE 0

class Codec
{

private:
  int ret;

public:
  const AVCodec *codec;
  AVCodecContext *context;
  AVCodecParserContext *parser;

  Codec(bool is_encoder, int bit_rate, int rc_buf)
  {
    if (is_encoder)
      codec = avcodec_find_encoder_by_name("libx264");
    else
    {
      codec = avcodec_find_decoder_by_name("h264");
      parser = av_parser_init(codec->id);
    }

    context = avcodec_alloc_context3(codec);

    if (is_encoder)
    {
      context->width = WAYMO_WIDTH;
      context->height = WAYMO_HEIGHT;
      context->time_base = (AVRational){1, 10};
      context->framerate = (AVRational){10, 1};
      context->gop_size = 10;
      context->max_b_frames = 1;
      context->pix_fmt = AV_PIX_FMT_YUV420P;

    }
    else {
      context->width = WAYMO_WIDTH;
      context->height = WAYMO_HEIGHT;
    }

    ret = avcodec_open2(context, codec, NULL);
  }

  ~Codec()
  {

    avcodec_free_context(&context);
  }
};

AVPacket *allocatePacket(AVPacket *packet)
{

  packet = av_packet_alloc();

  return packet;
}

AVFrame *allocateFrame(AVFrame *frame)
{

  int ret;
  frame = av_frame_alloc();
  frame->format = AV_PIX_FMT_YUV420P;
  frame->width = WAYMO_WIDTH;
  frame->height = WAYMO_HEIGHT;
  ret = av_frame_get_buffer(frame, 0);

  return frame;
}

AVFrame *allocateFrame2(AVFrame *frame) {

  int ret;
  frame = av_frame_alloc();
  frame->format = AV_PIX_FMT_YUV420P;
  frame->width = WAYMO_WIDTH;
  frame->height = WAYMO_HEIGHT;
  ret = av_frame_get_buffer(frame, 0);

  return frame;
}

void deallocateResources(AVPacket *packet, AVFrame *frame)
{
  av_frame_free(&frame);
  av_packet_free(&packet);
}

#ifdef IMAGE_SAVE
void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
              char *filename)
{
  FILE *f;
  int i;

  f = fopen(filename, "wb");
  fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
  for (i = 0; i < ysize; i++)
    fwrite(buf + i * wrap, 1, xsize, f);
  fclose(f);
}
#endif

/* Implementation to save in lossless RGB */
void rgb_save(AVFrame* frame, char* filename) {

  FILE* file;
  struct SwsContext* sws_ctx;
  AVFrame* frame_rgb = av_frame_alloc();

  frame_rgb->width = frame->width;
  frame_rgb->height = frame->height;
  frame_rgb->format = AV_PIX_FMT_RGB24;

  av_frame_get_buffer(frame_rgb, 0);

  sws_ctx = sws_getContext(WAYMO_WIDTH, WAYMO_HEIGHT, AV_PIX_FMT_YUV444P, WAYMO_WIDTH, WAYMO_HEIGHT,
              AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

  sws_scale(sws_ctx, frame->data, frame->linesize, 0, WAYMO_HEIGHT, frame_rgb->data, frame_rgb->linesize);

  file = fopen(filename, "wb");
  fwrite(frame_rgb->data[0], 1, WAYMO_HEIGHT*WAYMO_WIDTH*3, file);
  fclose(file);

}

void saveVideo(AVPacket *pkt, FILE *outfile)
{
  fwrite(pkt->data, 1, pkt->size, outfile);
}

