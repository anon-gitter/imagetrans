#include <stdio.h>
#include <windows.h>
#include <winsock.h>
#include <dsound.h>
#include "comdata.h" 

#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "winmm.lib")

char g_encryption_key[16];

struct Addrs {
        HWND hwnd;
        SOCKET* socket;
};

byte* readSoundData(long* dataSize)
{
    const char*        FileName = "musicbox3.wav";// 再生したいWaveファイル
    byte* waveData;
    
    WAVEFORMATEX        WaveFormat;                 // Waveフォーマット
    // byte*               WaveData;                   // 音の生データ
    // int                 DataSize;                   // データサイズ
    // バイナリ読み込みモードで開く
    FILE* fp;
    if (!(fp = fopen(FileName, "rb"))) {
        printf("Failed to open sound file.\n");
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
        printf("Specified file is not wave file.\n");
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
                printf("File broken\n");
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
    printf("In readSoundData  wave data: %x, size: %d\n", waveData, *dataSize);
    printf("Sample rate: %d\n.", WaveFormat.nSamplesPerSec);

    return waveData;
}

//送信処理
long send_data_enc(SOCKET *p_sock,char *p_buf,long p_size)
{
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
long send_data(SOCKET *p_sock,char *p_buf,long p_size)
{
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
long recv_data(SOCKET *p_sock,char *p_buf,long p_size)
{
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

int startListen(SOCKET acceptSoc, SOCKADDR_IN addr)
{
    // SOCKET acceptSoc;
    acceptSoc = socket(AF_INET, SOCK_STREAM, 0);

    if( acceptSoc == INVALID_SOCKET){
        printf("Failed to create socket.\n");
        printf("Error %d occured.\n", WSAGetLastError());
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
                printf("Wait for client...(sock:%d)\n", acceptSoc);
                return 0;
            }
            else{
                printf("Listen error.\n");
                printf("Error %d occured.\n", WSAGetLastError());
                return -1;
            }
        }
        else{
            printf("Bind error\n");
            printf("Error %d occured.\n", WSAGetLastError());
            return -1;
        }
    }
}

int acceptLoop(SOCKET *listenSocket, SOCKET *serverSocket) {
    const int RECVSIZE = 256;
    SOCKET g_sock1, clientSoc;
    g_sock1 = socket(AF_INET,SOCK_STREAM,0);
    SOCKADDR_IN addr;
    SOCKADDR_IN clientAddr;
    IN_ADDR clientIn;
    int nClientAddrLen;

    printf("Accept loop started!\n");
    g_sock1 = -1;

    // printf("クライアントの接続待ち・・・\n");
    // クライアントのアドレス構造体の大きさを設定する
    nClientAddrLen = sizeof(clientAddr);

    printf("Socket state -> listen:%d, server:%d\n", *listenSocket, *serverSocket);

    // 接続の受容
    *serverSocket = accept(*listenSocket, (LPSOCKADDR)&clientAddr, &nClientAddrLen);
    if(*serverSocket != INVALID_SOCKET){
        memcpy(&clientIn, &clientAddr.sin_addr.s_addr, 4);
        printf("Connected. Client IP:%s\n", inet_ntoa(clientIn));
    }
    else{
        printf("Connect error.\n");
        printf("Error %d occured.\n", WSAGetLastError());
        return -1;
    }
    return 0;
}

DWORD WINAPI runOperateThread(LPVOID serverSocket) {
    COM_DATA com_data;

    while(1) {
        Sleep(10);

        if (recv(*(SOCKET*)serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            printf("ThreadOperate receive error.\n");
            printf("Error %d occured.\n", WSAGetLastError());
        }

        if (send(*(SOCKET*)serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            printf("ThreadOperate send error.\n");
            printf("Error %d occured.\n", WSAGetLastError());
        }
    }
    ExitThread(TRUE);
    return TRUE;
}

DWORD WINAPI runImageThread(LPVOID serverSocket) {

    COM_DATA com_data;

    char lpszPath[256];
    DWORD dwLoadSize;
    strcpy(lpszPath, "test1.jpg");

    /* 画像ファイルオープン */
    HANDLE hFile = CreateFile(lpszPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    /* ファイルサイズ取得 */
    DWORD dwFileSize = GetFileSize(hFile, NULL);
    /* 画像ファイルデータ格納用メモリブロック確保 */
    HGLOBAL hBuf = GlobalAlloc(GMEM_MOVEABLE, dwFileSize);
    /* メモリブロックをロックしアドレスを取得 */
    char *lpBuf = (char*)GlobalLock(hBuf);
    /* 画像ファイルのデータをメモリブロックに読み込む */

    printf("File name:%s, size:%d, addr:%x.\n", lpszPath, dwFileSize, lpBuf);
    if (ReadFile(hFile, lpBuf, dwFileSize, &dwLoadSize, NULL) == 0) {
        printf("ReadFile error: %d\n", GetLastError());
    }
    CloseHandle(hFile);
   
    while(1) {
        Sleep(10);

        com_data.data_size = dwLoadSize;
        if (send(*(SOCKET*)serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            printf("ThreadImage send error(header).\n");
            printf("Error %d occured.\n", WSAGetLastError());
        }

        if (send(*(SOCKET*)serverSocket,lpBuf,dwFileSize,0) < 0)
        {
            printf("ThreadImage send error(image).\n");
            printf("Error %d occured.\n", WSAGetLastError());
        }
    }
    /* メモリブロックのロック解除 */
    GlobalUnlock(hBuf);
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

    LPDIRECTSOUND lpds; 
    HRESULT hr = DirectSoundCreate(NULL, &lpds, NULL);
    hr = lpds->SetCooperativeLevel(addrs.hwnd,DSSCL_NORMAL);

    waveData = readSoundData(&dataSize);

    if ( waveData == NULL ) {
        printf("Sound data read error.\n" );
    } else {
        printf("Sound file read, at %x, size: %d\n",waveData, dataSize);
    }

    while(1){
        Sleep(100);

        com_data.data_size = dataSize;
        com_data.samplerate = 44100;
        // com_data.data_size = dwLoadSize;
        if (send(*serverSocket,(char*)&com_data,sizeof(COM_DATA),0) < 0)
        {
            printf("ThreadSound send error(header).\n");
            printf("Error %d occured.\n", WSAGetLastError());
        }

        if (send(*serverSocket,(char*)waveData,dataSize,0) < 0)
        {
            printf("ThreadSound send error(sound).\n");
            printf("Error %d occured.\n", WSAGetLastError());
            printf("wavedata %x, size:%d\n", waveData, dataSize);
        }    
    }

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
			WS_OVERLAPPEDWINDOW | WS_VISIBLE ,
			CW_USEDEFAULT , CW_USEDEFAULT ,
			CW_USEDEFAULT , CW_USEDEFAULT ,
			NULL , NULL ,
			hInstance , NULL
	);

	if (hwnd == NULL) return -1;

    strcpy(g_encryption_key, "1234");

	WSADATA wsadata;
	WSAStartup(MAKEWORD(1,1),&wsadata);

    SOCKET listensock;
    SOCKADDR_IN addr;

    listensock = socket(AF_INET, SOCK_STREAM, 0);

    if( listensock == INVALID_SOCKET){
        printf("Failed to create socket.\n");
        printf("Error %d occured.\n", WSAGetLastError());
    } else {
        // ソケットのアドレス情報記入
        addr.sin_family = AF_INET;
        addr.sin_port = htons(55500);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        // ソケットをアドレスにバインドする
        if( bind(listensock, (LPSOCKADDR)&addr, sizeof(addr) ) != SOCKET_ERROR){
            // 接続を聴取する
            if( listen(listensock, 3) != SOCKET_ERROR){
                printf("Wait for client...(sock:%d)\n", listensock);
            }
            else{
                printf("Listen error.\n");
                printf("Error %d occured.\n", WSAGetLastError());
            }
        }
        else{
            printf("Bind error\n");
            printf("Error %d occured.\n", WSAGetLastError());
        }
    }

    // if (startListen(listensock, addr) != 0){
    //    printf("Listen error.");
    // }
    
    printf("Listen socket created:%d\n", listensock);

    SOCKET serverSocket[3];

    HANDLE threadHandle[3];

    struct Addrs addrs;

    for(int i=0;i<3;i++){
        serverSocket[i] = socket(AF_INET, SOCK_STREAM, 0);
        printf("Server socket[%d] created:listen%d, server%d\n", i, listensock, serverSocket[i]);
        while(1){
            acceptLoop(&listensock, &serverSocket[i]);
            if ( serverSocket[i] != -1 ) break;
        }

        switch(i) {
            case 0:
                threadHandle[i] = CreateThread(0,0,runOperateThread,(LPVOID)&serverSocket[i],0,0);
                break;
            case 1:
                threadHandle[i] = CreateThread(0,0,runImageThread,(LPVOID)&serverSocket[i],0,0);
                break;
            case 2:
                addrs.hwnd = hwnd;
                addrs.socket = &serverSocket[i];
                threadHandle[i] = CreateThread(0,0,runSoundThread,(LPVOID)&addrs,0,0);
                break;
        }
        
    }

	while(GetMessage(&msg , NULL , 0 , 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}