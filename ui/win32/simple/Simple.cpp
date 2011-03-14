// ------------------------------------------------
// File : simple.cpp
// Date: 4-apr-2002
// Author: giles
// Desc: 
//		Simple tray icon interface to PeerCast, mostly win32 related stuff.
//		
// (c) 2002 peercast.org
// ------------------------------------------------
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// ------------------------------------------------

//#include <windows.h>
//#include <direct.h>
//#include <objbase.h>
#include "stdafx.h"
#include "resource.h"
#include "channel.h"
#include "servent.h"
#include "servmgr.h"
#include "win32/wsys.h"
#include "peercast.h"
#include "simple.h"
#include "version2.h"
#include "Utility.h"
#include <clocale>
#include "MainFrm.h"

#pragma comment(lib, "winmm.lib")	// PlaySound

CAppModule _Module;


#define MAX_LOADSTRING 100
#define MAX_CHANNELS 99


// PeerCast globals

CString	g_strIniFilePath;	//!< 設定ファイルのパス
static int currNotify=0;
String iniFileName;
HWND guiWnd=NULL; // fix
HWND mainWnd;

int		seenNewVersionTime=0;
ChanInfo chanInfo;
bool chanInfoIsRelayed;
//GnuID	lastPlayID;

// ---------------------------------
Sys * APICALL MyPeercastInst::createSys()
{
	return new WSys(mainWnd);
}
// ---------------------------------
const char * APICALL MyPeercastApp ::getIniFilename()
{
	return iniFileName.cstr();
}

// ---------------------------------
const char *APICALL MyPeercastApp ::getClientTypeOS() 
{
	return PCX_OS_WIN32;
}

// ---------------------------------
const char * APICALL MyPeercastApp::getPath()
{
	return GetExeFilePath();//exePath.cstr();
}

// --------------------------------- JP-EX
void	APICALL MyPeercastApp ::openLogFile()
{
	logFile.openWriteReplace("log.txt");
}
// --------------------------------- JP-EX
void	APICALL MyPeercastApp ::getDirectory()
{
#if 0
	char path_buffer[256],drive[32],dir[128];
	GetModuleFileName(NULL,path_buffer,255);
	_splitpath(path_buffer,drive,dir,NULL,NULL);
	sprintf(servMgr->modulePath,"%s%s",drive,dir);
#endif
	lstrcpy(servMgr->modulePath, GetExeDirectory());
}
// --------------------------------- JP-EX
bool	APICALL MyPeercastApp ::clearTemp()
{
	if (servMgr->clearPLS)
		return true;

	return false;
}


// --------------------------------------------------
void LOG2(const char *fmt,...)
{
	va_list ap;
  	va_start(ap, fmt);
	char str[4096];
	vsprintf(str,fmt,ap);
	OutputDebugString(str);
   	va_end(ap);	
}

// ---------------------------------------
/// コマンドラインをパースする
void	CommandlinePerse(LPCTSTR lpstrCmdLine, bool& bShowGUI, bool& bAllowMulti, bool& bKillme, CString& chanURL)
{
	if (strlen(lpstrCmdLine) > 0) {
		char *p;
		if (strstr(lpstrCmdLine, _T("-zen"))) 
			bShowGUI = false;
		if (strstr(lpstrCmdLine, _T("-multi"))) 
			bAllowMulti = true;
		if (strstr(lpstrCmdLine, _T("-kill"))) 
			bKillme = true;
		if ((p = const_cast<char*>(strstr(lpstrCmdLine, _T("-url")))) != NULL) {
			p+=4;
			while (*p)
			{
				if (*p=='"')
				{
					p++;
					break;
				}				
				if (*p != ' ')
					break;
				p++;
			}
			if (*p)
				chanURL = p;//strncpy(tmpURL,p,sizeof(tmpURL)-1);
		}
	}
}

// ---------------------------------------
/// 一時ファイルを削除する
void	DeleteTempFile()
{
	WIN32_FIND_DATA fd; //JP-EX
	HANDLE hFind; //JP-EX

	DeleteFile(GetExeDirectory() + _T("play.pls"));
	hFind = FindFirstFile(GetExeDirectory() + _T("*.asx"), &fd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			DeleteFile(GetExeDirectory() + fd.cFileName);
		} while (FindNextFile(hFind, &fd));

		FindClose(hFind);
	}
}

// ---------------------------------------
/// メッセージループ開始
int Run(LPTSTR lpstrCmdLine, int nCmdShow = SW_SHOWDEFAULT)
{
	iniFileName.set(GetExeDirectory() + _T("peercast.ini"));
	g_strIniFilePath = GetExeFilePath() + _T(".ini");

	CString chanURL;
	bool showGUI	= false;	// off by default now;
	bool allowMulti	= false;
	bool killMe		= false;
	CommandlinePerse(lpstrCmdLine, showGUI, allowMulti, killMe, chanURL);
	
	if (chanURL.Left(11) == _T("peercast://")) {
		if (chanURL.Mid(11, 4) == _T("pls/"))
			chanURL = chanURL.Mid(11 + 4);
		else
			chanURL = chanURL.Mid(11);
		showGUI = false;
	}

	if (!allowMulti) {
		HANDLE mutex = CreateMutex(NULL, TRUE, MAINFRAME_WINDOWCLASSNAME);
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			HWND oldWin = FindWindow(MAINFRAME_WINDOWCLASSNAME, NULL);
			if (oldWin) {
				if (killMe) 
					SendMessage(oldWin,WM_DESTROY,0,0);

				if (chanURL.IsEmpty() == FALSE) {
					COPYDATASTRUCT copy;
					copy.dwData = WM_PLAYCHANNEL;
					copy.cbData = lstrlen(chanURL)+1;			// plus null term
					copy.lpData = (LPVOID)(LPCTSTR)chanURL;
					SendMessage(oldWin,WM_COPYDATA,NULL,(LPARAM)&copy);
				} else {
					if (showGUI)
						ShowWindow(oldWin, TRUE);
				}
			}
			return 0;
		}
	}

	if (killMe)
		return 0;
	

	// Perform application initialization:
	peercastInst = new MyPeercastInst();
	peercastApp = new MyPeercastApp();

	peercastInst->init();

	DeleteTempFile();

	if (chanURL.IsEmpty() == FALSE) {
		ChanInfo info;
		servMgr->procConnectArgs(chanURL.GetBuffer(0), info);
		chanMgr->findAndPlayChannel(info, false);
	}

	/////////////////////////////////
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	if(wndMain.CreateEx() == NULL)
	{
		ATLTRACE(_T("メイン ウィンドウの作成に失敗しました！\n"));
		return 0;
	}
	mainWnd = wndMain;

	wndMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	/////////////////////////////////

	peercastInst->saveSettings();
	peercastInst->quit();

	return nRet;
}

// ---------------------------------------
/// エントリーポイント
int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE	 /*hPrevInstance*/,
                     LPTSTR      lpCmdLine,
                     int         nCmdShow)
{
	// DLL攻撃対策
	SetDllDirectory(_T(""));

  #ifdef _DEBUG
	// ATLTRACEで日本語を使うために必要
	_tsetlocale( LC_ALL, _T("japanese") );
  #endif

#ifdef _DEBUG
	// memory leak check
	//::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	
	HRESULT hRes = ::CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hRes));

	//ユニコードレイヤー（MSLU: Windows9x用）使用時のATLウィンドウのサンキング問題を解消します。
	::DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(ICC_WIN95_CLASSES | ICC_COOL_CLASSES);	// ほかのコントロール用のフラグを追加してください

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	int	nRet = Run(lpCmdLine, nCmdShow);

	_Module.Term();
	::CoUninitialize(); //JP-MOD

	return nRet;
}





//-----------------------------
void channelPopup(const char *title, const char *msg, bool isPopup = true)
{
	CTrayIcon*	pTray = CTrayIcon::GetInstance();
	if (pTray == nullptr)
		return;

	pTray->ChannelPopup(title, msg, isPopup);
}
//-----------------------------
void clearChannelPopup()
{
	CTrayIcon*	pTray = CTrayIcon::GetInstance();
	if (pTray == nullptr)
		return;

	pTray->ClearChannelPopup();
}

//-----------------------------
// PopupEntry
struct PopupEntry {
	GnuID id;
	String name;
	String track;
	String comment;
	PopupEntry *next;
};
static PopupEntry *PEList = NULL;
static WLock PELock;

static void putPopupEntry(PopupEntry *pe)
{
	PELock.on();
	pe->next = PEList;
	PEList = pe;
	PELock.off();
}

static PopupEntry *getPopupEntry(GnuID id)
{
	PELock.on();
	PopupEntry *pe = PEList;
	PopupEntry *prev = NULL;
	while (pe) {
		if (id.isSame(pe->id)) {
			if (prev) prev->next = pe->next;
			else PEList = pe->next;
			PELock.off();
			pe->next = NULL;
			return pe;
		}
		prev = pe;
		pe = pe->next;
	}
	PELock.off();
	return NULL;
}

static PopupEntry *getTopPopupEntry()
{
	PopupEntry *p = NULL;
	PELock.on();
	if (PEList) {
		p = PEList;
		PEList = PEList->next;
	}
	PELock.off();
	return p;
}

//-----------------------------
void	APICALL MyPeercastApp::channelStart(ChanInfo *info)
{

//	lastPlayID = info->id;
//
//	if(!isIndexTxt(info))	// for PCRaw (popup)
//		clearChannelPopup();

	PopupEntry *pe = getPopupEntry(info->id);
	if (!pe) {
		pe = new PopupEntry;
		pe->id = info->id;
	}
	if (!isIndexTxt(info))
	{
		::PostMessage(mainWnd, WM_ANTENNA, 0, 0); //JP-MOD
		putPopupEntry(pe);
	}
	else
		delete pe;
}
//-----------------------------
void	APICALL MyPeercastApp::channelStop(ChanInfo *info)
{
//	if (info->id.isSame(lastPlayID))
//	{
//		lastPlayID.clear();
//
//		if(!isIndexTxt(info))	// for PCRaw (popup)
//			clearChannelPopup();
//	}

	PopupEntry *pe = getPopupEntry(info->id);
	if (pe) delete pe;

	pe = getTopPopupEntry();
	if (!pe) {
		clearChannelPopup();
	} else {
		if (ServMgr::NT_TRACKINFO & peercastInst->getNotifyMask())
		{
			String name,track; //JP-Patch
			name = pe->name; //JP-Patch
			track = pe->track; //JP-Patch
			name.convertTo(String::T_SJIS); //JP-Patch
			track.convertTo(String::T_SJIS); //JP-Patch
			clearChannelPopup();
		//	channelPopup(info->name.cstr(),trackTitle.cstr());
			channelPopup(name.cstr(),track.cstr(), false); //JP-Patch
		}
		putPopupEntry(pe);
	}
}
//-----------------------------
void	APICALL MyPeercastApp::channelUpdate(ChanInfo *info)
{
	if (info)
	{
		if(guiWnd) //JP-MOD
			::PostMessage(guiWnd, WM_CHANINFOCHANGED, 0, 0);

		PopupEntry *pe = getPopupEntry(info->id);
		if (!pe) return;

		String tmp;
		tmp.append(info->track.artist);
		tmp.append(" ");
		tmp.append(info->track.title);


		if (!tmp.isSame(pe->track))
		{
			pe->name = info->name;
			pe->track = tmp;
			if (ServMgr::NT_TRACKINFO & peercastInst->getNotifyMask())
			{
				//trackTitle=tmp;
				String name,track; //JP-Patch
				name = info->name; //JP-Patch
				track = tmp; //JP-Patch
				name.convertTo(String::T_SJIS); //JP-Patch
				track.convertTo(String::T_SJIS); //JP-Patch
				if(!isIndexTxt(info))	// for PCRaw (popup)
				{
					clearChannelPopup();
				//	channelPopup(info->name.cstr(),trackTitle.cstr());
					channelPopup(name.cstr(),track.cstr()); //JP-Patch
				}
			}
		} else if (!info->comment.isSame(pe->comment))
		{
			pe->name = info->name;
			pe->comment = info->comment;
			if (ServMgr::NT_BROADCASTERS & peercastInst->getNotifyMask())
			{
				//channelComment = info->comment;
				String name,comment; //JP-Patch
				name = info->name; //JP-Patch
				comment = info->comment; //JP-Patch
				name.convertTo(String::T_SJIS); //JP-Patch
				comment.convertTo(String::T_SJIS); //JP-Patch
				if(!isIndexTxt(info))	// for PCRaw (popup)
				{
					clearChannelPopup();
				//	channelPopup(info->name.cstr(),channelComment.cstr());
					channelPopup(name.cstr(),comment.cstr());
				}
			}
		}

		if (!isIndexTxt(info))
			putPopupEntry(pe);
		else
			delete pe;
	}
}
//-----------------------------
void	APICALL MyPeercastApp::notifyMessage(ServMgr::NOTIFY_TYPE type, const char *msg)
{
	static bool shownUpgradeAlert=false;

	currNotify = type;

	if (!shownUpgradeAlert)
	{
		//ATLASSERT(FALSE);
#if 0
		trayIcon.uFlags = NIF_ICON;

		if (type == ServMgr::NT_UPGRADE)
		{
			shownUpgradeAlert = true;
			trayIcon.hIcon = icon2;
		}else
		{
			if(trayIcon.hIcon == icon2) //JP-MOD
				trayIcon.hIcon = icon1;
		}
#endif
	} else {
		if (type == ServMgr::NT_UPGRADE)
			return;
	}

	const char *title="";

	switch(type)
	{
		case ServMgr::NT_UPGRADE:
			title = "Upgrade alert";
			break;
		case ServMgr::NT_PEERCAST:
			title = "Message from PeerCast:";
			break;
		case ServMgr::NT_APPLAUSE: //JP-MOD
			title = "Applause from Listener:";
			if(servMgr->ppClapSound)
				PlaySound(servMgr->ppClapSoundPath.cstr(), NULL, SND_ASYNC | SND_FILENAME | SND_NODEFAULT);
			break;

	}

	if (type & peercastInst->getNotifyMask())
		channelPopup(title, msg);
}


typedef int (*COMPARE_FUNC)(const void *,const void *);

static int compareHitLists(ChanHitList **c2, ChanHitList **c1)
{
	return stricmp(c1[0]->info.name.cstr(),c2[0]->info.name.cstr());
}

static int compareChannels(Channel **c2, Channel **c1)
{
	return stricmp(c1[0]->info.name.cstr(),c2[0]->info.name.cstr());
}

extern void ADDLOG(const char *str, int id, bool sel, void *data, LogBuffer::TYPE type);
// ---------------------------------
void APICALL MyPeercastApp ::printLog(LogBuffer::TYPE t, const char *str)
{
	ADDLOG(str,IDC_LOGLIST,true,NULL,t);
	if (logFile.isOpen())
	{
		logFile.writeLine(str);
		logFile.flush();
	}
}

// --------------------------------------------------
void APICALL MyPeercastApp::updateSettings() //JP-MOD: Thread safe fix
{
	HWND hwnd = guiWnd;
	::SendNotifyMessage(hwnd, WM_UPDATESETTINGS, 0, 0);
}