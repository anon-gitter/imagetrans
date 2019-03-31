
#include					<stdio.h>
#include					<winsock.h>
#include					<olectl.h>
#include					<audioclient.h>
#include					"dsound.h"
#include					"comdata.h"
#include					"userlog.h"

#pragma						comment(lib,"wsock32")
#pragma						comment(lib,"dsound")
#pragma						comment(lib,"imm32")
#pragma						comment(lib,"winmm.lib")


//global constant


	char						*g_connect_ver;						//通信バージョン
	char						*g_ip;								//IP
	unsigned short				g_port;								//Port
	char						*g_encryption_key;					//パスワード（最大16byte）
	SOCKET						g_sock1;							//ソケット（操作系）
	SOCKET						g_sock2;							//ソケット（画像系）
	SOCKET						g_sock3;							//ソケット（音声系）
	long						g_sock1_ct;							//操作系接続カウント
	long						g_sock2_ct;							//画像系接続カウント
	struct sockaddr_in			g_addr;

	long						g_end;								//終了フラグ
	HWND						g_hwnd;								//ウインドウハンドル
	long						g_window_cx;						//ウインドウ幅
	long						g_window_cy;						//ウインドウ高さ
	long						g_capture_cx;						//キャプチャー幅
	long						g_capture_cy;						//キャプチャー高さ
	long						g_mouse_x;							//マウス座標Ｘ軸
	long						g_mouse_y;							//マウス座標Ｙ軸
	char						g_mouse_left[256];					//マウスボタン（左）
	char						g_mouse_right[256];					//マウスボタン（右）
	char						g_mouse_middle[256];				//マウスボタン（中）
	char						g_mouse_wheel[256];					//マウスホイール
	char						g_mouse_x1[256];					//マウスボタン（戻る）
	char						g_mouse_x2[256];					//マウスボタン（進む）
	char						g_keyboard1[256];					//キーボード
	char						g_keyboard2[256];					//キーボード
	long						g_shift_down;						//SHIFTキー
	long						g_alt_down;							//ALTキー
	long						g_ctrl_down;						//CTRLキー

	LPDIRECTSOUND				g_ds;
	LPDIRECTSOUNDBUFFER			g_dsb;
	DSBUFFERDESC				g_ds_desc;
	WAVEFORMATEXTENSIBLE		*g_ca_format;
	char						*g_ds_sound_buf;
	long						g_ds_sound_pt;

//音声処理初期化
bool ds_init(long p_samplerate)
{
	DirectSoundCreate(0,&g_ds,0);

	if (g_ds == 0)
	{
		return false;
	}

	g_ds->SetCooperativeLevel(GetDesktopWindow(),DSSCL_NORMAL);

	g_ca_format = (WAVEFORMATEXTENSIBLE*)malloc(sizeof(WAVEFORMATEXTENSIBLE));

	memset(g_ca_format,0,sizeof(WAVEFORMATEXTENSIBLE));

	g_ca_format->Format.wFormatTag = WAVE_FORMAT_PCM;
	g_ca_format->Format.wBitsPerSample = 16;
	g_ca_format->Format.nSamplesPerSec = p_samplerate;
	g_ca_format->Format.nChannels = 2;
	g_ca_format->Format.nBlockAlign = g_ca_format->Format.nChannels * (g_ca_format->Format.wBitsPerSample / 8);
	g_ca_format->Format.nAvgBytesPerSec = g_ca_format->Format.nBlockAlign * g_ca_format->Format.nSamplesPerSec;

	memset(&g_ds_desc,0,sizeof(DSBUFFERDESC));

	g_ds_desc.dwSize = sizeof(DSBUFFERDESC);
	g_ds_desc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_LOCSOFTWARE | DSBCAPS_CTRLFREQUENCY;
	g_ds_desc.dwBufferBytes = g_ca_format->Format.nAvgBytesPerSec * 3;
	g_ds_desc.lpwfxFormat = &g_ca_format->Format;

	g_ds->CreateSoundBuffer(&g_ds_desc,&g_dsb,0);

    return true;
}

//音声処理解放
void ds_exit()
{
	if (g_dsb != 0)
	{
		g_dsb->Stop();
		g_dsb->Release();
		g_dsb = 0;
	}

	if (g_ds != 0)
	{
		g_ds->Release();
		g_ds = 0;
	}

	if (g_ca_format != 0)
	{
		free(g_ca_format);
		g_ca_format = 0;
	}
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

//マウス操作
void mouse_send(char p_type,char p_value)
{
	for (long sx1 = 0;sx1 < 256;sx1 ++)
	{
		if (p_type == 0)
		{
			if (g_mouse_left[sx1] == 0)
			{
				g_mouse_left[sx1] = p_value;

				break;
			}
		}

		if (p_type == 1)
		{
			if (g_mouse_right[sx1] == 0)
			{
				g_mouse_right[sx1] = p_value;

				break;
			}
		}

		if (p_type == 2)
		{
			if (g_mouse_middle[sx1] == 0)
			{
				g_mouse_middle[sx1] = p_value;

				break;
			}
		}

		if (p_type == 3)
		{
			if (g_mouse_wheel[sx1] == 0)
			{
				g_mouse_wheel[sx1] = p_value;

				break;
			}
		}

		if (p_type == 4)
		{
			if (g_mouse_x1[sx1] == 0)
			{
				g_mouse_x1[sx1] = p_value;

				break;
			}
		}

		if (p_type == 5)
		{
			if (g_mouse_x2[sx1] == 0)
			{
				g_mouse_x2[sx1] = p_value;

				break;
			}
		}
	}
}

//キーボード操作
void keyboard_send(char p_type,char p_value)
{
	if (p_value == VK_OEM_AUTO ||
		p_value == VK_OEM_ENLW)
	{
		p_value = VK_HANJA;
	}

	for (long sx1 = 0;sx1 < 256;sx1 ++)
	{
		if (g_keyboard1[sx1] == 0 &&
			g_keyboard2[sx1] == 0)
		{
			g_keyboard1[sx1] = p_value;
			g_keyboard2[sx1] = p_type;

			break;
		}
	}
}

//操作系スレッド
DWORD WINAPI thread1(LPVOID p_arg)
{
	COM_DATA com_data;
	long mouse_x_old = -1;
	long mouse_y_old = -1;
	POINT mousePos;
	WINDOWINFO windowInfo;
	windowInfo.cbSize = sizeof(WINDOWINFO);

	//GetWindowInfo((HWND)p_arg, &windowInfo);
	//printf("left:%d, right:%d, top:%d, bottom:%d\n", windowInfo.rcWindow.left,
	//	windowInfo.rcWindow.right, windowInfo.rcWindow.top, windowInfo.rcWindow.bottom);

	// printf("Thread1 started\n");

	for (;g_end == 0;)
	{
		Sleep(10);
		//操作系
		if (g_sock1 == -1)
		{
			g_sock1 = socket(AF_INET,SOCK_STREAM,0);
			g_sock1_ct = 0;
			if (connect(g_sock1,(struct sockaddr *)&g_addr,sizeof(sockaddr_in)) == SOCKET_ERROR)
			{
				LOG_ERROR("connect error.\n");
				LOG_ERROR("addr:%d, port:%d\n", (unsigned long)ntohl(g_addr.sin_addr.s_addr), g_addr.sin_port);
				//接続エラー
				closesocket(g_sock1);
				g_sock1 = -1;
				closesocket(g_sock2);
				g_sock2 = -1;
				closesocket(g_sock3);
				g_sock3 = -1;
				Sleep(1000);
				continue;
			}
		}
		memset(&com_data,0,sizeof(COM_DATA));
		memcpy(&com_data.ver,g_connect_ver,4);
		com_data.data_type = 1;
		com_data.thread = 1;
		com_data.mode = 5;
		com_data.monitor_no = 1;

		//操作系
		if (!GetWindowInfo((HWND)p_arg, &windowInfo)) {
			LOG_ERROR("GetWindowInfo error:%d\n", GetLastError());
		}
		com_data.control = 1;
		com_data.mouse_cursor = 0;

		if (mouse_x_old == -1 && 
			mouse_y_old == -1)
		{
			mouse_x_old = g_mouse_x;
			mouse_y_old = g_mouse_y;
		}

		GetCursorPos(&mousePos);
		g_mouse_x = mousePos.x;
		g_mouse_y = mousePos.y;

		if ((mouse_x_old != g_mouse_x || 
			mouse_y_old != g_mouse_y) &&
			(windowInfo.rcClient.left < g_mouse_x &&
			g_mouse_x < windowInfo.rcClient.right &&
			windowInfo.rcClient.top < g_mouse_y &&
			g_mouse_y < windowInfo.rcClient.bottom))
		{
			mouse_x_old = g_mouse_x;
			mouse_y_old = g_mouse_y;

			com_data.mouse_move = 1;

			//long x = (long)((double)(g_mouse_x - windowInfo.rcWindow.left) * ((double)g_capture_cx / (double)g_window_cx));
			//long y = (long)((double)(g_mouse_y - windowInfo.rcWindow.top) * ((double)g_capture_cy / (double)g_window_cy));
			long x = g_mouse_x - windowInfo.rcClient.left;
			long y = g_mouse_y - windowInfo.rcClient.top;

			com_data.mouse_x = x;
			com_data.mouse_y = y;
			//printf("send mouse move <abs> x:%d y:%d <relative> x:%d, y:%d, clienttop:%d\n",
			//	g_mouse_x, g_mouse_y, x, y, windowInfo.rcClient.top);
		}

		if (g_mouse_left[0] != 0)
		{
			com_data.mouse_left = g_mouse_left[0];
		}
		if (com_data.mouse_left != 0)
		{
			for (long sx1 = 0;sx1 < 255;sx1 ++)
			{
				g_mouse_left[sx1] = g_mouse_left[sx1 + 1];
			}
			g_mouse_left[255] = 0;
		}
		if (g_mouse_right[0] != 0)
		{
			com_data.mouse_right = g_mouse_right[0];
		}
		if (com_data.mouse_right != 0)
		{
			for (long sx1 = 0;sx1 < 255;sx1 ++)
			{
				g_mouse_right[sx1] = g_mouse_right[sx1 + 1];
			}
			g_mouse_right[255] = 0;
		}
		if (g_mouse_middle[0] != 0)
		{
			com_data.mouse_middle = g_mouse_middle[0];
		}
		if (com_data.mouse_middle != 0)
		{
			for (long sx1 = 0;sx1 < 255;sx1 ++)
			{
				g_mouse_middle[sx1] = g_mouse_middle[sx1 + 1];
			}
			g_mouse_middle[255] = 0;
		}
		if (g_mouse_wheel[0] != 0)
		{
			com_data.mouse_wheel = g_mouse_wheel[0];
		}
		if (com_data.mouse_wheel != 0)
		{
			for (long sx1 = 0;sx1 < 255;sx1 ++)
			{
				g_mouse_wheel[sx1] = g_mouse_wheel[sx1 + 1];
			}
			g_mouse_wheel[255] = 0;
		}
		if (g_mouse_x1[0] != 0)
		{
			com_data.mouse_x1 = g_mouse_x1[0];
		}
		if (com_data.mouse_x1 != 0)
		{
			for (long sx1 = 0;sx1 < 255;sx1 ++)
			{
				g_mouse_x1[sx1] = g_mouse_x1[sx1 + 1];
			}
			g_mouse_x1[255] = 0;
		}
		if (g_mouse_x2[0] != 0)
		{
			com_data.mouse_x2 = g_mouse_x2[0];
		}
		if (com_data.mouse_x2 != 0)
		{
			for (long sx1 = 0;sx1 < 255;sx1 ++)
			{
				g_mouse_x2[sx1] = g_mouse_x2[sx1 + 1];
			}
			g_mouse_x2[255] = 0;
		}
		if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
		{
			if (g_shift_down != 1)
			{
				keyboard_send((char)0x80,VK_SHIFT);
			}
			g_shift_down = 1;
		}
		else
		{
			if (g_shift_down != 0)
			{
				keyboard_send((char)0x00,VK_SHIFT);
			}
			g_shift_down = 0;
		}
		if ((GetKeyState(VK_MENU) & 0x8000) != 0)
		{
			if (g_alt_down != 1)
			{
				keyboard_send((char)0x80,VK_MENU);
			}
			g_alt_down = 1;
		}
		else
		{
			if (g_alt_down != 0)
			{
				keyboard_send((char)0x00,VK_MENU);
			}
			g_alt_down = 0;
		}
		if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
		{
			if (g_ctrl_down != 1)
			{
				keyboard_send((char)0x80,VK_CONTROL);
			}
			g_ctrl_down = 1;
		}
		else
		{
			if (g_ctrl_down != 0)
			{
				keyboard_send((char)0x00,VK_CONTROL);
			}
			g_ctrl_down = 0;
		}
		if (g_shift_down != 0 ||
			g_alt_down != 0 ||
			g_ctrl_down != 0)
		{
			com_data.keydown = 1;
		}
		com_data.keycode = g_keyboard1[0];
		com_data.keycode_flg = g_keyboard2[0];

		if (com_data.keycode != 0 || 
			com_data.keycode_flg != 0)
		{
			com_data.keydown = 1;

			for (long sx1 = 0;sx1 < 255;sx1 ++)
			{
				g_keyboard1[sx1] = g_keyboard1[sx1 + 1];
				g_keyboard2[sx1] = g_keyboard2[sx1 + 1];
			}

			g_keyboard1[255] = 0;
			g_keyboard2[255] = 0;
		}

		JOYINFOEX gamepad_btn;
		gamepad_btn.dwFlags = JOY_RETURNALL;
		gamepad_btn.dwSize = sizeof(JOYINFOEX);
		if (::joyGetPosEx(JOYSTICKID1,&gamepad_btn) == JOYERR_NOERROR)
		{
			com_data.gamepad1 = gamepad_btn.dwXpos;
			com_data.gamepad2 = gamepad_btn.dwYpos;
			com_data.gamepad3 = gamepad_btn.dwZpos;
			com_data.gamepad4 = gamepad_btn.dwRpos;
			com_data.gamepad5 = gamepad_btn.dwPOV;
			com_data.gamepad6 = gamepad_btn.dwButtons;
			com_data.gamepad7 = gamepad_btn.dwUpos;
			com_data.gamepad8 = gamepad_btn.dwVpos;
		}
		else
		{
			// printf("joystick capture error.\n");
			com_data.gamepad1 = 0x7fff;
			com_data.gamepad2 = 0x7fff;
			com_data.gamepad3 = 0x7fff;
			com_data.gamepad4 = 0x7fff;
			com_data.gamepad5 = 0xffff;
			com_data.gamepad6 = 0x0000;
			com_data.gamepad7 = 0x0000;
			com_data.gamepad8 = 0x0000;
		}

		//画像系
		com_data.zoom = 1.0;
		com_data.image_cx = g_capture_cx;
		com_data.image_cy = g_capture_cy;
		com_data.client_scroll_x = 0;
		com_data.client_scroll_y = 0;
		com_data.video_quality = 3;

		//音声系
		com_data.sound_type = 1;
		com_data.sound_capture = 1;
		com_data.sound_quality = 3;

		//ヘッダー送信
		//printf("Thread1 sending.\n");
		if (send_data(&g_sock1,(char*)&com_data,sizeof(COM_DATA)) < 0)
		{
			//printf("Thread1 send error.\n");
			closesocket(g_sock1);
			g_sock1 = -1;

			closesocket(g_sock2);
			g_sock2 = -1;

			closesocket(g_sock3);
			g_sock3 = -1;

			continue;
		}

		//ヘッダー受信
		//printf("Thread1 receive wait.\n");
		if (recv_data(&g_sock1,(char*)&com_data,sizeof(COM_DATA)) < 0)
		{
			//printf("Thread1 receive error.\n");
			closesocket(g_sock1);
			g_sock1 = -1;

			closesocket(g_sock2);
			g_sock2 = -1;

			closesocket(g_sock3);
			g_sock3 = -1;

			continue;
		}

		if (com_data.mode == 0)
		{
			//パスワードエラー
			LOG_ERROR("Password error.\n");
			continue;
		}

		if (g_sock1_ct < 999)
		{
			g_sock1_ct ++;
		}
	}

	ExitThread(TRUE);

	return 0;
}

//画像系スレッド
DWORD WINAPI thread2(LPVOID p_arg)
{
	COM_DATA com_data;
	WINDOWINFO windowInfo;
	int windowWidth, windowHeight, oldWidth=0, oldHeight=0;
	HWND hWnd = (HWND)p_arg;

	for (;g_end == 0;)
	{
		Sleep(10);
		//画像系
		if (g_sock1 == -1 || g_sock1_ct < 5)
		{
			continue;
		}
		if (g_sock2 == -1)
		{
			g_sock2 = socket(AF_INET,SOCK_STREAM,0);
			g_sock2_ct = 0;
			if (connect(g_sock2,(struct sockaddr *)&g_addr,sizeof(sockaddr_in)) == SOCKET_ERROR)
			{
				//接続エラー
				LOG_ERROR("Thread2 connect error.\n");
				closesocket(g_sock2);
				g_sock2 = -1;
				continue;
			}
		}
		//ヘッダー受信
		//printf("Thread2 receive wait(image header).\n");
		if (recv_data(&g_sock2,(char *)&com_data,sizeof(COM_DATA)) < 0)
		{
			LOG_ERROR("Thread2 receive error(image header).\n");
			LOG_ERROR("Error %d occured.\n", WSAGetLastError());
			closesocket(g_sock1);
			g_sock1 = -1;
			closesocket(g_sock2);
			g_sock2 = -1;
			closesocket(g_sock3);
			g_sock3 = -1;
			continue;
		}

		long image_size = com_data.data_size;
		char *image_buf = (char *)malloc(image_size);
		//本体受信
		//printf("Thread2 receive wait(image data).\n");
		if (recv_data(&g_sock2,(char *)image_buf,image_size) < 0)
		{
			LOG_ERROR("Thread2 receive error(image data).\n");
			LOG_ERROR("Error %d occured.\n", WSAGetLastError());
			free(image_buf);
			closesocket(g_sock1);
			g_sock1 = -1;
			closesocket(g_sock2);
			g_sock2 = -1;
			closesocket(g_sock3);
			g_sock3 = -1;
			continue;
		}

		//ウィンドウサイズの変更
		GetWindowInfo(hWnd, &windowInfo);
		windowWidth = ( windowInfo.rcWindow.right - windowInfo.rcWindow.left )
					 - ( windowInfo.rcClient.right - windowInfo.rcClient.left )
					 + com_data.server_cx;
		windowHeight = ( windowInfo.rcWindow.bottom - windowInfo.rcWindow.top )
					 - ( windowInfo.rcClient.bottom - windowInfo.rcClient.top )
					 + com_data.server_cy;
		if (windowWidth != oldWidth || windowHeight != oldHeight){
			SetWindowPos(hWnd, NULL, 0, 0, windowWidth, windowHeight, SWP_NOMOVE | SWP_NOZORDER);
			oldWidth = windowWidth;
			oldHeight = windowHeight;
			g_window_cx = com_data.server_cx;
			g_window_cy = com_data.server_cy;
			GetWindowInfo(hWnd, &windowInfo);
			// LOG_INFO("Client area size: %d, %d\n", windowInfo.rcClient.right - windowInfo.rcClient.left,
			//		windowInfo.rcClient.bottom - windowInfo.rcClient.top);
		}

		//描画
		IPicture *pic;
		IStream *str;
		long cx, cy;
		HDC hdc = GetDC(g_hwnd);
		HGLOBAL hgbl = GlobalAlloc(GPTR,image_size);
		memcpy(hgbl,image_buf,image_size);
		CreateStreamOnHGlobal(hgbl,TRUE,&str);
		OleLoadPicture(str,image_size,TRUE,IID_IPicture,(LPVOID*)&pic);
		pic->get_Width(&cx);
		pic->get_Height(&cy);
		pic->Render(hdc,0,0,g_window_cx,g_window_cy,0,cy,cx,-cy,0);
		pic->Release();
		str->Release();
		GlobalUnlock(hgbl);
		ReleaseDC(g_hwnd,hdc);
		free(image_buf);
		if (g_sock2_ct < 999)
		{
			g_sock2_ct ++;
		}
	}
	ExitThread(TRUE);
	return 0;
}

//音声系スレッド
DWORD WINAPI thread3(LPVOID p_arg)
{
	COM_DATA com_data;

	// printf("Thread3 started\n");

	for (;g_end == 0;)
	{
		Sleep(10);
		//音声系
		if (g_sock2 == -1 || g_sock2_ct < 5)
		{
			continue;
		}
		if (g_sock3 == -1)
		{
			g_sock3 = socket(AF_INET,SOCK_STREAM,0);

			if (connect(g_sock3,(struct sockaddr *)&g_addr,sizeof(sockaddr_in)) == SOCKET_ERROR)
			{
				//接続エラー
 				LOG_ERROR("Thread3 connect error.\n");
				closesocket(g_sock3);
				g_sock3 = -1;

				continue;
			}
			g_ds = 0;
			g_dsb = 0;
			g_ca_format = 0;
		}

		//ヘッダー受信
		// printf("Thread3 receive wait(sound header).\n");
		if (recv_data(&g_sock3,(char *)&com_data,sizeof(COM_DATA)) < 0)
		{
			LOG_ERROR("Thread3 receive error(sound header).\n");
			closesocket(g_sock3);
			g_sock3 = -1;
			ds_exit();
			continue;
		}

		long samplerate = com_data.samplerate;
		long sound_size = com_data.data_size;
		char *sound_buf = (char *)malloc(sound_size);
		//本体受信
		// printf("Thread3 receive wait(sound data).\n");
		if (recv_data(&g_sock3,(char *)sound_buf,sound_size) < 0)
		{
			LOG_ERROR("Thread3 receive error(sound data).\n");
			closesocket(g_sock3);
			g_sock3 = -1;
			ds_exit();
			continue;
		}

		if (g_ds == 0)
		{
			ds_init(com_data.samplerate);
		}

		//再生
		if (g_dsb != 0)
		{
			LPVOID	lock_data;
			DWORD	lock_size;
			g_dsb->Lock(0,g_ca_format->Format.nAvgBytesPerSec * 3,&lock_data,&lock_size,0,0,0);
			if (g_ds_sound_pt + com_data.data_size <= long(g_ca_format->Format.nAvgBytesPerSec) * 3)
			{
				memcpy((char *)lock_data + g_ds_sound_pt,sound_buf,long(com_data.data_size));
				g_ds_sound_pt += long(com_data.data_size);
			}
			else
			{
				long pt = g_ca_format->Format.nAvgBytesPerSec * 3 - g_ds_sound_pt;
				memcpy((char *)lock_data + g_ds_sound_pt,sound_buf,pt);
				memcpy((char *)lock_data,sound_buf + pt,long(com_data.data_size) - pt);
				g_ds_sound_pt = long(com_data.data_size) - pt;
			}
			g_dsb->Unlock(lock_data,lock_size,0,0);
			DWORD status;
			g_dsb->GetStatus(&status);
			if (g_ds_sound_pt >= long(double(g_ca_format->Format.nAvgBytesPerSec) * 0.1))
			{
				if ((status & DSBSTATUS_PLAYING) == 0)
				{
					g_dsb->Play(0,0,DSBPLAY_LOOPING);
				}
			}
		}
		free(sound_buf);
	}
	ds_exit();
	ExitThread(TRUE);
	return 0;
}

//ウインドウプロシージャ
LRESULT CALLBACK proc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
	if (message == WM_SYSCOMMAND)
	{
		if ((wParam & 0xfff0) == SC_KEYMENU)
		{
			//MENUキー無効

			return false;
		}
	}

	if (message == WM_CLOSE)
	{
		//ウインドウクローズ

		g_end = 1;
	}

	if (g_end == -1)
	{
		if (message == WM_COMMAND)
		{
			if (LOWORD(wParam) == 65200)
			{
				//Okボタン

				g_end = 0;
			}
		}
	}

	if (g_end == 0)
	{
		//マウス系

		if (message == WM_MOUSEMOVE)
		{
			g_mouse_x = LOWORD(lParam);
			g_mouse_y = HIWORD(lParam);
		}

		if (message == WM_LBUTTONDOWN)
		{
			mouse_send(0,1);
		}

		if (message == WM_LBUTTONUP)
		{
			mouse_send(0,2);
		}

		if (message == WM_LBUTTONDBLCLK)
		{
			mouse_send(0,3);
		}

		if (message == WM_RBUTTONDOWN)
		{
			mouse_send(1,1);
		}

		if (message == WM_RBUTTONUP)
		{
			mouse_send(1,2);
		}

		if (message == WM_RBUTTONDBLCLK)
		{
			mouse_send(1,3);
		}

		if (message == WM_MBUTTONDOWN)
		{
			mouse_send(2,1);
		}

		if (message == WM_MBUTTONUP)
		{
			mouse_send(2,2);
		}

		if (message == WM_MBUTTONDBLCLK)
		{
			mouse_send(2,3);
		}

		if (message == WM_MOUSEWHEEL)
		{
			short delta = HIWORD(wParam);

			if (delta > 0)
			{
				if (delta / 120 > 9)
				{
					delta = 9 * 120;
				}

				mouse_send(3,delta / 10);
			}

			if (delta < 0)
			{
				if (delta / 120 < -9)
				{
					delta = -9 * 120;
				}

				mouse_send(3,delta / 10);
			}
		}

		if (message == WM_XBUTTONDOWN)
		{
			if ((GET_XBUTTON_WPARAM(wParam) & XBUTTON1) != 0)
			{
				mouse_send(4,1);
			}
			else
			{
				mouse_send(5,1);
			}
		}

		if (message == WM_XBUTTONUP)
		{
			if ((GET_XBUTTON_WPARAM(wParam) & XBUTTON1) != 0)
			{
				mouse_send(4,2);
			}
			else
			{
				mouse_send(5,2);
			}
		}

		if (message == WM_XBUTTONDBLCLK)
		{
			if ((GET_XBUTTON_WPARAM(wParam) & XBUTTON1) != 0)
			{
				mouse_send(4,3);
			}
			else
			{
				mouse_send(5,3);
			}
		}

		//キーボード系

		if (message == WM_KEYDOWN ||
			message == WM_SYSKEYDOWN)
		{
			if (GetFocus() != 0)
			{
				if (wParam != VK_SHIFT &&
					wParam != VK_MENU &&
					wParam != VK_CONTROL)
				{
					keyboard_send((char)0x80,wParam);
				}
			}
		}

		if (message == WM_KEYUP ||
			message == WM_SYSKEYUP)
		{
			if (GetFocus() != 0)
			{
				if (wParam != VK_SHIFT &&
					wParam != VK_MENU &&
					wParam != VK_CONTROL)
				{
					keyboard_send(0,wParam);
				}
			}
		}
	}

	return DefWindowProc(hWnd,message,wParam,lParam);
}

//塗り潰し
void fill_window(COLORREF p_rgb)
{
	RECT rect;
	HBRUSH brs;
	HDC hdc;

	GetClientRect(g_hwnd,&rect);
	brs = CreateSolidBrush(p_rgb);
	hdc = GetDC(g_hwnd);
	FillRect(hdc,&rect,brs);

	ReleaseDC(g_hwnd,hdc);
	DeleteObject(brs);
}

//メインルーチン
int WINAPI WinMain(HINSTANCE hInst,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{
	g_end = -1;

	g_ip = (char *)malloc(256);
	g_encryption_key = (char *)malloc(17);
	g_connect_ver = (char *)malloc(5);

	g_window_cx = 800;										//ウインドウ幅
	g_window_cy = 480;										//ウインドウ高さ

	g_capture_cx = 800;									//キャプチャー幅
	g_capture_cy = 480;									//キャプチャー高さ

	strcpy(g_connect_ver,"0000");							//通信バージョン

	g_sock1 = -1;
	g_sock2 = -1;
	g_sock3 = -1;

	g_sock1_ct = 0;
	g_sock2_ct = 0;

	WNDCLASSEX wc;
	memset(&wc,0,sizeof(WNDCLASSEX));
	wc.cbSize			= sizeof(wc);
	wc.lpszClassName	= "remotetransClient";
	wc.lpfnWndProc		= proc;
	wc.style			= CS_VREDRAW | CS_HREDRAW;
	wc.hCursor			= LoadCursor(0,IDC_ARROW);
	wc.hInstance		= hInst;
	RegisterClassEx(&wc);

	g_hwnd = CreateWindowEx(WS_EX_DLGMODALFRAME,wc.lpszClassName,wc.lpszClassName,WS_DLGFRAME | WS_SYSMENU,16,16,g_window_cx,g_window_cy + 28,0,0,hInst,0);

	HFONT font1 = CreateFont(13,0,0,0,FW_NORMAL,0,0,0,SHIFTJIS_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Tahoma");

	CreateWindowEx(0,"Static","ip",WS_VISIBLE | WS_CHILD | SS_CENTER,310,252,133,15,g_hwnd,(HMENU)65000,0,0);
	CreateWindowEx(0,"Static","port",WS_VISIBLE | WS_CHILD | SS_CENTER,444,252,66,15,g_hwnd,(HMENU)65001,0,0);
	CreateWindowEx(0,"Static","password",WS_VISIBLE | WS_CHILD | SS_CENTER,511,252,133,15,g_hwnd,(HMENU)65002,0,0);

	CreateWindowEx(WS_EX_CLIENTEDGE,"Edit","192.168.11.8",WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_CENTER | ES_AUTOHSCROLL,310,268,134,21,g_hwnd,(HMENU)65100,0,0);
	CreateWindowEx(WS_EX_CLIENTEDGE,"Edit","55500",WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_CENTER | ES_AUTOHSCROLL,444,268,67,21,g_hwnd,(HMENU)65101,0,0);
	CreateWindowEx(WS_EX_CLIENTEDGE,"Edit","1234",WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_CENTER | ES_AUTOHSCROLL | ES_PASSWORD,511,268,134,21,g_hwnd,(HMENU)65102,0,0);

	CreateWindowEx(0,"Button","Ok",WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,645,268,31,20,g_hwnd,(HMENU)65200,0,0);

	SendMessage(GetDlgItem(g_hwnd,65000),WM_SETFONT,(WPARAM)font1,0);
	SendMessage(GetDlgItem(g_hwnd,65001),WM_SETFONT,(WPARAM)font1,0);
	SendMessage(GetDlgItem(g_hwnd,65002),WM_SETFONT,(WPARAM)font1,0);
	SendMessage(GetDlgItem(g_hwnd,65100),WM_SETFONT,(WPARAM)font1,0);
	SendMessage(GetDlgItem(g_hwnd,65101),WM_SETFONT,(WPARAM)font1,0);
	SendMessage(GetDlgItem(g_hwnd,65102),WM_SETFONT,(WPARAM)font1,0);
	SendMessage(GetDlgItem(g_hwnd,65200),WM_SETFONT,(WPARAM)font1,0);

	ImmDisableIME(0);

	ShowWindow(g_hwnd,SW_SHOW);

	MSG msg;

	for (;;)
	{
		if (PeekMessage(&msg,0,0,0,PM_NOREMOVE))
		{
			if (GetMessage(&msg,0,0,0) > 0)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		Sleep(10);

		if (g_end != -1)
		{
			break;
		}
	}

	char data[256];

	memset(&data,0,sizeof(data));
	GetWindowText(GetDlgItem(g_hwnd,65100),data,256);
	//wcstombs(g_ip,data,256);
	//char destIP[] = "192.168.11.8";
	//printf("destIP:%s\n", destIP);
	strcpy(g_ip, data);
	//printf("IP addr g_ip:%x, data:%x, inet_addr: %x\n", *g_ip, *data, inet_addr(g_ip));

	memset(&data,0,sizeof(data));
	GetWindowText(GetDlgItem(g_hwnd,65101),data,6);
	// g_port = (unsigned short)_wtol(data);
	g_port = (unsigned short)atol(data);
	// printf("port: %d\n", g_port);

	memset(&data,0,sizeof(data));
	GetWindowText(GetDlgItem(g_hwnd,65102),data,17);
	// wcstombs(g_encryption_key,data,17);
	strcpy(g_encryption_key, data);

	memset(&g_addr,0,sizeof(sockaddr_in));
	g_addr.sin_family = AF_INET;
	g_addr.sin_addr.s_addr = inet_addr(g_ip);
	g_addr.sin_port = htons(g_port);

	WSADATA wsadata;
	WSAStartup(MAKEWORD(1,1),&wsadata);

	ShowWindow(GetDlgItem(g_hwnd,65000),SW_HIDE);
	ShowWindow(GetDlgItem(g_hwnd,65001),SW_HIDE);
	ShowWindow(GetDlgItem(g_hwnd,65002),SW_HIDE);
	ShowWindow(GetDlgItem(g_hwnd,65100),SW_HIDE);
	ShowWindow(GetDlgItem(g_hwnd,65101),SW_HIDE);
	ShowWindow(GetDlgItem(g_hwnd,65102),SW_HIDE);
	ShowWindow(GetDlgItem(g_hwnd,65200),SW_HIDE);

	fill_window(RGB(0,0,0));

	HANDLE hthd1 = 0;
	HANDLE hthd2 = 0;
	HANDLE hthd3 = 0;

	if (g_end == 0)
	{
		hthd1 = CreateThread(0,0,thread1,g_hwnd,0,0);
		hthd2 = CreateThread(0,0,thread2,g_hwnd,0,0);
		hthd3 = CreateThread(0,0,thread3,0,0,0);
	}

	for (;;)
	{
		if (PeekMessage(&msg,0,0,0,PM_NOREMOVE))
		{
			if (GetMessage(&msg,0,0,0) > 0)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		Sleep(10);

		if (g_end != 0)
		{
			if (g_sock1 != -1)
			{
				closesocket(g_sock1);
				g_sock1 = -1;
			}

			if (g_sock2 != -1)
			{
				closesocket(g_sock2);
				g_sock2 = -1;
			}

			if (g_sock3 != -1)
			{
				closesocket(g_sock3);
				g_sock3 = -1;
			}

			if (hthd1 != 0)
			{
				WaitForSingleObject(hthd1,3000);
				CloseHandle(hthd1);
				hthd1 = 0;
			}

			if (hthd2 != 0)
			{
				WaitForSingleObject(hthd2,3000);
				CloseHandle(hthd2);
				hthd2 = 0;
			}

			if (hthd3 != 0)
			{
				WaitForSingleObject(hthd3,3000);
				CloseHandle(hthd3);
				hthd3 = 0;
			}

			if (hthd1 == 0 &&
				hthd2 == 0 &&
				hthd3 == 0)
			{
				break;
			}
		}
	}

	free(g_ip);
	free(g_encryption_key);
	free(g_connect_ver);

	DeleteObject(font1);

	return 0;
}
