/*!	@file	ConfigDialog.h
	@brief	�ݒ�_�C�A���O

*/

#pragma once

#include <atldlgs.h>
#include "resource.h"



///////////////////////////////////////////////////////////
///  �ݒ�v���p�e�B�V�[�g

class CConfigSheet : public CPropertySheetImpl<CConfigSheet>
{
public:
	INT_PTR	Show(HWND hWndParent);

    BEGIN_MSG_MAP_EX( CConfigSheet )
        CHAIN_MSG_MAP( CPropertySheetImpl<CConfigSheet> )
    END_MSG_MAP()

};






















