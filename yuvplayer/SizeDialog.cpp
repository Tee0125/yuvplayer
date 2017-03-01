/*
 * Copyright (c) 2010, Tae-young Jung
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the <organization>.
 * 4. Neither the name of the <organization> nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// SizeDialog.cpp : implementation file
//

#include "stdafx.h"
#include "yuvplayer.h"
#include "SizeDialog.h"


// CSizeDialog dialog

IMPLEMENT_DYNAMIC(CSizeDialog, CDialog)

CSizeDialog::CSizeDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CSizeDialog::IDD, pParent)
	, width(4096)
	, height(2048)
{

}

CSizeDialog::~CSizeDialog()
{
}

void CSizeDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_WIDTH, width);
	DDV_MinMaxUInt(pDX, width, 1, 4096);
	DDX_Text(pDX, IDC_HEIGHT, height);
	DDV_MinMaxUInt(pDX, height, 1, 4096);
}


BEGIN_MESSAGE_MAP(CSizeDialog, CDialog)
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()


// CSizeDialog message handlers

void CSizeDialog::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);

	// TODO: Add your message handler code here
	CEdit* edit = (CEdit*)GetDlgItem(IDC_WIDTH);
	edit->SetFocus();
}
