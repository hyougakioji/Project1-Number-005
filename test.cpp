#include <windows.h>

//Windowsの特殊フォルダを取得する
#include <shlobj_core.h> //Shlobj.hを含む

#include <iostream>
//#include <string> //iostream内で読んでいるが、保証されていない
#include <vector>
#include <filesystem>

//ウィンドウハンドル
HWND hwnd_holder_ = {};

//ウィンドウクラスの登録済みフラグ
bool window_class_registered_ = false;

//コンソール再描画フラグ
bool console_refresh = true;

//ビットマップ情報
HBITMAP    h_bitmap_   = NULL;
HDC        mem_dc_     = NULL;
HGDIOBJ    old_bitmap_ = NULL;
LPBYTE     lp_dib_buf_ = NULL;
BITMAPINFO bmp_info_   = {};


/*****************************
*** ビットマップを転送する ***
*****************************/
void dibBlt( HDC src_dc, int src_pos_x, int src_pos_y, int width, int height, HDC dest_dc, int dest_pos_x, int dest_pos_y )
{
	//単純転送
	::BitBlt( dest_dc, dest_pos_x, dest_pos_y, width, height, src_dc, src_pos_x, src_pos_y, SRCCOPY );
}

/***************************
*** ビットマップを埋める ***
***************************/
void dibFill( int pos_x, int pos_y, int width, int height, int image_width, int image_height, BYTE color_a, BYTE color_r, BYTE color_g, BYTE color_b )
{
	if ( h_bitmap_ == NULL || lp_dib_buf_ == NULL ) { return; } //無効なオブジェクトの場合は処理しない

	if ( pos_x <  0 || pos_y  <  0 ) { return; } //ゼロ未満の場合は処理しない
	if ( width <= 0 || height <= 0 ) { return; } //ゼロ以下の場合は処理しない

	//矩形サイズをループ
	for ( int32_t loop_y = 0 ; loop_y < height ; loop_y++ )
	for ( int32_t loop_x = 0 ; loop_x < width  ; loop_x++ )
	{
		//範囲内のみ処理する（実装に合わせて最適化すること）
		if ( ( loop_y + pos_y ) >= 0 && ( loop_y + pos_y ) < image_height )
		if ( ( loop_x + pos_x ) >= 0 && ( loop_x + pos_x ) < image_width  )
		{
			//ポインタ位置を計算（桁溢れに注意すること）
			size_t temp_x  = static_cast < size_t > ( loop_x + pos_x );
			size_t temp_y  = static_cast < size_t > ( loop_y + pos_y );
			size_t p_image = ( ( temp_y * static_cast < size_t > ( image_width ) ) + temp_x ) * 4;

			//値をメモリにコピー（リトルエンディアンのためBGRA）
			lp_dib_buf_[p_image]     = color_b;
			lp_dib_buf_[p_image + 1] = color_g;
			lp_dib_buf_[p_image + 2] = color_r;
			lp_dib_buf_[p_image + 3] = color_a; //単純転送ではあまり意味のない値
		}
	}
}

/*****************************
*** ビットマップを削除する ***
*****************************/
void deleteDibImage( )
{
	//ビットマップを元に戻す
	if ( mem_dc_ != NULL && old_bitmap_ != NULL )
	{
		::SelectObject( mem_dc_, old_bitmap_ );
		old_bitmap_ = NULL;
	}

	//デバイスコンテキスト削除
	if ( mem_dc_ != NULL )
	{
		::DeleteDC( mem_dc_ );
		mem_dc_   = NULL;
	}

	//ビットマップ削除
	if ( h_bitmap_ != NULL )
	{
		::DeleteObject( h_bitmap_ );

		h_bitmap_   = NULL;
		lp_dib_buf_ = NULL;
	}

	//ビットマップ情報をクリア
	bmp_info_ = {};
}

/*****************************
*** ビットマップを作成する ***
*****************************/
bool createDibImage( int width, int height )
{
	//ビットマップのサイズ制限（適切に制限すること）
	if ( width <=    0 || height <=    0 ) { return false; }
	if ( width >  4096 || height >  4096 ) { return false; }

	//作成済みのオブジェクトを削除する
	deleteDibImage( );

	//ビットマップ情報
	bmp_info_.bmiHeader.biSize          = sizeof( BITMAPINFOHEADER );
	bmp_info_.bmiHeader.biWidth         = width; //幅
	bmp_info_.bmiHeader.biHeight        = height * -1; //トップダウンビットマップ
	bmp_info_.bmiHeader.biPlanes        = 1; //非マルチプレーン
	bmp_info_.bmiHeader.biBitCount      = static_cast < WORD > ( 32 ); //4バイトサイズの場合、アライメント調整は不要
	bmp_info_.bmiHeader.biCompression   = BI_RGB; //RGB配列（非圧縮）
	bmp_info_.bmiHeader.biSizeImage     = 0;
	bmp_info_.bmiHeader.biXPelsPerMeter = 0;
	bmp_info_.bmiHeader.biYPelsPerMeter = 0;
	bmp_info_.bmiHeader.biClrUsed       = 0;
	bmp_info_.bmiHeader.biClrImportant  = 0;

	//ビットマップ作成
	h_bitmap_ = ::CreateDIBSection( NULL, &bmp_info_, DIB_RGB_COLORS, ( void ** ) &lp_dib_buf_, NULL, NULL );
	if ( h_bitmap_ == NULL ) { goto clean_up; }

	//デバイスコンテキストを作成（アプリケーションの現在の画面と互換性のあるメモリデバイスコンテキストを作成）
	mem_dc_ = ::CreateCompatibleDC( NULL );
	if ( mem_dc_ == NULL ) { goto clean_up; }

	//ビットマップをデバイスコンテキストに選択
	old_bitmap_ = ::SelectObject( mem_dc_, h_bitmap_ );

	if ( old_bitmap_ == HGDI_ERROR || old_bitmap_ == NULL )
	{
		old_bitmap_ = NULL;
		goto clean_up;
	}

	//処理成功
	return true;

//失敗時の処理
clean_up:

	//ビットマップを元に戻す
	if ( mem_dc_ != NULL && old_bitmap_ != NULL )
	{
		::SelectObject( mem_dc_, old_bitmap_ );
	}

	//デバイスコンテキスト削除
	if ( mem_dc_ != NULL )
	{
		::DeleteDC( mem_dc_ );

		mem_dc_ = NULL;
	}

	//ビットマップ削除
	if ( h_bitmap_ != NULL )
	{
		::DeleteObject( h_bitmap_ );

		h_bitmap_   = NULL;
		lp_dib_buf_ = NULL;
	}

	//ビットマップ情報をクリア
	bmp_info_ = {};

	//処理失敗
	return false;
}


/*******************************
*** コンソールに文字列を出力 ***
*******************************/
void writeConsoleW( std::wstring write_string )
{
	//標準出力のハンドルを取得
	HANDLE hCout = GetStdHandle( STD_OUTPUT_HANDLE );

	::WriteConsoleW
	(
		hCout,
		reinterpret_cast < const void * > ( write_string.c_str( ) ),
		static_cast      < DWORD >        ( write_string.size ( ) ),
		nullptr,
		NULL
	);
}

template < typename TypeName > void writeConsoleW( TypeName write_string, int length )
{
	//標準出力のハンドルを取得
	HANDLE hCout = ::GetStdHandle( STD_OUTPUT_HANDLE );

	::WriteConsoleW
	(
		hCout,
		reinterpret_cast < const void * > ( write_string ),
		static_cast      < DWORD >        ( length ),
		nullptr,
		NULL
	);
}


/***************************************
*** アプリケーションの実行パスを表示 ***
***************************************/
std::wstring getAppPath( )
{
	int32_t retry_count   = 5; //リトライ回数
	DWORD   buffer_length = MAX_PATH; //パスの最大文字列長

	std::wstring text_buffer_w;

	for ( int32_t loop_count = 0 ; loop_count < retry_count ; loop_count++ )
	{
		text_buffer_w.resize( buffer_length );
		
		DWORD result_value = ::GetModuleFileNameW( nullptr, text_buffer_w.data( ), buffer_length );
		LPWSTR;WCHAR;
		//エラー
		if ( result_value == 0 ) { break; }

		//バッファサイズ不足
		if ( result_value == buffer_length ) { buffer_length *= 2; continue; } //仕様通りならこちらが正しい
		//if ( result_value >= buffer_length ) { buffer_length *= 2; continue; } //イマイチ不安ならこちら

		//取得した文字数でリサイズ（終端のNull文字を含まない）
		text_buffer_w.resize( result_value );

		//フルパスを各要素に分解する
		std::filesystem::path full_path( text_buffer_w );

		//パスの各要素を表示
		writeConsoleW( std::wstring( L"\n実行ファイルのパス    ：" ) + full_path.wstring( ) );
		writeConsoleW( std::wstring( L"\n親ディレクトリ        ：" ) + full_path.parent_path( ).wstring( ) );
		writeConsoleW( std::wstring( L"\nファイル名            ：" ) + full_path.filename( ).wstring( ) );
		writeConsoleW( std::wstring( L"\nファイル名(拡張子なし)：" ) + full_path.stem( ).wstring( ) );
		writeConsoleW( std::wstring( L"\n拡張子のみ            ：" ) + full_path.extension( ).wstring( ) );

		return std::wstring( L"\n\n［処理成功］\n" );

		break;
	}

	return std::wstring( L"\n［処理失敗］\n" );
}


/***********************************************************************
*** 既知のフォルダ（システムフォルダやユーザーフォルダ）のパスを取得 ***
***********************************************************************/
std::wstring getKnownFolderPath( REFKNOWNFOLDERID folder_id )
{
	PWSTR text_buffer = nullptr;

	//パスを取得
	HRESULT hr = ::SHGetKnownFolderPath
	(
		folder_id,
		0,
		nullptr,
		& text_buffer
	);

	//エラー処理（エラー処理は実装に合わせて考慮する事）
	if ( FAILED( hr ) )
	{
		//テキストバッファを解放（仕様上必要なし、もし心配なら）
		//if ( text_buffer != nullptr ) { ::CoTaskMemFree( text_buffer ); }

		if      ( hr == E_FAIL       ) { return std::wstring( L"エラー：E_FAIL"        ); }
		else if ( hr == E_INVALIDARG ) { return std::wstring( L"エラー：E_INVALIDARG"  ); }
		else                           { return std::wstring( L"エラー：Unknown_Error" ); }
	}

	//NULLチェック（予期せぬエラー時の保険）
    if ( text_buffer == nullptr ) { return std::wstring( L"エラー：Null_Check_Error" ); }

	//テキストをコピー
	std::wstring text_buffer_w( text_buffer );

	//テキストバッファを解放
	::CoTaskMemFree( text_buffer );

	//フォルダパスを返す
	return text_buffer_w;
}

/***********************************
*** ユーザーフォルダのパスを表示 ***
***********************************/
void getUserFolderPath( )
{
	writeConsoleW( std::wstring( L"\nデスクトップ    ：" ) + getKnownFolderPath( FOLDERID_Desktop   ) );
	writeConsoleW( std::wstring( L"\nマイドキュメント：" ) + getKnownFolderPath( FOLDERID_Documents ) );
	writeConsoleW( std::wstring( L"\nピクチャ        ：" ) + getKnownFolderPath( FOLDERID_Pictures  ) );
}


/*******************************************************************************
*** コンソールのイベントハンドラ（処理の実装時はスレッド安全性に配慮する事） ***
*** ここでクリーンアップ処理を行う場合、慎重に実装する必要があります         ***
*******************************************************************************/
BOOL WINAPI consoleCallback( DWORD ctrl_type )
{
	switch ( ctrl_type )
	{
		//コンソールウィンドウを閉じる操作
		//コンソールを直接閉じると、基本的にアプリケーションは強制終了される
		case CTRL_CLOSE_EVENT: { break; }

		//Windowsのログオフ操作
		case CTRL_LOGOFF_EVENT: { break; }

		//Windowsのシャットダウン操作
		case CTRL_SHUTDOWN_EVENT:
		{
			//クリーンアップ処理（ファイルのクローズ、メモリ解放など）

			//TRUEを返した場合、イベントをアプリケーションが処理した事をOSに通知して、プロセスは終了する
			//return TRUE;

			break;
		}

		//Ctrl + C
		case CTRL_C_EVENT: { break; }

		//Ctrl + Break
		case CTRL_BREAK_EVENT: { break; }
	}

	//デフォルトの処理に進む
	return FALSE;
}


/***************************************************************
*** ウィンドウプロシージャ（ウィンドウのメッセージハンドラ） ***
***************************************************************/
LRESULT CALLBACK wndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
    {
		case WM_LBUTTONUP:
		{
			writeConsoleW( std::wstring( L"\n[マウス左クリック]" ) );

			break;
		}

		case WM_RBUTTONUP:
		{
			writeConsoleW( std::wstring( L"\n[マウス右クリック]" ) );

			break;
		}

		case WM_PAINT:
		{
			writeConsoleW( std::wstring( L"\n[ウィンドウの再描画がリクエストされました]" ) );

			//ウィンドウの更新領域を無効化する
			::ValidateRect( hwnd_holder_, nullptr );

			return 0; //メッセージ処理終了
		}

		case WM_CLOSE:
		{
			writeConsoleW( std::wstring( L"\n[ウィンドウを閉じます]" ) );

			//ウィンドウを破棄
			::DestroyWindow( hwnd_holder_ );
			
			return 0; //メッセージ処理終了
		}

		case WM_DESTROY:
		{
			writeConsoleW( std::wstring( L"\n[ウィンドウを破棄します]" ) );

			//WM_QUITメッセージをPOSTしてメッセージループを終了
			::PostQuitMessage( 0 );

			return 0; //メッセージ処理終了
		}

		case WM_NCDESTROY:
		{
			hwnd_holder_ = {};
			writeConsoleW( std::wstring( L"\n[ウィンドウを破棄しました]" ) );

			break;
		}
    }

    //既定のウィンドウメッセージ処理を実行
    return ::DefWindowProcW( hWnd, message, wParam, lParam );
}


/***************************
*** ウィンドウクラス登録 ***
***************************/
ATOM regWindowClass( HINSTANCE hInst )
{
	//エラー処理
	//エラー時にどのような値を返すかは、それぞれの設計に沿って考慮すべき
	if ( window_class_registered_ ) { writeConsoleW( std::wstring( L"\n[ウィンドウクラスは登録済みです]" ) ); return( 0 ); }

	//ウィンドウクラス設定
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof( WNDCLASSEX );

	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = &wndProc; //ウィンドウプロシージャのポインタ
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInst; //インスタンスハンドル
	wcex.hIcon          = LoadIcon( hInst, IDI_APPLICATION );
	wcex.hCursor        = LoadCursor( nullptr, IDC_ARROW );
	wcex.hbrBackground  = reinterpret_cast < HBRUSH > ( COLOR_WINDOW + 1 );
	wcex.lpszMenuName   = nullptr;
	wcex.lpszClassName  = L"App Window"; //ウィンドウクラス名
	wcex.hIconSm        = LoadIcon( wcex.hInstance, IDI_APPLICATION );

	//ウィンドウクラス登録
	ATOM result_value = ::RegisterClassExW( &wcex );

	//登録失敗
	if ( result_value == 0 )
	{
		writeConsoleW( std::wstring( L"\n[エラー: ウィンドウクラスの登録に失敗しました]" ) );
		return( result_value );
	}

	window_class_registered_ = true; //登録済みフラグを設定

	console_refresh = true; //コンソールを再描画

	return( result_value );
}


/*********************
*** ウィンドウ作成 ***
*********************/
BOOL createAppWindow( HINSTANCE hInst, int nCmdShow )
{
	//エラー処理
	//エラー時にどのような値を返すかは、それぞれの設計に沿って考慮すべき
	if      ( hwnd_holder_ != NULL      ) { writeConsoleW( std::wstring( L"\n[ウィンドウは作成済みです]" ) );             return( TRUE  ); }
	else if ( !window_class_registered_ ) { writeConsoleW( std::wstring( L"\n[ウィンドウクラスが登録されていません]" ) ); return( FALSE ); }

	//ウィンドウ作成
	hwnd_holder_ = ::CreateWindowExW
	(
		0,
		L"App Window",      //ウィンドウクラス名
		L"Test App Window", //ウィンドウタイトル
		WS_OVERLAPPEDWINDOW,
		256, //横位置
		256, //縦位置
		640, //幅
		480, //高さ
		nullptr,
		nullptr,
		hInst, //実行中のプログラムとウィンドウをリンクする
		nullptr
	);

	//作成失敗
	if ( hwnd_holder_ == NULL )
	{
		writeConsoleW( std::wstring( L"\n[エラー: ウィンドウの作成に失敗しました]" ) );
		hwnd_holder_ = {};
		return( FALSE );
	}

	::ShowWindow( hwnd_holder_, nCmdShow ); //ウィンドウを可視化

	console_refresh = true; //コンソールを再描画

	//処理成功
	return( TRUE );
}


/*****************
*** メイン関数 ***
*****************/
int main( )
{
	//コンパイル要件
	#ifndef _MSC_VER
		#error "This project requires MSVC compiler."
	#else
		static_assert( sizeof( wchar_t  ) == 2, "This project requires size of wchar_t is 2byte." );
	#endif

	//インスタンスハンドルの取得（実行中のプログラム自身を表す）
	HINSTANCE hInst_holder = ::GetModuleHandleW( NULL );

	//コンソールのイベントハンドラを登録する
    if ( !::SetConsoleCtrlHandler( &consoleCallback, TRUE ) )
	{
		writeConsoleW( std::wstring( L"\n[エラー: コールバック関数の登録に失敗しました]" ) );
		writeConsoleW( std::wstring( L"\n[アプリケーションを終了します]\n" ) );
		return 1; //異常終了
    }

	//標準入力のハンドルを取得
	HANDLE h_std_input = ::GetStdHandle( STD_INPUT_HANDLE );
    
	//コンソール入力バッファの入力モードを設定
	DWORD console_mode;
	::GetConsoleMode( h_std_input, &console_mode );
	::SetConsoleMode( h_std_input,  console_mode | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT );


	//選択項目
	std::vector < std::wstring > command_list =
	{
		L"アプリケーションの実行パスを取得",
		L"ユーザーフォルダのパスを取得",
		L"ウィンドウにビットマップを転送",
		L"ウィンドウクラスを登録",
		L"ウィンドウを作成",
		L"Exit"
	};

	//項目数
	int max_list = static_cast < int > ( command_list.size( ) );
    
	//現在のカーソル位置
	int current_cursor = 0;

	//ループフラグ
	bool loop_flag = true;


	//スレッド間でCOMインターフェースを共有しない
	if ( CoInitializeEx( 0, COINIT_APARTMENTTHREADED ) != S_OK )
	{
		//必要ならエラー処理をすること
		//return 1; //異常終了
	}


	//ウィンドウクラス登録
	//regWindowClass( hInst_holder );

	//ウィンドウ作成
	//createAppWindow( hInst_holder, SW_SHOWNORMAL );


	/*******************
	*** メインループ ***
	*******************/

	//メッセージ構造体
	MSG msg = {};

	while ( loop_flag )
	{
		//メッセージキューまたはコンソールに入力が来るまで待機する
		DWORD result = ::MsgWaitForMultipleObjects( 1, &h_std_input, FALSE, INFINITE, QS_ALLINPUT );

		//コンソールに入力があった場合
		if ( result == WAIT_OBJECT_0 )
		{
			INPUT_RECORD input_buffer[256];
			DWORD input_count;
            
			//コンソール入力バッファから値を取得
			if ( ::ReadConsoleInputW( h_std_input, input_buffer, 256, &input_count ) )
			{
				for ( DWORD count_buffer = 0 ; count_buffer < input_count ; ++count_buffer )
				{
					//キーイベント、キーダウンのみ処理する
					if ( input_buffer[count_buffer].EventType == KEY_EVENT && input_buffer[count_buffer].Event.KeyEvent.bKeyDown )
					{
						//キーコードを取得
						char input_key    = input_buffer[count_buffer].Event.KeyEvent.uChar.AsciiChar;
						WORD input_key_vk = input_buffer[count_buffer].Event.KeyEvent.wVirtualKeyCode;

						switch ( input_key_vk )
						{
							//↑キー
							case VK_UP:
							{
								current_cursor--;
				
								if ( current_cursor < 0 ) { current_cursor = max_list - 1; }

								console_refresh = true; //コンソールを再描画

								break;
							}

							//↓キー
							case VK_DOWN:
							{
								current_cursor++;
				
								if ( current_cursor >= max_list ) { current_cursor = 0; }

								console_refresh = true; //コンソールを再描画

								break;
							}

							//Enterキー
							case VK_RETURN:
							{
								//アプリケーションの実行パスを表示
								if ( current_cursor == ( max_list - 6 ) )
								{
									writeConsoleW( getAppPath( ) );
								}

								//ユーザーフォルダのパスを表示
								else if ( current_cursor == ( max_list - 5 ) )
								{
									getUserFolderPath( );
								}

								//ウィンドウにビットマップを転送
								else if ( current_cursor == ( max_list - 4 ) )
								{
									if ( hwnd_holder_ != NULL )
									{
										//ビットマップを作成
										if ( createDibImage( 480, 360 ) )
										{
											dibFill( 48, 36, 360, 270, 480, 360, 255, 255, 128, 64 ); //ビットマップを埋める

											HDC window_dc = ::GetDC( hwnd_holder_ ); //ウィンドウのデバイスコンテキストを取得

											dibBlt( mem_dc_, 0, 0, 480, 360, window_dc, 32, 32 ); //BitBlt() Win32関数を呼び出す

											::ReleaseDC( hwnd_holder_, window_dc ); //ウィンドウのデバイスコンテキストを解放

											deleteDibImage( ); //ビットマップを削除
										}
									}

									else { writeConsoleW( std::wstring( L"\n[ウィンドウが作成されていません]" ) ); }
								}

								//ウィンドウクラス登録
								else if ( current_cursor == ( max_list - 3 ) )
								{
									regWindowClass( hInst_holder );
								}

								//ウィンドウ作成
								else if ( current_cursor == ( max_list - 2 ) )
								{
									createAppWindow( hInst_holder, SW_SHOWNORMAL );
								}

								//アプリケーション終了
								else if ( current_cursor == ( max_list - 1 ) )
								{
									writeConsoleW( std::wstring( L"\n[メインループを終了します]" ) );

									loop_flag = false;
								}

								break;
							}

							default:
							{
								//NULL文字以外の場合
								if ( input_key != 0 ) { }
							}
						}
					}
				}

				/*************************
				*** コンソールの再描画 ***
				*************************/
				if ( console_refresh )
				{
					//画面消去
					std::system( "cls" );
		
					writeConsoleW( std::wstring( L"=== コンソールウィンドウを閉じると、アプリケーションは強制終了されます ===\n" ) );
					writeConsoleW( std::wstring( L"=== その場合は正常に終了処理が行われませんので、ご注意ください         ===\n\n" ) );

					writeConsoleW( std::wstring( L"=== 各機能を選択して実行する（↑↓で選択、Enterで実行）===\n\n" ) );

					for ( int count_list = 0 ; count_list < max_list ; ++count_list )
					{
						std::wstring text_buffer;

						if ( count_list == current_cursor ) { text_buffer = L" > "; }
						else                                { text_buffer = L"   "; }

						text_buffer += command_list[count_list];
						text_buffer += L"\n";

						writeConsoleW( text_buffer );
					}

					//ウィンドウクラス登録状況
					if ( window_class_registered_ ) { writeConsoleW( std::wstring( L"\nウィンドウクラス：登録済み" ) ); }
					else                            { writeConsoleW( std::wstring( L"\nウィンドウクラス：未登録"   ) ); }

					//ウィンドウ作成状況
					if ( hwnd_holder_ != NULL )     { writeConsoleW( std::wstring( L"\nウィンドウ　　　：作成済み\n" ) ); }
					else                            { writeConsoleW( std::wstring( L"\nウィンドウ　　　：未作成\n"   ) ); }

					console_refresh = false; //コンソールの再描画終了
				}
			}
		}

		//ウィンドウメッセージを取得
		else if ( hwnd_holder_ != NULL && result == WAIT_OBJECT_0 + 1 )
		{
			//現在のスレッドのメッセージキューからメッセージを取得
			while ( ::PeekMessageW( & msg, nullptr, 0, 0, PM_REMOVE ) != 0 )
			{
				switch ( msg.message )
				{
					//WM_QUITを取得したらメインループ終了
					case WM_QUIT:
					{
						writeConsoleW( std::wstring( L"\n[メインループを終了します]" ) );

						loop_flag = false;
						break;
					}

					default:
					{
						::TranslateMessage( & msg ); //キーボード入力を文字に変換
						::DispatchMessageW( & msg ); //ウィンドウプロシージャにメッセージを転送
					}
				}
			}
		}
	}
	
	writeConsoleW( std::wstring( L"\n[メインループを終了しました]" ) );
	writeConsoleW( std::wstring( L"\n[アプリケーションを終了します]\n" ) );

	//COMライブラリとの接続を解除する
	CoUninitialize( );

	/***********
	*** 終了 ***
	***********/
	return 0; //正常終了
}
