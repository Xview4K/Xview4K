Xview4Kのビルド方法

ソース類をC:\Xview4Kフォルダに展開。

Xview4KはVS2019 x64でビルドする。
旧ライブラリのビルド時はSDK8.1が必要。https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/
(SDK8.1のダウンロードが途中で止まるときはVPNでアメリカに接続してからダウンロード。日本IPだと上手くいかない場合がある)
暗号ライブラリはcryptopp(ソース)とOpenSSLのWindowsビルド済みバージョンを使用。
https://cryptopp.com/release890.html
cryptoppのソースをx64用に自分でビルドする。

https://slproweb.com/products/Win32OpenSSL.html
Win64 OpenSSL v3.2.2や3.2.1の202MB Installerをダウンロード

リンカー→追加ファイルにwinscard.lib;libssl.lib;libcrypto.lib;cryptlib.lib;とavcodec-lav.lib;avfilter-lav.lib;avformat-lav.lib;avresample-lav.lib;avutil-lav.lib;swscale-lav.lib;freetype28d.libを記述。

exe実行時には、libcrypto-3-x64.dll、libssl-3-x64.dll、avcodec-lav-58.dll、avfilter-lav-7.dll、avformat-lav-58.dll、avresample-lav-4.dll、avutil-lav-56.dllをexeファイルと同じフォルダに置く。


# Xview 4K 処理の概要

チャンネル切り替えなどはmain.cpp、ACAS処理はa_cas_card.cで行う。

tlv_parse.cppのsetPacketBufPtr(LPBYTE lpBuf)で受信データlpBufを読み込む。
parsePacket()で映像・音声データ、ECMデータのパケットを抽出、暗号解除し、
暗号解除した映像データにヘッダw[0]～[3]を付加してvideo_frame_bufferの、
音声データにヘッダheader[0]～[2]を付加してaudio_frame_bufferに書き込み動画バッファを作成する。

lpBufの先頭とラストは断片化したTLVパケットなのでparseScheduler()で断片化した部分を接続する。


