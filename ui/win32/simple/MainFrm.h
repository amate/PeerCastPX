/*! @file	MainFrm.h
	@brief	メインフレーム
*/
#pragma once

#include "resource.h"
#include <atltheme.h>

#ifdef	NDEBUG
#define	MAINFRAME_WINDOWCLASSNAME	_T("PeerCast")
#else
#define	MAINFRAME_WINDOWCLASSNAME	_T("PeerCast_DEBUG")
#endif

enum {
	WM_INITSETTINGS = WM_USER,
	WM_GETPORTNUMBER,
	WM_PLAYCHANNEL,
	WM_TRAYICON,
	WM_SHOWGUI,
	WM_SHOWMENU,
	WM_PROCURL,
	WM_ANTENNA, //JP-MOD
	WM_REFRESH, //JP-MOD
	WM_CHANINFOCHANGED, //JP-MOD
	WM_ADDLOG, //JP-MOD
	WM_UPDATESETTINGS, //JP-MOD

};

class Channel;

///////////////////////////////////////////
/// チャンネルリストビュー

class CListViewChan : public CWindowImpl<CListViewChan, CListViewCtrl>
{
public:
	DECLARE_WND_SUPERCLASS(_T("ChannelListView"), CListViewCtrl::GetWndClassName())

	// Message map
	BEGIN_MSG_MAP_EX( CListViewChan )

	END_MSG_MAP()

private:

};

///////////////////////////////////////////
/// 接続リストビュー

class CListViewConnect : public CWindowImpl<CListViewConnect, CListViewCtrl>
{
public:
	DECLARE_WND_SUPERCLASS(_T("ConnectListView"), CListViewCtrl::GetWndClassName())

	// Message map
	BEGIN_MSG_MAP_EX( CListViewChan )

	END_MSG_MAP()

private:

};

///////////////////////////////////////////////////
/// メインフレーム

class CMainFrame : 
	public CFrameWindowImpl<CMainFrame>, 
	public CUpdateUI<CMainFrame>,
	public CMessageFilter, 
	public CIdleHandler,
	public CCustomDraw<CMainFrame>,
	public CThemeImpl<CMainFrame>
{
public:
	DECLARE_FRAME_WND_CLASS(MAINFRAME_WINDOWCLASSNAME, 0)

	CMainFrame();

	// Overredes
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnIdle();

	void	UpdateLayout(BOOL bResizeBars = TRUE);

    // UI更新ハンドラマップ(使わなくてもいい？)
    BEGIN_UPDATE_UI_MAP( CMainWindow )
#if 0
        UPDATE_ELEMENT(IDC_CHECK1, UPDUI_TOOLBAR)
		UPDATE_ELEMENT(IDC_LOGDEBUG, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(IDC_LOGERRORS, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(IDC_LOGNETWORK, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(IDC_LOGCHANNELS, UPDUI_MENUPOPUP)
#endif
    END_UPDATE_UI_MAP()


	/// Message map
	BEGIN_MSG_MAP_EX( CMainFrame )
		MSG_WM_CREATE	( OnCreate	)
		MSG_WM_CLOSE	( OnClose )
		MSG_WM_DESTROY	( OnDestroy	)
		MSG_WM_SYSCOMMAND( OnSysCommand )
		MSG_WM_COPYDATA	( OnCopyData )
		NOTIFY_HANDLER_EX(IDC_LIST3, LVN_ITEMCHANGED, OnChanListItemChanged)
		NOTIFY_HANDLER_EX(IDC_LIST3, LVN_GETDISPINFO, OnChanListGetDispInfo)
		NOTIFY_HANDLER_EX(IDC_LIST3, NM_CLICK, OnChanListClick )
		NOTIFY_HANDLER_EX(IDC_LIST3, NM_DBLCLK, OnChanListDblClick )
		NOTIFY_HANDLER_EX(IDC_LIST3, NM_RCLICK, OnChanListRClick )
		NOTIFY_HANDLER_EX(IDC_LIST2, LVN_GETDISPINFO, OnConnectListGetDispInfo)
		NOTIFY_HANDLER_EX(IDC_LIST2, NM_RCLICK, OnConnectListRClick )
		NOTIFY_CODE_HANDLER_EX( TBN_DROPDOWN, OnDropDownLogMenu )
		MESSAGE_HANDLER_EX( WM_ADDLOG			, OnAddLog )
		MESSAGE_HANDLER_EX( WM_REFRESH			, OnRefresh )
		MESSAGE_HANDLER_EX( WM_CHANINFOCHANGED	, OnChanInfoChanged )
		MESSAGE_HANDLER_EX( WM_ANTENNA			, OnAntenna )
		MSG_WM_TIMER( OnTimer )
		COMMAND_ID_HANDLER_EX( IDC_CHECK1, OnStartServer )
		COMMAND_ID_HANDLER_EX( IDC_CHECK2, OnStartOutgoing )
		//COMMAND_ID_HANDLER_EX( ID_OPEN_BROADCAST_URL, OnOpenBroadcastURL )	// 使われない？
		COMMAND_ID_HANDLER_EX( ID_PLAY_SELECTED		, OnPlaySelected )
		//COMMAND_ID_HANDLER_EX( ID_OPEN_SETTING_URL, OnOpenSettingURL )	// 使われない？
		COMMAND_ID_HANDLER_EX( ID_SERVER_DISCONNECT	, OnServentDisconnect )
		COMMAND_ID_HANDLER_EX( ID_CHANNEL_DISCONNECT, OnChanDisconnect )
		COMMAND_ID_HANDLER_EX( ID_CHANNLE_BUMP		, OnChanBump )
		//COMMAND_ID_HANDLER_EX( IDC_BUTTON4, OnGetChannel )	// 使われてない？
		COMMAND_ID_HANDLER_EX( ID_CHANNEL_KEEP		, OnChanKeep )
		COMMAND_ID_HANDLER_EX( ID_CLEAR_LOG			, OnClearLog )
		COMMAND_ID_HANDLER_EX( ID_PAUSE_LOG			, OnPauseLog )
		COMMAND_ID_HANDLER_EX( IDC_LOGDEBUG			, OnLoglevelChange )
		COMMAND_ID_HANDLER_EX( IDC_LOGERRORS		, OnLoglevelChange )
		COMMAND_ID_HANDLER_EX( IDC_LOGNETWORK		, OnLoglevelChange )
		COMMAND_ID_HANDLER_EX( IDC_LOGCHANNELS		, OnLoglevelChange )
		COMMAND_ID_HANDLER_EX( ID_COPY_STREAMURL	, OnCopyStreamUrl )
		COMMAND_ID_HANDLER_EX( ID_COPY_CONTACTURL	, OnCopyContactUrl)
		COMMAND_ID_HANDLER_EX( ID_COPY_ADDRESS		, OnCopyAddress )
		COMMAND_RANGE_HANDLER_EX( ID_PLAY_CMD, ID_PLAY_CMD + 99, OnPlayChannnel )
		COMMAND_RANGE_HANDLER_EX( ID_URL_CMD , ID_URL_CMD + 99 , OnOpenContactURL )
		COMMAND_ID_HANDLER_EX( ID_OPEN_CONTACTURL, OnOpenContactURL )
		COMMAND_RANGE_HANDLER_EX( ID_INFO_CMD, ID_INFO_CMD + 99, OnOpenChannelInfo )
		MSG_WM_COMMAND( OnCommand )
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CCustomDraw<CMainFrame>)
		NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnCustomDraw)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
		//REFLECT_NOTIFICATIONS()
	ALT_MSG_MAP(1)
		MESSAGE_HANDLER_EX( WM_TRAYICON, OnTrayIcon )
	END_MSG_MAP()


	int		OnCreate(LPCREATESTRUCT /*lpCreateStruct*/);
	void	OnClose();
	void	OnDestroy();
	void	OnSysCommand(UINT nID, CPoint pt);
	BOOL	OnCopyData(CWindow wnd, PCOPYDATASTRUCT pCopyDataStruct);
	LRESULT OnChanListItemChanged(LPNMHDR pnmh);
	LRESULT OnChanListGetDispInfo(LPNMHDR pnmh);
	LRESULT	OnChanListClick(LPNMHDR pnmh);
	LRESULT	OnChanListDblClick(LPNMHDR pnmh);
	LRESULT	OnChanListRClick(LPNMHDR pnmh);
	LRESULT OnConnectListGetDispInfo(LPNMHDR pnmh);
	LRESULT OnConnectListRClick(LPNMHDR pnmh);
	LRESULT OnDropDownLogMenu(LPNMHDR pnmh);

	LRESULT OnAddLog(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnRefresh(UINT uMsg, WPARAM wParam, LPARAM lParam) {
		UpdateLayout(FALSE); return 0;
	}
	LRESULT OnChanInfoChanged(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnAntenna(UINT uMsg, WPARAM wParam, LPARAM lParam);
	void	OnTimer(UINT_PTR nIDEvent);
	LRESULT OnTrayIcon(UINT uMsg, WPARAM wParam, LPARAM lParam);

	void	OnStartServer(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnStartOutgoing(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnOpenBroadcastURL(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnPlaySelected(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnOpenSettingURL(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnServentDisconnect(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnChanDisconnect(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnChanBump(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnGetChannel(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnChanKeep(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnClearLog(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnPauseLog(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnLoglevelChange(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnCopyStreamUrl(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnCopyContactUrl(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnCopyAddress(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnPlayChannnel(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnOpenContactURL(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnOpenChannelInfo(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnCommand(UINT uNotifyCode, int nID, CWindow wndCtl);

	// Overriedes
	LRESULT OnCustomDraw(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
	DWORD	OnPrePaint(int nID, LPNMCUSTOMDRAW lpnmcd);
	DWORD	OnItemPrePaint(int nID, LPNMCUSTOMDRAW lpnmcd);
	DWORD	OnSubItemPrePaint(int nID, LPNMCUSTOMDRAW lpnmcd);
	DWORD	OnSubItemPostPaint(int nID, LPNMCUSTOMDRAW lpnmcd);

private:

	void	_CreateControls();
	void	_InitToolBar();
	void	_InitListView();
	void	_InitSysMenu();
	BOOL	_AddSimpleReBarBandCtrl(HWND hWndReBar, HWND hWndBand, int nID = 0, LPCTSTR lpstrTitle = NULL, BOOL bNewRow = FALSE, int cxWidth = 0, BOOL bFullWidthAlways = FALSE);
	int		_EnumSelectedChannels(function<bool (Channel* c)> funcDiscovered);
	Channel* _GetSelectedChannel();
	void	_SetControls();
	void	_ShowTrayMenu(bool bRClick);
	HMENU	_PrepareTrayClickMenu();
	HMENU	_PrepareTrayRClickMenu();

	// Data members
	CToolBarCtrl		m_ToolBar;
	CListViewChan		m_ListChan;
	CListViewConnect	m_ListConnect;
	CListBox			m_LogList;
public:
	CContainedWindow	m_wndMessage;
};


























