/*! @file	Utility.cpp
	@brief	便利な関数群


*/

#include "stdafx.h"
#include "Utility.h"





// exeのフルパス名を返す
CString GetExeFilePath()
{
	CString buf;
	::GetModuleFileName(_Module.get_m_hInst(), buf.GetBuffer(MAX_PATH), MAX_PATH);
	buf.ReleaseBuffer();
	return buf;
}

// exeのあるフォルダを返す(最後に \ がつく)
CString GetExeDirectory()
{
	CString str = GetExeFilePath();
	int		n	= str.ReverseFind( _T('\\') );
	return str.Left(n+1);
}

// exeのあるフォルダを返す(最後に \ が付かない)
CString GetExeDirName()
{
	CString str = GetExeFilePath();
	int		n	= str.ReverseFind( _T('\\') );
	return str.Left(n);
}


/// ファイルパスよりファイル名を返す
CString GetFileBaseName(const CString& file)
{
	int	n = file.ReverseFind(_T('\\'));
	int	m = file.ReverseFind(_T('/'));
	if (n < 0 && m < 0)
		return file;

	n = std::max(n, m);
	return file.Mid(n + 1);
}

/// ファイルパスより拡張子無しのファイル名を返す
CString GetFileBaseNameNoExt(const CString& file)
{
	CString strBase = GetFileBaseName(file);
	int n = strBase.ReverseFind(_T('.'));
	if (n == -1)
		return strBase;

	return strBase.Left(n);

}

/// ファイル名の拡張子の取得. ※ 結果の文字列には'.'は含まれない
CString GetFileExt(const CString& file)
{
	CString strBase = GetFileBaseName(file);
	int n = strBase.ReverseFind(_T('.'));
	if (n  == -1)
		return strBase;

	return strBase.Mid(n + 1);
}



/// 文字列をクリップボードにコピー
void	SetClipboardText(LPCTSTR strText)
{
	ATLASSERT(strText);
	int nLen = ::lstrlen(strText);
	if ( nLen == 0 )
		return;

	int 	nByte = (nLen + 1) * sizeof(TCHAR);
	HGLOBAL hText = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, nByte);
	if (hText == NULL)
		return ;

	BYTE*	pText = (BYTE *)::GlobalLock(hText);
	if (pText == NULL)
		return ;

	::memcpy(pText, (LPCTSTR) strText, nByte);

	::GlobalUnlock(hText);

	::OpenClipboard(NULL);
	::EmptyClipboard();
#ifdef _UNICODE
	::SetClipboardData(CF_UNICODETEXT, hText);
#else
	::SetClipboardData(CF_TEXT, hText);
#endif
	::CloseClipboard();
}











