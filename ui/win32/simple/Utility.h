/*! @file	Utility.h
	@brief	便利な関数群

*/


/// exeのフルパス名を返す
CString GetExeFilePath();

/// exeのあるフォルダを返す(最後に \ がつく)
CString GetExeDirectory();

/// exeのあるフォルダを返す(最後に \ が付かない)
CString GetExeDirName();

/// ファイルパスよりファイル名を返す
CString GetFileBaseName(const CString& file);

/// ファイルパスより拡張子無しのファイル名を返す
CString GetFileBaseNameNoExt(const CString& file);

/// ファイル名の拡張子の取得. ※ 結果の文字列には'.'は含まれない.
CString GetFileExt(const CString& file);


/// 文字列をクリップボードにコピー
void	SetClipboardText(LPCTSTR strText);




