/*! @file	Utility.h
	@brief	便利な関数群

*/


/// exeのフルパス名を返す
CString GetExeFilePath();

/// exeのあるフォルダを返す(最後に \ がつく)
CString GetExeDirectory();

/// exeのあるフォルダを返す(最後に \ が付かない)
CString GetExeDirName();


/// 文字列をクリップボードにコピー
void	SetClipboardText(LPCTSTR strText);




