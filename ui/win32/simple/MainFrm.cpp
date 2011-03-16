/*! @file	MainFrm.h
	@brief	メインフレーム
*/

#include "stdafx.h"
#include "MainFrm.h"
#include "Utility.h"
#include "ConfigDialog.h"

#include "channel.h"
#include "servent.h"
#include "servmgr.h"
#include "win32/wsys.h"
#include "peercast.h"
#include "stats.h"
#include "version2.h"

#if defined(NTDDI_VERSION) && (NTDDI_VERSION >= NTDDI_LONGHORN)
  #include <vssym32.h>
#else
  #include <tmschema.h>
#endif

#include <uxtheme.h>	// for XP
#pragma comment(lib, "uxtheme.lib")

#include <dwmapi.h>		// for Vista
#pragma comment(lib, "dwmapi.lib")

/// とりあえず定義はSimple.cppにある
extern ChanInfo chanInfo;
extern bool		chanInfoIsRelayed;
extern String	iniFileName;	// ?
extern HWND		guiWnd;
WINDOWPLACEMENT winPlace;



///////////////////////////////////////
// CTrayIcon

CTrayIcon*	CTrayIcon::s_pThis = NULL;

/// コンストラクタ
CTrayIcon::CTrayIcon() : m_hWndNotify(NULL), m_nNowQuality(-1)
{
	s_pThis = this;

	::SecureZeroMemory(&m_trayIcon, sizeof m_trayIcon);
	m_trayIcon.cbSize = sizeof (m_trayIcon);

	WM_TASKBARCREATED = RegisterWindowMessage(_T("TaskbarCreated"));
}

/// デストラクタ
CTrayIcon::~CTrayIcon()
{
	s_pThis = NULL;
}

/// エクスプローラーが再起動したのでトレイアイコンを作り直す
LRESULT CTrayIcon::OnTaskbarCreated(UINT uMsg, WPARAM wParam, LPARAM lParam)
{ 
	AddTrayIcon(m_hWndNotify);
	return 0;
}

/// タスクトレイにアイコンを登録する
void	CTrayIcon::AddTrayIcon(HWND hWnd)
{
	m_hWndNotify = hWnd;

	// アイコンを読み込む
	m_IconAntenna[0].LoadIcon(IDI_ANTENNA0);
	m_IconAntenna[1].LoadIcon(IDI_ANTENNA1);
	m_IconAntenna[2].LoadIcon(IDI_ANTENNA2);
	m_IconAntenna[3].LoadIcon(IDI_ANTENNA3);

	m_IconAntenna[4].LoadIcon(IDI_SMALL);

    m_trayIcon.hWnd = hWnd;
    m_trayIcon.uID = 100;
    m_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_trayIcon.uCallbackMessage = WM_TRAYICON;
    m_trayIcon.hIcon = m_IconAntenna[4];
    StringCchCopy(m_trayIcon.szTip, 64, _T("PeerCast"));


	/* タスクトレイにアイコンを登録する */
    ::Shell_NotifyIcon(NIM_ADD, &m_trayIcon);
}

/// タスクトレイからアイコンを削除
void	CTrayIcon::RemoveTrayIcon()
{
	/* タスクトレイからアイコンを削除 */
    ::Shell_NotifyIcon(NIM_DELETE, &m_trayIcon);
}

/// タスクトレイアイコンを変更する
void	CTrayIcon::ModifyIcon(int nQuality /*= 4*/)	// 4はデフォルトアイコン
{
	ATLASSERT(0 <= nQuality && nQuality <= 4);
	if (m_nNowQuality != nQuality) {
		m_trayIcon.uFlags = NIF_ICON;
		m_trayIcon.hIcon = m_IconAntenna[nQuality];
		::Shell_NotifyIcon(NIM_MODIFY, &m_trayIcon);

		m_nNowQuality = nQuality;
	}
}

/// ツールチップを表示する
void	CTrayIcon::ChannelPopup(LPCTSTR title, LPCTSTR msg, bool bBallonTip /*= true*/)
{
	ATLASSERT(title);
	ATLASSERT(msg);
	if (title[0] == _T('\0'))
		return;

	CString strboth;
	strboth.Format(_T(" (%s)"), title);

	m_trayIcon.uFlags = NIF_ICON | NIF_TIP;
	StringCchCopy(m_trayIcon.szTip, 128, strboth);
	//trayIcon.szTip[sizeof(trayIcon.szTip)-1]=0;

	if (bBallonTip) m_trayIcon.uFlags |= NIF_INFO;
	m_trayIcon.uTimeout = 10 * 1000;	// 10秒
	StringCchCopy(m_trayIcon.szInfoTitle, 64, title);
	StringCchCopy(m_trayIcon.szInfo, 256, msg);
	
	::Shell_NotifyIcon(NIM_MODIFY, &m_trayIcon);
}

/// ツールチップを消す
void	CTrayIcon::ClearChannelPopup()
{
	m_trayIcon.uFlags = NIF_ICON | NIF_INFO;
	m_trayIcon.uTimeout = 10 * 1000;
    m_trayIcon.szInfo[0] = _T('\0');
	m_trayIcon.szInfoTitle[0] = _T('\0');
	::Shell_NotifyIcon(NIM_MODIFY, &m_trayIcon);
}



typedef struct tagADDLOGINFO {
	String strMessage;
	int id;
	bool sel;
	void *data;
	LogBuffer::TYPE type;
} ADDLOGINFO, *LPADDLOGINFO;

void ADDLOG(const char *str, int id, bool sel, void *data, LogBuffer::TYPE type) //JP-MOD: Thread safe fix
{
	HWND hwnd = guiWnd;

	String sjis; //JP-Patch
	sjis = str;
	sjis.convertTo(String::T_SJIS);

	if (type != LogBuffer::T_NONE)
	{
#if _DEBUG
		::OutputDebugString(sjis.cstr());
		::OutputDebugString("\n");
#endif
	}

	if(::IsWindow(hwnd))
	{
		LPADDLOGINFO pInfo = new ADDLOGINFO;

		pInfo->strMessage = sjis;
		pInfo->id = id;
		pInfo->sel = sel;
		pInfo->data = data;
		pInfo->type = type;

		//他スレッドへのSendMessageはデッドロックを引き起こす場合があり危険なので
		//非同期メッセージ関数を使用する(メモリの解放は受け取り側で行う)
		if(!::SendNotifyMessage(hwnd, WM_ADDLOG, (WPARAM)pInfo, 0))
			delete pInfo;
	}
}



namespace {

	class WSingleLock {
	public:
		WSingleLock(WLock *lock) : m_pLock(lock) { m_pLock->on(); }
		~WSingleLock()							 { m_pLock->off(); }
	private:
		WLock *m_pLock;
	};

	class HostName {
	private:
		unsigned int	ip;
		String name;
	public:
		HostName() : ip(0) {}

		const char *set(Host& host) throw()
		{
			if(ip != host.ip)
			{
				ip = host.ip;

				if(!(servMgr->enableGetName && host.globalIP() && ClientSocket::getHostname(name.cstr(), String::MAX_LEN, ip)))
					host.IPtoStr(name.cstr());
			}

			return name.cstr();
		}

		const char *cstr() throw()
		{
			return name.cstr();
		}
	};


	struct ListData {
		int channel_id;
		char name[21];
		int bitRate;
		int status;
		const char *statusStr;
		int totalListeners;
		int totalRelays;
		int totalClaps; //JP-MOD
		int localListeners;
		int localRelays;
		bool stayConnected;
		bool stealth; //JP-MOD
		int linkQuality; //JP-MOD
		unsigned int skipCount; //JP-MOD
		ChanHit chDisp;
		bool bTracker;

		bool flg;
		ListData *next;
	};

	struct ServentData {
		int servent_id;
		unsigned int tnum;
		int type;
		int status;
		String agent;
		Host h;
		HostName hostName; //JP-MOD
		unsigned int syncpos;
		char *typeStr;
		char *statusStr;
		bool infoFlg;
		bool relay;
		bool firewalled;
		unsigned int numRelays;
		unsigned int totalRelays;
		unsigned int totalListeners;
		int vp_ver;
		char ver_ex_prefix[2];
		int ver_ex_number;

		bool flg;
		ServentData *next;

		unsigned int lastSkipTime;
		unsigned int lastSkipCount;
	};
#if 0
	typedef struct tagADDLOGINFO {
		String strMessage;
		int id;
		bool sel;
		void *data;
		LogBuffer::TYPE type;
	} ADDLOGINFO, *LPADDLOGINFO;
#endif
	// グローバル変数
	ThreadInfo guiThread;
	bool sleep_skip = false;
	ListData *list_top = NULL;
	WLock ld_lock;
	ServentData *servent_top = NULL;
	WLock sd_lock;
	GnuID selchanID;
	WLock selchanID_lock;

	// --------------------------------------------------
	int convertScaleX(int src) //JP-MOD
	{
		struct Local
		{
			static double get()
			{
				HDC hScreen = GetDC(0);
				if(!hScreen)
					return 1.0;

				double ret = GetDeviceCaps(hScreen, LOGPIXELSX) / 96.0;
				ReleaseDC(0, hScreen);

				return ret;
			}
		};

		static double scaleX = Local::get();

		return (int)(src * scaleX);
	}
	#define convertScaleY(y)	convertScaleX(y)

	// --------------------------------------------------
	INT_PTR pp_formatTitle(char *str, size_t size, bool minimized)
	{
		const char *format = minimized ? servMgr->guiTitleModifyMinimized.cstr() : servMgr->guiTitleModifyNormal.cstr();
		size_t destPos = 0;

		enum
		{
			normal,
			percent,
			variable
		} state = normal;

		char partStr[16];
		size_t partPos = 0;
		unsigned int level = 0;

		enum
		{
			error = -1,
			rx,
			tx
		} var = error;

		struct
		{
			double prefix;
			int precision;
		} rxtx;
		memset(&rxtx, 0, sizeof(rxtx));

		for(;*format != '\0' && destPos < size && partPos < sizeof(partStr);format++)
		{
			switch(state)
			{
			case normal:
				if(*format == '%')
					state = percent;
				else
					str[destPos++] = *format;
				break;
			case percent:
				if(*format == '%'){
					state = normal;
					str[destPos++] = *format;
					break;
				}

				partPos = level = rxtx.precision = 0;
				rxtx.prefix = 1024.0f / 8;
				var = error;
				state = variable;
				// breakなしで続行
			case variable:
				switch(*format){
				case '%':
					state = normal;
					// breakなしで続行
				case '.':
					partStr[partPos] = '\0';

					switch(level++){
					case 0:
						if(!strcmp(partStr, "rx"))
							var = rx;
						else if(!strcmp(partStr, "tx"))
							var = tx;
						else
							return -1;

						break;
					case 1:
						{
							const char *readPointer = partStr;

							switch(*readPointer){
							case 'k':
								readPointer++;
								// breakなしで続行
							case '\0':
								rxtx.prefix = 1024.0f / 8;
								break;
							case 'm':
								readPointer++;
								rxtx.prefix = 1024.0f * 1024 / 8;
								break;
							default:
								rxtx.prefix = 1.0f / 8;
								break;
							}

							if(!strcmp(readPointer, "bytes"))
								rxtx.prefix *= 8;
						}
						break;
					case 2:
						rxtx.precision = atoi(partStr);
						break;
					default:
						return -1;
					}

					partPos = 0;

					if(state == normal){
						int bandwidth = (var == rx) ?	stats.getPerSecond(Stats::BYTESIN) - stats.getPerSecond(Stats::LOCALBYTESIN):
														stats.getPerSecond(Stats::BYTESOUT) - stats.getPerSecond(Stats::LOCALBYTESOUT);
						int ret = _snprintf_s(&str[destPos], size - destPos, _TRUNCATE, "%.*f", rxtx.precision, bandwidth / rxtx.prefix);
						if(ret > 0)
							destPos += ret;
					}
					break;
				default:
					if(!isalnum(*format))
						return -1;

					partStr[partPos++] = *format;
					break;
				}
				break;
			}
		}

		str[destPos] = '\0';
		return (state == normal) ? destPos : -1;
	}


	// --------------------------------------------------
	int checkLinkQuality(Channel *c) //JP-MOD
	{
		if(!c->isPlaying())
			return -1;
		if(!c->skipCount)
			return 3;

		unsigned int timeago = sys->getTime() - c->lastSkipTime;

		if(timeago >= 120){
			c->skipCount = 0;
			return 3;
		}

		int ret = (timeago / 4) - (c->skipCount / 2);

		return	ret < 0 ?	0:
				ret < 2 ?	ret:
							2;
	}

	// --------------------------------------------------
	THREAD_PROC showConnections(ThreadInfo *thread)
	{
		HWND hwnd = (HWND)thread->data;
		//	bool shownChannels = false;
		//	thread->lock();

		while (thread->active) {
			int diff = 0;
			bool changed = false; //JP-MOD

			LRESULT sel = ListView_GetNextItem(::GetDlgItem(hwnd, IDC_CONNECTLIST), -1, LVNI_FOCUSED | LVNI_SELECTED); //JP-MOD

			ServentData *sd = servent_top;
			for (; sd; sd = sd->next) {
				sd->flg = false;
			}

			{ //JP-MOD
				GnuID show_chanID;
				{ //JP-MOD: Thread safe fix
					WSingleLock lock(&selchanID_lock);
					show_chanID = selchanID;
				}

				WSingleLock lock(&servMgr->lock);
				Servent *s = servMgr->servents;
				for (; s; s = s->next) {
					bool foundFlg = false;
					bool infoFlg = false;
					bool relay = true;
					bool firewalled = false;
					unsigned int numRelays = 0;
					unsigned int totalRelays = 0;
					unsigned int totalListeners = 0;
					int vp_ver = 0;
					char ver_ex_prefix[2] = {0};
					int ver_ex_number = 0;

					// for PCRaw (connection list) start
					if (show_chanID.isSet() && !show_chanID.isSame(s->chanID))
					{
						continue;
					}
					// for PCRaw (connection list) end

					//JP-MOD	接続リストを簡易表示にするがオンならスキップ
					if (servMgr->guiSimpleConnectionList && (
						((s->type == Servent::T_DIRECT) && (s->status == Servent::S_CONNECTED)) ||
						((s->type == Servent::T_SERVER) && (s->status == Servent::S_LISTENING))
						)) {
							continue;
					}

					if (s->type != Servent::T_NONE){

						WSingleLock lock(&chanMgr->hitlistlock); //JP-MOD
						ChanHitList *chl = chanMgr->findHitListByID(s->chanID);

						if (chl){
							ChanHit *hit = chl->hit;
							for (; hit; hit = hit->next){
								if (hit->servent_id == s->servent_id){
									if ((hit->numHops == 1)/* && (hit->host.ip == s->getHost().ip)*/){
										infoFlg = true;
										relay = hit->relay;
										firewalled = hit->firewalled;
										numRelays = hit->numRelays;
										vp_ver = hit->version_vp;
										ver_ex_prefix[0] = hit->version_ex_prefix[0];
										ver_ex_prefix[1] = hit->version_ex_prefix[1];
										ver_ex_number = hit->version_ex_number;
									}
									totalRelays += hit->numRelays;
									totalListeners += hit->numListeners;
								}
							}
						}
					}

					{ //JP-MOD
						WSingleLock lock(&sd_lock); //JP-MOD: new lock
						ServentData *sd = servent_top;

						for (; sd; sd = sd->next){
							if (sd->servent_id == s->servent_id){
								foundFlg = true;
								if (s->thread.finish) break;
								sd->flg = true;
								sd->type = s->type;
								sd->status = s->status;
								sd->agent = s->agent;
								sd->h = s->getHost();
								sd->hostName.set(sd->h);
								sd->syncpos = s->syncPos;
								sd->tnum = (s->lastConnect) ? sys->getTime()-s->lastConnect : 0;
								sd->typeStr = s->getTypeStr();
								sd->statusStr = s->getStatusStr();
								sd->infoFlg = infoFlg;
								sd->relay = relay;
								sd->firewalled = firewalled;
								sd->numRelays = numRelays;
								sd->totalRelays = totalRelays;
								sd->totalListeners = totalListeners;
								sd->vp_ver = vp_ver;
								sd->lastSkipTime = s->lastSkipTime;
								sd->lastSkipCount = s->lastSkipCount;
								sd->ver_ex_prefix[0] = ver_ex_prefix[0];
								sd->ver_ex_prefix[1] = ver_ex_prefix[1];
								sd->ver_ex_number = ver_ex_number;
								break;
							}
						}

						if (!foundFlg && (s->type != Servent::T_NONE) && !s->thread.finish){
							ServentData *newData = new ServentData();
							newData->next = servent_top;
							servent_top = newData;
							newData->flg = true;
							newData->servent_id = s->servent_id;
							newData->type = s->type;
							newData->status = s->status;
							newData->agent = s->agent;
							newData->h = s->getHost();
							newData->hostName.set(newData->h);
							newData->syncpos = s->syncPos;
							newData->tnum = (s->lastConnect) ? sys->getTime()-s->lastConnect : 0;
							newData->typeStr = s->getTypeStr();
							newData->statusStr = s->getStatusStr();
							newData->infoFlg = infoFlg;
							newData->relay = relay;
							newData->firewalled = firewalled;
							newData->numRelays = numRelays;
							newData->totalRelays = totalRelays;
							newData->totalListeners = totalListeners;
							newData->vp_ver = vp_ver;
							newData->lastSkipTime = s->lastSkipTime;
							newData->lastSkipCount = s->lastSkipCount;
							newData->ver_ex_prefix[0] = ver_ex_prefix[0];
							newData->ver_ex_prefix[1] = ver_ex_prefix[1];
							newData->ver_ex_number = ver_ex_number;

							diff++;
							changed = true;
						}
					}
				}
			}

			{ //JP-MOD
				int idx = 0;

				{
					WSingleLock lock(&sd_lock);
					ServentData *sd = servent_top;
					ServentData *prev = NULL;
					while (sd) {
						if (!sd->flg || (sd->type == Servent::T_NONE)) {
							ServentData *next = sd->next;
							if (!prev) {
								servent_top = next;
							} else {
								prev->next = next;
							}
							delete sd;

							changed = true;

							sd = next;
							//				diff--;
						} else {
							idx++;
							prev = sd;
							sd = sd->next;
						}
					}
				}

				if ((sel >= 0) && (diff != 0))
					ListView_SetItemState(::GetDlgItem(hwnd, IDC_CONNECTLIST), sel + diff, LVNI_FOCUSED | LVIS_SELECTED, LVNI_FOCUSED | LVIS_SELECTED);
				ListView_SetItemCountEx(::GetDlgItem(hwnd, IDC_CONNECTLIST), idx, LVSICF_NOSCROLL);
			}

			{
				ListData *ld = list_top;
				for (; ld; ld = ld->next) {
					ld->flg = false;
				}

				{ //JP-MOD
					WSingleLock lock(&chanMgr->channellock); //JP-MOD: new lock
					Channel *c = chanMgr->channel;
					for (; c; c = c->next) {
						bool foundFlg = false;
						String sjis;
						sjis = c->getName();
						sjis.convertTo(String::T_SJIS);

						{
							WSingleLock lock(&ld_lock); //JP-MOD: new lock
							ListData *ld = list_top;
							for (; ld; ld = ld->next){
								if (ld->channel_id == c->channel_id){
									foundFlg = true;
									if (c->thread.finish) break;
									ld->flg = true;
									strncpy(ld->name, sjis, 20);
									ld->name[20] = '\0';
									ld->bitRate = c->info.bitrate;
									ld->status = c->status;
									ld->statusStr = c->getStatusStr();
									ld->totalListeners = c->totalListeners();
									ld->totalRelays = c->totalRelays();
									ld->totalClaps = c->totalClaps();
									ld->localListeners = c->localListeners();
									ld->localRelays = c->localRelays();
									ld->stayConnected = c->stayConnected;
									ld->stealth = c->stealth;
									ld->linkQuality = checkLinkQuality(c);
									ld->skipCount = c->skipCount;
									ld->chDisp = c->chDisp;
									ld->bTracker = c->sourceHost.tracker;
									break;
								}
							}
							if (!foundFlg && !c->thread.finish){
								ListData *newData = new ListData();
								newData->next = list_top;
								list_top = newData;
								newData->flg = true;
								newData->channel_id = c->channel_id;
								strncpy(newData->name, sjis, 20);
								newData->name[20] = '\0';
								newData->bitRate = c->info.bitrate;
								newData->status = c->status;
								newData->statusStr = c->getStatusStr();
								newData->totalListeners = c->totalListeners();
								newData->totalRelays = c->totalRelays();
								newData->totalClaps = c->totalClaps();
								newData->localListeners = c->localListeners();
								newData->localRelays = c->localRelays();
								newData->stayConnected = c->stayConnected;
								newData->stealth = c->stealth;
								newData->linkQuality = checkLinkQuality(c);
								newData->skipCount = c->skipCount;
								newData->chDisp = c->chDisp;
								newData->bTracker = c->sourceHost.tracker;

								changed = true;
							}
						}
					}
				}

				{ //JP-MOD
					int idx = 0;
					{
						WSingleLock lock(&ld_lock);
						ListData *ld = list_top;
						ListData *prev = NULL;

						while(ld){
							if (!ld->flg){
								ListData *next = ld->next;
								if (!prev){
									list_top = next;
								} else {
									prev->next = next;
								}
								delete ld;

								changed = true;
								ld = next;
							} else {
								idx++;
								prev = ld;
								ld = ld->next;
							}
						}
					}
					ListView_SetItemCountEx(::GetDlgItem(hwnd, IDC_CHANNELLIST), idx, LVSICF_NOSCROLL);
				}

				if (changed &&
					(servMgr->guiConnListDisplays < 0 ||
					servMgr->guiChanListDisplays < 0) ) 
				{
					::PostMessage(hwnd, WM_REFRESH, 0, 0);
				}
			}
	#if 0
			bool update = ((sys->getTime() - chanMgr->lastHit) < 3)||(!shownChannels);

			if (update)
			{
				shownChannels = true;

				sel = ::SendDlgItemMessage(hwnd, hitID,LB_GETCURSEL, 0, 0);
				LRESULT top = ::SendDlgItemMessage(hwnd, hitID,LB_GETTOPINDEX, 0, 0);
				::SendDlgItemMessage(hwnd, hitID, LB_RESETCONTENT, 0, 0);

				{
					WSingleLock lock(&chanMgr->hitlistlock); //JP-MOD
					ChanHitList *chl = chanMgr->hitlist;

					while (chl)
					{
						if (chl->isUsed())
						{
							if (chl->info.match(chanMgr->searchInfo))
							{
								char cname[34];
								strncpy(cname,chl->info.name.cstr(),16);
								cname[16] = 0;
								ADDHIT(chl,"%s - %d kb/s - %d/%d",cname,chl->info.bitrate,chl->numListeners(),chl->numHits());
							}
						}
						chl = chl->next;
					}
				}

				if (sel >= 0)
					::SendDlgItemMessage(hwnd, hitID,LB_SETCURSEL, sel, 0);
				if (top >= 0)
					::SendDlgItemMessage(hwnd, hitID,LB_SETTOPINDEX, top, 0);
			}

			{
				switch (servMgr->getFirewall())
				{
				case ServMgr::FW_ON:
					::SendDlgItemMessage(hwnd, IDC_EDIT4,WM_SETTEXT, 0, (LPARAM)"Firewalled");
					break;
				case ServMgr::FW_UNKNOWN:
					::SendDlgItemMessage(hwnd, IDC_EDIT4,WM_SETTEXT, 0, (LPARAM)"Unknown");
					break;
				case ServMgr::FW_OFF:
					::SendDlgItemMessage(hwnd, IDC_EDIT4,WM_SETTEXT, 0, (LPARAM)"Normal");
					break;
				}
			}
	#endif
			char buf[64]; //JP-MOD	表示を動的に変更するがオンのとき
			if (servMgr->guiTitleModify && pp_formatTitle(buf, sizeof(buf), ::IsIconic(hwnd) != 0) > 0)
				::SetWindowText(hwnd, buf);

			// sleep for 1 second .. check every 1/10th for shutdown
			for (int i=0; i<10; i++) {
				if (sleep_skip) {	// for PCRaw (connection list)
					sleep_skip = false;
					break;	// スリープを抜ける
				}

				if (!thread->active)
					break;
				sys->sleep(100);
			}
		}
		
		
		// ループを抜けたので後始末?

		ListData *ld = list_top;
		while (ld) {
			ListData *next;
			next = ld->next;

			delete ld;

			ld = next;
		}
		list_top = NULL;

		ServentData *sd = servent_top;
		while (sd) {
			ServentData *next;
			next = sd->next;

			delete sd;

			sd = next;
		}
		servent_top = NULL;

		//	thread->unlock();
		return 0;
	}

	// --------------------------------------------------
	ListData *getListData(DWORD_PTR index) //JP-MOD
	{
		ListData *ld;
		for(ld = list_top;
			ld != NULL && index;
			ld = ld->next, --index);

		return ld;
	}

	// --------------------------------------------------
	ServentData *getServentData(DWORD_PTR index) //JP-MOD
	{
		ServentData *sd;
		for(sd = servent_top;
			sd != NULL && index;
			sd = sd->next, --index);

		return sd;
	}

	// --------------------------------------------------
	ChanInfo getChannelInfo(int index)
	{
		Channel *c = chanMgr->findChannelByIndex(index);
		if (c)
			return c->info;

		ChanInfo info;
		return info;
	}

	// --------------------------------------------------
	void flipNotifyPopup(int id, ServMgr::NOTIFY_TYPE nt)
	{
		int mask = peercastInst->getNotifyMask();
		mask ^= nt;
		peercastInst->setNotifyMask(mask);
		peercastInst->saveSettings();
	}

	// --------------------------------------------------
	void showHTML(const char *file)
	{
		char url[256];
		sprintf(url,"%s/%s",servMgr->htmlPath,file);					

	//	sys->callLocalURL(url,servMgr->serverHost.port);
		sys->callLocalURL(url,	// for PCRaw (url)
			(servMgr->allowServer1&Servent::ALLOW_HTML)?(servMgr->serverHost.port):(servMgr->serverHost.port+1));
	}

	/// [Info]ダイアログ
	class CChannelInfoDialog : public CDialogImpl<CChannelInfoDialog>
	{
	public:
		enum { IDD = IDD_CHANINFO };

		BEGIN_MSG_MAP_EX( CChannelInfoDialog )
			MSG_WM_INITDIALOG( OnInitDialog )
			MSG_WM_COMMAND( OnCommand )
			MSG_WM_CLOSE( OnClose )
		END_MSG_MAP()

		BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
		{
			char str[1024];
			//strcpy(str,chanInfo.track.artist.cstr());
			strcpy(str,chanInfo.track.artist); //JP-Patch
			strcat(str," - ");
			//strcat(str,chanInfo.track.title.cstr());
			strcat(str,chanInfo.track.title);

			String name,track,comment,desc,genre; //JP-Patch
			name = chanInfo.name; //JP-Patch
			track = str; //JP-Patch
			comment = chanInfo.comment; //JP-Patch
			desc = chanInfo.desc; //JP-Patc
			genre = chanInfo.genre; //JP-Patch
			name.convertTo(String::T_SJIS); //JP-Patc
			track.convertTo(String::T_SJIS); //JP-Patch
			comment.convertTo(String::T_SJIS); //JP-Patch
			desc.convertTo(String::T_SJIS); //JP-Patch
			genre.convertTo(String::T_SJIS); //JP-Patch
				
			//SendDlgItemMessage(hDlg,IDC_EDIT_NAME,WM_SETTEXT,0,(LONG)chanInfo.name.cstr());
			SendDlgItemMessage(IDC_EDIT_NAME,WM_SETTEXT,0,(LPARAM)name.cstr()); //JP-Patch
			//SendDlgItemMessage(hDlg,IDC_EDIT_PLAYING,WM_SETTEXT,0,(LONG)str);
			SendDlgItemMessage(IDC_EDIT_PLAYING,WM_SETTEXT,0,(LPARAM)track.cstr()); //JP-Patch
			//SendDlgItemMessage(hDlg,IDC_EDIT_MESSAGE,WM_SETTEXT,0,(LONG)chanInfo.comment.cstr());
			SendDlgItemMessage(IDC_EDIT_MESSAGE,WM_SETTEXT,0,(LPARAM)comment.cstr()); //JP-Patch
			//SendDlgItemMessage(hDlg,IDC_EDIT_DESC,WM_SETTEXT,0,(LONG)chanInfo.desc.cstr());
			SendDlgItemMessage(IDC_EDIT_DESC,WM_SETTEXT,0,(LPARAM)desc.cstr()); //JP-Patch
			//SendDlgItemMessage(hDlg,IDC_EDIT_GENRE,WM_SETTEXT,0,(LONG)chanInfo.genre.cstr());
			SendDlgItemMessage(IDC_EDIT_GENRE,WM_SETTEXT,0,(LPARAM)genre.cstr()); //JP-Patch

			sprintf(str,"%d kb/s %s",chanInfo.bitrate,ChanInfo::getTypeStr(chanInfo.contentType));
			SendDlgItemMessage(IDC_FORMAT,WM_SETTEXT,0,(LPARAM)str);


			if (!chanInfo.url.isValidURL())
				::EnableWindow(GetDlgItem(IDC_CONTACT), false);

			Channel *ch = chanMgr->findChannelByID(chanInfo.id);
			if (ch) {
				SendDlgItemMessage(IDC_EDIT_STATUS,WM_SETTEXT,0,(LPARAM)ch->getStatusStr());
				SendDlgItemMessage( IDC_KEEP,BM_SETCHECK, ch->stayConnected, 0);
			} else {
				SendDlgItemMessage(IDC_EDIT_STATUS,WM_SETTEXT,0,(LPARAM)"OK");
				::EnableWindow(GetDlgItem(IDC_KEEP), false);
			}



			POINT point;
			RECT rect,drect;
			HWND hDsk = GetDesktopWindow();
			::GetWindowRect(hDsk,&drect);
			GetWindowRect(&rect);
			GetCursorPos(&point);

			POINT pos,size;
			size.x = rect.right-rect.left;
			size.y = rect.bottom-rect.top;

			if (point.x-drect.left < size.x)
				pos.x = point.x;
			else
				pos.x = point.x-size.x;

			if (point.y-drect.top < size.y)
				pos.y = point.y;
			else
				pos.y = point.y-size.y;

			SetWindowPos(HWND_TOPMOST, pos.x, pos.y, size.x, size.y, 0);

			return TRUE;
		}
		void OnCommand(UINT uNotifyCode, int nID, CWindow wndCtl)
		{
			TCHAR str[1024],idstr[64];
			chanInfo.id.toStr(idstr);

			switch (nID) {
			case IDC_CONTACT: sys->getURL(chanInfo.url); break;
			case IDC_DETAILS:
				sprintf(str, _T("admin?page=chaninfo&id=%s&relay=%d"), idstr, chanInfoIsRelayed);
				sys->callLocalURL(str, servMgr->serverHost.port);
				break;
			case IDC_KEEP: {
				Channel *ch = chanMgr->findChannelByID(chanInfo.id);
				if (ch)
					ch->stayConnected = (SendDlgItemMessage(IDC_KEEP,BM_GETCHECK, 0, 0) == BST_CHECKED);
				break;
			}
			case IDC_PLAY: chanMgr->findAndPlayChannel(chanInfo, false); break;
			}
		}
		void OnClose() { EndDialog(0); }
	};

	/// バージョン情報ダイアログ
	class CAboutDialog : public CDialogImpl<CAboutDialog>
	{
	public:
		enum { IDD = IDD_ABOUTBOX };

		BEGIN_MSG_MAP_EX( CAboutDialog )
			MSG_WM_INITDIALOG( OnInitDialog )
			MSG_WM_COMMAND( OnCommand )
		END_MSG_MAP()

		BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
		{
#ifdef VERSION_EX
			SendDlgItemMessage(IDC_ABOUTVER,WM_SETTEXT,0,(LPARAM)PCX_AGENTEX);
#else
			SendDlgItemMessage(IDC_ABOUTVER,WM_SETTEXT,0,(LPARAM)PCX_AGENTVP);
#endif
			return TRUE;
		}
		void OnCommand(UINT uNotifyCode, int nID, CWindow wndCtl)
		{
			switch (nID) {
			case IDOK:
			case IDCANCEL:
				EndDialog(nID);
				break;
			case IDC_BUTTON1:
				sys->getURL("http://www.peercast.org");
				EndDialog(nID);
			}
		}
	};

}


//////////////////////////////////////////////////
// class CListViewChan

/// チャンネルリストがミドルクリックされた
void	CListViewChan::OnMButtonDown(UINT nFlags, CPoint point)
{
	UINT uFlags;
	int nIndex = HitTest(point, &uFlags);
	if (nIndex != -1) {	// コンタクトURLを開く
		GetTopLevelWindow().SendMessage(WM_COMMAND, ID_URL_CMD + nIndex);
	}
}


//////////////////////////////////////////////////
// class CMainFrame

/// コンストラクタ
CMainFrame::CMainFrame() : 
	m_wndMessage(_T("PeerCastTaskTrayNotifyWindow"),this, 1)
{ }


BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
	if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return FALSE;//m_view.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle()
{
	UIUpdateToolBar();
	return FALSE;
}


/// ウィンドウの配置を更新する
void CMainFrame::UpdateLayout(BOOL bResizeBars)
{
	RECT rect = { 0 };
	GetClientRect(&rect);

	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);

	if (m_ListChan.IsWindow() == FALSE)
		return;

	// resize client window
	HDWP hDwp = ::BeginDeferWindowPos(3);
	ATLASSERT(hDwp);
	{
		int cyDoubleEdge = ::GetSystemMetrics(SM_CYEDGE) * 2;
		int y = rect.top;

		{
			DWORD dwSize = m_ListChan.ApproximateViewRect(-1, -1, (servMgr->guiChanListDisplays > -1) ? servMgr->guiChanListDisplays : -1);
			int nHeight = HIWORD(dwSize) + cyDoubleEdge;
			hDwp = ::DeferWindowPos(hDwp, m_ListChan, HWND_BOTTOM , 0, y, rect.right, nHeight, SWP_SHOWWINDOW);
			y += nHeight;
		}

		{
			DWORD dwSize = m_ListConnect.ApproximateViewRect(-1, -1, (servMgr->guiConnListDisplays > -1) ? servMgr->guiConnListDisplays : -1);
			int nHeight = HIWORD(dwSize) + cyDoubleEdge;
			hDwp = ::DeferWindowPos(hDwp, m_ListConnect, HWND_BOTTOM , 0, y,  rect.right, nHeight, SWP_SHOWWINDOW);
			y += nHeight;

			nHeight = rect.bottom - y;
			hDwp = ::DeferWindowPos(hDwp, m_LogList, HWND_BOTTOM , 0, y,  rect.right, nHeight, SWP_SHOWWINDOW);
		}
	}
	::EndDeferWindowPos(hDwp);

	/*if(m_hWndClient != NULL)
		::SetWindowPos(m_hWndClient, NULL, rect.left, rect.top,
			rect.right - rect.left, rect.bottom - rect.top,
			SWP_NOZORDER | SWP_NOACTIVATE);*/
}


/// ウィンドウ初期化
int		CMainFrame::OnCreate(LPCREATESTRUCT /*lpCreateStruct*/)
{
    // 大きいアイコン設定
    HICON hIcon = AtlLoadIconImage(IDI_SIMPLE, LR_DEFAULTCOLOR,
        ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
    SetIcon(hIcon, TRUE);
        
    // 小さいアイコン設定
    HICON hIconSmall = AtlLoadIconImage(IDI_SIMPLE, LR_DEFAULTCOLOR,
        ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
    SetIcon(hIconSmall, FALSE);

	{
		WSingleLock lock(&selchanID_lock);
		selchanID.clear();
	}
	
	_CreateControls();
	_InitToolBar();
	UIAddToolBar(m_ToolBar);	//
	_InitListView();
	_InitSysMenu();

	peercastApp->updateSettings();

	if(servMgr->autoServe) {
		m_ToolBar.CheckButton(IDC_CHECK1, TRUE);
		SendMessage(WM_COMMAND, IDC_CHECK1);
	}
	if(servMgr->autoConnect) {
		//setButtonStateEx(hwnd, IDC_CHECK2, true);
		SendMessage(WM_COMMAND, IDC_CHECK2);
	}

	guiThread.func = showConnections;
	guiThread.data = m_hWnd;
	if (!sys->startThread(&guiThread)) {
		ATLASSERT(FALSE);
		LOG_ERROR("Worker thread creation failed.");
		::MessageBox(m_hWnd, _T("GUIが開始できません"), NULL ,MB_OK | MB_ICONERROR);
		return FALSE;
	}

	if (servMgr->saveGuiPos)
		SetWindowPlacement(&winPlace);
	/* とりあえずログを強制的に有効化 */
	servMgr->pauseLog = false;
	extern HWND guiWnd;
	guiWnd = m_hWnd;

	/* タスクトレイにアイコンを登録 */
	CWindow wnd;
	wnd.Create(_T("Button"), NULL, 0, 0, WS_POPUP);
	ATLASSERT(wnd.IsWindow());
	m_wndMessage.SubclassWindow(wnd);	
	m_TrayIcon.AddTrayIcon(m_wndMessage);

	// メッセージ フィルターおよび画面更新用のオブジェクト登録
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	return 0;
}

/// ウィンドウが閉じられるとき
void	CMainFrame::OnClose()
{
	GetWindowPlacement(&winPlace);

	if (MessageBox(_T("終了しますか？"), _T("確認"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
		SetMsgHandled(false);

		return;
	}
	
}

/// ウィンドウ破棄時
void	CMainFrame::OnDestroy()
{
	SetMsgHandled(FALSE);

	guiThread.active = false;

	/* タスクトレイからアイコンを削除する */
	m_TrayIcon.RemoveTrayIcon();
	m_wndMessage.UnsubclassWindow();

	// メッセージ フィルターおよび画面更新用のオブジェクト登録解除
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);
}

void	CMainFrame::OnSysCommand(UINT nID, CPoint pt)
{
	if (nID == IDC_CONFIG) {
		CConfigSheet	config;
		if (config.Show(m_hWnd) == IDOK) {
			_SetControls();
			OnRefresh(0, 0, 0);
			if (servMgr->guiAntennaNotifyIcon)
				PostMessage(WM_ANTENNA);
		}
		return ;
	}
	if (nID == SC_MINIMIZE) {
		if (servMgr->guiMinimizeStore) {
			ShowWindow(SW_HIDE);
			return ;
		}
	}
	SetMsgHandled(false);
}

BOOL	CMainFrame::OnCopyData(CWindow wnd, PCOPYDATASTRUCT pCopyDataStruct)
{
	LOG_DEBUG("URL request: %s", pCopyDataStruct->lpData);
	if (pCopyDataStruct->dwData == WM_PLAYCHANNEL) {
		ChanInfo info;
		servMgr->procConnectArgs((char *)pCopyDataStruct->lpData,info);
		chanMgr->findAndPlayChannel(info, false);
	}

	return TRUE;
}

LRESULT CMainFrame::OnChanListItemChanged(LPNMHDR pnmh)
{
	WSingleLock lock(&chanMgr->channellock);
	Channel *channel = _GetSelectedChannel();
	{
		WSingleLock lock(&selchanID_lock);
		if (channel)
			selchanID = channel->info.id;
		else
			selchanID.clear();
	}
	return 0;
}

LRESULT CMainFrame::OnChanListGetDispInfo(LPNMHDR pnmh)
{
	NMLVDISPINFO* nmv = (NMLVDISPINFO*)pnmh;
	LVITEM &lvItem = nmv->item;

	WSingleLock lock(&ld_lock);
	ListData *ld = getListData(lvItem.iItem);
	if(ld == NULL)
		return 0;

	if(lvItem.mask & LVIF_TEXT)
	{
		switch(lvItem.iSubItem)
		{
		case 0:
			strncpy_s(lvItem.pszText, lvItem.cchTextMax, ld->name, _TRUNCATE);
			break;
		case 1:
			_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "%dkbps", ld->bitRate);
			break;
		case 2:
			strncpy_s(lvItem.pszText, lvItem.cchTextMax, ld->statusStr, _TRUNCATE);
			break;
		case 3:
			_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "%d / %d", ld->totalListeners, ld->totalRelays);
			break;
		case 4:
			_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "%d / %d", ld->localListeners, ld->localRelays);
			break;
		case 5:
#ifdef PP_PUBLISH
			if(ld->status == Channel::S_BROADCASTING)
#endif /* PP_PUBLISH */
				_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "%d", ld->totalClaps);
			break;
		case 6:
			_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "%d", ld->skipCount);
			break;
		}
	}

	if(lvItem.mask & LVIF_IMAGE)
	{
		switch(lvItem.iSubItem)
		{
		case 0:
			lvItem.iImage =
				(ld->status == Channel::S_RECEIVING) ?
				(ld->chDisp.status == Channel::S_RECEIVING) ?
				(ld->chDisp.relay) ? 2
				: (ld->chDisp.numRelays) ? 3
				: 4
				: 1
				: 0;
			break;
		case 1:
			if(ld->linkQuality >= 0)
				lvItem.iImage = 5 + ld->linkQuality;
			break;
		}
	}
	return 0;
}

/// チャンネルリストクリック
LRESULT	CMainFrame::OnChanListClick(LPNMHDR pnmh)
{
	LPNMITEMACTIVATE pnmItem = (LPNMITEMACTIVATE)pnmh;
	switch (pnmItem->iSubItem) {
	case 4: {
		int currentOverrideMaxRelaysPerChannel = chanMgr->maxRelaysPerChannel;
		{
			WSingleLock lock(&chanMgr->channellock);
			Channel *channel = _GetSelectedChannel();

			if (channel && channel->overrideMaxRelaysPerChannel >= 1)
				currentOverrideMaxRelaysPerChannel = channel->overrideMaxRelaysPerChannel;
		}

		TPMPARAMS tpmParams = { sizeof(TPMPARAMS) };
		if (m_ListChan.GetSubItemRect(pnmItem->iItem, pnmItem->iSubItem, LVIR_BOUNDS, &tpmParams.rcExclude)) {
			CMenu menu;
			menu.CreatePopupMenu();
			if(menu.m_hMenu == NULL)
				break;

			MENUITEMINFO menuInfo = { sizeof(MENUITEMINFO) };
			menuInfo.fMask = MIIM_ID | MIIM_STATE | MIIM_TYPE;
			menuInfo.fType = MFT_STRING;

			// maxRelaysまでメニューアイテムを追加していく
			for (menuInfo.wID = 1;
				 menuInfo.wID <= servMgr->maxRelays;
				 menuInfo.wID++) 
			{
				CString strNum;
				strNum.Format(_T("%d"), menuInfo.wID);

				menuInfo.fState = (menuInfo.wID == currentOverrideMaxRelaysPerChannel) ? MFS_CHECKED : MFS_UNCHECKED;
				menuInfo.dwTypeData = strNum.GetBuffer(0);
				menu.InsertMenuItem(-1, true, &menuInfo);
			}

			{
				m_ListChan.ClientToScreen(&tpmParams.rcExclude);
				// ポップアップ表示
				DWORD dwResult = menu.TrackPopupMenuEx(TPM_LEFTALIGN | TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_VERTICAL,
										tpmParams.rcExclude.left, tpmParams.rcExclude.bottom, m_ListChan, &tpmParams);
				::PostMessage(m_hWnd, WM_NULL, 0, 0);

				if(dwResult >= 1) {	// 設定
					_EnumSelectedChannels([dwResult](Channel* c) -> bool {
						c->overrideMaxRelaysPerChannel = static_cast<int>(dwResult);
						return true;
					});
				}
			}
		}
		break;
	}
	case 7: {	// キープ
		OnChanKeep(0, 0, NULL);
		break;
	default:
		sleep_skip = true;	// お試し
		break;
	}
	} // switch
	return 0;
}

/// チャンネルリスト ダブルクリック
LRESULT	CMainFrame::OnChanListDblClick(LPNMHDR pnmh)
{
	OnPlaySelected(0, 0, NULL);
	return 0;
}

/// チャンネルリスト右クリック
LRESULT	CMainFrame::OnChanListRClick(LPNMHDR pnmh)
{
	LPNMITEMACTIVATE pnmItem = (LPNMITEMACTIVATE)pnmh;
	if (pnmItem->iItem != -1) {
		CMenu menu;
		menu.LoadMenu(IDM_CHANLIST);
		CMenu submenu = menu.GetSubMenu(0);
		POINT	pt;
		::GetCursorPos(&pt);
		submenu.TrackPopupMenu(TPM_LEFTALIGN, pt.x, pt.y, m_hWnd);
	}

	return 0;
}

LRESULT CMainFrame::OnConnectListGetDispInfo(LPNMHDR pnmh)
{
	NMLVDISPINFO* nmv = (NMLVDISPINFO*)pnmh;
	LVITEM &lvItem = nmv->item;
	WSingleLock lock(&sd_lock);
	ServentData *sd = getServentData(lvItem.iItem);

	if(sd)
	{
		if(lvItem.mask & LVIF_TEXT)
		{
			switch(lvItem.iSubItem)
			{
			case 0:
				if(sd->type == Servent::T_RELAY && sd->status == Servent::S_CONNECTED)
				{
					int pos = 0;
					if(sd->lastSkipCount)
						pos = _snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "(%d)", sd->lastSkipCount);
					if(pos >= 0)
					{
						strncpy_s(lvItem.pszText + pos, lvItem.cchTextMax - pos, "RELAYING", _TRUNCATE);
					}
				}
				else
					_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "%s-%s", sd->typeStr, sd->statusStr);
				break;
			case 1:
				_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "%d秒", sd->tnum);
				break;
			case 2:
				if(sd->type == Servent::T_RELAY)
					_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "%d/%d", sd->totalListeners, sd->totalRelays);
				break;
			case 3:
				strncpy_s(lvItem.pszText, lvItem.cchTextMax, sd->hostName.cstr(), _TRUNCATE);
				break;
			case 4:
				_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "%d",
					(sd->type == Servent::T_SERVER) ? sd->h.port : ntohs(sd->h.port));
				break;
			case 5:
				if (sd->type == Servent::T_RELAY	||
					sd->type == Servent::T_DIRECT	||
					sd->status == Servent::S_CONNECTED
					)
					_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "%d", sd->syncpos);
				break;
			case 6:
				if (sd->type != Servent::T_RELAY	&&
					sd->type != Servent::T_DIRECT	&&
					sd->status == Servent::S_CONNECTED
					)
				{
					_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "luid/%Ix", sd->agent.cstr()); //'Local Unique Identifier' generate from pointer.
				}
				else
				{
					strncpy_s(lvItem.pszText, lvItem.cchTextMax, sd->agent.cstr(), _TRUNCATE);
				}
				break;
			case 7:
				if(sd->ver_ex_number)
					_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "%c%c%02d", sd->ver_ex_prefix[0], sd->ver_ex_prefix[1], sd->ver_ex_number);
				else if(sd->vp_ver)
					_snprintf_s(lvItem.pszText, lvItem.cchTextMax, _TRUNCATE, "VP%02d", sd->vp_ver);
				break;
			}
		}

		if(lvItem.mask & LVIF_IMAGE)
		{
			switch(lvItem.iSubItem)
			{
			case 0:
				lvItem.iImage =
					(sd->type == Servent::T_RELAY) ?
					(sd->infoFlg) ?
					(sd->relay) ? 2
					: (sd->numRelays) ? 3
					: 4
					: 1
					: 0;
				break;
			}
		}

		if((lvItem.mask & LVIF_STATE) && sd->lastSkipTime + 120 > sys->getTime())
			lvItem.state = INDEXTOOVERLAYMASK(1);
	}
	return 0;
}

/// 接続リスト右クリック
LRESULT CMainFrame::OnConnectListRClick(LPNMHDR pnmh)
{
	LPNMITEMACTIVATE pnmItem = (LPNMITEMACTIVATE)pnmh;
	if (pnmItem->iItem != -1) {
		CMenu menu;
		menu.LoadMenu(IDM_CONNECTLIST);
		CMenu submenu = menu.GetSubMenu(0);
		POINT	pt;
		::GetCursorPos(&pt);
		submenu.TrackPopupMenu(TPM_LEFTALIGN, pt.x, pt.y, m_hWnd);
	}

	return 0;
}

/// ツールバーの[ログ]のドロップダウンメニュー
LRESULT CMainFrame::OnDropDownLogMenu(LPNMHDR pnmh)
{
	LPNMTOOLBAR	pnmt = (LPNMTOOLBAR)pnmh;
	if (pnmt->iItem == ID_PAUSE_LOG) {
		CMenu logmenu;
		logmenu.LoadMenu(IDR_LOGMENU);
		ATLASSERT(logmenu);
		CMenu submenu = logmenu.GetSubMenu(0);
		ATLASSERT(submenu);

		MENUITEMINFO menuInfo = { sizeof (MENUITEMINFO) };
		menuInfo.fMask	= MIIM_STATE;

		// 有効/無効の設定
		submenu.CheckMenuItem(IDC_LOGDEBUG, ((servMgr->showLog & (1 << LogBuffer::T_DEBUG))!=0) ? MFS_CHECKED : MFS_UNCHECKED);
		submenu.CheckMenuItem(IDC_LOGERRORS, ((servMgr->showLog & (1 <<LogBuffer::T_ERROR))!=0) ? MFS_CHECKED : MFS_UNCHECKED);
		submenu.CheckMenuItem(IDC_LOGNETWORK, ((servMgr->showLog & (1 <<LogBuffer::T_NETWORK))!=0) ? MFS_CHECKED : MFS_UNCHECKED);
		submenu.CheckMenuItem(IDC_LOGCHANNELS, ((servMgr->showLog & (1 << LogBuffer::T_CHANNEL))!=0) ? MFS_CHECKED : MFS_UNCHECKED);

		// ポップアップメニューの表示
		TPMPARAMS tpmParams = { sizeof(tpmParams) };
		tpmParams.rcExclude = pnmt->rcButton;
		m_ToolBar.ClientToScreen(&tpmParams.rcExclude);

		submenu.TrackPopupMenuEx(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, 
			tpmParams.rcExclude.left, tpmParams.rcExclude.bottom, m_hWnd, &tpmParams);
		PostMessage(WM_NULL);
		return TBDDRET_DEFAULT;
	}
	return TBDDRET_NODEFAULT;
}

// ログリストにログを追加する
LRESULT CMainFrame::OnAddLog(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPADDLOGINFO pInfo = (LPADDLOGINFO)wParam;
	if(pInfo == NULL)
		return 0;

	int num = m_LogList.GetCount();
	if (num > 100) {	// ログは100以上追加しない
		m_LogList.DeleteString(0);
		num--;
	}

	int idx = m_LogList.AddString((LPSTR)pInfo->strMessage.cstr());
	m_LogList.SetItemDataPtr(idx, pInfo->data);
	if (pInfo->sel)
		m_LogList.SetCurSel(num);

	delete pInfo;
	return 0;
}

/// MyPeercastApp::channelUpdateから呼ばれる (拍手関係っぽい)
LRESULT CMainFrame::OnChanInfoChanged(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#if 0
	bool bEnable = false;
	_EnumSelectedChannels([&bEnable](Channel* c) -> bool {
		if (c->info.ppFlags & ServMgr::bcstClap)
			bEnable = true;
		return !bEnable;
	});
	m_ToolBar.EnableButton(IDC_CLAP, bEnable);
#endif
	return 0;
}


LRESULT CMainFrame::OnAntenna(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (servMgr->guiAntennaNotifyIcon)
		SetTimer(1, 1000, NULL);
	return 0;
}

void	CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1) {
		int quality = 4;

		if (servMgr->guiAntennaNotifyIcon) {
			for (Channel *c = chanMgr->channel; c != NULL; c = c->next) {
				if (isIndexTxt(c)) // for PCRaw (skip index.txt)
					continue;

				int ret = checkLinkQuality(c);
				if (ret >= 0)
					quality = ret < quality ? ret : quality;
			}
		}

		if (quality == 4)
			KillTimer(1);

		m_TrayIcon.ModifyIcon(quality);
	}
}

/// トレイアイコンから
LRESULT CMainFrame::OnTrayIcon(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ((UINT)lParam) {
	case WM_LBUTTONDOWN:
		SetForegroundWindow(m_hWnd);
		_ShowTrayMenu(false); 
		break;
	case WM_RBUTTONDOWN:
		_ShowTrayMenu(true);   
		break;
	case WM_LBUTTONDBLCLK:
		ShowWindow(TRUE);
		SetForegroundWindow(m_hWnd);
		break;
	}
	return 0;
}

/// サーバーを開始する
void	CMainFrame::OnStartServer(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	servMgr->autoServe = m_ToolBar.IsButtonChecked(IDC_CHECK1) ? true : false;
}

void	CMainFrame::OnStartOutgoing(UINT uNotifyCode, int nID, CWindow wndCtl)
{
}

/// 配信開始設定画面を開く
void	CMainFrame::OnOpenBroadcastURL(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	Host sh = servMgr->serverHost;
	if (sh.isValid()) {
		CString strCmd;
		strCmd.Format(_T("http://localhost:%d/admin?page=broadcast"), sh.port);
		::ShellExecute(NULL, NULL, strCmd, NULL, NULL, SW_SHOWNORMAL);

	} else {
		MessageBox(_T("Server is not currently connected.\nPlease wait until you have a connection."));
	}
}

/// 再生
void	CMainFrame::OnPlaySelected(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	_EnumSelectedChannels([](Channel* c) -> bool {
		if (::PathFileExists(servMgr->PlayerPath.cstr())) {
			if (c && c->info.id.isSet()) {	// 指定したプレイヤーで開く場合はストリームURLで開く
				char idStr[64];
				c->getIDStr(idStr);
				CString strText;
				strText.Format(_T("http://localhost:%d/pls/%s"), servMgr->serverHost.port, idStr);
				::ShellExecute(NULL, NULL, servMgr->PlayerPath, strText, NULL, SW_NORMAL);
				return true;
			}
		}

		chanMgr->playChannel(c->info);
		return true;
	});
}

/// HTML設定画面を開く
void	CMainFrame::OnOpenSettingURL(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	sys->callLocalURL("admin?page=settings", servMgr->serverHost.port);
}

/// 接続切断
void	CMainFrame::OnServentDisconnect(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	WSingleLock lock(&servMgr->lock);

	auto func_getListBoxServent = [this]() -> Servent* {
		int sel = m_ListConnect.GetNextItem(-1, LVNI_FOCUSED | LVNI_SELECTED);
		if (sel >= 0) {
			WSingleLock lock(&sd_lock);
			ServentData *sd = servent_top;

			int idx = 0;

			while(sd){
				if (sel == idx){
					return servMgr->findServentByServentID(sd->servent_id);
				}
				sd = sd->next;
				idx++;
			}
		}
		return NULL;
	};

	Servent *s = func_getListBoxServent();
	if (s) {
		s->thread.active = false;
		s->thread.finish = true;
	}

	sleep_skip = true;
}

/// (チャンネル)切断
void	CMainFrame::OnChanDisconnect(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	_EnumSelectedChannels([](Channel* c) -> bool {
		c->thread.active = false;
		c->thread.finish = true;
		return true;
	});

	sleep_skip = true;
}

/// チャンネル再接続
void	CMainFrame::OnChanBump(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	_EnumSelectedChannels([](Channel* c) -> bool {
		c->bump = true;
		return true;
	});
}

void	CMainFrame::OnGetChannel(UINT uNotifyCode, int nID, CWindow wndCtl)
{
}

/// チャンネルをキープする
void	CMainFrame::OnChanKeep(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	_EnumSelectedChannels([](Channel* c) -> bool {
		c->stayConnected = !c->stayConnected;
		return true;
	});
}

/// ログをクリアする
void	CMainFrame::OnClearLog(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	m_LogList.ResetContent();	// ログリストをクリア
	sys->logBuf->clear();	// for PCRaw (clear log)
}

/// ログを流す/止める
void	CMainFrame::OnPauseLog(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	servMgr->pauseLog = !servMgr->pauseLog;
	m_ToolBar.CheckButton(ID_PAUSE_LOG, !servMgr->pauseLog ); 
}

/// ログリストに表示するログの種類を変更する
void	CMainFrame::OnLoglevelChange(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	switch (nID) {
	case IDC_LOGDEBUG:		// log debug
		servMgr->showLog ^= 1 << LogBuffer::T_DEBUG;
		break;
	case IDC_LOGERRORS:		// log errors
		servMgr->showLog ^= 1 << LogBuffer::T_ERROR;
		break;
	case IDC_LOGNETWORK:	// log network
		servMgr->showLog ^= 1 << LogBuffer::T_NETWORK;
		break;
	case IDC_LOGCHANNELS:	// log channels
		servMgr->showLog ^= 1 << LogBuffer::T_CHANNEL;
		break;
	default:
		ATLASSERT(FALSE);
		break;
	}
}

/// 再生URLをコピー
void	CMainFrame::OnCopyStreamUrl(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	WSingleLock lock(&chanMgr->channellock);
	Channel *c = _GetSelectedChannel();

	if (c && c->info.id.isSet()) {
		char idStr[64];
		c->getIDStr(idStr);
		CString strText;
		strText.Format(_T("http://localhost:%d/pls/%s"), servMgr->serverHost.port, idStr);
		SetClipboardText(strText);
	}
}

/// コンタクトURLをコピー
void	CMainFrame::OnCopyContactUrl(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	WSingleLock lock(&chanMgr->channellock);
	Channel *c = _GetSelectedChannel();

	if (c && !c->info.url.isEmpty())
		SetClipboardText(c->info.url.cstr());
}

/// アドレスをコピー
void	CMainFrame::OnCopyAddress(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	int sel = m_ListConnect.GetNextItem(-1, LVNI_FOCUSED | LVNI_SELECTED);
	if (sel >= 0) {
		WSingleLock lock(&sd_lock);
		ServentData *sd = getServentData(sel);
		if (sd)
			SetClipboardText(sd->hostName.cstr());
	}
}


/// チャンネルを再生する
void	CMainFrame::OnPlayChannnel(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	int c = nID - ID_PLAY_CMD;
	chanInfo = getChannelInfo(c);
	chanMgr->findAndPlayChannel(chanInfo, false);
}

/// コンタクトURLを開く
void	CMainFrame::OnOpenContactURL(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	if (nID == ID_OPEN_CONTACTURL) {
		_EnumSelectedChannels([](Channel* c) -> bool {
			sys->getURL(c->info.url);
			return true;
		});
	} else {
		int c = nID - ID_URL_CMD;
		chanInfo = getChannelInfo(c);
		if (chanInfo.url.isValidURL())
			sys->getURL(chanInfo.url);
	}
}

void	CMainFrame::OnOpenChannelInfo(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	int c = nID - ID_INFO_CMD;
	chanInfo = getChannelInfo(c);
	chanInfoIsRelayed = false;

	CChannelInfoDialog info;
	info.DoModal(NULL);
}


void	CMainFrame::OnCommand(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	TCHAR buf[1024];
	switch (nID) {
	case ID_POPUP_SHOWMESSAGES_PEERCAST:
		flipNotifyPopup(ID_POPUP_SHOWMESSAGES_PEERCAST,ServMgr::NT_PEERCAST);
		break;
	case ID_POPUP_SHOWMESSAGES_BROADCASTERS:
		flipNotifyPopup(ID_POPUP_SHOWMESSAGES_BROADCASTERS,ServMgr::NT_BROADCASTERS);
		break;
	case ID_POPUP_SHOWMESSAGES_TRACKINFO:
		flipNotifyPopup(ID_POPUP_SHOWMESSAGES_TRACKINFO,ServMgr::NT_TRACKINFO);
		break;
	case ID_POPUP_SHOWMESSAGES_APPLAUSE:
		flipNotifyPopup(ID_POPUP_SHOWMESSAGES_APPLAUSE,ServMgr::NT_APPLAUSE);
		break;

	case ID_POPUP_ABOUT:
	case IDM_ABOUT: {
		CAboutDialog	about;
		about.DoModal(NULL);
		break;
	}
	case ID_POPUP_SHOWGUI:
	case IDM_SETTINGS_GUI:
	case ID_POPUP_ADVANCED_SHOWGUI: {
		ShowWindow(TRUE);
		SetForegroundWindow(m_hWnd);
		break;
	}
	case ID_POPUP_YELLOWPAGES:
		sys->getURL("http://yp.peercast.org/");
		break;
	case ID_POPUP_YELLOWPAGES1:
		sprintf(buf, "http://%s",servMgr->rootHost.cstr());
		sys->getURL(buf);
		break;
	case ID_POPUP_YELLOWPAGES2:
		sprintf(buf, "http://%s",servMgr->rootHost2.cstr());
		sys->getURL(buf);
		break;

	case ID_POPUP_ADVANCED_VIEWLOG:
		showHTML("viewlog.html");
		break;
	case ID_POPUP_ADVANCED_SAVESETTINGS:
		servMgr->saveSettings(iniFileName.cstr());
		break;
	case ID_POPUP_ADVANCED_INFORMATION:
		showHTML("index.html");
		break;
	case ID_FIND_CHANNELS:
	case ID_POPUP_ADVANCED_ALLCHANNELS:
	case ID_POPUP_UPGRADE:
		sys->callLocalURL("admin?cmd=upgrade",servMgr->serverHost.port);
		break;
	case ID_POPUP_ADVANCED_RELAYEDCHANNELS:
	case ID_POPUP_FAVORITES_EDIT:
		showHTML("relays.html");
		break;
	case ID_POPUP_ADVANCED_BROADCAST:
		showHTML("broadcast.html");
		break;
	case ID_POPUP_SETTINGS:
		showHTML("settings.html");
		break;
	case ID_POPUP_CONNECTIONS:
		showHTML("connections.html");
		break;
	case ID_POPUP_HELP:
		sys->getURL("http://www.peercast.org/help.php");
		break;

	case ID_POPUP_SAVE_GUI_POS:
		servMgr->saveGuiPos = !servMgr->saveGuiPos;
		peercastInst->saveSettings();
		break;

	case ID_POPUP_KEEP_DOWNSTREAMS:
		servMgr->keepDownstreams = !servMgr->keepDownstreams;
		//peercastInst->saveSettings();
		break;
	case ID_POPUP_TOPMOST: //JP-MOD
		servMgr->guiTopMost = !servMgr->guiTopMost;
		if (servMgr->guiTopMost) 
			SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		else
			SetWindowPos(HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		break;

	case ID_POPUP_EXIT_CONFIRM:
	case IDM_EXIT:
			PostMessage(WM_CLOSE);
		break;
	default:
		SetMsgHandled(false);
		break;
	}
}

LRESULT CMainFrame::OnCustomDraw(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	SetMsgHandled(true);
	LPNMCUSTOMDRAW lpNMCustomDraw = (LPNMCUSTOMDRAW)pnmh;
	DWORD dwRet = 0;
	switch (lpNMCustomDraw->dwDrawStage)  {	// CCustomDrawにはなぜかこれだけないので
		case CDDS_ITEMPOSTPAINT | CDDS_SUBITEM:
			return OnSubItemPostPaint(idCtrl, lpNMCustomDraw);
		default:
			break;
	}
	SetMsgHandled(false);
	return 0;
}

DWORD	CMainFrame::OnPrePaint(int nID, LPNMCUSTOMDRAW lpnmcd)
{
	if (lpnmcd->hdr.idFrom == IDC_CHANNELLIST) {
		return CDRF_NOTIFYITEMDRAW;	// チャンネルリスト
	} else if (lpnmcd->hdr.idFrom == IDC_CONNECTLIST) {
		return CDRF_NOTIFYITEMDRAW;	// 接続リスト
	}

	return CDRF_DODEFAULT;
}

DWORD	CMainFrame::OnItemPrePaint(int nID, LPNMCUSTOMDRAW lpnmcd)
{
	if (lpnmcd->hdr.idFrom == IDC_CHANNELLIST) {			// チャンネルリスト
		if (servMgr->getFirewall() == ServMgr::FW_ON) {
			LPNMLVCUSTOMDRAW(lpnmcd)->clrText = RGB(255, 0, 0);
			return CDRF_DODEFAULT;
		}
		return CDRF_NOTIFYSUBITEMDRAW;
	} else if (lpnmcd->hdr.idFrom == IDC_CONNECTLIST) {	// 接続リスト
		WSingleLock lock(&sd_lock);
		ServentData *sd = getServentData(LPNMLVCUSTOMDRAW(lpnmcd)->nmcd.dwItemSpec);

		if (sd && sd->firewalled)
			LPNMLVCUSTOMDRAW(lpnmcd)->clrText = (!sd->numRelays) ? RGB(255, 0, 0) : RGB(255, 168, 0);
		return CDRF_DODEFAULT;
	}

	return CDRF_DODEFAULT;
}

DWORD	CMainFrame::OnSubItemPrePaint(int nID, LPNMCUSTOMDRAW lpnmcd)
{
	if (lpnmcd->hdr.idFrom == IDC_CHANNELLIST) {
		LPNMLVCUSTOMDRAW	plvcmd = (LPNMLVCUSTOMDRAW)lpnmcd;
		plvcmd->clrText = ::GetSysColor(COLOR_WINDOWTEXT);

		WSingleLock lock(&ld_lock);
		ListData *ld = getListData(plvcmd->nmcd.dwItemSpec);
		if(ld) {
			switch(plvcmd->iSubItem)
			{
			case 0:
				if(ld->bTracker && (ld->status == Channel::S_RECEIVING))
					plvcmd->clrText = RGB(0, 128, 0);
				break;
			case 4:
				if(ld->stealth)
					plvcmd->clrText = GetSysColor(COLOR_GRAYTEXT);
				if(plvcmd->nmcd.uItemState & CDIS_FOCUS)
					return CDRF_NOTIFYPOSTPAINT;
				break;
			case 7:
				return CDRF_NOTIFYPOSTPAINT;
			}
		}
	}
	return CDRF_DODEFAULT;
}

DWORD	CMainFrame::OnSubItemPostPaint(int nID, LPNMCUSTOMDRAW lpnmcd)
{
	if (lpnmcd->hdr.idFrom == IDC_CHANNELLIST) {
		LPNMLVCUSTOMDRAW	plvcmd = (LPNMLVCUSTOMDRAW)lpnmcd;
		CRect rc;
		m_ListChan.GetSubItemRect(plvcmd->nmcd.dwItemSpec, plvcmd->iSubItem, LVIR_LABEL, &rc);

		switch (plvcmd->iSubItem) {
		case 4: {	// 下流の数を変更
			static int s_cxvscroll = ::GetSystemMetrics(SM_CXVSCROLL);
			CRect rcClip = rc;
			rcClip.left	 = rcClip.right - s_cxvscroll;
			if (rc.left <= rcClip.left) {
				CTheme theme;
				theme.OpenThemeData(m_ListChan, L"ComboBox");
				if (theme.IsThemeNull() == false) {
					int iStateID = (plvcmd->nmcd.uItemState & CDIS_HOT) ? CBXS_HOT : CBXS_NORMAL;
					if (theme.IsThemePartDefined(CP_READONLY, 0)) {
						theme.DrawThemeBackground(plvcmd->nmcd.hdc, CP_READONLY, iStateID, &rc, &rc);
						CRect rcExcludeClip;
						if (::SubtractRect(&rcExcludeClip, &rc, &rcClip)) {
							CRect rcContent;
							if (SUCCEEDED(theme.GetThemeBackgroundContentRect(plvcmd->nmcd.hdc, CP_READONLY, iStateID, &rcExcludeClip, &rcContent))) {
								CString strText;
								m_ListChan.GetItemText(plvcmd->nmcd.dwItemSpec, plvcmd->iSubItem, strText);
								theme.DrawThemeText(plvcmd->nmcd.hdc, CP_READONLY, iStateID, CT2W(strText), -1, DT_NOPREFIX, 0, &rcContent);
							}
						}
						theme.DrawThemeBackground(plvcmd->nmcd.hdc, CP_DROPDOWNBUTTONRIGHT, CBXSR_NORMAL, &rcClip, &rcClip);
					} else {
						theme.DrawThemeBackground(plvcmd->nmcd.hdc, CP_DROPDOWNBUTTON, iStateID, &rcClip, &rcClip);
					}
				} else {	// テーマが無効である
					::DrawFrameControl(plvcmd->nmcd.hdc, &rcClip, DFC_SCROLL, DFCS_SCROLLCOMBOBOX | DFCS_TRANSPARENT);
				}
				theme.CloseThemeData();
			}
			break;
		}
		case 7: {	// キープ(チェックボックス)
			WSingleLock lock(&ld_lock);
			ListData *ld = getListData(plvcmd->nmcd.dwItemSpec);
			if(ld) {
				CTheme theme;
				theme.OpenThemeData(m_ListChan, L"Button");
				if (theme.IsThemeNull() == false) {
					int iStateID = (ld->stayConnected) ? CBS_CHECKEDNORMAL : CBS_UNCHECKEDNORMAL;
					if (plvcmd->nmcd.uItemState & CDIS_HOT)
						++iStateID;

					theme.DrawThemeBackground(plvcmd->nmcd.hdc, BP_CHECKBOX, iStateID, &rc, &rc);
				} else {	// テーマが無効
					UINT uState = DFCS_BUTTONCHECK | DFCS_TRANSPARENT;
					if(ld->stayConnected)
						uState |= DFCS_CHECKED;

					::DrawFrameControl(plvcmd->nmcd.hdc, &rc, DFC_BUTTON, uState);
				}
				theme.CloseThemeData();
			}
			break;
		}
		} // switch

	}

	return CDRF_DODEFAULT;
}


/// 上に乗るウィンドウを作成する
void	CMainFrame::_CreateControls()
{
   // リバーを作成
	//CReBarCtrl	rebar;
	//m_hWndToolBar = rebar.Create(m_hWnd, 0, _T("コマンドバー"), 
	//	WS_BORDER | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP | WS_VISIBLE |
	//	CCS_NODIVIDER | CCS_NOPARENTALIGN | CCS_NORESIZE | RBS_AUTOSIZE | RBS_VARHEIGHT);
	//REBARINFO rbi = { 0 };
	//rbi.cbSize = sizeof(REBARINFO);
	//rbi.fMask  = 0;
	//rebar.SetBarInfo(&rbi);
	CreateSimpleReBar();
	CReBarCtrl rebar = m_hWndToolBar;
	rebar.ModifyStyle(0, CCS_NODIVIDER);

	m_ToolBar.Create(m_hWndToolBar, 0, _T("ツールバー"), 
		WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP | WS_VISIBLE |
		CCS_ADJUSTABLE | CCS_NODIVIDER | CCS_NOPARENTALIGN | CCS_NORESIZE | 
		TBSTYLE_ALTDRAG | TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS | TBSTYLE_TRANSPARENT);

	m_ListChan.Create(m_hWnd, 0, _T("チャンネルリストビュー"),  
		WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP | WS_VISIBLE |
		LVS_AUTOARRANGE | LVS_NOSORTHEADER | LVS_OWNERDATA | LVS_REPORT | LVS_SHAREIMAGELISTS | LVS_SHOWSELALWAYS);
	m_ListChan.SetDlgCtrlID(IDC_CHANNELLIST);

	m_ListConnect.Create(m_hWnd, 0, _T("コネクションリストビュー"), 
		WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP | WS_VISIBLE |
		LVS_AUTOARRANGE | LVS_NOSORTHEADER | LVS_OWNERDATA | LVS_REPORT | LVS_SHAREIMAGELISTS | LVS_SINGLESEL);
	m_ListConnect.SetDlgCtrlID(IDC_CONNECTLIST);

	m_LogList.Create(m_hWnd, 0, _T("ログリスト"), 
		WS_CHILD | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT | WS_BORDER);
	m_LogList.SetDlgCtrlID(IDC_LOGLIST);
}

/// リストビューを初期化
void	CMainFrame::_InitListView()
{
	//if(ThemeHelper::IsThemingSupported()) // for Vista (Explorer selection visuals)
	{
		::SetWindowTheme(m_ListChan	  , L"Explorer", NULL);
		::SetWindowTheme(m_ListConnect, L"Explorer", NULL);
	}

	DWORD dwStyle = LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP | LVS_EX_SUBITEMIMAGES;
	m_ListChan.SetExtendedListViewStyle(dwStyle);
	m_ListConnect.SetExtendedListViewStyle(dwStyle);

	CImageList	image;
	image.CreateFromImage(MAKEINTRESOURCE(IDB_CHANLIST), 0, 10, CLR_DEFAULT, IMAGE_BITMAP, LR_CREATEDIBSECTION | LR_DEFAULTSIZE);
	ATLASSERT(image.m_hImageList);
	image.SetOverlayImage(9, 1);

	m_ListChan.SetImageList(image, LVSIL_SMALL);
	m_ListConnect.SetImageList(image, LVSIL_SMALL);
	m_ListConnect.SetCallbackMask(LVIS_OVERLAYMASK);

	
	{
		LV_COLUMN column;

		column.mask = LVCF_FMT | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
		column.fmt = LVCFMT_LEFT;
		column.cx = convertScaleX(130);
		column.pszText = _T("チャンネル名");
		column.iSubItem = 0;
		m_ListChan.InsertColumn(column.iSubItem, &column);

		column.fmt = LVCFMT_RIGHT;
		column.cx = convertScaleX(85);
		column.pszText = _T("ビットレート");
		column.iSubItem++;
		m_ListChan.InsertColumn(column.iSubItem, &column);

		column.fmt = LVCFMT_LEFT;
		column.pszText = _T("状態");
		column.iSubItem++;
		m_ListChan.InsertColumn(column.iSubItem, &column);

		column.cx = convertScaleX(60);
		column.pszText = _T("リスナー/リレー");
		column.iSubItem++;
		m_ListChan.InsertColumn(column.iSubItem, &column);

		column.pszText = _T("ローカルリスナー/リレー");
		column.iSubItem++;
		m_ListChan.InsertColumn(column.iSubItem, &column);

		column.fmt = LVCFMT_RIGHT;
		column.cx = convertScaleX(0);
		column.pszText = _T("拍手");
		column.iSubItem++;
		m_ListChan.InsertColumn(column.iSubItem, &column);

		column.cx = convertScaleX(40);
		column.pszText = _T("スキップ");
		column.iSubItem++;
		m_ListChan.InsertColumn(column.iSubItem, &column);

		column.fmt = LVCFMT_CENTER;
		column.cx = convertScaleX(50);
		column.pszText = _T("キープ");
		column.iSubItem++;
		m_ListChan.InsertColumn(column.iSubItem, &column);
	}

	
	{
		LV_COLUMN column;

		column.mask = LVCF_FMT | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
		column.fmt = LVCFMT_LEFT;
		column.cx = convertScaleX(80);
		column.pszText = _T("状態");
		column.iSubItem = 0;
		m_ListConnect.InsertColumn(column.iSubItem, &column);

		column.fmt = LVCFMT_RIGHT;
		column.cx = convertScaleX(60);
		column.pszText = _T("時間");
		column.iSubItem++;
		m_ListConnect.InsertColumn(column.iSubItem, &column);

		column.fmt = LVCFMT_LEFT;
		column.cx = convertScaleX(40);
		column.pszText = _T("リスナー/リレー");
		column.iSubItem++;
		m_ListConnect.InsertColumn(column.iSubItem, &column);

		column.cx = convertScaleX(110);
		column.pszText = _T("アドレス");
		column.iSubItem++;
		m_ListConnect.InsertColumn(column.iSubItem, &column);

		column.fmt = LVCFMT_RIGHT;
		column.cx = convertScaleX(50);
		column.pszText = _T("ポート");
		column.iSubItem++;
		m_ListConnect.InsertColumn(column.iSubItem, &column);

		column.pszText = _T("同期位置");
		column.iSubItem++;
		m_ListConnect.InsertColumn(column.iSubItem, &column);

		column.fmt = LVCFMT_LEFT;
		column.cx = convertScaleX(100);
		column.pszText = _T("エージェント");
		column.iSubItem++;
		m_ListConnect.InsertColumn(column.iSubItem, &column);

		column.cx = convertScaleX(60);
		column.pszText = _T("拡張情報");
		column.iSubItem++;
		m_ListConnect.InsertColumn(column.iSubItem, &column);
	}

	// ログリストのフォント設定
	m_LogList.SetFont(AtlGetDefaultGuiFont());
}

/// ツールバーを初期化
void	CMainFrame::_InitToolBar()
{
	m_ToolBar.SetButtonStructSize();
	m_ToolBar.SetExtendedStyle(TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_DOUBLEBUFFER);
	CReBarCtrl(m_hWndToolBar).SetWindowTheme(L"Media");
	m_ToolBar.SetWindowTheme(L"Alternate");

	m_ToolBar.AddStrings(_T("再生\0再接続\0切断\0接続切断\0サーバー\0ログ\0"));
	TBBUTTON tbButtons[] = {
		{7, ID_PLAY_SELECTED,	TBSTATE_ENABLED,	BTNS_AUTOSIZE | BTNS_BUTTON,	{0}, 0, 0},	// 再生
		{0, ID_CHANNLE_BUMP,	TBSTATE_ENABLED,	BTNS_AUTOSIZE | BTNS_BUTTON,	{0}, 0, 1},	// 再接続
		{1, ID_CHANNEL_DISCONNECT,	TBSTATE_ENABLED,	BTNS_AUTOSIZE | BTNS_BUTTON,	{0}, 0, 2},	// (チャンネル)切断

		{0, 0,				TBSTATE_ENABLED,	BTNS_SEP,						{0}, 0, 0},

		{4, ID_SERVER_DISCONNECT,	TBSTATE_ENABLED,	BTNS_AUTOSIZE | BTNS_BUTTON,	{0}, 0, 3},	// 接続切断

		{0, 0,				TBSTATE_ENABLED,	BTNS_SEP,						{0}, 0, 0},

		{5, IDC_CHECK1,		TBSTATE_ENABLED,	BTNS_AUTOSIZE | BTNS_CHECK,		{0}, 0, 4},	// サーバー
		{6, ID_PAUSE_LOG,		TBSTATE_ENABLED,	BTNS_AUTOSIZE | BTNS_DROPDOWN,	{0}, 0, 5}	// ログ
	};


	CImageList	image;
	image.CreateFromImage(MAKEINTRESOURCE(IDB_CMDBAR_32), 0, 8, CLR_NONE, IMAGE_BITMAP, LR_CREATEDIBSECTION | LR_DEFAULTSIZE);
	ATLASSERT(image.m_hImageList);
	{	// 再生プレイヤーのアイコンを読み込む
		CString strPlayerPath;
		if (::PathFileExists(servMgr->PlayerPath.cstr())) {
			strPlayerPath = servMgr->PlayerPath.cstr();
		} else {
			TCHAR szPlayerPath[MAX_PATH] = _T("\0");
			DWORD cchOut = _countof(szPlayerPath);
			::AssocQueryString(ASSOCF_NOTRUNCATE, ASSOCSTR_EXECUTABLE, _T(".asx"), _T("open"), szPlayerPath, &cchOut);
			strPlayerPath = szPlayerPath;
		}
		SHFILEINFO sfInfo;
		if (::SHGetFileInfo(strPlayerPath, 0, &sfInfo, sizeof(sfInfo),  SHGFI_ICON | SHGFI_SMALLICON)) {
			int nCount = image.AddIcon(sfInfo.hIcon);
			ATLASSERT(nCount == 7);
			DestroyIcon(sfInfo.hIcon);
		}

	}
	m_ToolBar.SetImageList(image);

	m_ToolBar.AddButtons(ARRAYSIZE(tbButtons), tbButtons);
	m_ToolBar.CheckButton(IDC_CHECK1, servMgr->autoServe);
	m_ToolBar.CheckButton(ID_PAUSE_LOG, !servMgr->pauseLog ); 
	
	_AddSimpleReBarBandCtrl(m_hWndToolBar, m_ToolBar);
	//m_ToolBar.SetExtendedStyle(TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_DOUBLEBUFFER);
}

/// タイトルバー右クリックメニューを編集する
void	CMainFrame::_InitSysMenu()
{
	CMenuHandle SysMenu = GetSystemMenu(FALSE);

	MENUITEMINFO info = { sizeof(MENUITEMINFO) };
	info.fMask = MIIM_ID | MIIM_TYPE;
	info.fType = MFT_STRING;
	info.wID = IDC_CONFIG;
	info.dwTypeData = _T("設定(&O)...");
	SysMenu.InsertMenuItem(SC_CLOSE, FALSE, &info);

	MENUITEMINFO sep = { sizeof(MENUITEMINFO) };
	sep.fMask = MIIM_ID | MIIM_TYPE;
	sep.fType = MFT_SEPARATOR;
	SysMenu.InsertMenuItem(SC_CLOSE, FALSE, &sep);
}


/// リバーにバンドを追加する
BOOL CMainFrame::_AddSimpleReBarBandCtrl(HWND hWndReBar, HWND hWndBand, int nID, LPCTSTR lpstrTitle, BOOL bNewRow, int cxWidth, BOOL bFullWidthAlways)
{
	ATLASSERT(::IsWindow(hWndReBar));   // must be already created
#ifdef _DEBUG
	// block - check if this is really a rebar
	{
		TCHAR lpszClassName[sizeof(REBARCLASSNAME)] = { 0 };
		::GetClassName(hWndReBar, lpszClassName, sizeof(REBARCLASSNAME));
		ATLASSERT(lstrcmp(lpszClassName, REBARCLASSNAME) == 0);
	}
#endif // _DEBUG
	ATLASSERT(::IsWindow(hWndBand));   // must be already created

	// Get number of buttons on the toolbar
	int nBtnCount = (int)::SendMessage(hWndBand, TB_BUTTONCOUNT, 0, 0L);

	// Set band info structure
	REBARBANDINFO rbBand = { RunTimeHelper::SizeOf_REBARBANDINFO() };
#if (_WIN32_IE >= 0x0400)
	rbBand.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_STYLE | RBBIM_ID | RBBIM_SIZE | RBBIM_IDEALSIZE;
#else
	rbBand.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_STYLE | RBBIM_ID | RBBIM_SIZE;
#endif // !(_WIN32_IE >= 0x0400)
	if(lpstrTitle != NULL)
		rbBand.fMask |= RBBIM_TEXT;
	rbBand.fStyle = RBBS_CHILDEDGE;
	if(bNewRow)
		rbBand.fStyle |= RBBS_BREAK;

	rbBand.lpText = (LPTSTR)lpstrTitle;
	rbBand.hwndChild = hWndBand;
	if(nID == 0)   // calc band ID
		nID = ATL_IDW_BAND_FIRST + (int)::SendMessage(hWndReBar, RB_GETBANDCOUNT, 0, 0L);
	rbBand.wID = nID;

	// Calculate the size of the band
	BOOL bRet = FALSE;
	RECT rcTmp = { 0 };
	if(nBtnCount > 0)
	{
		bRet = (BOOL)::SendMessage(hWndBand, TB_GETITEMRECT, nBtnCount - 1, (LPARAM)&rcTmp);
		ATLASSERT(bRet);
		rbBand.cx = (cxWidth != 0) ? cxWidth : rcTmp.right;
		rbBand.cyMinChild = rcTmp.bottom - rcTmp.top;
		if(bFullWidthAlways)
		{
			rbBand.cxMinChild = rbBand.cx;
		}
		else if(lpstrTitle == NULL)
		{
			bRet = (BOOL)::SendMessage(hWndBand, TB_GETITEMRECT, 0, (LPARAM)&rcTmp);
			ATLASSERT(bRet);
			rbBand.cxMinChild = rcTmp.right;
		}
		else
		{
			rbBand.cxMinChild = 0;
		}
	}
	else	// no buttons, either not a toolbar or really has no buttons
	{
		bRet = ::GetWindowRect(hWndBand, &rcTmp);
		ATLASSERT(bRet);
		rbBand.cx = (cxWidth != 0) ? cxWidth : (rcTmp.right - rcTmp.left);
		rbBand.cxMinChild = bFullWidthAlways ? rbBand.cx : 0;
		rbBand.cyMinChild = rcTmp.bottom - rcTmp.top;
	}

#if (_WIN32_IE >= 0x0400)
	rbBand.cxIdeal = rbBand.cx;
#endif // (_WIN32_IE >= 0x0400)

	// Add the band
	LRESULT lRes = ::SendMessage(hWndReBar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);
	if(lRes == 0)
	{
		ATLTRACE2(atlTraceUI, 0, _T("Failed to add a band to the rebar.\n"));
		return FALSE;
	}

	return TRUE;
}

/// チャンネルを列挙する
int CMainFrame::_EnumSelectedChannels(function<bool (Channel* c)> funcDiscovered)
{
	int count = 0;
	if (m_ListChan.IsWindow() == FALSE)
		return 0;

	int sel = m_ListChan.GetNextItem(-1, LVNI_SELECTED);
	if (sel >= 0) {
		ListData *ld;
		int index;

		WSingleLock lock(&chanMgr->channellock);
		WSingleLock lock_2nd(&ld_lock);
		for(ld = list_top, index = 0;
			ld != NULL;
			ld = ld->next, ++index)
		{
			if(sel == index)
			{
				sel = m_ListChan.GetNextItem(sel, LVNI_SELECTED);

				Channel *c = chanMgr->findChannelByChannelID(ld->channel_id);
				if(!c)
					continue;

				++count;

				if(!funcDiscovered(c) || sel < 0)
					break;
			}
		}
	}

	return count;
}

/// 現在チャンネルリストビューで選択されているChannelを返す
Channel* CMainFrame::_GetSelectedChannel()
{
	int sel = m_ListChan.GetNextItem(-1, LVNI_FOCUSED | LVNI_SELECTED); //JP-MOD
	if (sel >= 0){
		WSingleLock lock(&ld_lock); //JP-MOD
		ListData *ld = list_top;
		int idx = 0;

		while(ld){
			if (sel == idx)
				return chanMgr->findChannelByChannelID(ld->channel_id);

			ld = ld->next;
			idx++;
		}
	}
	return NULL;
}


void	CMainFrame::_SetControls()
{
	::SetWindowPos(m_hWnd, servMgr->guiTopMost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	if(!servMgr->guiTitleModify)
		SetWindowText(_T("PeerCast"));	// タイトルを動的に変更しない

	if (servMgr->guiSimpleChannelList) {
		m_ListChan.ModifyStyle(0, LVS_NOCOLUMNHEADER);
	} else {
		m_ListChan.ModifyStyle(LVS_NOCOLUMNHEADER, 0);
	}
	
	m_ToolBar.CheckButton(IDC_CHECK1, servMgr->autoServe);
	SendMessage(WM_COMMAND, IDC_CHECK1);
}


void	CMainFrame::_ShowTrayMenu(bool bRClick)
{
	// 表示位置計算
	POINT	point;
	RECT rcWnd;
	UINT flg = 0;

	SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWnd, 0);
	::GetCursorPos(&point);

	if (point.x < rcWnd.left){
		point.x = rcWnd.left;
		flg |= TPM_LEFTALIGN;
	}
	if (point.x > rcWnd.right){
		point.x = rcWnd.right;
		flg |= TPM_RIGHTALIGN;
	}
#if 0
	if (point.y < rcWnd.top){
		point.y = rcWnd.top;
		flg |= TPM_TOPALIGN;
	}
	if (point.y > rcWnd.bottom){
		point.y = rcWnd.bottom;
		flg |= TPM_BOTTOMALIGN;
	}
#endif
	if (flg == 0){
		flg = TPM_RIGHTALIGN;
	}

	CMenu menu;
	if (bRClick) {	// 右クリック
		menu = _PrepareTrayRClickMenu();
	} else {		// クリック
		menu = _PrepareTrayClickMenu();
	}

	SetForegroundWindow(m_wndMessage);
	//SetForegroundWindow(m_hWnd);
	menu.TrackPopupMenu(flg, point.x, point.y, m_hWnd);
	m_wndMessage.PostMessage(WM_NULL); 
}

/// タスクトレイをクリックしたときに出すメニューを準備する
HMENU	CMainFrame::_PrepareTrayClickMenu()
{
	CMenuHandle menu;
	menu.CreatePopupMenu();

	// チャンネルをメニューに追加
	int numActive = 0;
	Channel *ch = chanMgr->channel;
	for (; ch; ch = ch->next) {
		String sjis = ch->info.name;
		sjis.convertTo(String::T_SJIS);
		CString str;
		str.Format(_T("%s  (%d kb/s %s)"), sjis.cstr(), ch->info.bitrate, ChanInfo::getTypeStr(ch->info.contentType));
		CMenuHandle submenu;
		submenu.CreatePopupMenu();
		submenu.AppendMenu(0, ID_PLAY_CMD + numActive, _T("Play"));
		if (ch->info.url.isValidURL())
			submenu.AppendMenu(0, ID_URL_CMD  + numActive, _T("URL"));
		submenu.AppendMenu(0, ID_INFO_CMD + numActive, _T("Info"));
		//if(ch->info.ppFlags & ServMgr::bcstClap)
			//InsertMenu(opMenu,0,MF_BYPOSITION,CLAP_CMD+numActive,"拍手"); //JP-MOD

		UINT flags = MF_POPUP;
		flags |=  ch->isPlaying() ? MF_CHECKED : 0;
		menu.AppendMenu(flags, submenu, str);
		
		numActive++;
	}
	// イェローページを追加
	if (servMgr->rootHost.isEmpty() == false) {
		if (numActive != 0)
			menu.AppendMenu(MF_SEPARATOR, (UINT_PTR)0, (HBITMAP)0);
		menu.AppendMenu(0, ID_POPUP_YELLOWPAGES1, servMgr->rootHost);
		if (servMgr->rootHost2.isEmpty() == false)
			menu.AppendMenu(0, ID_POPUP_YELLOWPAGES2, servMgr->rootHost2);
	}
	return menu;
}

/// タスクトレイを右クリックしたときに出すメニューを準備する
HMENU	CMainFrame::_PrepareTrayRClickMenu()
{
	CMenuHandle menu;
	menu.LoadMenu(IDR_TRAYMENU);
	menu = menu.GetSubMenu(0);

	auto funcCheckMenuItem = [&menu](UINT nID, bool bCheck) {
		menu.CheckMenuItem(nID, bCheck ?  MF_CHECKED : MF_UNCHECKED);
	};
	funcCheckMenuItem(ID_POPUP_SAVE_GUI_POS, servMgr->saveGuiPos);
	funcCheckMenuItem(ID_POPUP_TOPMOST, servMgr->guiTopMost);
	funcCheckMenuItem(ID_POPUP_KEEP_DOWNSTREAMS, servMgr->keepDownstreams);

	int mask = peercastInst->getNotifyMask();
	funcCheckMenuItem(ID_POPUP_SHOWMESSAGES_PEERCAST, (mask & ServMgr::NT_PEERCAST) != 0);
	funcCheckMenuItem(ID_POPUP_SHOWMESSAGES_BROADCASTERS, (mask & ServMgr::NT_BROADCASTERS) != 0);
	funcCheckMenuItem(ID_POPUP_SHOWMESSAGES_TRACKINFO, (mask & ServMgr::NT_TRACKINFO) != 0);
	return menu;
}













