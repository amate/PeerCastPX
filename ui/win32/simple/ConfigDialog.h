/*!	@file	ConfigDialog.h
	@brief	設定ダイアログ

*/

#pragma once

#include <atldlgs.h>
#include "resource.h"



///////////////////////////////////////////////////////////
///  設定プロパティシート

class CConfigSheet : public CPropertySheetImpl<CConfigSheet>
{
public:
	INT_PTR	Show(HWND hWndParent);

    BEGIN_MSG_MAP_EX( CConfigSheet )
        CHAIN_MSG_MAP( CPropertySheetImpl<CConfigSheet> )
    END_MSG_MAP()

};






















