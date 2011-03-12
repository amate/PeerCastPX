/*!	@file	ConfigDialog.h
	@brief	設定ダイアログ

*/

#include "stdafx.h"
#include "ConfigDialog.h"
#include "peercast.h"


namespace {

// --------------------------------------------------
void setButtonStateEx(HWND hwnd, int id, bool on) //JP-MOD
{
	::SendDlgItemMessage(hwnd, id, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
}

// --------------------------------------------------
bool getButtonState(HWND hwnd, int id) //JP-MOD
{
	return ::SendDlgItemMessage(hwnd, id, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

}	// namespace


//////////////////////////////////////////////
/// 全般
class CGeneralPropertyPage : public CPropertyPageImpl<CGeneralPropertyPage>
{
public:
	enum { IDD = IDD_PROPPAGE_GENERAL };


	BEGIN_MSG_MAP_EX( CGeneralPropertyPage )
		MSG_WM_INITDIALOG( OnInitDialog )
		COMMAND_ID_HANDLER_EX( IDC_BTN_PLAYERBROWSE, OnPlayerBrowse )
		CHAIN_MSG_MAP( CPropertyPageImpl<CGeneralPropertyPage> )
	END_MSG_MAP()

	BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
	{
		setButtonStateEx(m_hWnd, IDC_SIMPLE_CHANNELLIST, servMgr->guiSimpleChannelList);
		setButtonStateEx(m_hWnd, IDC_SIMPLE_CONNECTIONLIST, servMgr->guiSimpleConnectionList);
		setButtonStateEx(m_hWnd, IDC_TOPMOST, servMgr->guiTopMost);
		setButtonStateEx(m_hWnd, IDC_MINIMIZE_STORE, servMgr->guiMinimizeStore);
		SetDlgItemText(IDC_EDIT_PLAYER, servMgr->PlayerPath.cstr());
		return TRUE;
	}

	BOOL OnApply()
	{
		servMgr->guiSimpleChannelList = getButtonState(m_hWnd, IDC_SIMPLE_CHANNELLIST);
		servMgr->guiSimpleConnectionList = getButtonState(m_hWnd, IDC_SIMPLE_CONNECTIONLIST);
		servMgr->guiTopMost = getButtonState(m_hWnd, IDC_TOPMOST);
		servMgr->guiMinimizeStore = getButtonState(m_hWnd, IDC_MINIMIZE_STORE);
		GetDlgItemText(IDC_EDIT_PLAYER, servMgr->PlayerPath.cstr(), String::MAX_LEN);
		return TRUE;
	}

	void OnPlayerBrowse(UINT uNotifyCode, int nID, CWindow wndCtl)
	{
		OPENFILENAME dlgParam;
		String strPath;

		memset(&dlgParam, 0, sizeof(dlgParam));
		GetDlgItemText(IDC_EDIT_PLAYER, strPath.cstr(), String::MAX_LEN);

		dlgParam.lStructSize = sizeof(dlgParam);
		dlgParam.hwndOwner = m_hWnd;
		dlgParam.lpstrFilter = "実行ファイル(*.exe)\0*.exe\0\0";
		dlgParam.lpstrFile = strPath.cstr();
		dlgParam.nMaxFile = String::MAX_LEN;
		dlgParam.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
		dlgParam.lpstrDefExt = "exe";

		if (::GetOpenFileName(&dlgParam))
			SetDlgItemText(IDC_EDIT_PLAYER, strPath.cstr());
	}

};



//////////////////////////////////////////////
/// タイトル
class CTitlePropertyPage : public CPropertyPageImpl<CTitlePropertyPage>
{
public:
	enum { IDD = IDD_PROPPAGE_TITLE };


	BEGIN_MSG_MAP_EX( CTitlePropertyPage )
		MSG_WM_INITDIALOG( OnInitDialog )
		COMMAND_ID_HANDLER_EX( IDC_TITLEMODIFY, OnTitleModify )
		CHAIN_MSG_MAP( CPropertyPageImpl<CTitlePropertyPage> )
	END_MSG_MAP()

	BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
	{
		setButtonStateEx(m_hWnd, IDC_TITLEMODIFY, servMgr->guiTitleModify);
		::SendMessage(m_hWnd, WM_COMMAND, IDC_TITLEMODIFY, 0);

		::SetDlgItemText(m_hWnd, IDC_TITLENORMAL, servMgr->guiTitleModifyNormal.cstr());
		::SetDlgItemText(m_hWnd, IDC_TITLEMINIMIZED, servMgr->guiTitleModifyMinimized.cstr());
		return TRUE;
	}
	BOOL OnApply()
	{
		servMgr->guiTitleModify = getButtonState(m_hWnd, IDC_TITLEMODIFY);
		::GetDlgItemText(m_hWnd, IDC_TITLENORMAL, servMgr->guiTitleModifyNormal.cstr(), String::MAX_LEN);
		::GetDlgItemText(m_hWnd, IDC_TITLEMINIMIZED, servMgr->guiTitleModifyMinimized.cstr(), String::MAX_LEN);
		return TRUE;
	}

	void OnTitleModify(UINT uNotifyCode, int nID, CWindow wndCtl)
	{
		bool isChecked = getButtonState(m_hWnd, IDC_TITLEMODIFY);
		::EnableWindow(GetDlgItem(IDC_TITLENORMAL), isChecked);
		::EnableWindow(GetDlgItem(IDC_TITLEMINIMIZED), isChecked);
	}

};

//////////////////////////////////////////////
/// レイアウト
class CLayoutPropertyPage : public CPropertyPageImpl<CLayoutPropertyPage>
{
public:
	enum { IDD = IDD_PROPPAGE_LAYOUT };


	BEGIN_MSG_MAP_EX( CLayoutPropertyPage )
		MSG_WM_INITDIALOG( OnInitDialog )
		CHAIN_MSG_MAP( CPropertyPageImpl<CLayoutPropertyPage> )
	END_MSG_MAP()

	BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
	{
		::SetDlgItemInt(m_hWnd, IDC_CHANLIST_DISPLAYS, servMgr->guiChanListDisplays, TRUE);
		::SetDlgItemInt(m_hWnd, IDC_CONNLIST_DISPLAYS, servMgr->guiConnListDisplays, TRUE);

		::SendDlgItemMessage(m_hWnd, IDC_SPIN_CHANLIST_DISPLAYS, UDM_SETRANGE32, -1, 99);
		::SendDlgItemMessage(m_hWnd, IDC_SPIN_CONNLIST_DISPLAYS, UDM_SETRANGE32, -1, 99);
		return TRUE;
	}
	BOOL OnApply()
	{
		servMgr->guiChanListDisplays = ::GetDlgItemInt(m_hWnd, IDC_CHANLIST_DISPLAYS, NULL, TRUE);
		servMgr->guiConnListDisplays = ::GetDlgItemInt(m_hWnd, IDC_CONNLIST_DISPLAYS, NULL, TRUE);
		return TRUE;
	}

};

//////////////////////////////////////////////
/// 拡張機能
class CExpansionPropertyPage : public CPropertyPageImpl<CExpansionPropertyPage>
{
public:
	enum { IDD = IDD_PROPPAGE_EXPANSION };


	BEGIN_MSG_MAP_EX( CExpansionPropertyPage )
		MSG_WM_INITDIALOG( OnInitDialog )
		CHAIN_MSG_MAP( CPropertyPageImpl<CExpansionPropertyPage> )
	END_MSG_MAP()

	BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
	{
		setButtonStateEx(m_hWnd, IDC_CLAP_ENABLESOUND, servMgr->ppClapSound);
		::SetDlgItemText(m_hWnd, IDC_CLAP_SOUNDPATH, servMgr->ppClapSoundPath.cstr());
		{
			HWND hwndPath = ::GetDlgItem(m_hWnd, IDC_CLAP_SOUNDPATH);
			if(::IsWindow(hwndPath))
				::SHAutoComplete(hwndPath, SHACF_FILESYSTEM);
		}

		setButtonStateEx(m_hWnd, IDC_ANTENNA_ICON, servMgr->guiAntennaNotifyIcon);
		return TRUE;
	}
	BOOL OnApply()
	{
		servMgr->ppClapSound = getButtonState(m_hWnd, IDC_CLAP_ENABLESOUND);
		::GetDlgItemText(m_hWnd, IDC_CLAP_SOUNDPATH, servMgr->ppClapSoundPath.cstr(), String::MAX_LEN);

		servMgr->guiAntennaNotifyIcon = getButtonState(m_hWnd, IDC_ANTENNA_ICON);
		return TRUE;
	}

};







/// プロパティシートを表示
INT_PTR	CConfigSheet::Show(HWND hWndParent)
{
	SetTitle(_T("GUI 設定"));
	m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;

	CGeneralPropertyPage	generalPage;
	AddPage(generalPage);
	CTitlePropertyPage		titlePage;
	AddPage(titlePage);
	CLayoutPropertyPage		layoutPage;
	AddPage(layoutPage);
	CExpansionPropertyPage	expansionPage;
	AddPage(expansionPage);
	
	return DoModal(hWndParent);
}


