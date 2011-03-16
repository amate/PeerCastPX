/*! @file	Utility.h
	@brief	�֗��Ȋ֐��Q

*/


/// exe�̃t���p�X����Ԃ�
CString GetExeFilePath();

/// exe�̂���t�H���_��Ԃ�(�Ō�� \ ����)
CString GetExeDirectory();

/// exe�̂���t�H���_��Ԃ�(�Ō�� \ ���t���Ȃ�)
CString GetExeDirName();

/// �t�@�C���p�X���t�@�C������Ԃ�
CString GetFileBaseName(const CString& file);

/// �t�@�C���p�X���g���q�����̃t�@�C������Ԃ�
CString GetFileBaseNameNoExt(const CString& file);

/// �t�@�C�����̊g���q�̎擾. �� ���ʂ̕�����ɂ�'.'�͊܂܂�Ȃ�.
CString GetFileExt(const CString& file);


/// ��������N���b�v�{�[�h�ɃR�s�[
void	SetClipboardText(LPCTSTR strText);




