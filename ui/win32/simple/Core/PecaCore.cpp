/*!	@file	PecaCore.cpp
	@brief	Peercastのすべてをまとめる
*/

#include "../stdafx.h"	// インテリセンス用
#include "stdafx.h"
#include "PecaCore.h"
#include <boost/thread.hpp>
#include "common.h"
#include "http.h"
#include "version2.h"
#include "../Utility.h"

#define	DEFAULTPECA_PORT	(5121)

///////////////////////////////////////
// class CSocket

/// コンストラクタ
CSocket::CSocket() :
	m_nPos(0),
	m_nDataSize(0),
	m_uReadTimeout(30000),
	m_uWriteTimeout(30000)
{
}

void	CSocket::init()
{
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD( 2, 0 );
	int err = ::WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		throw SockException("Unable to init sockets");
	}
}


void	CSocket::bind()
{
	m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_sock ==  INVALID_SOCKET)
		throw SockException("Can`t open socket");

	_setBlocking(false);
	_setReuse(true);

	sockaddr_in localAddr = { 0 };
	localAddr.sin_family = AF_INET;
	localAddr.sin_port = htons(DEFAULTPECA_PORT);
	localAddr.sin_addr.s_addr = INADDR_ANY;
	if( ::bind(m_sock, (sockaddr *)&localAddr, sizeof(localAddr)) == SOCKET_ERROR)
		throw SockException("Can`t bind socket");

	if (::listen(m_sock, SOMAXCONN) == SOCKET_ERROR)
		throw SockException("Can`t listen",WSAGetLastError());
}

CSocket*	CSocket::accept()
{
	int fromSize = sizeof(sockaddr_in);
	sockaddr_in from;
	SOCKET conSock = ::accept(m_sock, (sockaddr *)&from, &fromSize);
	if (conSock ==  INVALID_SOCKET)
		return NULL;

	// 接続があった
    CSocket*	pSocket = new CSocket;
	pSocket->m_sock = conSock;

	pSocket->m_fromHost.port = from.sin_port;
	pSocket->m_fromHost.ip = from.sin_addr.S_un.S_un_b.s_b1<<24 |
				  from.sin_addr.S_un.S_un_b.s_b2<<16 |
				  from.sin_addr.S_un.S_un_b.s_b3<<8 |
				  from.sin_addr.S_un.S_un_b.s_b4;


	pSocket->_setBlocking(false);
	pSocket->_setBufSize(65535);

	return pSocket;
}

void	CSocket::close()
{
	if (m_sock) {
		shutdown(m_sock, SD_SEND);

		//setReadTimeout(2000);
		unsigned int stime = sys->getTime();
		try {
			char c[1024];
			while (read(&c, sizeof(c)) > 0)
				if (sys->getTime() - stime > 5)
					break;
		} catch(StreamException &) {}

		if (closesocket(m_sock) == SOCKET_ERROR)
			LOG_ERROR("closesocket() error");

		m_sock = 0;
	}
}



// --------------------------------------------------
/// pにnlength分読み取る
int CSocket::read(void *p, int nlength)
{
	int bytesRead = 0;

	while (nlength) {
		if (m_nDataSize >= nlength) {	// nlength以上読み込んだ
			memcpy(p, &m_ReadBuf[m_nPos], nlength);
			bytesRead += nlength;
			/* 次にreadが呼ばれる時用にバッファの位置と残りのサイズを保存しておく */
			m_nPos += nlength;
			m_nDataSize -= nlength;
			break;

		} else if (m_nDataSize > 0) {	// バッファに残っている部分を書き込む
			memcpy(p, &m_ReadBuf[m_nPos], m_nDataSize);
			p = (char *) p + m_nDataSize;	// 書き込み位置更新
			nlength -= m_nDataSize;
			bytesRead += m_nDataSize;
		}

		m_nPos = 0;
		m_nDataSize = 0;
		//int r = recv(sockNum, (char *)p, l, 0);
		int readed = recv(m_sock, m_ReadBuf, RBSIZE, 0);	// ソケットから読み取る
		if (readed == SOCKET_ERROR) {
			// non-blocking sockets always fall through to here
			_checkTimeout(true);

		} else if (readed == 0) {
			throw EOFException("Closed on read");

		} else {
			//updateTotals(r,0);
			//bytesRead += r;
			//l -= r;
			//p = (char *)p+r;

			m_nDataSize += readed;
		}
	}
	return bytesRead;
}

// --------------------------------------------------
/// 1行読み取る
int		CSocket::readLine(char* str, int nMax)
{
	ATLASSERT(nMax > 0);

    int i = 0;
	nMax -= 2;

	while(nMax) {
		nMax--;
    	char c;         
    	read(&c,1);
		if (c == '\n')
			break;
		if (c == '\r')
			continue;
        str[i] = c;
		i++;
    }
    str[i] = 0;
	return i;
}

// --------------------------------------------------
/// 送信する
void	CSocket::send(const void *p, int nlength)
{
	while (nlength) {
		int r = ::send(m_sock, (char *)p, nlength, 0);
		if (r == SOCKET_ERROR) {
			_checkTimeout(false);	
		} else if (r == 0) {
			throw SockException("Closed on write");
		} else if (r > 0) {
			nlength -= r;
			p = (char *)p + r;
		}
	}
}

// --------------------------------------------------
void CSocket::_setBlocking(bool yes)
{
	unsigned long op = yes ? 0 : 1;
	if (ioctlsocket(m_sock, FIONBIO, &op) == SOCKET_ERROR)
		throw SockException("Can`t set blocking");
}

// --------------------------------------------------
void CSocket::_setReuse(bool yes)
{
	unsigned long op = yes ? 1 : 0;
	if (setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&op, sizeof(op)) == SOCKET_ERROR) 
		throw SockException("Unable to set REUSE");
}

// --------------------------------------------------
void CSocket::_setBufSize(int size)
{
	int oldop;
	int op = size;
	int len = sizeof(op);
	if (getsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, (char *)&oldop, &len) == SOCKET_ERROR) {
		LOG_DEBUG("Unable to get RCVBUF");
	} else if (oldop < size) {
		if (setsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, (char *)&op, len) == SOCKET_ERROR) 
			LOG_DEBUG("Unable to set RCVBUF");
		//else
		//	LOG_DEBUG("*** recvbufsize:%d -> %d", oldop, op);
	}

	if (getsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, (char *)&oldop, &len) == SOCKET_ERROR) {
		LOG_DEBUG("Unable to get SNDBUF");
	} else if (oldop < size) {
		if (setsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, (char *)&op,len) == SOCKET_ERROR) 
			LOG_DEBUG("Unable to set SNDBUF");
		//else
		//	LOG_DEBUG("*** sendbufsize: %d -> %d", oldop, op);
	}
}

// --------------------------------------------------
void	CSocket::_checkTimeout(bool bRead)
{
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK) {
		timeval timeout = { 0 };
		fd_set read_fds;
		FD_ZERO (&read_fds);
		fd_set write_fds;
        FD_ZERO(&write_fds);
		
		if (bRead) {
			timeout.tv_sec = (int)m_uReadTimeout/1000;
	        FD_SET(m_sock, &read_fds);
		} else {
			timeout.tv_sec = (int)m_uWriteTimeout/1000;
			FD_SET(m_sock, &write_fds);
		}

		timeval *tp;
		if (timeout.tv_sec)
			tp = &timeout;
		else
			tp = NULL;

		int r = select(NULL, &read_fds, &write_fds, NULL, tp);
        if (r == 0)
			throw TimeoutException();
		else if (r == SOCKET_ERROR)
			throw SockException("select failed.");

	} else {
		char str[32];
		sprintf(str, "%d", err);
		throw SockException(str);
	}
}



//////////////////////////////////////
// class CFileStream

CFileStream::CFileStream(LPCSTR strFile, LPCSTR mode)
{
	fp = fopen(strFile, mode);
	if (fp == NULL)
		throw StreamException("ファイルのオープンに失敗");
}

CFileStream::~CFileStream()
{
	fclose(fp);
}

int	CFileStream::read(char* p, int nlength)
{
	return fread(p, 1, nlength, fp);
}

void CFileStream::read(std::string& str)
{
	str.reserve(str.size() + length());

	char temp[1024 + 1];
	int r;
	do {
		r = read(temp, 1024);
		temp[r] = '\0';
		str += temp;
	} while (r != 0);
}

int	CFileStream::write(const char* p, int nlength)
{
	return fwrite(p, 1, nlength, fp);
}

int CFileStream::length()
{
	int old = ftell(fp);
	fseek(fp, 0, SEEK_END);
	int nFileSize = ftell(fp);	// ファイルサイズ取得
	fseek(fp, old, SEEK_SET);
	return nFileSize;
}


///////////////////////////////////////
// class CHtmlFile

void	CHtmlFile::sendfile(CString strFilePath)
{
	LOG_DEBUG(_T("ファイルを送信 : %s"), strFilePath);

	CString strArg;
	int n = strFilePath.Find(_T('?'));
	if (n != -1) {
		strArg = strFilePath.Mid(n + 1);
		strFilePath.Delete(n + 1);
	}

	enum { HeaderSize = 1012 };
	m_buf.reserve(HeaderSize);

	CString strExt = GetFileExt(strFilePath);
	strExt.MakeLower();
	try {
		if (strExt == _T("htm") || strExt == _T("html")) {
			_writeHeader(MIME_HTML);
			_writeTemplate(strFilePath);

		} else if (strExt == _T("css")) {
			_writeHeader(MIME_CSS);
			_writeRawFile(strFilePath);

		} else if (strExt == _T("js")) {
			_writeHeader("application/x-javascript");
			_writeRawFile(strFilePath);

		} else if (strExt == _T("jpg")) {
			_writeHeader(MIME_JPEG);
			_writeRawFile(strFilePath);

		} else if (strExt == _T("gif")) {
			_writeHeader(MIME_GIF);
			_writeRawFile(strFilePath);

		} else if (strExt == _T("png")) {
			_writeHeader(MIME_PNG);
			_writeRawFile(strFilePath);

		} else {
			_writeHeader(MIME_TEXT);
			_writeRawFile(strFilePath);
		}
	} catch (StreamException& e) {	// 404
		LOG_ERROR("%s : 404エラーを送信", e.msg);
		m_buf.clear();
		_writeLine(HTTP_SC_NOTFOUND);
		_writeLine("%s %s", HTTP_HS_SERVER, PCX_AGENT);
		_writeLine("%s %s", HTTP_HS_CONNECTION, "close");
		_writeLine("%s %s", HTTP_HS_CONTENT, MIME_HTML);
	}

	m_psock->send(m_buf.data(), m_buf.size());
}

void	CHtmlFile::_writeHeader(LPCSTR strMineType)
{
	_writeLine(HTTP_SC_OK);
	_writeLine("%s %s", HTTP_HS_SERVER, PCX_AGENT);
	_writeLine("%s %s", HTTP_HS_CONNECTION, "close");
    _writeLine("%s %s", HTTP_HS_CONTENT, strMineType);
	_writeLine("");
}

void	CHtmlFile::_writeLine(const char* fmt, ...)
{
	va_list	arg;
	va_start(arg, fmt);
	CString temp;
	temp.FormatV(fmt, arg);
	va_end(arg);

	m_buf += temp + "\r\n";
}

void	CHtmlFile::_writeRawFile(LPCTSTR strFilePath)
{
	CFileStream	fs(strFilePath, "rb");
	int nPos = m_buf.size();
	int nSize = fs.length();
	m_buf.reserve(nPos + nSize);
	m_buf._Mysize = nPos + nSize;

	fs.read(&m_buf[nPos], nSize);
}


void	CHtmlFile::_writeTemplate(LPCSTR strFilePath)
{
	CFileStream	fs(strFilePath, "rb");
	std::string buf;
	fs.read(buf);

	for (auto it = buf.cbegin(); it != buf.cend(); ++it) {
		char c = *it;
		if (c == '{') {
			++it;
			if (it == buf.cend())
				break;
			c = *it;
			if (c == '$') {
				_readVariable(it, buf.cend());
				//readVariable(in,outp,loop);
			} else if (c == '@') {
				//int t = readCmd(in,outp,loop);
				//if (t == TMPL_END)
				//	return false;
				//else if (t == TMPL_ELSE)
				//	return true;
			} else if (c == '{') {
				//if (outp)
				//	outp->writeChar(c);
			} else {
				throw StreamException("Unknown template escape");
			}
		} else {
			m_buf += c;
		}
	}
}

void	CHtmlFile::_readVariable(citerator& it, citerator itend)
{
	++it;
	std::string buf;
	for (; it != itend; ++it) {
		char c = *it;
		if (c == '}') {
			_writeVariable(buf);
			return;
		}
		buf += c;
	}
}

void	CHtmlFile::_writeVariable(const std::string& varName)
{
#if 0
	bool r = false;
	if (varName == "servMgr.")
		r=servMgr->writeVariable(s,varName+8);
	else if (varName.startsWith("chanMgr."))
		r=chanMgr->writeVariable(s,varName+8,loop);
	else if (varName.startsWith("stats."))
		r=stats.writeVariable(s,varName+6);
	else if (varName.startsWith("sys."))
	{
		if (varName == "sys.log.dumpHTML")
		{
			sys->logBuf->dumpHTML(s);
			r=true;
		}
	}
	else if (varName.startsWith("loop."))
	{
		if (varName.startsWith("loop.channel."))
		{
			Channel *ch = chanMgr->findChannelByIndex(loop);
			if (ch)
				r = ch->writeVariable(s,varName+13,loop);
		}else if (varName.startsWith("loop.servent."))
		{
			Servent *sv = servMgr->findServentByIndex(loop);
			if (sv)
				r = sv->writeVariable(s,varName+13);
		}else if (varName.startsWith("loop.filter."))
		{
			ServFilter *sf = &servMgr->filters[loop];
			r = sf->writeVariable(s,varName+12);

		}else if (varName.startsWith("loop.bcid."))
		{
			BCID *bcid = servMgr->findValidBCID(loop);
			if (bcid)
				r = bcid->writeVariable(s,varName+10);
		
		}else if (varName == "loop.indexEven")
		{
			s.writeStringF("%d",(loop&1)==0);
			r = true;
		}else if (varName == "loop.index")
		{
			s.writeStringF("%d",loop);
			r = true;
		}else if (varName.startsWith("loop.hit."))
		{
			char *idstr = getCGIarg(tmplArgs,"id=");
			if (idstr)
			{
				GnuID id;
				id.fromStr(idstr);
				ChanHitList *chl = chanMgr->findHitListByID(id);
				if (chl)
				{
					int cnt=0;
					ChanHit *ch = chl->hit;
					while (ch)
					{
						if (ch->host.ip && !ch->dead)
						{
							if (cnt == loop)
							{
								r = ch->writeVariable(s,varName+9);
								break;
							}
							cnt++;
						}
						ch=ch->next;
					}

				}
			}
		}

	}
	else if (varName.startsWith("page."))
	{
		if (varName.startsWith("page.channel."))
		{
			char *idstr = getCGIarg(tmplArgs,"id=");
			if (idstr)
			{
				GnuID id;
				id.fromStr(idstr);
				Channel *ch = chanMgr->findChannelByID(id);
				if (ch)
					r = ch->writeVariable(s,varName+13,loop);
			}
		}else
		{

			String v = varName+5;
			v.append('=');
			char *a = getCGIarg(tmplArgs,v);
			if (a)
			{
				s.writeString(a);		
				r=true;
			}
		}
	}


	if (!r)
		s.writeString(varName);
#endif
}



///////////////////////////////////////
// class CPecaCore

/// サーバーを開始する
bool	CPecaCore::start()
{
	// ソケットを使う前準備
	CSocket::init();

	boost::thread(boost::bind(&CPecaCore::threadServer, this));

	return true;
}



void	CPecaCore::threadServer()
{
	CPecaServer	server;

	CSocket	sockServer;
	sockServer.bind();

	while (1) {
		CSocket*	psock = sockServer.accept();
		if (psock) {
			char strIP[64];
			psock->GetFromHost().toStr(strIP);
			LOG_DEBUG(_T("%s から接続要求"), strIP);

			boost::thread(boost::bind(&CPecaCore::handshake, this, psock));
		}
		::Sleep(10);
	}


	//server.

}



void	CPecaCore::handshake(CSocket* psock)
{
	CString buf;
	psock->readLine(buf.GetBuffer(256), 256);
	buf.ReleaseBuffer();
	if (buf.Find(HTTP_PROTO1) != -1) {
		LOG_DEBUG(_T("HTTP : %s"), buf);
		if (buf.Left(5) == "GET /") {
			int nSpaceIndex = buf.ReverseFind(' ');
			ATLASSERT(nSpaceIndex != -1 && nSpaceIndex >= 5);
			CString strFilePath = GetExeDirectory() + buf.Mid(5, nSpaceIndex - 5);

			CHtmlFile	file(psock);
			file.sendfile(strFilePath);
		}

	}

	psock->close();
}



















