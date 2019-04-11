#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "vpx/tools_common.h"
#include "vpx/video_common.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vpx_decoder.h"
#include "libyuv/libyuv.h"

int bgr2vpx(char* bmpData, int width, int height, unsigned char** vp9RetBuf, int *vp9BufSize){
  unsigned char *yuv;
  yuv = (unsigned char*)malloc(width*height+width*height/4*2);
  int uOffset = width*height, vOffset = width*height + (width*height)/4;
  FILE *yuvFile;

  bmpData = bmpData+sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER); //add header offset
  RGB24ToI420( bmpData, width*3, yuv, width,
  	yuv+uOffset, width/2, yuv+vOffset, width/2,
  	width, height);

  const VpxInterface *encoder = NULL;
  const int fps = 30;
  const int bitrate = 200;
  VpxVideoInfo info = { 0, 0, 0, { 0, 0 } };
  int keyframe_interval = 1;
  vpx_codec_err_t res;
  vpx_codec_enc_cfg_t cfg;
  vpx_codec_ctx_t codec;
  vpx_image_t raw;
  int frame_count = 0, max_frames = 0, frames_encoded = 0;

  encoder = get_vpx_encoder_by_name("vp9");
  info.codec_fourcc = encoder->fourcc;
  info.frame_width = width;
  info.frame_height = height;
  info.time_base.numerator = 1;
  info.time_base.denominator = fps;

  if (!vpx_img_alloc(&raw, VPX_IMG_FMT_I420, info.frame_width,
                     info.frame_height, 1)) {
    printf("Can't allocate memory.\n");
    exit(-1);
  }

  res = vpx_codec_enc_config_default(encoder->codec_interface(), &cfg, 0);
  if (res) printf("Failed to get default codec config.");

  cfg.g_w = info.frame_width;
  cfg.g_h = info.frame_height;
  cfg.g_timebase.num = info.time_base.numerator;
  cfg.g_timebase.den = info.time_base.denominator;
  cfg.rc_target_bitrate = bitrate;
  cfg.g_error_resilient = (vpx_codec_er_flags_t)1;

  if (vpx_codec_enc_init(&codec, encoder->codec_interface(), &cfg, 0)) {
    printf("Failed to initialize encoder");
    exit(-1);
  }

  raw.planes[0] = yuv;
  raw.planes[1] = yuv+(width*height);
  raw.planes[2] = yuv+(width*height)+(width*height/4);
  int flags = 0;
  uint8_t *vp9buf = NULL;

  vpx_codec_iter_t iter = NULL;
  const vpx_codec_cx_pkt_t *pkt = NULL;
  res = vpx_codec_encode(&codec, &raw, frame_count++, 1, flags, VPX_DL_GOOD_QUALITY);
  if (res != VPX_CODEC_OK) {
    printf("Failed to encode frame.\n");
  }

  res = vpx_codec_encode(&codec, NULL, -1, 1, 0, VPX_DL_GOOD_QUALITY);
  if (res != VPX_CODEC_OK) {
    printf("Failed to encode frame.\n");
  }

  while ((pkt = vpx_codec_get_cx_data(&codec, &iter)) != NULL) {
    if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
      vp9buf = pkt->data.frame.buf;
      *vp9BufSize = pkt->data.frame.sz;
    }
  }

  *vp9RetBuf = (unsigned char*)malloc(*vp9BufSize);
  memcpy(*vp9RetBuf, vp9buf, *vp9BufSize);
  vpx_img_free(&raw);
  if (vpx_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec.");

  return 0;
}

int vpx2rgb(unsigned char *vp9buf, int vp9BufSize, unsigned char** yuvBuf,
  int *retWidth, int *retHeight)
{
  const VpxInterface *decoder = NULL;
  int frame_count = 0;
  VpxVideoInfo info = (VpxVideoInfo){0};
  vpx_codec_ctx_t dec_codec;
  vpx_codec_iter_t iter = NULL;

  decoder = get_vpx_decoder_by_fourcc(VP9_FOURCC);
  if (!decoder) {
    printf("Unknown input codec.");
    exit(-1);
  }

  if (vpx_codec_dec_init(&dec_codec, decoder->codec_interface(), NULL, 0)) {
    printf("Failed to initialize decoder.");
    exit(-1);
  }

  vpx_image_t *img = NULL;
  size_t frame_size = vp9BufSize;

  if (vpx_codec_decode(&dec_codec, vp9buf, (unsigned int)frame_size, NULL, 0)) {
    printf("Failed to decode frame.\n");
    exit(-1);
  }

  vpx_image_t *retImage;
  while ((img = vpx_codec_get_frame(&dec_codec, &iter)) != NULL) {
    // vpx_img_write(img, outFile2);
    retImage = img; // storing img before NULL clear
    ++frame_count;
  }

  int width = *retWidth = retImage->d_w;
  int height = *retHeight = retImage->d_h;

  printf("w:%d, h:%d, stride: %d, %d, %d\n", width, height,
    retImage->stride[0], retImage->stride[1], retImage->stride[2]);

  int offset = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  BITMAPFILEHEADER bmpFileHeader;
  BITMAPINFOHEADER bmpInfoHeader;
  *yuvBuf = (unsigned char *)malloc(width*height*4+offset);

  bmpFileHeader.bfType = 0x4d42;
  bmpFileHeader.bfReserved1 = 0;
  bmpFileHeader.bfReserved2 = 0;
  bmpFileHeader.bfOffBits = offset;
  bmpFileHeader.bfSize = offset + width*height*4;

  bmpInfoHeader.biPlanes=1;
  bmpInfoHeader.biBitCount=32;
  bmpInfoHeader.biCompression=BI_RGB;
  bmpInfoHeader.biXPelsPerMeter = 0;
  bmpInfoHeader.biYPelsPerMeter = 0;
  bmpInfoHeader.biClrUsed = 0;
  bmpInfoHeader.biClrImportant = 0;
  bmpInfoHeader.biSize=sizeof(BITMAPINFOHEADER);
  bmpInfoHeader.biWidth=width;
  bmpInfoHeader.biHeight=height;
  bmpInfoHeader.biSizeImage=width*height*4;

  int ret = I420ToARGB(retImage->planes[0],
               retImage->stride[0],
               retImage->planes[1],
               retImage->stride[1],
               retImage->planes[2],
               retImage->stride[2],
               (*yuvBuf)+offset,
               width*4,
               width,
               height);

  memcpy(*yuvBuf, &bmpFileHeader, sizeof(bmpFileHeader));
  memcpy((*yuvBuf)+sizeof(bmpFileHeader), &bmpInfoHeader, sizeof(bmpInfoHeader));

  if (vpx_codec_destroy(&dec_codec)) die_codec(&dec_codec, "Failed to destroy codec.");

  return 0;

}

/*

// old main routine 

int main() {

  BITMAPFILEHEADER bmpFileHeader;
  BITMAPINFOHEADER bmpInfoHeader;
  unsigned char *bmpData;
  FILE *bmpFile, *outFile;
  int offset = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  int bmpDataSize;

  if ((bmpFile = fopen("test2.bmp", "rb")) == NULL)
    printf("Error opening test2.bmp\n");
  if (fread(&bmpFileHeader, sizeof(BITMAPFILEHEADER), 1, bmpFile) < 1)
    printf("Error reading Bitmap file header");
  if (fread(&(bmpInfoHeader), sizeof(BITMAPINFOHEADER), 1, bmpFile) < 1)
    printf("Error reading Bitmap info header");
  bmpDataSize = bmpFileHeader.bfSize-offset;
  bmpData = (unsigned char*)malloc(bmpDataSize+offset);
  if (fread(bmpData+offset, bmpDataSize, 1, bmpFile) < 1)
    printf("Error reading Bitmap data");
  fclose(bmpFile);
  memcpy(bmpData, &bmpFileHeader, sizeof(bmpFileHeader));
  memcpy(bmpData+sizeof(bmpFileHeader), &bmpInfoHeader, sizeof(bmpInfoHeader));

  int width = bmpInfoHeader.biWidth, height = bmpInfoHeader.biHeight;
  unsigned char *vpxData = NULL, *retBmpData = NULL;
  int vpxSize=0, yuvWidth=0, yuvHeight=0, retBmpDataSize;

  printf("Encoding...\n");
	bgr2vpx(bmpData, width, height, &vpxData, &vpxSize);
  
  FILE *vpxFile = NULL;
  if ((vpxFile = fopen("vp9.webm", "wb")) == NULL) printf("Error opening vp9.webm\n");
  else if (fwrite(vpxData, vpxSize, 1, vpxFile) < 1) printf("Error reading Bitmap file header");
  printf("vpxData:%x, vpxSize:%d\n", *vpxData, vpxSize);
  fclose(vpxFile);

  printf("Decoding...\n");
  vpx2rgb(vpxData, vpxSize, &retBmpData, &yuvWidth, &yuvHeight);

  printf("Write file.\n");
  if (!(outFile = fopen("converted.bmp", "wb"))) {
    printf("Failed to open %s for writing.", "converted.yuv");
    exit(-1);
  }
  retBmpDataSize = yuvWidth*yuvHeight*4+sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);

  int ret = fwrite(retBmpData, retBmpDataSize, 1, outFile);
  if (ret < 1) {
    printf("Error to write file. data:%x, size:%d\n", retBmpData, vpxSize);
  }
  fclose(outFile);

	return 0;
}

*/
