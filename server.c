#include <stdio.h>
#include <windows.h>
#include <winsock.h>
#include <dsound.h>
#include <psapi.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <tlhelp32.h>
#include <Gdiplus.h>
#include <sys/stat.h>
#include <pthread.h>
#include <zlib.h>
#include "comdata.h"
#include "wavestruct.h"
#include "public.h"
#include "vjoyinterface.h"
#include "turbojpeg.h"
#include "userlog.h"
#include "vpxConverter.h"
#include "lz4.h"
#include "opus.h"
#include "opus_custom.h"

int pcm2opus(OpusCustomEncoder *encoder, char *buf, int dataSize, unsigned char **cbits){
    // const int SAMPLE_RATE = 48000;
    const int MAX_PACKET_SIZE = 1000000;
    const int FRAME_SIZE = 64;
    opus_int16 in[dataSize/2];
    /* Convert from little-endian ordering. */
    for (int i=0;i<(dataSize/2);i++)
        in[i]=buf[2*i+1]<<8|buf[2*i];

    int opusEncodedBytes=0;
    int FRAME_OFFSET = FRAME_SIZE*CHANNELS*BITSPERSAMPLE/8;
    /* Encode the frame. */
    *cbits = (unsigned char*)malloc(MAX_PACKET_SIZE);
    for(int i=0;i<(dataSize-FRAME_OFFSET);i+=FRAME_OFFSET){
        int nbBytes = opus_custom_encode(encoder, &in[0]+FRAME_OFFSET/2, FRAME_SIZE, \
            (*cbits)+opusEncodedBytes, MAX_PACKET_SIZE);
        if (nbBytes<0)
        {
            LOG_ERROR("encode failed: %s\n", opus_strerror(nbBytes));
            // return EXIT_FAILURE;
        } else {
            opusEncodedBytes += nbBytes;
        }
    }
    return opusEncodedBytes;
}

long bmp2lz4(unsigned char *bmpData, long bmpSize, unsigned char **zData, unsigned long *zSize) {
    // LARGE_INTEGER start, end, freq;
    *zSize = LZ4_compressBound(bmpSize);
    *zData = (unsigned char*)malloc(*zSize+sizeof(COM_DATA));
    // QueryPerformanceCounter(&start);
    int ret = LZ4_compress_default((char*)bmpData, (char*)*zData, bmpSize, *zSize);
    if (ret == 0) {
        printf("LZ4 compress error. %d\n", ret);
    }
    // QueryPerformanceCounter(&end);
    // QueryPerformanceFrequency(&freq);
    // LOG_INFO("zlib convert:%fs ", (float(end.QuadPart-start.QuadPart)/(float)freq.QuadPart));
    *zSize = ret;
    // printf("compressed %ld -> %lu\n", bmpSize, *zSize);
    return ret;
}

int bmp2z(unsigned char *bmpData, long bmpSize, unsigned char **zData, unsigned long *zSize) {
    //LARGE_INTEGER start, end, freq;
    *zSize = compressBound(bmpSize);
    *zData = (unsigned char*)malloc(*zSize);
    //QueryPerformanceCounter(&start);
    int ret = compress( *zData, zSize, bmpData, bmpSize );
    if (ret != Z_OK) {
        LOG_ERROR("Zlib compress error. %d\n", ret);
    }
    // LOG_DEBUG("compressed %ld -> %ld\n", bmpSize, *zSize);
    // writeFile(bmpData, bmpSize, "pic/cantalloc.bmp");
    //QueryPerformanceCounter(&end);
    //QueryPerformanceFrequency(&freq);
    //LOG_INFO("zlib convert:%f s", (float(end.QuadPart-start.QuadPart)/(float)freq.QuadPart));
    return 0;
}

BOOL CALLBACK DSEnumProc(LPGUID lpGUID,  
             LPCTSTR lpszDesc, 
             LPCTSTR lpszDrvName,  
             LPVOID lpContext ) 
{ 
    if (lpGUID != NULL) { //  NULL only for "Primary Sound Driver". 
        LOG_INFO("Driver name: %s, Description:%s\n", lpszDrvName, lpszDesc);
    }
    return TRUE;
} 

int vJoyInit(int vJoyId) {

    if (!vJoyEnabled()) {
        LOG_WARN("Failed Getting vJoy attributes.\n");
        return -2;
    } else {
        /*
        printf(
        "Vendor: %S\nProduct :%S\nVersion Number:%S\n",
        TEXT(GetvJoyManufacturerString()),\
        TEXT(GetvJoyProductString()),\
        TEXT(GetvJoySerialNumberString()));
        */
    };

    WORD VerDll, VerDrv;
    if (!DriverMatch(&VerDll, &VerDrv))
        LOG_WARN("Failed\r\nvJoy Driver (version %04x) does not match vJoyInterface DLL (version %04x)\n", VerDrv ,VerDll);
    // else
        // printf("OK - vJoy Driver and vJoyInterface DLL match vJoyInterface DLL (version %04x)\n",VerDrv);

    // Get the state of the requested device
    UINT iInterface = vJoyId;
    VjdStat status = GetVJDStatus(iInterface);
    switch(status) {
        case VJD_STAT_OWN:
            // printf("vJoy Device %d is already owned by this feeder\n", iInterface);
            break;
        case VJD_STAT_FREE:
            // printf("vJoy Device %d is free\n", iInterface);
            break;
        case
            VJD_STAT_BUSY:
            LOG_INFO("vJoy Device %d is already owned by another feeder\nCannot continue\n", iInterface);
            return -3;
        case VJD_STAT_MISS:
            LOG_WARN("vJoy Device %d is not installed or disabled\nCannot continue\n", iInterface);
            return -4;
        default:
            LOG_WARN("vJoy Device %d general error\nCannot continue\n", iInterface);
            return -1;
    };

    // Check which axes are supported
    /*
    BOOL AxisX  = GetVJDAxisExist(iInterface, HID_USAGE_X);
    BOOL AxisY  = GetVJDAxisExist(iInterface, HID_USAGE_Y);
    BOOL AxisZ  = GetVJDAxisExist(iInterface, HID_USAGE_Z);
    BOOL AxisRX = GetVJDAxisExist(iInterface, HID_USAGE_RX);
    BOOL AxisRY = GetVJDAxisExist(iInterface, HID_USAGE_RY);
    // Get the number of buttons supported by this vJoy device
    int nButtons  = GetVJDButtonNumber(iInterface);
    int povNumber = GetVJDContPovNumber(iInterface);
    */
    // Print results
    /*
    printf("\nvJoy Device %d capabilities\n", iInterface);
    printf("Numner of buttons\t\t%d\n", nButtons);
    printf("Axis X\t\t%s\n", AxisX?"Yes":"No");
    printf("Axis Y\t\t%s\n", AxisX?"Yes":"No");
    printf("Axis Z\t\t%s\n", AxisX?"Yes":"No");
    printf("Axis Rx\t\t%s\n", AxisRX?"Yes":"No");
    printf("Axis Ry\t\t%s\n", AxisRY?"Yes":"No");
    printf("POV  \t\t%d\n", povNumber);
    */

    // Acquire the target
    if ((status == VJD_STAT_OWN) || ((status == VJD_STAT_FREE) && (!AcquireVJD(iInterface))))
    {
        LOG_WARN("Failed to acquire vJoy device number %d.\n", iInterface);
        return -1;
    } else {
        // printf("Acquired: vJoy device number %d.\n", iInterface);
    }
    
    return 0;
}

typedef struct {
        HWND hwnd;
        HWND targetHwnd;
        SOCKET* socket;
} Addrs;

HWND getWindowHandleFileName(char *processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe32;
    if(hSnapshot) {
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if(Process32First(hSnapshot, &pe32)) {
            while(strcmp(pe32.szExeFile, processName)) {
                Process32Next(hSnapshot, &pe32);
                // printf("pid %d %s\n", pe32.th32ProcessID, pe32.szExeFile);
                // Sleep(100);
            }
            //} while(Process32Next(hSnapshot, &pe32));
         }
         // printf("pid %d %s\n", pe32.th32ProcessID, pe32.szExeFile);
         CloseHandle(hSnapshot);
    }

    HWND hWnd = GetTopWindow(NULL);
    do {
        if(GetWindowLong( hWnd, GWL_HWNDPARENT) != 0 || !IsWindowVisible( hWnd))
            continue;
        DWORD ProcessID;
        GetWindowThreadProcessId( hWnd, &ProcessID);
        if(pe32.th32ProcessID == ProcessID)
            return hWnd;
    } while((hWnd = GetNextWindow( hWnd, GW_HWNDNEXT)) != NULL);

    return NULL;
}

int bmp2jpg(BYTE *bmpData, int bmpSize, int width, int height, BYTE** jpegBuf, unsigned long *jpegSize) {
    // tjhandle _jpegCompressor = tjInitCompress();
    int /*inSubsamp,*/ outSubsamp = TJSAMP_444, /*inColorspace,*/ pixelFormat = TJPF_BGR;
    int outQual = 95, flags=0;
    tjhandle tjInstance = NULL;
    // unsigned char *imgBuf = NULL;
    // FILE *bmpFile, *jpegFile;
    const int BMPHEADERSIZE = 54;

    /*
    LOG_INFO("Writing bmp file.\n");
    
    if ((bmpFile = fopen("turbotest.bmp", "wb")) == NULL)
        LOG_ERROR("Error opening output file\n");
    else {
        if (fwrite(bmpData, bmpSize, 1, bmpFile) < 1)
            LOG_ERROR("Error writing output file");
        fclose(bmpFile);
    }
    */
    // writeFile(bmpData, bmpSize, (char*)"bmp1.bmp");
    // Sleep(100);
    /*
    if ((imgBuf = tjLoadImage("bmp1.bmp", &width, 1, &height, &pixelFormat, 0)) == NULL)
        LOG_DEBUG("loading input image: %s\n", tjGetErrorStr2(tjInstance));
    LOG_DEBUG("%d x %d, pixel format: %d\n", width, height, pixelFormat);

    if (tjSaveImage("bmp2.bmp", imgBuf, width, 0, height, pixelFormat, 0) != 0)
        LOG_DEBUG("loading input image: %s\n", tjGetErrorStr2(tjInstance));
    // LOG_DEBUG("%d x %d, pixel format: %d\n", width, height, pixelFormat);
    */
    // printf("Compress jpeg.\n");
    /*
    LOG_DEBUG("Compare bmp1 &2 -> %d\n.", memcmp(bmpData+BMPHEADERSIZE, imgBuf, BMPHEADERSIZE));
    LOG_DEBUG("bmp1:\n");
    dumpBinary(bmpData+BMPHEADERSIZE, 64);
    LOG_DEBUG("bmp2:\n");
    dumpBinary(imgBuf, 64);
    */
    // LOG_INFO("Initializing jpeg convert.\n");

    *jpegBuf = NULL;
    *jpegSize = 0;
    if ((tjInstance = tjInitCompress()) == NULL) {
        LOG_ERROR("Error: initializing compressor: %s\n", tjGetErrorStr2(NULL));
        return 0;
    }

    // LOG_INFO("Start jpeg convert.\n");

    if (tjCompress2(tjInstance, bmpData+BMPHEADERSIZE, width,
        TJPAD(width*tjPixelSize[pixelFormat]), height, pixelFormat,
    // if (tjCompress2(tjInstance, imgBuf, width, 0, height, pixelFormat,
                  jpegBuf, jpegSize, outSubsamp, outQual, flags) < 0) {
        LOG_ERROR("Error: compressing image: %s\n",  tjGetErrorStr2(tjInstance));
        return 0;
    }

    // writeFile(*jpegBuf, *jpegSize, (char*)"jpeg_converted.jpg");

    /*
    if ((jpegFile = fopen("turbotest_converted.jpg", "wb")) == NULL)
        printf("opening output file\n");
    if (fwrite(*jpegBuf, *jpegSize, 1, jpegFile) < 1)
        printf("writing output file");
    fclose(jpegFile);
    */


    // rotation test
    // printf("Rotate jpeg file.\n");

    unsigned char *dstBuf = NULL;  // Dynamically allocate the JPEG buffer
    unsigned long dstSize = 0;
    tjtransform xform;

    memset(&xform, 0, sizeof(tjtransform));
    xform.op = TJXOP_VFLIP;

    // LOG_INFO("Starting to flip jpeg.\n");

    if ((tjInstance = tjInitTransform()) == NULL)
        LOG_ERROR("Error: initializing transformer\n");
    xform.options |= TJXOPT_TRIM;
    if (tjTransform(tjInstance, *jpegBuf, *jpegSize, 1, &dstBuf, &dstSize,
                      &xform, flags) < 0)
        LOG_ERROR("Error: transforming input image\n");

    // end of test

    tjFree(*jpegBuf);
    *jpegBuf = dstBuf;
    *jpegSize = dstSize;
    
    // writeFile(*jpegBuf, *jpegSize, (char*)"jpeg_flipped.jpg");

    /*
    if ((jpegFile = fopen("turbotest_rotated.jpg", "wb")) == NULL)
        printf("opening output file\n");
    if (fwrite(*jpegBuf, *jpegSize, 1, jpegFile) < 1)
        printf("writing output file");
    fclose(jpegFile);
    Sleep(1000);
    */

    tjDestroy(tjInstance);
    tjInstance = NULL;

    return 1;
}

int startListen(SOCKET* listenSock){
    SOCKADDR_IN addr;
    *listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if( *listenSock == INVALID_SOCKET){
        LOG_ERROR("Failed to create socket.\n");
        LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        return -1;
    } else {
        // ソケットのアドレス情報記入
        addr.sin_family = AF_INET;
        addr.sin_port = htons(55500);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        // ソケットをアドレスにバインドする
        if( bind(*listenSock, (LPSOCKADDR)&addr, sizeof(addr) ) != SOCKET_ERROR){
            // 接続を聴取する
            if( listen(*listenSock, 3) != SOCKET_ERROR){
                // printf("Wait for client...(sock:%d)\n", *listenSock);
            }
            else{
                LOG_ERROR("Listen error.\n");
                LOG_ERROR("Error %d occured.\n", WSAGetLastError());
                return -1;
            }
        }
        else{
            LOG_ERROR("Bind error\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
            return -1;
        }
    }

    return 0;
}

int startListenUDP(SOCKET* listensock) {
    SOCKADDR_IN addr;
    *listensock = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(55500);
    addr.sin_addr.s_addr = INADDR_ANY;

    if( bind(*listensock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR ){
        LOG_ERROR("UDP bind error.\n");
        return -1;
    }

    return 0;
}

int acceptLoop(SOCKET *listenSocket, SOCKET *serverSocket) {
    // const int RECVSIZE = 256;
    //SOCKET g_sock1/*, clientSoc*/;
    //g_sock1 = socket(AF_INET,SOCK_STREAM,0);
    // SOCKADDR_IN addr;
    SOCKADDR_IN clientAddr;
    //IN_ADDR clientIn;
    int nClientAddrLen;

    // printf("Accept loop started!\n");
    //g_sock1 = -1;

    // printf("クライアントの接続待ち・・・\n");
    // クライアントのアドレス構造体の大きさを設定する
    nClientAddrLen = sizeof(clientAddr);

    // printf("Socket state -> listen:%d, server:%d\n", *listenSocket, *serverSocket);

    *serverSocket = accept(*listenSocket, (LPSOCKADDR)&clientAddr, &nClientAddrLen);
    if(*serverSocket == INVALID_SOCKET){
        LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        return -1;   
    }
    else{
        // memcpy(&clientIn, &clientAddr.sin_addr.s_addr, 4);
        // printf("Connected. Client IP:%s\n", inet_ntoa(clientIn));
        LOG_INFO("Operate socket accepted.\n");
    }
    return 0;
}

void *runOperateThread(void *param) {
    COM_DATA com_data;
    Addrs addrs = *(Addrs*)param;
    SOCKET *serverSocket = addrs.socket;
    HWND targetHwnd = addrs.targetHwnd;
    int mouse_x=0, mouse_y=0/*, old_mouse_x=0, old_mouse_y=0*/;
    int mouse_left=0, mouse_right=0, mouse_middle=0, mouse_wheel=0;
    int vJoyId = 1;
    POINT pt;
    double xCoeff = (double)0xffff / (double)GetSystemMetrics(SM_CXSCREEN);
    double yCoeff = (double)0xffff / (double)GetSystemMetrics(SM_CYSCREEN);

    WINDOWINFO windowInfo;
    windowInfo.cbSize = sizeof(WINDOWINFO);

    LOG_INFO("Operate thread started.\n");

    vJoyInit(vJoyId);
    JOYSTICK_POSITION iReport;
    iReport.bDevice = vJoyId;

    while(1) {
        Sleep(10);

        if (recv(*serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            LOG_ERROR("ThreadOperate receive error.\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        }

        /*
        printf("x:%ld, y:%ld, z:%ld, r:%ld, u:%ld, v:%ld\n",
            com_data.gamepad1, com_data.gamepad2, com_data.gamepad3,
            com_data.gamepad4, com_data.gamepad7, com_data.gamepad8 );
        printf("pov:%ld, buttons:%lx\n", com_data.gamepad5, com_data.gamepad6);
        */

        // for DirectInput
        iReport.wAxisX=com_data.gamepad1 / 2;
        iReport.wAxisY=com_data.gamepad2 / 2;
        iReport.wAxisZ=com_data.gamepad3 / 2;
        iReport.wAxisXRot=com_data.gamepad4 / 2;
        iReport.wAxisYRot=com_data.gamepad7 / 2;
        iReport.wAxisZRot=com_data.gamepad8 / 2;
        iReport.lButtons = com_data.gamepad6;
        iReport.bHats = com_data.gamepad5;
        
        if(!UpdateVJD(vJoyId, &iReport)){
        //    printf("Error on VJoy command injection.\n");
        }

        GetWindowInfo(targetHwnd, &windowInfo);

        // mouse_x = com_data.mouse_x + windowInfo.rcWindow.left;
        // mouse_y = com_data.mouse_y + windowInfo.rcWindow.top;
        // if (  mouse_x != old_mouse_x || mouse_y != old_mouse_y ) {
        if (com_data.mouse_move == 1) {
            mouse_x = com_data.mouse_x + windowInfo.rcWindow.left;
            mouse_y = com_data.mouse_y + windowInfo.rcWindow.top;
            // old_mouse_x = mouse_x;
            // old_mouse_y = mouse_y;
            INPUT input = { INPUT_MOUSE, (int)((double)mouse_x*xCoeff), (int)((double)mouse_y*yCoeff),
             0, MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE };
            SendInput(1, &input, sizeof(INPUT));
            GetCursorPos(&pt);
            //printf("abs(%d, %d) pos (%d, %d)\n", mouse_x, mouse_y, pt.x, pt.y);
        } 

        if (com_data.mouse_left != mouse_left ) {
            mouse_left = com_data.mouse_left;
            if ( mouse_left == 1 ) {
                INPUT input = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_LEFTDOWN };
                SendInput(1, &input, sizeof(INPUT));
                //printf("left button down.\n");
                //GetCursorPos(&pt);
                //printf("Cursor position x:%d, y:%d.\n", pt.x, pt.y);
            } else if ( mouse_left == 2) {
                INPUT input = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_LEFTUP };
                SendInput(1, &input, sizeof(INPUT));
                //printf("left button up.\n");
            } else if ( mouse_left == 3) {
                INPUT input1 = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_LEFTDOWN },
                      input2 = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_LEFTUP };
                SendInput(1, &input1, sizeof(INPUT));
                SendInput(1, &input2, sizeof(INPUT));
                SendInput(1, &input1, sizeof(INPUT));
                SendInput(1, &input2, sizeof(INPUT));
            }
        }

        if (com_data.mouse_right != mouse_right ) {
            mouse_right = com_data.mouse_right;
            if ( mouse_right == 1 ) {
                INPUT input = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_RIGHTDOWN };
                SendInput(1, &input, sizeof(INPUT));
            } else if ( mouse_right == 2) {
                INPUT input = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_RIGHTUP };
                SendInput(1, &input, sizeof(INPUT));
            } else if ( mouse_right == 3) {
                INPUT input1 = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_RIGHTDOWN },
                      input2 = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_RIGHTUP };
                SendInput(1, &input1, sizeof(INPUT));
                SendInput(1, &input2, sizeof(INPUT));
                SendInput(1, &input1, sizeof(INPUT));
                SendInput(1, &input2, sizeof(INPUT));
            }
        }

        if (com_data.mouse_middle != mouse_middle ) {
            mouse_middle = com_data.mouse_middle;
            if ( mouse_middle == 1 ) {
                INPUT input = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_MIDDLEDOWN };
                SendInput(1, &input, sizeof(INPUT));
            } else if ( mouse_middle == 2) {
                INPUT input = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_MIDDLEUP };
                SendInput(1, &input, sizeof(INPUT));
            } else if ( mouse_middle == 3) {
                INPUT input1 = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_MIDDLEDOWN },
                      input2 = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_MIDDLEUP };
                SendInput(1, &input1, sizeof(INPUT));
                SendInput(1, &input2, sizeof(INPUT));
                SendInput(1, &input1, sizeof(INPUT));
                SendInput(1, &input2, sizeof(INPUT));
            }
        }

        if (com_data.keydown == 1 ) {
            INPUT input; 
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = com_data.keycode;
            input.ki.wScan = MapVirtualKey(com_data.keycode, 0);
            input.ki.dwFlags = 0;
            if (com_data.keycode_flg == 0x00) {
                input.ki.dwFlags |= KEYEVENTF_KEYUP;
            }
            SendInput(1, &input, sizeof(INPUT));
            //printf("Sent %d\n", com_data.keycode);
        }

        if (send(*serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            LOG_ERROR("ThreadOperate send error.\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        }
    }

    RelinquishVJD(vJoyId);
    // ExitThread(TRUE);
    return 0;
}

void *runImageThread(void *param) {

    COM_DATA com_data;
    // DWORD dwLoadSize;
    RECT rc;
    Addrs addrs = *(Addrs*)param;
    SOCKET *serverSocket = addrs.socket;
    HWND targetHwnd = addrs.targetHwnd;
    // BITMAP bitmap;
    BITMAPFILEHEADER bitmapFileHeader;
    BITMAPINFO bmpInfo;
    LPDWORD lpPixel;
    const int offset = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    DWORD lineSizeDW, imageSize, bmpFileSize;
    HDC targetHdc, hdc;
    HBITMAP hBitmap;
    // BOOL succeedBitBlt;
    BYTE *bmpData;
    int width, height, oldWidth = 0, oldHeight = 0;

    bitmapFileHeader.bfType = 0x4d42;
    bitmapFileHeader.bfReserved1 = 0;
    bitmapFileHeader.bfReserved2 = 0;
    bitmapFileHeader.bfOffBits = offset;

    bmpInfo.bmiHeader.biPlanes=1;
    bmpInfo.bmiHeader.biBitCount=24;
    bmpInfo.bmiHeader.biCompression=BI_RGB;
    bmpInfo.bmiHeader.biXPelsPerMeter = 0;
    bmpInfo.bmiHeader.biYPelsPerMeter = 0;
    bmpInfo.bmiHeader.biClrUsed = 0;
    bmpInfo.bmiHeader.biClrImportant = 0;
    bmpInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);

    LOG_INFO("Image thread started.\n");
    // fileWriteReadTest();

    while(1) {
        // Sleep(1000);
        GetWindowRect(targetHwnd, &rc);
        width = rc.right-rc.left;
        height = rc.bottom-rc.top;
        if ( width != oldWidth || height != oldHeight) {
            com_data.server_cx = oldWidth = width;
            com_data.server_cy = oldHeight = height;
            // LOG_INFO("server rect size changed:%d, %d\n", width, height);
        }
        // ファイルサイズ計算
        lineSizeDW = (bmpInfo.bmiHeader.biBitCount / 8) * width;
        //imageSize = (int)ceil((double)lineSizeDW / (double)sizeof(DWORD)) * sizeof(DWORD) * height;
        imageSize = (lineSizeDW % 4 == 0 ? lineSizeDW : (lineSizeDW / 4)*4+4 ) * height;

        bmpFileSize = offset + imageSize;
        //LOG_INFO("File size:%d = %d + %d + %d\n", dwLoadSize,
        //    sizeof(BITMAPFILEHEADER), sizeof(BITMAPINFOHEADER), imageSize);
        bitmapFileHeader.bfSize = bmpFileSize;
        //DIBの情報を設定する
        bmpInfo.bmiHeader.biWidth=width;
        bmpInfo.bmiHeader.biHeight=height;
        // bmpInfo.bmiHeader.biSizeImage = imageSize;
        bmpInfo.bmiHeader.biSizeImage = 0; // Follow libjpeg-turbo specification 

        // targetHdc = GetWindowDC(targetHwnd);
        targetHdc = GetWindowDC(GetDesktopWindow());
        hdc = CreateCompatibleDC(targetHdc);
        hBitmap = CreateDIBSection(targetHdc, &bmpInfo, DIB_RGB_COLORS,(void**)&lpPixel,NULL,0);
        SelectObject(hdc,hBitmap);
        BitBlt(hdc, 0, 0, width, height, targetHdc, rc.left, rc.top, SRCCOPY);
        bmpData = (BYTE*)malloc(bmpFileSize);
        memcpy(bmpData,&bitmapFileHeader,sizeof(bitmapFileHeader));
        memcpy(bmpData+sizeof(bitmapFileHeader),&bmpInfo.bmiHeader,sizeof(bmpInfo.bmiHeader));
        memcpy(bmpData+sizeof(bitmapFileHeader)+sizeof(bmpInfo.bmiHeader), lpPixel, imageSize);

        // LOG_INFO("Image data size: %d bytes.\n", dwLoadSize);
        // LOG_DEBUG("Convert image to jpeg.\n");
        
        // Using JPEG
        /*
        BYTE* jpegData;
        unsigned long jpegSize;
        bmp2jpg(bmpData, bitmapFileHeader.bfSize, width, height, &jpegData, &jpegSize);
        com_data.data_size = jpegSize;
        */
        //Using VP9
        /*
        unsigned char* vpxData=NULL;
        long vpxSize=0;
        bmp2vpx((unsigned char*)bmpData, width, height, &vpxData, &vpxSize);
        // readDummyVpx(&vpxData, &vpxSize);
        com_data.data_size = vpxSize;
        */

        //using zlib
        /*
        unsigned char* zData=NULL;
        unsigned long zSize=0;
        bmp2z(bmpData, bmpFileSize, &zData, &zSize);
        com_data.data_size = zSize;
        com_data.original_size = bmpFileSize;
        */
        //using lz4
        unsigned char* lz4Data=NULL;
        unsigned long lz4Size=0;
        bmp2lz4(bmpData, bmpFileSize, &lz4Data, &lz4Size);
        com_data.data_size = lz4Size;
        com_data.original_size = bmpFileSize;

        // LOG_DEBUG("Sending image header data.\n");
        if (send(*serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            LOG_ERROR("ThreadImage send error(header).\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        }

        //if (send(*serverSocket,(char*)bmpData,dwLoadSize,0) < 0)
        // LOG_DEBUG("Sending image data.\n");
        // if (send(*serverSocket,(char*)jpegData,jpegSize,0) < 0)
        // if (send(*serverSocket,(char*)vpxData,vpxSize,0) < 0)
        // if (send(*serverSocket,(char*)zData,zSize,0) < 0)
        if (send(*serverSocket,(char*)lz4Data,lz4Size,0) < 0)
        {
            LOG_ERROR("ThreadImage send error(image).\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        }
        // LOG_DEBUG("Image was sent.\n");
        ReleaseDC(targetHwnd, targetHdc);
        DeleteDC(hdc);
        DeleteObject(hBitmap);
        free(bmpData);
        free(lz4Data);
        // free(zData);
        // free(vpxData);
        // free(jpegData);
    }
    /* メモリブロックのロック解除 */
    //GlobalUnlock(hBuf);
    //ExitThread(TRUE);
    return 0;
}

void *runImageThreadUDP(void *param) {

    COM_DATA com_data;
    // DWORD dwLoadSize;
    RECT rc;
    Addrs addrs = *(Addrs*)param;
    struct sockaddr_in cliAddress;
    SOCKET *serverSocket = addrs.socket;
    HWND targetHwnd = addrs.targetHwnd;
    // BITMAP bitmap;
    BITMAPFILEHEADER bitmapFileHeader;
    BITMAPINFO bmpInfo;
    LPDWORD lpPixel;
    const int offset = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    DWORD lineSizeDW, imageSize, bmpFileSize;
    HDC targetHdc, hdc;
    HBITMAP hBitmap;
    // BOOL succeedBitBlt;
    BYTE *bmpData;
    int width, height, oldWidth = 0, oldHeight = 0;

    bitmapFileHeader.bfType = 0x4d42;
    bitmapFileHeader.bfReserved1 = 0;
    bitmapFileHeader.bfReserved2 = 0;
    bitmapFileHeader.bfOffBits = offset;

    bmpInfo.bmiHeader.biPlanes=1;
    bmpInfo.bmiHeader.biBitCount=24;
    bmpInfo.bmiHeader.biCompression=BI_RGB;
    bmpInfo.bmiHeader.biXPelsPerMeter = 0;
    bmpInfo.bmiHeader.biYPelsPerMeter = 0;
    bmpInfo.bmiHeader.biClrUsed = 0;
    bmpInfo.bmiHeader.biClrImportant = 0;
    bmpInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);

    LOG_INFO("Image thread started.\n");
    // fileWriteReadTest();
    // for getting client address
    char tmpBuf[1500];
    int len = sizeof(cliAddress);
    recvfrom(*serverSocket, tmpBuf, sizeof(tmpBuf), 
            0, (struct sockaddr*)&cliAddress,&len);
    LOG_INFO("Connected: %s:%d\n", inet_ntoa(cliAddress.sin_addr) , ntohs(cliAddress.sin_port));

    while(1) {
        //Sleep(500);
        GetWindowRect(targetHwnd, &rc);
        width = rc.right-rc.left;
        height = rc.bottom-rc.top;
        if ( width != oldWidth || height != oldHeight) {
            com_data.server_cx = oldWidth = width;
            com_data.server_cy = oldHeight = height;
            // LOG_INFO("server rect size changed:%d, %d\n", width, height);
        }
        // ファイルサイズ計算
        lineSizeDW = (bmpInfo.bmiHeader.biBitCount / 8) * width;
        //imageSize = (int)ceil((double)lineSizeDW / (double)sizeof(DWORD)) * sizeof(DWORD) * height;
        imageSize = (lineSizeDW % 4 == 0 ? lineSizeDW : (lineSizeDW / 4)*4+4 ) * height;

        bmpFileSize = offset + imageSize;
        //LOG_INFO("File size:%d = %d + %d + %d\n", dwLoadSize,
        //    sizeof(BITMAPFILEHEADER), sizeof(BITMAPINFOHEADER), imageSize);
        bitmapFileHeader.bfSize = bmpFileSize;
        //DIBの情報を設定する
        bmpInfo.bmiHeader.biWidth=width;
        bmpInfo.bmiHeader.biHeight=height;
        // bmpInfo.bmiHeader.biSizeImage = imageSize;
        bmpInfo.bmiHeader.biSizeImage = 0; // Follow libjpeg-turbo specification 

        // targetHdc = GetWindowDC(targetHwnd);
        targetHdc = GetWindowDC(GetDesktopWindow());
        hdc = CreateCompatibleDC(targetHdc);
        hBitmap = CreateDIBSection(targetHdc, &bmpInfo, DIB_RGB_COLORS,(void**)&lpPixel,NULL,0);
        SelectObject(hdc,hBitmap);
        BitBlt(hdc, 0, 0, width, height, targetHdc, rc.left, rc.top, SRCCOPY);
        bmpData = (BYTE*)malloc(bmpFileSize);
        memcpy(bmpData,&bitmapFileHeader,sizeof(bitmapFileHeader));
        memcpy(bmpData+sizeof(bitmapFileHeader),&bmpInfo.bmiHeader,sizeof(bmpInfo.bmiHeader));
        memcpy(bmpData+sizeof(bitmapFileHeader)+sizeof(bmpInfo.bmiHeader), lpPixel, imageSize);

        // LOG_INFO("Image data size: %d bytes.\n", dwLoadSize);
        // LOG_DEBUG("Convert image to jpeg.\n");
        
        // Using JPEG
        /*
        BYTE* jpegData;
        unsigned long jpegSize;
        bmp2jpg(bmpData, bitmapFileHeader.bfSize, width, height, &jpegData, &jpegSize);
        com_data.data_size = jpegSize;
        */
        //Using VP9
        /*
        unsigned char* vpxData=NULL;
        long vpxSize=0;
        bmp2vpx((unsigned char*)bmpData, width, height, &vpxData, &vpxSize);
        // readDummyVpx(&vpxData, &vpxSize);
        com_data.data_size = vpxSize;
        */

        //using zlib
        /*
        unsigned char* zData=NULL;
        unsigned long zSize=0;
        bmp2z(bmpData, bmpFileSize, &zData, &zSize);
        com_data.data_size = zSize;
        com_data.original_size = bmpFileSize;
        */
        //using lz4
        unsigned char* lz4Data=NULL;
        unsigned long lz4Size=0;
        bmp2lz4(bmpData, bmpFileSize, &lz4Data, &lz4Size);
        com_data.data_size = lz4Size+sizeof(COM_DATA);
        com_data.original_size = bmpFileSize;
        memcpy(lz4Data+lz4Size, &com_data, sizeof(COM_DATA));

        // LOG_DEBUG("Sending image header data.\n");
        //if (send(*serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        //{
        //    LOG_ERROR("ThreadImage send error(header).\n");
        //    LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        //}

        //if (send(*serverSocket,(char*)bmpData,dwLoadSize,0) < 0)
        // LOG_DEBUG("Sending image data.\n");
        // if (send(*serverSocket,(char*)jpegData,jpegSize,0) < 0)
        // if (send(*serverSocket,(char*)vpxData,vpxSize,0) < 0)
        // if (send(*serverSocket,(char*)zData,zSize,0) < 0)
        int ret = sendto(*serverSocket,(char*)lz4Data,lz4Size+sizeof(COM_DATA),0,
            (struct sockaddr*)&cliAddress, sizeof(cliAddress));
        if ( ret < 0)
        {
            LOG_ERROR("ThreadImage send error(image).\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        } else {
            //LOG_INFO("Sent %d bytes.\n", ret);
        }
        // LOG_DEBUG("Image was sent.\n");
        ReleaseDC(targetHwnd, targetHdc);
        DeleteDC(hdc);
        DeleteObject(hBitmap);
        free(bmpData);
        free(lz4Data);
        // free(zData);
        // free(vpxData);
        // free(jpegData);
    }
    /* メモリブロックのロック解除 */
    //GlobalUnlock(hBuf);
    //ExitThread(TRUE);
    return 0;
}

void *runSoundThread(void *param) {

    COM_DATA com_data;
    Addrs addrs = *(Addrs*)param;
    // long samplerate = com_data.samplerate;
    // long sound_size = com_data.data_size;
    SOCKET *serverSocket = addrs.socket;
    // byte* waveData;
    long dataSize=0;

    const int SAMPLE_RATE = 48000;
    // const int APPLICATION = OPUS_APPLICATION_AUDIO;
    const int FRAME_SIZE = 64;
    // const int MAX_PACKET_SIZE = SAMPLE_RATE*8;
    // const int MAX_FRAME_SIZE = 6*960;

    LPDIRECTSOUNDCAPTURE captureDevice = NULL;//DirectSoundCaptureDeviceオブジェクト
    LPDIRECTSOUNDCAPTUREBUFFER captureBuffer = NULL;//DirectSoundCaptureBufferオブジェクト

    WAVEFORMATEX wfx = {WAVE_FORMAT_PCM, CHANNELS, SAMPLERATE,
        SAMPLERATE*CHANNELS*BITSPERSAMPLE/8, CHANNELS*BITSPERSAMPLE/8, BITSPERSAMPLE, 0};
    DSCBUFFERDESC bufferDescriber
        = {sizeof(DSCBUFFERDESC), 0, wfx.nAvgBytesPerSec*1, 0, &wfx, 0, NULL};
    DWORD readablePos, capturedPos, readBufferPos, lockLength, capturedLength, wrappedCapturedLength;
    // DWORD copiedLength = 0;
    void *capturedData = NULL, *wrappedCapturedData = NULL;
    char *copiedBuffer;
    int recordDurationSec = 3; //録音時間(秒)
    HRESULT Hret;
    // time_t start, end;

    CoInitialize(NULL);
    DirectSoundCaptureCreate8( NULL, &captureDevice, NULL );

    DirectSoundEnumerate((LPDSENUMCALLBACK)DSEnumProc, (VOID*)NULL);

    readBufferPos = 0;
    copiedBuffer = (char*)malloc(
        wfx.nAvgBytesPerSec * wfx.nChannels * wfx.wBitsPerSample / 8 * recordDurationSec * 2); 
    captureDevice->CreateCaptureBuffer(&bufferDescriber,&captureBuffer,NULL);
    // start = time(NULL);
    captureBuffer->Start(DSCBSTART_LOOPING);

    LOG_INFO("Sound thread started.\n");

    /*
    OpusCustomMode *mode;
    OpusCustomEncoder *encoder;
    int err;
    mode = opus_custom_mode_create(SAMPLE_RATE, FRAME_SIZE, &err);
    if (err<0)
    {
        LOG_ERROR("failed to create an mode: %s\n", opus_strerror(err));
        // return EXIT_FAILURE;
    }

    encoder = opus_custom_encoder_create(mode, CHANNELS, &err);
    if (err<0)
    {
        LOG_ERROR("failed to create an encoder: %s\n", opus_strerror(err));
        // return EXIT_FAILURE;
    }
    */

    while(1){

        // Sleep(10);

        captureBuffer->GetCurrentPosition(&capturedPos, &readablePos);
        if  ( readablePos > readBufferPos ) lockLength = readablePos - readBufferPos;
        else lockLength = bufferDescriber.dwBufferBytes - readBufferPos + readablePos;

        // printf("Lock startRead:%d, readable:%d, locklen:%d, captured:%d\n",
        //  readBufferPos, readablePos, lockLength, capturedPos);
        Hret = captureBuffer->Lock(readBufferPos, lockLength,
            &capturedData, &capturedLength,
            &wrappedCapturedData, &wrappedCapturedLength,
            0);
        if( Hret != DS_OK ) {
            LOG_ERROR("Lock error:%ld\n", Hret);
        } else {
            // printf("buffer read, buf1:%d, buf2:%d\n", capturedLength, wrappedCapturedLength);
        }

        if (capturedData != NULL) {
            memcpy(copiedBuffer, capturedData, capturedLength);
            // copiedLength += capturedLength;
            readBufferPos += capturedLength;
            dataSize = capturedLength;
            if (readBufferPos >= bufferDescriber.dwBufferBytes)
                readBufferPos = 0;
        }

        if (wrappedCapturedData != NULL) { // Ring buffer wrapped
            memcpy(copiedBuffer+capturedLength, wrappedCapturedData, wrappedCapturedLength);
            // copiedLength += wrappedCapturedLength;
            readBufferPos = wrappedCapturedLength;
            dataSize += wrappedCapturedLength;
        }

        // writeWaveFile(copiedBuffer, dataSize, (char*)"beforeConvert.wav");
        // LOG_INFO("sent pcm data size:%ld\n", dataSize);

        Hret = captureBuffer->Unlock( capturedData, capturedLength,
            wrappedCapturedData, wrappedCapturedLength);
        /*
        unsigned char *cbits;
        int opusSize = pcm2opus(encoder, copiedBuffer, dataSize, &cbits);
        */

        com_data.data_size = dataSize;
        // com_data.data_size = opusSize;
        com_data.samplerate = SAMPLERATE;

        if (send(*serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            LOG_ERROR("ThreadSound send error(header).\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        }

        if (send(*serverSocket, (char*)copiedBuffer, dataSize, 0) < 0)
        // if (send(*serverSocket, (char*)cbits, opusSize, 0) < 0)
        {
            LOG_ERROR("ThreadSound send error(sound).\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
            // printf("wavedata %x, size:%d\n", waveData, dataSize);
        }
        // free(cbits);
    }

    captureBuffer->Stop();
    free(copiedBuffer);
    CoUninitialize();

    // ExitThread(TRUE);
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd , UINT msg , WPARAM wp , LPARAM lp) {
	// HDC hdc;
	// PAINTSTRUCT ps;
	// int x;

	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		// hdc = BeginPaint(hwnd , &ps);
		// for (x = 0 ; x < 100 ; x += 10) {
		// 	MoveToEx(hdc , x , 0 , NULL);
		// 	LineTo(hdc , x , 100);
		// }
		// BitBlt(hdc , 300 , 0 , 100 , 100 , hdc , 0 , 0 , SRCCOPY);
		// EndPaint(hwnd , &ps);
		return 0;
	}
	return DefWindowProc(hwnd , msg , wp , lp);
}

int WINAPI WinMain(HINSTANCE hInstance , HINSTANCE hPrevInstance ,
			PSTR lpCmdLine , int nCmdShow )
{
	HWND hwnd;
	MSG msg;
	WNDCLASS winc;
    char g_encryption_key[16];

	winc.style		= CS_HREDRAW | CS_VREDRAW;
	winc.lpfnWndProc	= WndProc;
	winc.cbClsExtra	= winc.cbWndExtra	= 0;
	winc.hInstance		= hInstance;
	winc.hIcon		= LoadIcon(NULL , IDI_APPLICATION);
	winc.hCursor		= LoadCursor(NULL , IDC_ARROW);
	winc.hbrBackground	= (HBRUSH)GetStockObject(WHITE_BRUSH);
	winc.lpszMenuName	= NULL;
	winc.lpszClassName	= TEXT("server");
	if (!RegisterClass(&winc)) return -1;
	hwnd = CreateWindow(
			TEXT("server") , TEXT("remotetransfer server") ,
			WS_OVERLAPPEDWINDOW /*| WS_VISIBLE*/,
			CW_USEDEFAULT , CW_USEDEFAULT ,
			CW_USEDEFAULT , CW_USEDEFAULT ,
			NULL , NULL ,
			hInstance , NULL
	);
	if (hwnd == NULL) return -1;

    OPENFILENAME ofn;
    char szFile[MAX_PATH] = "";
    char szPath[MAX_PATH] = "";
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = TEXT("Applications(*.exe)\0*.exe\0")
                      TEXT("All files(*.*)\0*.*\0\0");
    ofn.lpstrFile = szPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFileTitle = szFile;
    ofn.nMaxFileTitle = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;

    // GetOpenFileName(&ofn);
    strcpy(szFile, "notepad.exe"); // Specify Notepad.exe for debug
    
    //プロセス名からウィンドウハンドルの取得
    HWND targetHwnd = getWindowHandleFileName(szFile);
    // printf("Window handle: %x\n", targetHwnd);
    SetForegroundWindow(targetHwnd);

    LOG_INFO("Target process: %s\n", szFile);

    strcpy(g_encryption_key, "1234");
	WSADATA wsadata;
	WSAStartup(MAKEWORD(1,1),&wsadata);
    SOCKET listensock, udpListenSock;
    // SOCKADDR_IN addr;
    if (startListen(&listensock) != 0 ) {
        LOG_ERROR("Socket error\n");
    }

    // UDP bind
    /*
    if (startListenUDP(&udpListenSock) != 0) {
        LOG_ERROR("Udp socket error");
    }
    */

    // fileWriteReadTest();
    // printf("Listen socket created:%d\n", listensock);
    SOCKET serverSocket[3];
    // HANDLE threadHandle[3];
    pthread_t pthread[3];
    Addrs addrs;
    addrs.hwnd = hwnd;
    addrs.targetHwnd = targetHwnd;

    /*
    for(int i=0;i<3;i++){
        serverSocket[i] = socket(AF_INET, SOCK_STREAM, 0);
        // printf("Server socket[%d] created:listen%d, server%d\n", i, listensock, serverSocket[i]);
        while(1){
            int ret = acceptLoop(&listensock, &serverSocket[i]);
            if ( ret != -1 ) break;
        }
        addrs.socket = &serverSocket[i];
        switch(i) {
            case 0:
                // threadHandle[i] = CreateThread(0,0,runOperateThread,(LPVOID)&addrs,0,0);
                if (pthread_create(&pthread[i], NULL, runOperateThread, &addrs) != 0 ){
                    LOG_DEBUG("Error occured to operate thread run.\n");
                }
                break;
            case 1:
                // threadHandle[i] = CreateThread(0,0,runImageThread,(LPVOID)&addrs,0,0);
                if (pthread_create(&pthread[i], NULL, runImageThread, &addrs) != 0 ){
                    LOG_DEBUG("Error occured to image thread run.\n");
                }
                break;
            case 2:
                // threadHandle[i] = CreateThread(0,0,runSoundThread,(LPVOID)&addrs,0,0);
                //if (pthread_create(&pthread[i], NULL, runSoundThread, &addrs) != 0 ){
                //    LOG_DEBUG("Error occured to sound thread run.\n");
                //}
                break;
        }
    }
    */

    // operate thread
    serverSocket[0] = socket(AF_INET, SOCK_STREAM, 0);
    int ret = acceptLoop(&listensock, &serverSocket[0]);
    if (ret==-1) {
        LOG_ERROR("sock0 accept error\n");
    }
    addrs.socket = &serverSocket[0];
    if (pthread_create(&pthread[0], NULL, runOperateThread, &addrs) != 0 ){
        LOG_DEBUG("Error occured to operate thread run.\n");
    }

    // image thread

    startListenUDP(&serverSocket[1]);
    //ret = acceptLoop(&listensock, &serverSocket[1]);
    //if (ret==-1) {
    //    LOG_ERROR("sock1 accept error\n");
    //}
    addrs.socket = &serverSocket[1];
    if (pthread_create(&pthread[1], NULL, runImageThreadUDP, &addrs) != 0 ){
        LOG_DEBUG("Error occured to image thread run.\n");
    }

    /*
    // sound thread
    serverSocket[2] = socket(AF_INET, SOCK_STREAM, 0);
    ret = acceptLoop(&listensock, &serverSocket[2]);
    if (ret==-1) {
        LOG_ERROR("sock2 accept error\n");
    }
    
    addrs.socket = &serverSocket[2];
    if (pthread_create(&pthread[2], NULL, runSoundThread, &addrs) != 0 ){
        LOG_DEBUG("Error occured to sound thread run.\n");
    }
    */

	while(GetMessage(&msg , NULL , 0 , 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
    
	return msg.wParam;
}