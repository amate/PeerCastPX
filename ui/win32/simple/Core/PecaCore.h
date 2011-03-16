/*!	@file	PecaCore.h
	@brief	Peercastのすべてをまとめる

*/

#pragma once

#include <winsock2.h>
#include "common.h"


class CSocket
{
public:
	static void	init();

	CSocket();

	void		bind();
	CSocket*	accept();
	void		close();

	const Host&	GetFromHost() const { return m_fromHost; }
	void	setReadTimeout(UINT time)  { m_uReadTimeout  = time; }
	void	setWriteTimeout(UINT time) { m_uWriteTimeout = time; }

	int		read(void *p, int nlength);
	int		readLine(char* str, int nMax);
	void	send(const void *p, int nlength);

private:
	void	_setBlocking(bool yes);
	void	_setReuse(bool yes);
	void	_setBufSize(int size);
	void	_checkTimeout(bool bRead);

	friend class CSocket;
	SOCKET	m_sock;
	Host	m_fromHost;
	UINT	m_uReadTimeout;
	UINT	m_uWriteTimeout;

	enum { RBSIZE = 8192 };
	char m_ReadBuf[RBSIZE];
	int m_nPos;
	int m_nDataSize;
};


class CPecaServer
{
public:


};

/// ファイルの入出力
class CFileStream
{
public:
	CFileStream(LPCSTR strFile, LPCSTR mode);
	~CFileStream();

	int		read(char* p, int nlength);
	void	read(std::string& str);
	int		write(const char* p, int nlength);

	int length();

private:
	FILE*	fp;
};

/// クライアントにファイルを送信する
class CHtmlFile
{
	typedef std::string::const_iterator citerator;

public:
	CHtmlFile(CSocket*	psock) : m_psock(psock) { }

	void	sendfile(CString strFilePath);

private:
	void	_writeHeader(LPCSTR strMineType);
	void	_writeLine(const char* fmt, ...);
	void	_writeRawFile(LPCTSTR strFilePath);
	void	_writeTemplate(LPCSTR strFilePath);

	void	_readVariable(citerator& it, citerator itend);
	void	_writeVariable(const std::string& varName); 

	CSocket*	m_psock;
	std::string	m_buf;
};


class CPecaCore
{
public:

	bool	start();

private:
	void	threadServer();

	void	handshake(CSocket* psock);

	void	sendLocalFile(const CString& strFilePath);
};

























