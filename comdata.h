typedef struct													//ヘッダー（256byte）
{
	char					data_type;							//1:通常データ
	char					thread;								//1:操作系,2:画像系,3:音声系

	char					___filler_1[1];

	char					sound_type;							//0:音声なし,1:音声あり
	char					encryption;							//0:非暗号化通信

	char					___filler_2[3];

	long					data_size;							//データサイズ
	long					original_size;						//圧縮前のサイズ

	// char					___filler_3[4];

	char					check_digit_enc[16];				//チェックディジット（ハッシュ化）

	//32

	long					check_digit;						//チェックディジット
	char					ver[4];								//通信バージョン
	long					samplerate;							//サンプルレート
	long					image_cx;							//画像幅
	long					image_cy;							//画像高さ
	long					server_cx;							//サーバー側画面幅
	long					server_cy;							//サーバー側画面高さ
	long					control;							//1:操作

	//64

	char					mouse_move;							//0:マウス静止,1:マウス動作

	char					___filler_4[1];

	char					mouse_x1;							//マウス戻るボタン（1:ダウン,2:アップ,3:ダブルクリック）
	char					mouse_x2;							//マウス進むボタン（1:ダウン,2:アップ,3:ダブルクリック）
	long					mouse_x;							//マウス座標Ｘ軸
	long					mouse_y;							//マウス座標Ｙ軸
	char					mouse_left;							//マウス左ボタン（1:ダウン,2:アップ,3:ダブルクリック）
	char					mouse_right;						//マウス右ボタン（1:ダウン,2:アップ,3:ダブルクリック）
	char					mouse_middle;						//マウス中ボタン（1:ダウン,2:アップ,3:ダブルクリック）
	char					mouse_wheel;						//マウスホイール移動量
	char					keycode;							//キーコード
	char					keycode_flg;						//キーフラグ（0x00:KeyUp,0x80:KeyDown）

	char					___filler_6[2];

	char					monitor_no;							//モニター番号
	char					monitor_count;						//モニター数

	char					___filler_7[6];

	long					sound_capture;						//0:DirectSound,1:CoreAudio

	//96

	char					___filler_8[40];

	long					keydown;							//キー押下（1:押下あり,0:押下なし）
	long					video_quality;						//画質（1:最低画質,3:低画質,5:標準画質,7:高画質,9:最高画質）

	//144

	long					mouse_cursor;						//マウスカーソル表示（0:自動,1:表示固定,2:非表示固定）

	char					___filler_9[20];

	long					gamepad1;							//ゲームパッド（Xpos）
	long					gamepad2;							//ゲームパッド（Ypos）
	long					gamepad3;							//ゲームパッド（Zpos）
	long					gamepad4;							//ゲームパッド（Rpos）

	long					client_scroll_x;					//スクロール位置Ｘ軸
	long					client_scroll_y;					//スクロール位置Ｙ軸

	//192

	char					___filler_10[24];

	double					zoom;								//拡大率（1.0:等倍）

	//224

	char					___filler_11[4];

	long					mode;								//5:パブリックモード
	long					sound_quality;						//音質（1:最低音質,2:低音質,3:標準音質,4:高音質,5:最高音質）

	char					___filler_12[4];

	long					gamepad5;							//ゲームパッド（Pov）
	long					gamepad6;							//ゲームパッド（Buttons）
	long					gamepad7;							//ゲームパッド（Upos）
	long					gamepad8;							//ゲームパッド（Vpos）

	//256

} COM_DATA;
