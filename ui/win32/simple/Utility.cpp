/*! @file	Utility.cpp
	@brief	�֗��Ȋ֐��Q


*/

#include "stdafx.h"
#include "Utility.h"





// exe�̃t���p�X����Ԃ�
CString GetExeFilePath()
{
	CString buf;
	::GetModuleFileName(_Module.get_m_hInst(), buf.GetBuffer(MAX_PATH), MAX_PATH);
	buf.ReleaseBuffer();
	return buf;
}

// exe�̂���t�H���_��Ԃ�(�Ō�� \ ����)
CString GetExeDirectory()
{
	CString str = GetExeFilePath();
	int		n	= str.ReverseFind( _T('\\') );
	return str.Left(n+1);
}

// exe�̂���t�H���_��Ԃ�(�Ō�� \ ���t���Ȃ�)
CString GetExeDirName()
{
	CString str = GetExeFilePath();
	int		n	= str.ReverseFind( _T('\\') );
	return str.Left(n);
}


/// �t�@�C���p�X���t�@�C������Ԃ�
CString GetFileBaseName(const CString& file)
{
	int	n = file.ReverseFind(_T('\\'));
	int	m = file.ReverseFind(_T('/'));
	if (n < 0 && m < 0)
		return file;

	n = std::max(n, m);
	return file.Mid(n + 1);
}

/// �t�@�C���p�X���g���q�����̃t�@�C������Ԃ�
CString GetFileBaseNameNoExt(const CString& file)
{
	CString strBase = GetFileBaseName(file);
	int n = strBase.ReverseFind(_T('.'));
	if (n == -1)
		return strBase;

	return strBase.Left(n);

}

/// �t�@�C�����̊g���q�̎擾. �� ���ʂ̕�����ɂ�'.'�͊܂܂�Ȃ�
CString GetFileExt(const CString& file)
{
	CString strBase = GetFileBaseName(file);
	int n = strBase.ReverseFind(_T('.'));
	if (n  == -1)
		return strBase;

	return strBase.Mid(n + 1);
}



/// ��������N���b�v�{�[�h�ɃR�s�[
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











