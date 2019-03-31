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
#include "comdata.h"
#include "wavestruct.h"
#include "public.h"
#include "vjoyinterface.h"
#include "turbojpeg.h"
#include "userlog.h"

#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "winmm.lib")

char g_encryption_key[16];

int dumpBinary(unsigned char* data, int size)
{
    int flag = 0;
    int i=0, j=0;

    while (flag==0) {
        for (j=0; j<8; j++) {
            LOG_INFO("%02x ", *(data+(i*8+j)));
            if ((i*8+j)>=size){
                flag = 1;
                break;
            }
        }
        LOG_INFO("\n");
        i++;
    }
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

int showBitmapInfo(BITMAP bitmap) {

    /* BITMAP structure
    typedef struct tagBITMAP {
    int bmType;  
    int bmWidth;  
    int bmHeight;  
    int bmWidthBytes;  
    BYTE bmPlanes;  
    BYTE bmBitsPixel;  
    LPVOID bmBits;
    } BITMAP;
    */

    /* BITMAPFILEHEADER
    typedef struct tagBITMAPFILEHEADER { // bmfh 
        WORD    bfType;             // =="BM"==0x424D
        DWORD   bfSize;             // ファイル全体のサイズ(バイト単位)
        WORD    bfReserved1;        // ==0（予約済）
        WORD    bfReserved2;        // ==0（予約済）
        DWORD   bfOffBits;          // ピクセルビットのオフセット
    } BITMAPFILEHEADER;
    */

    /* BITMAPINFOHEADER
    typedef struct tagBITMAPINFOHEADER{ // bmih 
        DWORD  biSize;              // 構造体サイズ==40
        LONG   biWidth;             // 画像横幅
        LONG   biHeight;            // 画像高さ
        WORD   biPlanes;            // ==1(Windows初期の名残）
        WORD   biBitCount;          // カラービット数(1, 4, 8, 16, 24, 32)
        DWORD  biCompression;       // 圧縮コード(BI_RGB, BI_RLE4, BI_RLE8, BI_BITFIELDSのいずれか)
        DWORD  biSizeImage;         // イメージのバイト数
        LONG   biXPelsPerMeter;     // 水平解像度
        LONG   biYPelsPerMeter;     // 垂直解像度
        DWORD  biClrUsed;           // カラーテーブルの数
        DWORD  biClrImportant;      // 重要な色の数
    } BITMAPINFOHEADER;
    */

    /* BMP File size calclation memo
    DWORD BPP = bmpInfo.bmiHeader.biBitCount;
    DWORD bytesPerPixel = BPP / 8;
    DWORD lineSizeDW = bytesPerPixel * width;
    lineSizeDW = (int)ceil((double)lineSizeDW / (double)sizeof(DWORD));
    DWORD lineSize = lineSizeDW * sizeof(DWORD);
    DWORD imageSize = lineSize * height;
    dwLoadSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFO) + imageSize;
    */

    LOG_INFO("bfType=%x, width=%d, height=%d, bmWidthBytes=%d\n",
        bitmap.bmType, bitmap.bmWidth, bitmap.bmHeight, bitmap.bmWidthBytes);
    LOG_INFO("bmPlanes=%d, bmBitsPixel=%d, bmBits=%d\n",
        bitmap.bmPlanes, bitmap.bmBitsPixel, bitmap.bmBits);

    return 0;
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
    BOOL AxisX  = GetVJDAxisExist(iInterface, HID_USAGE_X);
    BOOL AxisY  = GetVJDAxisExist(iInterface, HID_USAGE_Y);
    BOOL AxisZ  = GetVJDAxisExist(iInterface, HID_USAGE_Z);
    BOOL AxisRX = GetVJDAxisExist(iInterface, HID_USAGE_RX);
    BOOL AxisRY = GetVJDAxisExist(iInterface, HID_USAGE_RY);
    // Get the number of buttons supported by this vJoy device
    int nButtons  = GetVJDButtonNumber(iInterface);
    int povNumber = GetVJDContPovNumber(iInterface);
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

struct Addrs {
        HWND hwnd;
        HWND targetHwnd;
        SOCKET* socket;
};

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

HWND GetWindowHandleByPID(const DWORD targetPID){
    HWND hWnd = GetTopWindow(NULL);
    do {
        if (GetWindowLong( hWnd, GWL_HWNDPARENT) != 0 || !IsWindowVisible( hWnd)) {
            continue;
        }
        DWORD getPID;
        GetWindowThreadProcessId( hWnd, &getPID);
        if (targetPID == getPID) {
            return hWnd;
        }
    } while((hWnd = GetNextWindow( hWnd, GW_HWNDNEXT)) != NULL);
     
    return NULL;
}

HWND GetWindowHandleByName(const char exeName[]) {
    DWORD allProc[1024];
    DWORD cbNeeded;
    int nProc;
    int i;
    // PID一覧を取得
    if (!EnumProcesses(allProc, sizeof(allProc), &cbNeeded)) {
        return NULL;
    }
     
    nProc = cbNeeded / sizeof(DWORD);
    for (i = 0; i < nProc; i++) {
        char procName[MAX_PATH] = TEXT("<unknown>");
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
                                      PROCESS_VM_READ,
                                      FALSE, allProc[i]);
         
        // プロセス名を取得
        if (NULL != hProcess) {
            HMODULE hMod;
            DWORD cbNeeded;
            if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), 
                                   &cbNeeded)) {
                GetModuleBaseName(hProcess, hMod, procName, 
                                  sizeof(procName)/sizeof(TCHAR));
            }
        }
        if (!lstrcmpi(procName, exeName)) {
            return GetWindowHandleByPID(allProc[i]);
        }
        CloseHandle(hProcess);
    }
    return NULL;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT  num = 0;          // number of image encoders
    UINT  size = 0;         // size of the image encoder array in bytes

    Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

    Gdiplus::GetImageEncodersSize(&num, &size);
    if(size == 0)
        return -1;  // Failure

    pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if(pImageCodecInfo == NULL)
        return -1;  // Failure

    GetImageEncoders(num, size, pImageCodecInfo);

    for(UINT j = 0; j < num; ++j)
    {
        if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // Success
        }    
    }

    free(pImageCodecInfo);
    return -1;  // Failure
}

int writeFile(unsigned char* data, long dataSize, char* fileName) {
    FILE *fp;
    long ret;

    fp = fopen(fileName, "wb");
    if ( fp == NULL ) {
        LOG_ERROR("Can't open file %s.\n", fileName);
        return -1;
    } else {
        ret = fwrite(data, dataSize, 1, fp);
        if ( ret < 1) {
            LOG_ERROR("Can't write file %s\n", fileName);
            fclose(fp);
            return -1;
        }
        fclose(fp);
        LOG_INFO("File %s written, %d files.\n", fileName, ret);
    }

    return 0;
}

int bmp2jpg(BYTE *bmpData, int bmpSize, int width, int height, BYTE** jpegBuf, unsigned long *jpegSize) {
    tjhandle _jpegCompressor = tjInitCompress();
    int inSubsamp, outSubsamp = TJSAMP_444, inColorspace, pixelFormat = TJPF_BGR;
    int outQual = 95, flags=0;
    tjhandle tjInstance = NULL;
    unsigned char *imgBuf = NULL;
    FILE *bmpFile, *jpegFile;
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

byte* readSoundData(long* dataSize){
    const char*        FileName = "musicbox2.wav";// 再生したいWaveファイル
    byte* waveData;
    
    WAVEFORMATEX        WaveFormat;                 // Waveフォーマット
    // byte*               WaveData;                   // 音の生データ
    // int                 DataSize;                   // データサイズ
    // バイナリ読み込みモードで開く
    FILE* fp;
    if (!(fp = fopen(FileName, "rb"))) {
        LOG_ERROR("Failed to open sound file.\n");
        return NULL;
    }

    char chunkId[5] = {};
    char tmp[5] = {};
    unsigned int chunkSize = 0;

    // RIFFチャンク読み込み
    fread(chunkId, sizeof(char) * 4, 1, fp);
    fread(&chunkSize, sizeof(unsigned int), 1, fp);
    fread(tmp, sizeof(char) * 4, 1, fp);
    if (strcmp(chunkId, "RIFF") || strcmp(tmp, "WAVE")){
        LOG_ERROR("Specified file is not wave file.\n");
        return NULL;  // WAVファイルじゃない
    }

    // 子チャンク読み込み
    bool fmtchunk   = false;
    bool datachunk  = false;
    while (true)
    {
        fread(chunkId, sizeof(char) * 4, 1, fp);
        fread(&chunkSize, sizeof(unsigned int), 1, fp);
        if (!strcmp(chunkId, "fmt "))
        {
            if (chunkSize >= sizeof(WAVEFORMATEX))
            {
                fread(&WaveFormat, sizeof(WAVEFORMATEX), 1, fp);
                int diff = chunkSize - sizeof(WAVEFORMATEX);
                fseek(fp, diff, SEEK_CUR);
            }
            else
            {
                memset(&WaveFormat, 0, sizeof(WAVEFORMATEX));
                fread(&WaveFormat, chunkSize, 1, fp);
            }
            fmtchunk = true;
        }
        else if (!strcmp(chunkId, "data"))
        {
            // データサイズ確保
            *dataSize = chunkSize;
            // WaveData = new byte[chunkSize];
            waveData = (byte*)malloc(chunkSize);
            // データ読み込み
            if (fread(waveData, sizeof(byte), chunkSize, fp) != chunkSize)
            {
                LOG_ERROR("File broken\n");
                fclose(fp);
                return NULL;  // ファイルが壊れている
            }
            datachunk = true;
        }
        else
        {
            fseek(fp, chunkSize, SEEK_CUR);
        }

        if (fmtchunk && datachunk)
            break;

        // ファイルサイズチェックも行なった方がいいかも
    }
    
    fclose(fp);
    LOG_INFO("In readSoundData  wave data: %x, size: %d\n", waveData, *dataSize);
    LOG_INFO("Sample rate: %d\n.", WaveFormat.nSamplesPerSec);

    return waveData;
}

//送信処理
long send_data_enc(SOCKET *p_sock,char *p_buf,long p_size){
    char key[16 + 1];
    memset(&key,'@',sizeof(key));
    key[16] = 0x00;

    for (long sx1 = 0;sx1 < 16;sx1 ++)
    {
        if (g_encryption_key[sx1] == 0x00)
        {
            break;
        }

        key[sx1] = g_encryption_key[sx1];
    }

    COM_DATA *com_data = (COM_DATA*)p_buf;
    long data_long = com_data->data_type + (com_data->data_size & 0x0000ffff);

    com_data->encryption = 0;
    com_data->check_digit = ~data_long;

    for (long sx1 = 0;sx1 < 16;sx1 ++)
    {
        char key_char = ~key[sx1];
        key_char += (char)(sx1 * ~com_data->check_digit);
        com_data->check_digit_enc[sx1] = key_char;
    }

    return send(*p_sock,p_buf,p_size,0);
}

//送信処理
long send_data(SOCKET *p_sock,char *p_buf,long p_size){
    char key[16 + 1];
    memset(&key,'@',sizeof(key));
    key[16] = 0x00;

    for (long sx1 = 0;sx1 < 16;sx1 ++)
    {
        if (g_encryption_key[sx1] == 0x00)
        {
            break;
        }

        key[sx1] = g_encryption_key[sx1];
    }

    COM_DATA *com_data = (COM_DATA*)p_buf;
    long data_long = com_data->data_type + (com_data->data_size & 0x0000ffff);

    com_data->encryption = 0;
    com_data->check_digit = ~data_long;

    for (long sx1 = 0;sx1 < 16;sx1 ++)
    {
        char key_char = ~key[sx1];
        key_char += (char)(sx1 * ~com_data->check_digit);
        com_data->check_digit_enc[sx1] = key_char;
    }

    return send(*p_sock,p_buf,p_size,0);
}

//受信処理
long recv_data(SOCKET *p_sock,char *p_buf,long p_size){
    long size = 0;

    for (;size < p_size;)
    {
        long ret = recv(*p_sock,p_buf + size,p_size - size,0);

        if (ret > 0)
        {
            size += ret;
        }
        else
        {
            size = -1;

            break;
        }
    }

    return size;
}

int startListen_old(SOCKET acceptSoc, SOCKADDR_IN addr){
    // SOCKET acceptSoc;
    acceptSoc = socket(AF_INET, SOCK_STREAM, 0);

    if( acceptSoc == INVALID_SOCKET){
        LOG_ERROR("Failed to create socket.\n");
        LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        return -1;
    } else {
        // ソケットのアドレス情報記入
        addr.sin_family = AF_INET;
        addr.sin_port = htons(55500);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        // ソケットをアドレスにバインドする
        if( bind(acceptSoc, (LPSOCKADDR)&addr, sizeof(addr) ) != SOCKET_ERROR){
            // 接続を聴取する
            if( listen(acceptSoc, 3) != SOCKET_ERROR){
                LOG_ERROR("Wait for client...(sock:%d)\n", acceptSoc);
                return 0;
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
}

int startListen(SOCKET* listenSock, SOCKADDR_IN addr){
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

int acceptLoop(SOCKET *listenSocket, SOCKET *serverSocket) {
    const int RECVSIZE = 256;
    SOCKET g_sock1, clientSoc;
    g_sock1 = socket(AF_INET,SOCK_STREAM,0);
    SOCKADDR_IN addr;
    SOCKADDR_IN clientAddr;
    IN_ADDR clientIn;
    int nClientAddrLen;

    // printf("Accept loop started!\n");
    g_sock1 = -1;

    // printf("クライアントの接続待ち・・・\n");
    // クライアントのアドレス構造体の大きさを設定する
    nClientAddrLen = sizeof(clientAddr);

    // printf("Socket state -> listen:%d, server:%d\n", *listenSocket, *serverSocket);

    // 接続の受容
    *serverSocket = accept(*listenSocket, (LPSOCKADDR)&clientAddr, &nClientAddrLen);
    if(*serverSocket != INVALID_SOCKET){
        memcpy(&clientIn, &clientAddr.sin_addr.s_addr, 4);
        // printf("Connected. Client IP:%s\n", inet_ntoa(clientIn));
    }
    else{
        LOG_ERROR("Connect error.\n");
        LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        return -1;
    }
    return 0;
}

/* This function is for VJoy 1.2
DWORD WINAPI runOperateThread_vjoy1_2(LPVOID param) {
    COM_DATA com_data;
    Addrs addrs = *(Addrs*)param;
    SOCKET *serverSocket = addrs.socket;
    int mouse_x=0, mouse_y=0;
    int mouse_left=0, mouse_right=0, mouse_middle=0, mouse_wheel=0;

    HINSTANCE hinst;
    JOYSTICK_STATE joyState[2] = {0};
    int (*fp_VJoy_Initialize)(char*, char*);
    int (*fp_VJoy_UpdateJoyState)(int,  JOYSTICK_STATE*);
    int (*fp_VJoy_Shutdown)();

    BOOL c;
    printf("started.\n");
    if ((hinst = LoadLibrary("VJoy32.dll")) == NULL) {
        printf("Error LoadLibrary\n");
    }
    fp_VJoy_Initialize = (int (*)(char*, char*))GetProcAddress(hinst, "VJoy_Initialize");
    fp_VJoy_UpdateJoyState = (int (*)(int, JOYSTICK_STATE*))GetProcAddress(hinst, "VJoy_UpdateJoyState");
    fp_VJoy_Shutdown = (int (*)())GetProcAddress(hinst, "VJoy_Shutdown");
    
    // printf("call VJoy_Initialize.\n");
    if(!fp_VJoy_Initialize("vJoy driver", "Image trans")) {
        printf("Error vJoy initialize.\n");
    }

    while(1) {
        Sleep(1000);

        if (recv(*serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            printf("ThreadOperate receive error.\n");
            printf("Error %d occured.\n", WSAGetLastError());
        }

        printf("x:%d, y:%d, z:%d, r:%d, u:%d, v:%d\n",
            com_data.gamepad1, com_data.gamepad2, com_data.gamepad3,
            com_data.gamepad4, com_data.gamepad7, com_data.gamepad8 );
        printf("pov:%d, buttons:%x\n", com_data.gamepad5, com_data.gamepad6);
        
        joyState[0].XAxis = com_data.gamepad1;
        joyState[0].YAxis = com_data.gamepad2;
        joyState[0].ZAxis = com_data.gamepad3;
        joyState[0].Buttons = com_data.gamepad6;
        joyState[0].POV = com_data.gamepad5;
        joyState[0].XRotation = com_data.gamepad4;
        joyState[0].YRotation = com_data.gamepad7;
        joyState[0].ZRotation = com_data.gamepad8;
        
        if(!fp_VJoy_UpdateJoyState(1, &joyState[0])){
            printf("Error on VJoy command injection.\n");
        }

        if ( com_data.mouse_x != mouse_x || com_data.mouse_y != mouse_y ) {
            mouse_x = com_data.mouse_x;
            mouse_y = com_data.mouse_y;
            INPUT input = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_MOVE };
            SendInput(1, &input, sizeof(INPUT));
        } 

        if (com_data.mouse_left != mouse_left ) {
            mouse_left = com_data.mouse_left;
            if ( mouse_left == 1 ) {
                INPUT input = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_LEFTDOWN };
                SendInput(1, &input, sizeof(INPUT));
            } else if ( mouse_left == 2) {
                INPUT input = { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_LEFTUP };
                SendInput(1, &input, sizeof(INPUT));
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
            INPUT input = { INPUT_KEYBOARD, com_data.keycode,
                MapVirtualKey(com_data.keycode, 0),
                KEYEVENTF_EXTENDEDKEY };
            if (com_data.keycode_flg == 0x80) {
                input.ki.dwFlags |= KEYEVENTF_KEYUP;
            }
            SendInput(1, &input, sizeof(INPUT));
        }

        if (send(*serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            printf("ThreadOperate send error.\n");
            printf("Error %d occured.\n", WSAGetLastError());
        }
    }

    fp_VJoy_Shutdown();
    FreeLibrary(hinst);
    ExitThread(TRUE);
    return TRUE;
}
*/

DWORD WINAPI runOperateThread(LPVOID param) {
    COM_DATA com_data;
    Addrs addrs = *(Addrs*)param;
    SOCKET *serverSocket = addrs.socket;
    HWND targetHwnd = addrs.targetHwnd;
    int mouse_x=0, mouse_y=0, old_mouse_x=0, old_mouse_y=0;
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
        printf("x:%d, y:%d, z:%d, r:%d, u:%d, v:%d\n",
            com_data.gamepad1, com_data.gamepad2, com_data.gamepad3,
            com_data.gamepad4, com_data.gamepad7, com_data.gamepad8 );
        printf("pov:%d, buttons:%x\n", com_data.gamepad5, com_data.gamepad6);
        */

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
    ExitThread(TRUE);
    return TRUE;
}

DWORD WINAPI runImageThread(LPVOID param) {

    COM_DATA com_data;
    DWORD dwLoadSize;
    RECT rc;
    Addrs addrs = *(Addrs*)param;
    SOCKET *serverSocket = addrs.socket;
    HWND targetHwnd = addrs.targetHwnd;
    BITMAP bitmap;
    BITMAPFILEHEADER bitmapFileHeader;
    BITMAPINFO bmpInfo;
    LPDWORD lpPixel;
    const int offset = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    DWORD lineSizeDW, imageSize;
    HDC targetHdc, hdc;
    HBITMAP hBitmap;
    BOOL succeedBitBlt;
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

    while(1) {
        Sleep(10);
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

        dwLoadSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageSize;
        //LOG_INFO("File size:%d = %d + %d + %d\n", dwLoadSize,
        //    sizeof(BITMAPFILEHEADER), sizeof(BITMAPINFOHEADER), imageSize);
        bitmapFileHeader.bfSize = offset + imageSize;
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
        succeedBitBlt = BitBlt(hdc, 0, 0, width, height, targetHdc, rc.left, rc.top, SRCCOPY);
        bmpData = (BYTE*)malloc(sizeof(bitmapFileHeader) + sizeof(bmpInfo.bmiHeader) + imageSize);
        memcpy(bmpData,&bitmapFileHeader,sizeof(bitmapFileHeader));
        memcpy(bmpData+sizeof(bitmapFileHeader),&bmpInfo.bmiHeader,sizeof(bmpInfo.bmiHeader));
        memcpy(bmpData+sizeof(bitmapFileHeader)+sizeof(bmpInfo.bmiHeader), lpPixel, imageSize);

        BYTE* jpegData;
        unsigned long jpegSize;
        // LOG_INFO("Image data size: %d bytes.\n", dwLoadSize);
        // LOG_DEBUG("Convert image to jpeg.\n");
        
        bmp2jpg(bmpData, dwLoadSize, width, height, &jpegData, &jpegSize);

        //com_data.data_size = dwLoadSize;
        com_data.data_size = jpegSize;
        // LOG_DEBUG("Sending image header data.\n");
        if (send(*serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            LOG_ERROR("ThreadImage send error(header).\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        }

        //if (send(*serverSocket,(char*)bmpData,dwLoadSize,0) < 0)
        // LOG_DEBUG("Sending image data.\n");
        if (send(*serverSocket,(char*)jpegData,jpegSize,0) < 0)
        {
            LOG_ERROR("ThreadImage send error(image).\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        }
        ReleaseDC(targetHwnd, targetHdc);
        DeleteDC(hdc);
        DeleteObject(hBitmap);
        free(bmpData);
    }
    /* メモリブロックのロック解除 */
    //GlobalUnlock(hBuf);
    ExitThread(TRUE);
    return TRUE;
}

DWORD WINAPI runSoundThread_alt(LPVOID param) {

    COM_DATA com_data;
    Addrs addrs = *(Addrs*)param;
    long samplerate = com_data.samplerate;
    long sound_size = com_data.data_size;
    SOCKET *serverSocket = addrs.socket;
    byte* waveData;
    long dataSize;

    waveData = readSoundData(&dataSize);


    // CoInitialize(NULL);
    // DirectSoundCaptureCreate8( NULL, &captureDevice, NULL );

    // DirectSoundEnumerate((LPDSENUMCALLBACK)DSEnumProc, (VOID*)NULL);

    LOG_INFO("Sound thread started.\n");

    while(1){

        Sleep(10000);

        
        com_data.data_size = dataSize;
        com_data.samplerate = SAMPLERATE;

        if (send(*serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            LOG_ERROR("ThreadSound send error(header).\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        }

        if (send(*serverSocket, (char*)waveData, dataSize, 0) < 0)
        {
            LOG_ERROR("ThreadSound send error(sound).\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
            //printf("wavedata %x, size:%d\n", waveData, dataSize);
        }
    }

    CoUninitialize();

    ExitThread(TRUE);
    return TRUE;
}

DWORD WINAPI runSoundThread(LPVOID param) {

    COM_DATA com_data;
    Addrs addrs = *(Addrs*)param;
    long samplerate = com_data.samplerate;
    long sound_size = com_data.data_size;
    SOCKET *serverSocket = addrs.socket;
    byte* waveData;
    long dataSize;

    LPDIRECTSOUNDCAPTURE captureDevice = NULL;//DirectSoundCaptureDeviceオブジェクト
    LPDIRECTSOUNDCAPTUREBUFFER captureBuffer = NULL;//DirectSoundCaptureBufferオブジェクト

    WAVEFORMATEX wfx = {WAVE_FORMAT_PCM, CHANNELS, SAMPLERATE,
        SAMPLERATE*CHANNELS*BITSPERSAMPLE/8, CHANNELS*BITSPERSAMPLE/8, BITSPERSAMPLE, 0};
    DSCBUFFERDESC bufferDescriber
        = {sizeof(DSCBUFFERDESC), 0, wfx.nAvgBytesPerSec*1, 0, &wfx, 0, NULL};
    DWORD readablePos, capturedPos, readBufferPos, lockLength, capturedLength, wrappedCapturedLength;
    DWORD copiedLength = 0;
    void *capturedData = NULL, *wrappedCapturedData = NULL;
    char *copiedBuffer;
    int recordDurationSec = 3; //録音時間(秒)
    HRESULT Hret;
    time_t start, end;

    CoInitialize(NULL);
    DirectSoundCaptureCreate8( NULL, &captureDevice, NULL );

    DirectSoundEnumerate((LPDSENUMCALLBACK)DSEnumProc, (VOID*)NULL);

    readBufferPos = 0;
    copiedBuffer = (char*)malloc(
        wfx.nAvgBytesPerSec * wfx.nChannels * wfx.wBitsPerSample / 8 * recordDurationSec * 2); 
    captureDevice->CreateCaptureBuffer(&bufferDescriber,&captureBuffer,NULL);
    start = time(NULL);
    captureBuffer->Start(DSCBSTART_LOOPING);

    LOG_INFO("Sound thread started.\n");

    while(1){

        Sleep(10);

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
            LOG_ERROR("Lock error:%x\n", Hret);
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

        Hret = captureBuffer->Unlock( capturedData, capturedLength,
            wrappedCapturedData, wrappedCapturedLength);
        
        com_data.data_size = dataSize;
        com_data.samplerate = SAMPLERATE;

        if (send(*serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            LOG_ERROR("ThreadSound send error(header).\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
        }

        if (send(*serverSocket, (char*)copiedBuffer, dataSize, 0) < 0)
        {
            LOG_ERROR("ThreadSound send error(sound).\n");
            LOG_ERROR("Error %d occured.\n", WSAGetLastError());
            // printf("wavedata %x, size:%d\n", waveData, dataSize);
        }
    }

    captureBuffer->Stop();
    free(copiedBuffer);
    CoUninitialize();

    ExitThread(TRUE);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hwnd , UINT msg , WPARAM wp , LPARAM lp) {
	HDC hdc;
	PAINTSTRUCT ps;
	int x;

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

    //GetOpenFileName(&ofn);
    strcpy(szFile, "notepad.exe"); // Spqcify Notepad.exe for debug
    
    //プロセス名からウィンドウハンドルの取得
    HWND targetHwnd = getWindowHandleFileName(szFile);
    // printf("Window handle: %x\n", targetHwnd);
    SetForegroundWindow(targetHwnd);

    LOG_INFO("Target process: %s\n", szFile);

    strcpy(g_encryption_key, "1234");
	WSADATA wsadata;
	WSAStartup(MAKEWORD(1,1),&wsadata);
    SOCKET listensock;
    SOCKADDR_IN addr;
    if (startListen(&listensock, addr) != 0 ) {
        LOG_ERROR("Socket error\n");
    }
    // printf("Listen socket created:%d\n", listensock);
    SOCKET serverSocket[3];
    HANDLE threadHandle[3];
    struct Addrs addrs;
    addrs.hwnd = hwnd;
    addrs.targetHwnd = targetHwnd;
    for(int i=0;i<3;i++){
        serverSocket[i] = socket(AF_INET, SOCK_STREAM, 0);
        // printf("Server socket[%d] created:listen%d, server%d\n", i, listensock, serverSocket[i]);
        while(1){
            acceptLoop(&listensock, &serverSocket[i]);
            if ( serverSocket[i] != -1 ) break;
        }
        addrs.socket = &serverSocket[i];
        switch(i) {
            case 0:
                threadHandle[i] = CreateThread(0,0,runOperateThread,(LPVOID)&addrs,0,0);
                break;
            case 1:
                threadHandle[i] = CreateThread(0,0,runImageThread,(LPVOID)&addrs,0,0);
                break;
            case 2:
                threadHandle[i] = CreateThread(0,0,runSoundThread,(LPVOID)&addrs,0,0);
                //threadHandle[i] = CreateThread(0,0,runSoundThread_alt,(LPVOID)&addrs,0,0);
                break;
        }
    }
	while(GetMessage(&msg , NULL , 0 , 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}