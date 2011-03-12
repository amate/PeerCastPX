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











