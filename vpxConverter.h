#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int bmp2vpx(unsigned char* bmpData, int width, int height, 
	unsigned char** vp9RetBuf, long *vp9BufSize);
int vpx2bmp(unsigned char *vp9buf, int vp9BufSize, 
	unsigned char** bmpBuf, int *retWidth, int *retHeight);
int fileWriteReadTest(void);
#ifdef __cplusplus
}
#endif