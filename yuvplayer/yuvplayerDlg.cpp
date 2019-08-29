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

// yuvplayerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "yuvplayer.h"
#include "yuvplayerDlg.h"

#include "SizeDialog.h"
#include "GoDialog.h"

#include <math.h>
#include <io.h>
#include <sys/types.h>
#include <share.h>

#ifdef SUPPORT_PCRE
#	include "pcre.h"
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#ifdef SUPPORT_PCRE
#	define REGEX_BUF_SIZE 10
#endif

typedef struct
{
	wchar_t* string;

	int   width;
	int   height;

	UINT  size_id;

} size_info_t;

#ifdef SUPPORT_PCRE
static const size_info_t size_info[] = {
	{ L"1080p", 1920, 1080, ID_SIZE_HD },
	{ L"720p", 1280, 720, ID_SIZE_SD },
	{ L"wvga", 832, 480, ID_SIZE_WVGA },
	{ L"wqvga", 416, 240, ID_SIZE_WQVGA },
	{ L"vga", 640, 480, ID_SIZE_VGA },
	{ L"qcif", 176, 144, ID_SIZE_QCIF },
	{ L"cif", 352, 288, ID_SIZE_CIF },

	// end delimiter
	{ NULL, 0, 0, 0 }
};
#else
// in near future, update me to PCRE
static const size_info_t size_info[] = {
	{ L"3840x2160", 3840, 2160, ID_SIZE_4K },
    { L"4K", 1920, 1080, ID_SIZE_4K },

	{ L"1920x1080", 1920, 1080, ID_SIZE_HD },
	{ L"1080p", 1920, 1080, ID_SIZE_HD },

	{ L"1280x720", 1280, 720, ID_SIZE_SD },
	{ L"720p", 1280, 720, ID_SIZE_SD },

	{ L"832x480", 832, 480, ID_SIZE_WVGA },
	{ L"wvga", 832, 480, ID_SIZE_WVGA },

	{ L"416x240", 416, 240, ID_SIZE_WQVGA },
	{ L"wqvga", 416, 240, ID_SIZE_WQVGA },

	{ L"640x480", 640, 480, ID_SIZE_VGA },
	{ L"vga", 640, 480, ID_SIZE_VGA },

	{ L"176x144", 176, 144, ID_SIZE_QCIF },
	{ L"qcif", 176, 144, ID_SIZE_QCIF },

	{ L"352x288", 352, 288, ID_SIZE_CIF },
	{ L"cif", 352, 288, ID_SIZE_CIF },

	{ L"192x256", 192, 256, ID_SIZE_192X256 },

	{ L"1920x1088", 1920, 1088, ID_SIZE_CUSTOM },
	{ L"2560x1600", 2560, 1600, ID_SIZE_CUSTOM },

	// end delimiter
	{ NULL, 0, 0, 0 }
};
#endif
#pragma pack(push,1)
typedef struct
{
	unsigned int U : 10;
	unsigned int Y : 10;
	unsigned int V : 10;
	unsigned int A : 2;
} Y410Pack;
#pragma pack(pop)


// CyuvplayerDlg dialog
CyuvplayerDlg::CyuvplayerDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CyuvplayerDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	segment = NULL;
    memset(&fb, 0, sizeof(fb));
    memset(&fbSecondary, 0, sizeof(fbSecondary));
	m_color = YUV420;

	fd = -1;
    fdSecondary = -1;
	ratio = 1.0;

	segment_option = 0;

	count = cur = 0;
	started = FALSE;

	wsprintf(filename, L"%s", L"YUV player");

    primaryFileSize = 0;
    isShowingDifference = false;

	OpenGLView = new COpenGLView;
}

CyuvplayerDlg::~CyuvplayerDlg()
{
    delete[] segment;
}

void CyuvplayerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SLIDER, m_slider);
	DDX_Control(pDX, IDC_VIEW, m_view);
}

BEGIN_MESSAGE_MAP(CyuvplayerDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_OPEN, &CyuvplayerDlg::OnOpen)
	ON_COMMAND(ID_OPEN, &CyuvplayerDlg::OnOpen)
	ON_COMMAND(ID_FILE_RELOAD, &CyuvplayerDlg::OnFileReload)
	ON_COMMAND(ID_FILE_EXIT, &CyuvplayerDlg::OnFileExit)

	ON_COMMAND_RANGE(ID_SIZE_START,  ID_SIZE_END,  OnSizeChange)
	ON_COMMAND_RANGE(ID_COLOR_START, ID_COLOR_END, OnColor)
	ON_COMMAND_RANGE(ID_ZOOM_START,  ID_ZOOM_END,  OnZoom)
	ON_COMMAND_RANGE(ID_SEGMENT_START, ID_SEGMENT_END, OnSegment)

	ON_BN_CLICKED(IDC_REWIND, &CyuvplayerDlg::OnBnClickedRewind)
	ON_BN_CLICKED(IDC_PLAY, &CyuvplayerDlg::OnBnClickedPlay)
	ON_BN_CLICKED(IDC_STOP, &CyuvplayerDlg::OnBnClickedStop)
	ON_BN_CLICKED(IDC_FFORWARD, &CyuvplayerDlg::OnBnClickedFforward)

	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_WM_DROPFILES()
	ON_COMMAND(ID_FILE_GO, &CyuvplayerDlg::OnFileGo)
	ON_WM_CONTEXTMENU()
	ON_WM_HSCROLL()
	ON_COMMAND(ID_CMENU_SAVE_LUMINANCE, &CyuvplayerDlg::OnCmenuSaveLuminance)
	ON_COMMAND(ID_CMENU_SAVE_YUV444, &CyuvplayerDlg::OnCmenuSaveYuv444)
	ON_COMMAND(ID_CMENU_SAVE_YUV422, &CyuvplayerDlg::OnCmenuSaveYuv422)
	ON_COMMAND(ID_CMENU_SAVE_YUV420, &CyuvplayerDlg::OnCmenuSaveYuv420)
	ON_COMMAND(ID_CMENU_SAVE_RGB, &CyuvplayerDlg::OnCmenuSaveRgb)
    ON_COMMAND(ID_OPEN_SECONDARY, &CyuvplayerDlg::OnOpenSecondary)
    ON_COMMAND(ID_VIEW_SHOWDIFFERENCE, &CyuvplayerDlg::OnViewShowdifference)
END_MESSAGE_MAP()


// CyuvplayerDlg message handlers

BOOL CyuvplayerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	CRect rect;

	m_view.GetWindowRect(rect);
	ScreenToClient(rect);

	OpenGLView->Create(NULL,NULL,WS_CHILD|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_VISIBLE, rect, this, 0);

	menu = GetMenu();
	DragAcceptFiles();

	// TODO: Add extra initialization here
	m_open_btn.AutoLoad( IDC_OPEN, this );
	m_rewind_btn.AutoLoad( IDC_REWIND, this );
	m_play_btn.AutoLoad( IDC_PLAY, this );
	m_stop_btn.AutoLoad( IDC_STOP, this );
	m_fforward_btn.AutoLoad( IDC_FFORWARD, this );
	
	m_open_btn.SetWindowPos(NULL, 0, 0, 16,16, 0 ) ;

	customDlg = new CSizeDialog;

	Resize( DEFAULT_WIDTH, DEFAULT_HEIGHT );

    if (__argc == 2)
    {
        if (fdSecondary>-1)
        {
            _close(fdSecondary);
            fdSecondary = -1;
            isShowingDifference = false;
            menu->CheckMenuItem(ID_VIEW_SHOWDIFFERENCE, MF_UNCHECKED);
        }
        FileOpen(__targv[1], fd);
    }

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CyuvplayerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();

	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CyuvplayerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CyuvplayerDlg::Resize(int width, int height)
{
	CRect wRect[2], btnRect;
	int w, h, new_height;

	h = 16;
	w = 16;

	this->width  = width;
	this->height = height;
	for( t_width = 2  ; t_width  < width  ; t_width  *= 2 );
	for( t_height = 2 ; t_height < height ; t_height *= 2 );

    ReallocateMem(fb);
    ReallocateMem(fbSecondary);

    if (segment != NULL) delete segment;
    segment = new unsigned char[t_width*t_height*4];
	
	// reset alpha value to 255
	memset( fb.rgba, 255, sizeof(unsigned char)*t_width*t_height*4 );

	UpdateParameter();
	OpenGLView->SetParam( t_width, t_height, ratio);
	
	DrawSegment();
	LoadFrame();

	this->GetWindowRect(wRect[0]);
	this->GetClientRect(wRect[1]);

	int margin_w = wRect[0].Width()-wRect[1].Width();

	width  = (int)floor(width*ratio);
	height = (int)floor(height*ratio);

	m_view.SetWindowPos(NULL, 0, 0, width, height, 0);
	OpenGLView->SetWindowPos(NULL, 0, 0, width, height, 0);

	new_height =  height+wRect[0].Height() - wRect[1].Height() + h + MARGIN;

	m_open_btn.SetWindowPos(	NULL, 0,   height+MARGIN, w, h, 0);
	m_rewind_btn.SetWindowPos(	NULL, w,   height+MARGIN, w, h, 0);
	m_play_btn.SetWindowPos(	NULL, w*2, height+MARGIN, w, h, 0);
	m_stop_btn.SetWindowPos(	NULL, w*3, height+MARGIN, w, h, 0);
	m_fforward_btn.SetWindowPos(NULL, w*4, height+MARGIN, w, h, 0);

	m_slider.SetWindowPos(		NULL, w*5+MARGIN, height+MARGIN, width-5*w-2*MARGIN, h, 0);
	SetWindowPos(NULL, 0, 0, width+margin_w, new_height, SWP_NOMOVE);

	this->GetWindowRect(wRect[0]);
	this->GetClientRect(wRect[1]);

	new_height =  height+wRect[0].Height() - wRect[1].Height() + h + MARGIN;
	SetWindowPos(NULL, 0, 0, width+margin_w, new_height, SWP_NOMOVE);
}

void CyuvplayerDlg::OnOpen()
{
	// TODO: Add your command handler code here
	CFileDialog	dlg(
			TRUE, _T("YUV"), NULL,
			OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_ALLOWMULTISELECT, 
			_T("Image Files (YUV, RAW, IMG, ...)|*.YUV;*.RAW;*.IMG|")
			);

	if( IDOK != dlg.DoModal() )
		return;	

    if (fdSecondary>-1)
    {
        _close(fdSecondary);
        fdSecondary = -1;
        isShowingDifference = false;
        menu->CheckMenuItem(ID_VIEW_SHOWDIFFERENCE, MF_UNCHECKED);
    }
	FileOpen(dlg.GetPathName(), fd);


    CFileStatus filestatus;
    if (CFile::GetStatus(dlg.GetPathName(), filestatus))
    {
        primaryFileSize = filestatus.m_size;
    }
}

void CyuvplayerDlg::OnOpenSecondary()
{
    // TODO: Add your command handler code here
    CFileDialog	dlg(
        TRUE, _T("YUV"), NULL,
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_ALLOWMULTISELECT,
        _T("Image Files (YUV, RAW, IMG, ...)|*.YUV;*.RAW;*.IMG|")
    );

    if (IDOK != dlg.DoModal())
        return;

    CFileStatus filestatus;
    if (primaryFileSize!=0 && CFile::GetStatus(dlg.GetPathName(), filestatus) && primaryFileSize == filestatus.m_size)
    {
        FileOpen(dlg.GetPathName(), fdSecondary);
    }
    else
    {
        AfxMessageBox(_T("Secondary file should be of exactly same size and format as primary. Primary file should be already loaded"));
    }
}


void CyuvplayerDlg::OnFileReload()
{
	UpdateParameter();
	LoadFrame();
}

void CyuvplayerDlg::OnSizeChange(UINT nID )
{

	// TODO: Add your command handler code here
	int i;

	// uncheck all size menu items
	for (i = ID_SIZE_START ; i <= ID_SIZE_END ; i++)
			menu->CheckMenuItem(i,	MF_UNCHECKED);

	switch( nID ){
		case ID_SIZE_4K:
			menu->CheckMenuItem(ID_SIZE_4K, MF_CHECKED);
			Resize(3840, 2160);
			return;

		case ID_SIZE_HD:
			menu->CheckMenuItem( ID_SIZE_HD,	MF_CHECKED);
			Resize( 1920, 1080 );
			return;
		
		case ID_SIZE_SD:
			menu->CheckMenuItem( ID_SIZE_SD,	MF_CHECKED);
			Resize( 1280, 720 );
			return;	

		case ID_SIZE_VGA:
			menu->CheckMenuItem( ID_SIZE_VGA,	MF_CHECKED);
			Resize( 640, 480 );
			return;
			
		case ID_SIZE_WVGA:
			menu->CheckMenuItem( ID_SIZE_WVGA,	MF_CHECKED);
			Resize( 832, 480 );
			return;

		case ID_SIZE_WQVGA:
			menu->CheckMenuItem( ID_SIZE_WQVGA,	MF_CHECKED);
			Resize( 416, 240 );
			return;

		case ID_SIZE_CIF:
			menu->CheckMenuItem( ID_SIZE_CIF,	MF_CHECKED);
			Resize( 352, 288 );
			return;

		case ID_SIZE_QCIF:
			menu->CheckMenuItem( ID_SIZE_QCIF,	MF_CHECKED);
			Resize( 176, 144 );
			return;

        case ID_SIZE_192X256:
			menu->CheckMenuItem( ID_SIZE_192X256,	MF_CHECKED);
			Resize( 192, 256 );
			return;

	}

	// custom resolution window 
	if( customDlg->DoModal() != IDOK )
		return;

	menu->CheckMenuItem( ID_SIZE_CUSTOM,MF_CHECKED);

	Resize( customDlg->width, customDlg->height );
}

void CyuvplayerDlg::OnZoom(UINT nID )
{

	// TODO: Add your command handler code here
	int i;

	// uncheck all size menu items
	for (i = ID_ZOOM_START ; i <= ID_ZOOM_END ; i++)
			menu->CheckMenuItem(i,	MF_UNCHECKED);

	switch( nID ){
		case ID_ZOOM_41:
			menu->CheckMenuItem( ID_ZOOM_41, MF_CHECKED);
			ratio = 4.0;
			break;

		case ID_ZOOM_21:
			menu->CheckMenuItem( ID_ZOOM_21, MF_CHECKED);
			ratio = 2.0;
			break;

		case ID_ZOOM_11:
			menu->CheckMenuItem( ID_ZOOM_11, MF_CHECKED);
			ratio = 1.0;
			break;

		case ID_ZOOM_12:
			menu->CheckMenuItem( ID_ZOOM_12, MF_CHECKED);
			ratio = 0.5;
			break;

		case ID_ZOOM_14:
			menu->CheckMenuItem( ID_ZOOM_14, MF_CHECKED);
			ratio = 0.25;
			break;
	}
	Resize( width, height );

}

void CyuvplayerDlg::OnSegment(UINT nID )
{

	// TODO: Add your command handler code here
	switch( nID ){
		case ID_SEGMENT_64X64:
			if (segment_option & SEGMENT64x64)
			{
				menu->CheckMenuItem( ID_SEGMENT_64X64, MF_UNCHECKED);
				segment_option ^= SEGMENT64x64;
			}
			else
			{
				menu->CheckMenuItem( ID_SEGMENT_64X64, MF_CHECKED);
				segment_option |= SEGMENT64x64;
			}
			break;
			
		case ID_SEGMENT_32X32:
			if (segment_option & SEGMENT32x32)
			{
				menu->CheckMenuItem( ID_SEGMENT_32X32, MF_UNCHECKED);
				segment_option ^= SEGMENT32x32;
			}
			else
			{
				menu->CheckMenuItem( ID_SEGMENT_32X32, MF_CHECKED);
				segment_option |= SEGMENT32x32;
			}
			break;

		case ID_SEGMENT_16X16:
			if (segment_option & SEGMENT16x16)
			{
				menu->CheckMenuItem( ID_SEGMENT_16X16, MF_UNCHECKED);
				segment_option ^= SEGMENT16x16;
			}
			else
			{
				menu->CheckMenuItem( ID_SEGMENT_16X16, MF_CHECKED);
				segment_option |= SEGMENT16x16;
			}
			break;
	}

	DrawSegment();

}

void CyuvplayerDlg::OnColor(UINT nID )
{

	// TODO: Add your command handler code here
	int i;

	// uncheck all size menu items
	for (i = ID_COLOR_START ; i <= ID_COLOR_END ; i++)
			menu->CheckMenuItem(i,	MF_UNCHECKED);

	switch( nID ){
		// planar formats
		case ID_COLOR_YUV420_10LE:
			menu->CheckMenuItem( ID_COLOR_YUV420_10LE, MF_CHECKED);
			m_color = YUV420_10LE;
			break;
        
		case ID_COLOR_YUV420_10BE:
			menu->CheckMenuItem( ID_COLOR_YUV420_10BE, MF_CHECKED);
			m_color = YUV420_10BE;
			break;
        
		case ID_COLOR_YUV444:
			menu->CheckMenuItem( ID_COLOR_YUV444, MF_CHECKED);
			m_color = YUV444;
			break;

		case ID_COLOR_YUV422:
			menu->CheckMenuItem( ID_COLOR_YUV422, MF_CHECKED);
			m_color = YUV422;
			break;

		case ID_COLOR_YUV420:
			menu->CheckMenuItem( ID_COLOR_YUV420, MF_CHECKED);
			m_color = YUV420;
			break;

		// packed array
        case ID_COLOR_PACKEDYUV444:
            menu->CheckMenuItem( ID_COLOR_PACKEDYUV444,   MF_CHECKED);
            m_color = PACKED_YUV444;
            break;

		case ID_COLOR_NV21:
			menu->CheckMenuItem( ID_COLOR_NV21,   MF_CHECKED);
			m_color = NV21;
			break;

		case ID_COLOR_NV12:
			menu->CheckMenuItem( ID_COLOR_NV12,   MF_CHECKED);
			m_color = NV12;
			break;

		case ID_COLOR_UYVY:
			menu->CheckMenuItem( ID_COLOR_UYVY,   MF_CHECKED);
			m_color = UYVY;
			break;

		case ID_COLOR_YUYV:
			menu->CheckMenuItem( ID_COLOR_YUYV,   MF_CHECKED);
			m_color = YUYV;
			break;

		case ID_COLOR_AYUV:
			menu->CheckMenuItem(ID_COLOR_AYUV, MF_CHECKED);
			m_color = AYUV;
			break;

		// RGB
		case ID_COLOR_RGB32:
			menu->CheckMenuItem( ID_COLOR_RGB32,   MF_CHECKED);
			m_color = RGB32;
			break;
			
		case ID_COLOR_RGB24:
			menu->CheckMenuItem( ID_COLOR_RGB24,   MF_CHECKED);
			m_color = RGB24;
			break;
			
		case ID_COLOR_RGB16:
			menu->CheckMenuItem( ID_COLOR_RGB16,   MF_CHECKED);
			m_color = RGB16;
			break;

        case ID_COLOR_RGBP:
            menu->CheckMenuItem(ID_COLOR_RGBP, MF_CHECKED);
            m_color = RGBP;
            break;
		case ID_COLOR_P010:
			menu->CheckMenuItem(ID_COLOR_P010, MF_CHECKED);
			m_color = P010;
			break;
		case ID_COLOR_P010MSB:
			menu->CheckMenuItem(ID_COLOR_P010MSB, MF_CHECKED);
			m_color = P010MSB;
			break;
		case ID_COLOR_P210:
			menu->CheckMenuItem(ID_COLOR_P210, MF_CHECKED);
			m_color = P210;
			break;
        case ID_COLOR_P210MSB:
            menu->CheckMenuItem(ID_COLOR_P210MSB, MF_CHECKED);
            m_color = P210MSB;
            break;
		case ID_COLOR_Y210:
			menu->CheckMenuItem(ID_COLOR_Y210, MF_CHECKED);
			m_color = Y210;
			break;
		case ID_COLOR_Y210MSB:
			menu->CheckMenuItem(ID_COLOR_Y210MSB, MF_CHECKED);
			m_color = Y210MSB;
			break;
		case ID_COLOR_Y216:
			menu->CheckMenuItem(ID_COLOR_Y216, MF_CHECKED);
			m_color = Y216;
			break;
		case ID_COLOR_Y410:
			menu->CheckMenuItem(ID_COLOR_Y410, MF_CHECKED);
			m_color = Y410;
			break;

		// grey
		default:
			menu->CheckMenuItem( ID_COLOR_Y     , MF_CHECKED);
			m_color = YYY;
			
	}
	UpdateParameter();
	LoadFrame();

}

void CyuvplayerDlg::UpdateParameter()
{
	__int64 size;

	if( fd < 0 ){
		count = 0;
		return;
	}

	_lseeki64( fd, 0, SEEK_END );
	size = _telli64(fd);

	frame_size_y = width*height;
	if (m_color == YUV444 || m_color == PACKED_YUV444)
		frame_size_uv = width*height;
	else if (m_color == YUV422 || m_color == UYVY || m_color == YUYV)
		frame_size_uv = ((width+1)>>1)*height;
	else if (m_color == P010 || m_color == P010MSB)
		frame_size_uv = ((width + 1) >> 1 << 1)*((height+1)>>1);
	else if(m_color == P210 || m_color == P210MSB)
		frame_size_uv = (((width + 1) >> 1) << 1)*((height + 1) >> 1 << 1);
	else if (m_color == YUV420 || m_color == NV12 || m_color == NV21 || m_color == YUV420_10LE || m_color == YUV420_10BE)
		frame_size_uv = ((width+1)>>1) * ((height+1)>>1);
	else 
		frame_size_uv = 0;
    
    if ( m_color == YUV420_10LE || m_color == YUV420_10BE || m_color == P010 || m_color == P010MSB || m_color == P210 || m_color == P210MSB)
    {
        frame_size_y  *= 2;
        frame_size_uv *= 2; 
    }

	if (m_color == RGB32 || m_color == Y210 || m_color == Y210MSB || m_color == Y216 || m_color == Y410 || m_color == AYUV)
		frame_size = frame_size_y*4;
	else if (m_color == RGB24 || m_color == RGBP)
		frame_size = frame_size_y*3;
	else if (m_color == RGB16)
		frame_size = frame_size_y*2;
	else if (m_color == P010 || m_color == P010MSB || m_color == P210 || m_color == P210MSB)
        frame_size = frame_size_y + frame_size_uv;
	else
		frame_size = frame_size_y + 2*frame_size_uv;

	count = (int)(size / frame_size);

	m_slider.SetRange(0, count);
	m_slider.SetTicFreq(1);
}

void CyuvplayerDlg::LoadFrame()
{
    CString caption;

    LoadFrame(fd, fb);

    if (fdSecondary > -1)
    {
        LoadFrame(fdSecondary,fbSecondary);
        CalcDifIfNeeded();
        caption.Format(L"%s(+%s) - frame: %d/%d", filename, filenameSecondary, cur + 1, count);
    }
    else
    {
        caption.Format(L"%s - frame: %d/%d", filename, cur + 1, count);
    }

    yuv2rgb();
    OpenGLView->LoadTexture(fb.rgba);

    m_slider.SetPos(cur);
    m_slider.UpdateData(FALSE);

    SetWindowText(caption);
}

void CyuvplayerDlg::LoadFrame(int fileDesc, const FrameBuffer& frameBuf)
{

	if( fileDesc < 0 ){
		m_slider.SetPos(0);
		return;
	}

	_lseeki64( fileDesc, (__int64)cur*(__int64)frame_size, SEEK_SET );

	if( m_color == RGB32 || m_color == Y210 || m_color == Y210MSB || m_color == Y216 || m_color == Y410 || m_color == AYUV)
		_read( fileDesc, frameBuf.misc, frame_size_y*4 );

	else if( m_color == RGB24 || m_color == RGBP)
		_read( fileDesc, frameBuf.misc, frame_size_y*3 );
	
	else if( m_color == RGB16 )
		_read( fileDesc, frameBuf.misc, frame_size_y*2 );

	else if( m_color == UYVY )
		_read( fileDesc, frameBuf.misc, frame_size_y*2 );

	else if( m_color == YUYV )
		_read( fileDesc, frameBuf.misc, frame_size_y*2 );

	else if( m_color == NV12 || m_color == NV21)
	{
		_read( fileDesc, frameBuf.y,    frame_size_y );
		_read( fileDesc, frameBuf.misc, frame_size_y/2 );
	}
    else if (m_color == P010 || m_color == P010MSB || m_color == P210 || m_color == P210MSB)
    {
        _read(fileDesc, frameBuf.y, frame_size_y);
        _read(fileDesc, frameBuf.misc, frame_size_uv);
    }
    else if ( m_color == PACKED_YUV444 )
        _read( fileDesc, frameBuf.misc, frame_size );
	else
	{
		_read( fileDesc, frameBuf.y, frame_size_y );
		_read( fileDesc, frameBuf.u, frame_size_uv );
		_read( fileDesc, frameBuf.v, frame_size_uv );
	}

}

#define clip(var) ((var>=255)?255:(var<=0)?0:var)
void CyuvplayerDlg::yuv2rgb(void)
{

	int j, i;
	int c, d, e;

	int stride_uv;

	int r, g, b;
	
	unsigned char* line = fb.rgba;
	unsigned char* cur;

	short* rgb16;

    if( m_color == PACKED_YUV444) {
        for( j = 0 ; j < height ; j++ ){
            cur = line;
            for( i = 0 ; i < width ; i++ ){
                c = fb.misc[(j*width+i)*3    ] - 16;
                d = fb.misc[(j*width+i)*3 + 1] - 128;
                e = fb.misc[(j*width+i)*3 + 2] - 128;

                (*cur) = clip(( 298 * c           + 409 * e + 128) >> 8);cur++;
                (*cur) = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);cur++;
                (*cur) = clip(( 298 * c + 516 * d           + 128) >> 8);cur+=2;
            }
            line += t_width<<2;
        }
    }
	else if( m_color == YUV444 ){
		for( j = 0 ; j < height ; j++ ){
			cur = line;
			for( i = 0 ; i < width ; i++ ){
				c = fb.y[j*width+i] - 16;
				d = fb.u[j*width+i] - 128;
				e = fb.v[j*width+i] - 128;

				(*cur) = clip(( 298 * c           + 409 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c + 516 * d           + 128) >> 8);cur+=2;
			}
			line += t_width<<2;
		}
	}
	else if( m_color == YUV422 ){
		stride_uv = (width+1)>>1;

		for( j = 0 ; j < height ; j++ ){
			cur = line;
			for( i = 0 ; i < width ; i++ ){
				c = fb.y[j*width+i] - 16;
				d = fb.u[j*stride_uv+(i>>1)] - 128;
				e = fb.v[j*stride_uv+(i>>1)] - 128;

				(*cur) = clip(( 298 * c           + 409 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c + 516 * d           + 128) >> 8);cur+=2;
			}
			line += t_width<<2;
		}
	}

	else if( m_color == UYVY ){
		unsigned char* t = fb.misc;
		for( j = 0 ; j < height ; j++ ){
			cur = line;
			for( i = 0 ; i < width ; i+=2 ){
				c = *(t+1) - 16;    // Y1
				d = *(t+0) - 128;   // U
				e = *(t+2) - 128;   // V

				(*cur) = clip(( 298 * c           + 409 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c + 516 * d           + 128) >> 8);cur+=2;

				c = *(t+3) - 16;    // Y2
				(*cur) = clip(( 298 * c           + 409 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c + 516 * d           + 128) >> 8);cur+=2;

				t += 4;
			}
			line += t_width<<2;
		}
	}

	else if( m_color == YUYV ){
		unsigned char* t = fb.misc;
		for( j = 0 ; j < height ; j++ ){
			cur = line;
			for( i = 0 ; i < width ; i+=2 ){
				c = *(t+0) - 16;    // Y1
				d = *(t+1) - 128;   // U
				e = *(t+3) - 128;   // V

				(*cur) = clip(( 298 * c           + 409 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c + 516 * d           + 128) >> 8);cur+=2;

				c = *(t+2) - 16;    // Y2
				(*cur) = clip(( 298 * c           + 409 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c + 516 * d           + 128) >> 8);cur+=2;

				t += 4;
			}
			line += t_width<<2;
		}
	}
	else if (m_color == AYUV) {
		unsigned char* t = fb.misc;
		for (j = 0; j < height; j++) {
			cur = line;
			for (i = 0; i < width; i++) 
			{
				c = fb.misc[j*width * 4 + i*4 +2] - 16;    // Y
				d = fb.misc[j*width * 4 + i*4 + 1] - 128;   // U
				e = fb.misc[j*width * 4 + i*4] - 128;   // V

				(*cur) = clip((298 * c + 409 * e + 128) >> 8); cur++;
				(*cur) = clip((298 * c - 100 * d - 208 * e + 128) >> 8); cur++;
				(*cur) = clip((298 * c + 516 * d + 128) >> 8); cur += 2;
			}
			line += t_width << 2;
		}
	}
	else if( m_color == YUV420 || m_color == NV12 || m_color == NV21 ){
		stride_uv = (width+1)>>1;

		for( j = 0 ; j < height ; j++ ){
			cur = line;
			for( i = 0 ; i < width ; i++ ){
				c = fb.y[j*width+i] - 16;

				if (m_color == YUV420)
				{
					d = fb.u[(j>>1)*stride_uv+(i>>1)] - 128;
					e = fb.v[(j>>1)*stride_uv+(i>>1)] - 128;
				}
				else if (m_color == NV12)
				{
					d = fb.misc[(j>>1)*width+(i>>1<<1)  ] - 128;
					e = fb.misc[(j>>1)*width+(i>>1<<1)+1] - 128;
				}
				else // if (m_color == NV21)
				{
					d = fb.misc[(j>>1)*width+(i>>1<<1)+1] - 128;
					e = fb.misc[(j>>1)*width+(i>>1<<1)  ] - 128;
				}

				(*cur) = clip(( 298 * c           + 409 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);cur++;
				(*cur) = clip(( 298 * c + 516 * d           + 128) >> 8);cur+=2;
			}
			line += t_width<<2;
		}
	}    
	else if( m_color == YUV420_10LE || m_color == YUV420_10BE ){
		for( j = 0 ; j < height ; j++ ){
			cur = line;
			for( i = 0 ; i < width ; i++ ){

                if (m_color == YUV420_10BE)
                {
                    c = (fb.y[j*width*2 +  i*2] << 8) | fb.y[j*width*2 +  i*2 + 1];
                    d = (fb.u[(j>>1)*width+(i>>1<<1)  ] << 8) | fb.u[(j>>1)*width+(i>>1<<1)+1];
                    e = (fb.v[(j>>1)*width+(i>>1<<1)  ] << 8) | fb.v[(j>>1)*width+(i>>1<<1)+1];
                }
                else
                {
                    c = (fb.y[j*width*2 +  i*2 + 1] << 8)  | fb.y[j*width*2 +  i*2];
                    d = (fb.u[(j>>1)*width+(i>>1<<1)+1] << 8)  | fb.u[(j>>1)*width+(i>>1<<1)  ];
                    e = (fb.v[(j>>1)*width+(i>>1<<1)+1] << 8)  | fb.v[(j>>1)*width+(i>>1<<1)  ];
                }

				c = c - (16<<2);
				d = d - (128<<2);
				e = e - (128<<2);

				(*cur) = clip(( 298 * c           + 409 * e + (128<<2)) >> 10);cur++;
				(*cur) = clip(( 298 * c - 100 * d - 208 * e + (128<<2)) >> 10);cur++;
				(*cur) = clip(( 298 * c + 516 * d           + (128<<2)) >> 10);cur+=2;
			}
			line += t_width<<2;
		}
	}
	else if (m_color == P010 || m_color == P010MSB)
	{
		int shift = m_color == P010 ? 10 : 16;
		int shift2 = shift - 8;

		for (j = 0; j < height; j++) 
		{
			cur = line;
			for (i = 0; i < width; i++) 
			{
				c = ((unsigned short*)fb.y)[j*width + i];
				d = ((unsigned short*)fb.misc)[(j>>1)*width + (i & 0xFFFFFFFE)]; // for fb.u
				e = ((unsigned short*)fb.misc)[(j>>1)*width + (i & 0xFFFFFFFE) + 1]; // for fb.v

				c = c - (16 << shift2);
				d = d - (128 << shift2);
				e = e - (128 << shift2);

				(*cur) = clip((298 * c + 409 * e + (128 << 2)) >> shift); cur++;
				(*cur) = clip((298 * c - 100 * d - 208 * e + (128 << 2)) >> shift); cur++;
				(*cur) = clip((298 * c + 516 * d + (128 << 2)) >> shift); cur += 2;
			}
			line += t_width << 2;
		}
	}
	else if (m_color == P210 || m_color == P210MSB)
	{
		int shift = m_color == P210 ? 10 : 16;
		int shift2 = shift - 8;

		for (j = 0; j < height; j++) {
            cur = line;
            for (i = 0; i < width; i++) {

				c = ((unsigned short*)fb.y)[j*width + i];
				d = ((unsigned short*)fb.misc)[j*width + (i & 0xFFFFFFFE)]; // for fb.u
				e = ((unsigned short*)fb.misc)[j*width + (i & 0xFFFFFFFE) + 1]; // for fb.v

				c = c - (16 << shift2);
				d = d - (128 << shift2);
				e = e - (128 << shift2);

				(*cur) = clip((298 * c + 409 * e + (128 << 2)) >> shift); cur++;
                (*cur) = clip((298 * c - 100 * d - 208 * e + (128 << 2)) >> shift); cur++;
                (*cur) = clip((298 * c + 516 * d + (128 << 2)) >> shift); cur += 2;
            }
            line += t_width << 2;
        }
    }
	else if (m_color == Y210 || m_color == Y210MSB || m_color == Y216)
	{
		int shift = 0;
		switch (m_color)
		{
		case Y210: shift = 10;  break;
		case Y210MSB: shift = 16; break;
		case Y216: shift = 16;  break;
		}

		int shift2 = shift-8;

		for (j = 0; j < height; j++) {
			cur = line;
			for (i = 0; i < width; i++) {

				c = ((unsigned short*)fb.misc)[j*width*2 + i*2];
				d = ((unsigned short*)fb.misc)[j*width*2 + (i & 0xFFFFFFFE)*2+1]; // for fb.u
				e = ((unsigned short*)fb.misc)[j*width*2 + (i & 0xFFFFFFFE)*2 + 3]; // for fb.v

				c = c - (16 << shift2);
				d = d - (128 << shift2);
				e = e - (128 << shift2);

				(*cur) = clip((298 * c + 409 * e + (128 << 2)) >> shift); cur++;
				(*cur) = clip((298 * c - 100 * d - 208 * e + (128 << 2)) >> shift); cur++;
				(*cur) = clip((298 * c + 516 * d + (128 << 2)) >> shift); cur += 2;
			}
			line += t_width << 2;
		}
	}
	else if (m_color == Y410) 
	{
		for (j = 0; j < height; j++) 
		{
			cur = line;
			for (i = 0; i < width; i++) 
			{
				Y410Pack pack = ((Y410Pack*)fb.misc)[j*width + i];
				c = pack.Y;
				d = pack.U; // for fb.u
				e = pack.V; // for fb.v

				c = c - (16 << 2);
				d = d - (128 << 2);
				e = e - (128 << 2);

				(*cur) = clip((298 * c + 409 * e + (128 << 2)) >> 10); cur++;
				(*cur) = clip((298 * c - 100 * d - 208 * e + (128 << 2)) >> 10); cur++;
				(*cur) = clip((298 * c + 516 * d + (128 << 2)) >> 10); cur += 2;
			}
			line += t_width << 2;
		}
	}
	else if (m_color == RGB32 || m_color == RGB24 || m_color == RGB16 || m_color == RGBP) {
		for( j = 0 ; j < height ; j++ ){
			cur = line;
			for( i = 0 ; i < width ; i++ ){
				if (m_color == RGB32) {
					r = fb.misc[(j*width+i)*4  ];
					g = fb.misc[(j*width+i)*4+1];
					b = fb.misc[(j*width+i)*4+2];
				}
				else if (m_color == RGB24) {
					r = fb.misc[(j*width+i)*3  ];
					g = fb.misc[(j*width+i)*3+1];
					b = fb.misc[(j*width+i)*3+2];
				}
                else if (m_color == RGB16) {
                    rgb16 = (short*)fb.misc;

                    r = ((rgb16[j*width + i] >> 11) & 0x1F) << 3;
                    g = ((rgb16[j*width + i] >> 5) & 0x3F) << 2;
                    b = ((rgb16[j*width + i]) & 0x1F) << 3;
                }
				else {
                    r = fb.misc[(j*width + i) ];
                    g = fb.misc[(j*width + i) + width*height];
                    b = fb.misc[(j*width + i) + 2*width*height];
                }

				(*cur) = r; cur++;
				(*cur) = g; cur++;
				(*cur) = b; cur+=2;
			}
			line += t_width<<2;
		}	
	}

	else { // YYY
		for( j = 0 ; j < height ; j++ ){
			cur = line;
			for( i = 0 ; i < width ; i++ ){
				(*cur) = fb.y[j*width+i]; cur++;
				(*cur) = fb.y[j*width+i]; cur++;
				(*cur) = fb.y[j*width+i]; cur+=2;
			}
			line += t_width<<2;
		}	
	}
}

void CyuvplayerDlg::fforward(void)
{

	if( ++cur >= count )
		cur = count;

	LoadFrame();
}

void CyuvplayerDlg::rewind(void)
{
	if( --cur < 0 )
		cur = 0;

	LoadFrame();
}

void CyuvplayerDlg::OnBnClickedRewind()
{
	// TODO: Add your control notification handler code here
	rewind();
}

void CyuvplayerDlg::OnBnClickedPlay()
{
	// TODO: Add your control notification handler code here
	if( cur >= (count-1) )
		cur = 0; //timer stop
	
	if( started ){
		StopTimer();
		started = FALSE;
	}
	else {
		StartTimer(); // timer start
		started = TRUE;
	}
}

void CyuvplayerDlg::OnBnClickedStop()
{
	// TODO: Add your control notification handler code here
	StopTimer();
	started = FALSE;

	cur = 0;
	LoadFrame();
}

void CyuvplayerDlg::OnBnClickedFforward()
{
	// TODO: Add your control notification handler code here
	fforward();
}

void CyuvplayerDlg::StartTimer(void)
{
	KillTimer(1);	
	SetTimer(1, (int)(1000.0/30), NULL);
}

void CyuvplayerDlg::StopTimer(void)
{
	KillTimer(1);
}

void CyuvplayerDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: Add your message handler code here and/or call default
	if( cur >= (count-1) ){
		started = FALSE;
		KillTimer(1);
	}
	else 
		fforward();

	CDialog::OnTimer(nIDEvent);
}

void CyuvplayerDlg::OnFileExit()
{
	// TODO: Add your command handler code here
	OnOK();
}

BOOL CyuvplayerDlg::PreTranslateMessage(MSG* pMsg) 
{
	if(pMsg->message == WM_KEYDOWN){

		switch( pMsg->wParam ){
			case VK_LEFT:
				rewind();
				return TRUE;

			case VK_RIGHT:
				fforward();
				return TRUE;

			case VK_UP:
				if( ratio > 3.5 )
					return TRUE;
				else if( ratio > 1.5 )
					OnZoom(ID_ZOOM_41);
				else if( ratio > 0.9 )
                    OnZoom(ID_ZOOM_21);
                else if( ratio > 0.4 )
                    OnZoom(ID_ZOOM_11);
				else if( ratio > 0.4 )
					OnZoom(ID_ZOOM_11);
				else 
					OnZoom(ID_ZOOM_12);

				return TRUE;

			case VK_DOWN:
				if( ratio < 0.4 )
					return TRUE;
				else if( ratio < 0.6 )
					OnZoom(ID_ZOOM_14);
				else if( ratio < 1.1 )
                    OnZoom(ID_ZOOM_12);
                else if( ratio < 2.1 )
                    OnZoom(ID_ZOOM_11);
				else if( ratio < 2.1 )
					OnZoom(ID_ZOOM_11);
				else 
					OnZoom(ID_ZOOM_21);

				return TRUE;

			case VK_RETURN:
			case VK_ESCAPE:
				return TRUE;

			case VK_SPACE:
			case 'p':
			case 'P':
				OnBnClickedPlay();
				return TRUE;

			case 'o':
			case 'O':
				OnOpen();
				return TRUE;

			case 'h':
			case 'H':
				OnSizeChange(ID_SIZE_HD);
				return TRUE;

			case 's':
			case 'S':
				OnSizeChange(ID_SIZE_SD);
				return TRUE;

			case 'c':
			case 'C':
				//if( GetKeyState(VK_CONTROL) < 0 ){
				//	MessageBox(L"hehe");
				//}
				//else
					OnSizeChange(ID_SIZE_CIF);
				return TRUE;

			case 'q':
			case 'Q':
				OnSizeChange(ID_SIZE_QCIF);
				return TRUE;

			case 'fb.v':
			case 'V':
				OnSizeChange(ID_SIZE_VGA);
				return TRUE;

			case 'g':
			case 'G':
				OnFileGo();
				return TRUE;

			case 'x':
			case 'X':
				OnOK();

		}

	}

	return CDialog::PreTranslateMessage(pMsg);
}
void CyuvplayerDlg::OnDestroy()
{
	CDialog::OnDestroy();

	// TODO: Add your message handler code here
	delete customDlg;

	if( fb.y != NULL ) delete fb.y;
	if( fb.u != NULL ) delete fb.u;
	if( fb.v != NULL ) delete fb.v;
	
	if( fb.rgba != NULL ) delete fb.rgba;
	if( fb.misc != NULL ) delete fb.misc;

    if (fbSecondary.y != NULL) delete fbSecondary.y;
    if (fbSecondary.u != NULL) delete fbSecondary.u;
    if (fbSecondary.v != NULL) delete fbSecondary.v;

    if (fbSecondary.rgba != NULL) delete fbSecondary.rgba;
    if (fbSecondary.misc != NULL) delete fbSecondary.misc;

    if (fd > -1)
    {
        _close(fd);
        primaryFileSize = 0;
    }

    if (fdSecondary > -1)
    {
        _close(fdSecondary);
        fdSecondary = -1;
        isShowingDifference = false;
        menu->CheckMenuItem(ID_VIEW_SHOWDIFFERENCE, MF_UNCHECKED);
    }
}

bool CyuvplayerDlg::FileOpen( const wchar_t* path, int& fileDesc)
{
	int i, j;
	const wchar_t* file;
	wchar_t* end;

#ifdef SUPPORT_PCRE
	pcre16* regex;

	wchar_t* pattern;
	int pattern_len;

	int pcre_vector[REGEX_BUF_SIZE];

	const char* err;
	int err_offset;

	int w, h;
	int rc;
#endif

	StopTimer();
	
    if (fileDesc > -1)
    {
        _close(fileDesc);
        if (fileDesc == fd)
        {
            primaryFileSize = 0;
        }
    }

	_wsopen_s( &fileDesc, path, O_RDONLY|O_BINARY, _SH_DENYNO, 0);
    if (!fileDesc)
    {
        return false;
    }

    UpdateFilename(fileDesc==fd ? filename : filenameSecondary, path);

	started = FALSE;
	cur = 0;

	// get filename
	if ((file = wcsrchr(path, L'\\')) == NULL)
		file = path;
	else
		file = file++;
	
	// guess size
	wcstol(file, &end, 0);

#ifdef SUPPORT_PCRE
	regex = pcre16_compile((PCRE_SPTR16)L"_(1080p|720p|wvga|wqvga|vga|qcif|cif|[1-9][0-9]*[x][1-9][0-9]*)_", PCRE_UTF16, &err, &err_offset, NULL);

	if (regex)
	{
		rc = pcre16_exec(regex, NULL, (PCRE_SPTR16)file, wcslen(file), 0, 0, pcre_vector, REGEX_BUF_SIZE);

		if (rc > 1)
		{
			pattern = file + pcre_vector[2 * 1 + 0];
			pattern_len = pcre_vector[2 * 1 + 1] - pcre_vector[2 * 1 + 0];

			for (i = 0; size_info[i].string != NULL; i++)
			{
				if (wcsncmp(pattern, size_info[i].string, pattern_len) == 0)
				{
					// uncheck all size menu items
					for (j = ID_SIZE_START; j <= ID_SIZE_END; j++)
						menu->CheckMenuItem(j, MF_UNCHECKED);

					// update SELECTED size
					menu->CheckMenuItem(size_info[i].size_id, MF_CHECKED);

					// reallocate memory
					Resize(size_info[i].width, size_info[i].height);

					break;
				}
			}

			if (size_info[i].string == NULL)
			{
				swscanf(pattern, L"%dx%d", &w, &h);

				// uncheck all size menu items
				for (j = ID_SIZE_START; j <= ID_SIZE_END; j++)
					menu->CheckMenuItem(j, MF_UNCHECKED);

				// update SELECTED size
				menu->CheckMenuItem(ID_SIZE_CUSTOM, MF_CHECKED);

				// reallocate memory
				Resize(w, h);
			}
		}

		pcre16_free(regex);
	}
#else
	for (i = 0; size_info[i].string != NULL; i++)
	{
		if (wcsstr(file, size_info[i].string) != NULL)
		{
			// uncheck all size menu items
			for (j = ID_SIZE_START; j <= ID_SIZE_END; j++)
				menu->CheckMenuItem(j, MF_UNCHECKED);

			// update SELECTED size
			menu->CheckMenuItem(size_info[i].size_id, MF_CHECKED);

			// reallocate memory
			Resize(size_info[i].width, size_info[i].height);

			break;
		}
	}
#endif

	UpdateParameter();
	LoadFrame();
    return true;
}

void CyuvplayerDlg::OnDropFiles(HDROP hDropInfo)
{
	// TODO: Add your message handler code here and/or call defaultTCHAR szFileName[_MAX_PATH];
	wchar_t szFileName[1024];

    ::DragQueryFile(hDropInfo, 0, szFileName, 1024);

    if (fdSecondary>-1)
    {
        _close(fdSecondary);
        fdSecondary = -1;
        isShowingDifference = false;
        menu->CheckMenuItem(ID_VIEW_SHOWDIFFERENCE, MF_UNCHECKED);
    }
    FileOpen( szFileName, fd);

	CDialog::OnDropFiles(hDropInfo);
}


void CyuvplayerDlg::OnFileGo()
{
	// TODO: Add your command handler code here
	CGoDialog* GoDlg = new CGoDialog;

	GoDlg->frame_no = cur+1;
	GoDlg->DoModal();

	cur = GoDlg->frame_no-1;
	delete GoDlg;

	if( cur >= count )
		cur = count-1;
	else if( cur < 0 )
		cur = 0;

	LoadFrame();

}

void CyuvplayerDlg::OnContextMenu(CWnd* pWnd, CPoint point)
{
	// TODO: Add your message handler code here
	CMenu  ContextMenu;
	CMenu* PopUp;

	CRect  rect;

	ContextMenu.LoadMenu(IDR_CMENU);
	PopUp = ContextMenu.GetSubMenu(0);

	m_view.GetWindowRect(&rect);

	if( rect.PtInRect(point) )
		PopUp->TrackPopupMenu(TPM_LEFTALIGN| TPM_RIGHTBUTTON, point.x, point.y, this);

	else
		CDialog::OnContextMenu(pWnd,point);

}

void CyuvplayerDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// TODO: Add your message handler code here and/or call default

	cur = m_slider.GetPos();
	if( cur >= count )
		cur = count-1;

	LoadFrame();

	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CyuvplayerDlg::OnCmenuSaveLuminance()
{
	// TODO: Add your command handler code here
	wchar_t buf[MAX_PATH_LEN];
	int ofd;

	if( started )
		StopTimer();
	
	wsprintf( buf, L"%s_%d_y.raw", filename, cur );
	CFileDialog	dlg(
			TRUE, _T("RAW"), buf,
			OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT,
			_T("RAW Files (RAW)|*.RAW|")
			);

	if( IDOK != dlg.DoModal() )
		return;	
	
	CString cpath = dlg.GetPathName();
	wchar_t* path = cpath.GetBuffer(0);

	_wsopen_s( &ofd, path, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, _SH_DENYWR, _S_IREAD | _S_IWRITE );
	if( ofd < 0 ){

		wsprintf( buf, L"Can't Open %s for writing", path );
		MessageBox( buf );

	}
	else {

		_write( ofd, fb.y, frame_size_y );
		_close(ofd );

	}
	cpath.ReleaseBuffer();

	if( started )
		StartTimer();
	
}

void CyuvplayerDlg::OnCmenuSaveYuv( color_format type )
{
	// TODO: Add your command handler code here
	wchar_t buf[MAX_PATH_LEN];
	int ofd;

	if( started )
		StopTimer();
	
	if( type == YUV444 )
		wsprintf( buf, L"%s_%d_yuv444.yuv", filename, cur );
	else if( type == YUV422 )
		wsprintf( buf, L"%s_%d_yuv422.yuv", filename, cur );
	else
		wsprintf( buf, L"%s_%d_yuv420.yuv", filename, cur );

	CFileDialog	dlg(
			TRUE, _T("YUV"), buf,
			OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT,
			_T("YUV Files (YUV)|*.YUV|")
			);

	if( IDOK != dlg.DoModal() )
		return;	
	
	CString cpath = dlg.GetPathName();
	wchar_t* path = cpath.GetBuffer(0);

	_wsopen_s( &ofd, path, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, _SH_DENYWR, _S_IREAD | _S_IWRITE );
	if( ofd < 0 ){

		wsprintf( buf, L"Can't Open %s for writing", path );
		MessageBox( buf );

	}
	else {

		// if color format is not yuv444, make yuv444 from rgb
		if( m_color != type ){

			if( type == YUV444 )
				rgb2yuv444();
			else if( type == YUV422 )
				rgb2yuv422();
			else
				rgb2yuv420();

		}

		_write( ofd, fb.y, width*height );
		if( type == YUV444 ){
			_write( ofd, fb.u, width*height );
			_write( ofd, fb.v, width*height );
		}
		else if( type == YUV422 ){
			_write( ofd, fb.u, width*height/2 );
			_write( ofd, fb.v, width*height/2 );
		}
		else {
			_write( ofd, fb.u, width*height/4 );
			_write( ofd, fb.v, width*height/4 );
		}
		_close(ofd );

	}
	cpath.ReleaseBuffer();

	if( started )
		StartTimer();
	

}

void CyuvplayerDlg::rgb2yuv444(){

	int j, i;
	int idx;

	int r, g, b;
	unsigned char* pos;
	unsigned char* line;
	
	line = fb.rgba; idx = 0;
	for( j = 0 ; j < height ; j++ ){
		pos = line;
		for( i = 0 ; i < width ; i++ ){
			r = *pos; pos++;
			g = *pos; pos++;
			b = *pos; pos+=2;

			//fb.y[idx] = ((  66 * r + 129 * g +  25 * b + 128) >> 8) + 16;
			fb.u[idx] = (( -38 * r -  74 * g + 112 * b + 128) >> 8) + 128;
			fb.v[idx] = (( 112 * r -  94 * g -  18 * b + 128) >> 8) + 128;

			idx++;
		}
		line += t_width*4;
	}

}

void CyuvplayerDlg::rgb2yuv422(){

	int j, i;
	int idx;
	int sum[2];

	int r, g, b;
	unsigned char* pos;
	unsigned char* line;
	
	/*
	idx = 0; line = fb.rgba;
	for( j = 0 ; j < height ; j++ ){
		pos = line;
		for( i = 0 ; i < width ; i++ ){
			r = *pos; pos++;
			g = *pos; pos++;
			b = *pos; pos+=2;

			fb.y[idx] = ((  66 * r + 129 * g +  25 * b + 128) >> 8) + 16;
			idx++;
		}
		line += t_width*4;
	}
	*/

	idx = 0; line = fb.rgba;
	for( j = 0 ; j < height ; j++ ){
		pos = line;
		for( i = 0 ; i < width ; i+=2 ){

			r = *pos; pos++;
			g = *pos; pos++;
			b = *pos; pos+=2;

			sum[0] = (( -38 * r -  74 * g + 112 * b + 128) >> 8) + 128;
			sum[1] = (( 112 * r -  94 * g -  18 * b + 128) >> 8) + 128;

			r = *pos; pos++;
			g = *pos; pos++;
			b = *pos; pos+=2;

			sum[0] += (( -38 * r -  74 * g + 112 * b + 128) >> 8) + 128;
			sum[1] += (( 112 * r -  94 * g -  18 * b + 128) >> 8) + 128;

			fb.u[idx] = sum[0]/2;
			fb.v[idx] = sum[1]/2;

			idx++;

		}
		line += t_width*4;
	}

}

void CyuvplayerDlg::rgb2yuv420(){

	int j, i;
	int idx;
	int sum[2];

	int r[2], g[2], b[2];
	unsigned char* pos;
	unsigned char* line;
	/*
	idx = 0; line = fb.rgba;
	for( j = 0 ; j < height ; j++ ){
		pos = line;
		for( i = 0 ; i < width ; i++ ){
			r[0] = *pos; pos++;
			g[0] = *pos; pos++;
			b[0] = *pos; pos+=2;

			fb.y[idx] = ((  66 * r[0] + 129 * g[0] +  25 * b[0] + 128) >> 8) + 16;
			idx++;
		}
		line += t_width*4;
	}
	*/

	idx = 0; line = fb.rgba;
	for( j = 0 ; j < height ; j+=2 ){
		pos = line;
		for( i = 0 ; i < width ; i+=2 ){

			r[0] = *pos; r[1] = *(pos+t_width*4); pos++;
			g[0] = *pos; g[1] = *(pos+t_width*4); pos++;
			b[0] = *pos; b[1] = *(pos+t_width*4); pos+=2;

			sum[0]  = (( -38 * r[0] -  74 * g[0] + 112 * b[0] + 128) >> 8) + 128;
			sum[1]  = (( 112 * r[0] -  94 * g[0] -  18 * b[0] + 128) >> 8) + 128;

			sum[0] += (( -38 * r[1] -  74 * g[1] + 112 * b[1] + 128) >> 8) + 128;
			sum[1] += (( 112 * r[1] -  94 * g[1] -  18 * b[1] + 128) >> 8) + 128;

			r[0] = *pos; r[1] = *(pos+t_width*4); pos++;
			g[0] = *pos; g[1] = *(pos+t_width*4); pos++;
			b[0] = *pos; b[1] = *(pos+t_width*4); pos+=2;

			sum[0] += (( -38 * r[0] -  74 * g[0] + 112 * b[0] + 128) >> 8) + 128;
			sum[1] += (( 112 * r[0] -  94 * g[0] -  18 * b[0] + 128) >> 8) + 128;

			sum[0] += (( -38 * r[1] -  74 * g[1] + 112 * b[1] + 128) >> 8) + 128;
			sum[1] += (( 112 * r[1] -  94 * g[1] -  18 * b[1] + 128) >> 8) + 128;

			fb.u[idx] = sum[0]/4;
			fb.v[idx] = sum[1]/4;

			idx++;
		}
		line += 2*t_width*4;

	}

}

void CyuvplayerDlg::OnCmenuSaveYuv444()
{
	OnCmenuSaveYuv(YUV444);
}

void CyuvplayerDlg::OnCmenuSaveYuv422()
{
	OnCmenuSaveYuv(YUV422);
}

void CyuvplayerDlg::OnCmenuSaveYuv420()
{
	OnCmenuSaveYuv(YUV420);
}

void CyuvplayerDlg::OnCmenuSaveRgb()
{
	// TODO: Add your command handler code here
	wchar_t buf[MAX_PATH_LEN];
	int ofd;

	int i, j, k;

	unsigned char* tmp;

	if( started )
		StopTimer();
	
	wsprintf( buf, L"%s_%d_rgb.bmp", filename, cur );
	CFileDialog	dlg(
			TRUE, _T("BMP"), buf,
			OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT,
			_T("BMP Files (BMP)|*.BMP|")
			);

	if( IDOK != dlg.DoModal() )
		return;	
	
	CString cpath = dlg.GetPathName();
	wchar_t* path = cpath.GetBuffer(0);

	_wsopen_s( &ofd, path, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, _SH_DENYWR, _S_IREAD | _S_IWRITE );
	if( ofd < 0 ){
		wsprintf( buf, L"Can't Open %s for writing", path );
		MessageBox( buf );
	}
	else {
		BITMAPFILEHEADER fheader;
		BITMAPINFOHEADER header;

		memset( &fheader, 0, sizeof(BITMAPFILEHEADER) );
		fheader.bfType = 'B' | 'M'<<8;
		fheader.bfOffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
		fheader.bfSize = fheader.bfOffBits + width*height*4;

		memset( &header, 0, sizeof(BITMAPINFOHEADER) );
		header.biSize = sizeof(BITMAPINFOHEADER);

		header.biWidth = width;
		header.biHeight = height;
		header.biPlanes = 1;
		header.biBitCount = 32;

		tmp = new unsigned char[width*height*4];
		for( j = 0, k = height-1 ; j < height ; j++, k--){
			for( i = 0 ; i < width ; i++ ){
				tmp[(j*width+i)*4+2] = fb.rgba[(k*t_width+i)*4];
				tmp[(j*width+i)*4+1] = fb.rgba[(k*t_width+i)*4+1];
				tmp[(j*width+i)*4] = fb.rgba[(k*t_width+i)*4+2];
			}
		}

		_write( ofd, &fheader, sizeof(BITMAPFILEHEADER) );
		_write( ofd, &header, sizeof(BITMAPINFOHEADER) );
		_write( ofd, tmp, width*height*4 );

		_close(ofd );
		delete tmp;

	}
	cpath.ReleaseBuffer();

	if( started )
		StartTimer();
	
}

void CyuvplayerDlg::UpdateFilename(wchar_t* fileName, const wchar_t* path)
{
	size_t len, start, end;

	len = wcslen(path);
	for( start = len-1 ; start >= 0 && path[start] != '\\' ; start-- ); start++;
	for( end = start+1 ; end < len && path[end] != '.' ; end++ );

	wcsncpy( fileName, path+start, end-start );
	fileName[end-start] = 0;
}

void CyuvplayerDlg::DrawSegment(void)
{
	int i, j, k;

	// erase segment texture
	memset(segment, 0, t_width*t_height*4);

	if (segment_option & SEGMENT16x16)
	{
		for (j = 0 ; j < height ; j+= 16)
		{
			for (i = 0 ; i < width ; i+= 16)
			{
				for (k = 0 ; k < 16 ; k++)
				{
					if (j+15 < height && i+k < width)
					{
						// horizontal line
						segment[((j+15)*t_width + i+k )*4   ] = 0;
						segment[((j+15)*t_width + i+k )*4 +1] = 0;
						segment[((j+15)*t_width + i+k )*4 +2] = 255;
						segment[((j+15)*t_width + i+k )*4 +3] = 255;
					}
					
					if (j+k < height && i+15 < width)
					{
						// vertical line
						segment[((j+k )*t_width + i+15)*4   ] = 0;
						segment[((j+k )*t_width + i+15)*4 +1] = 0;
						segment[((j+k )*t_width + i+15)*4 +2] = 255;
						segment[((j+k )*t_width + i+15)*4 +3] = 255;
					}
				}
			}
		}
	}

	if (segment_option & SEGMENT32x32)
	{
		for (j = 0 ; j < height ; j+= 32)
		{
			for (i = 0 ; i < width ; i+= 32)
			{
				for (k = 0 ; k < 32 ; k++)
				{
					if (j+31 < height && i+k < width)
					{
						// horizontal line
						segment[((j+31)*t_width + i+k )*4   ] = 0;
						segment[((j+31)*t_width + i+k )*4 +1] = 255;
						segment[((j+31)*t_width + i+k )*4 +2] = 0;
						segment[((j+31)*t_width + i+k )*4 +3] = 255;
					}
					
					if (j+k < height && i+31 < width)
					{
						// vertical line
						segment[((j+k )*t_width + i+31)*4   ] = 0;
						segment[((j+k )*t_width + i+31)*4 +1] = 255;
						segment[((j+k )*t_width + i+31)*4 +2] = 0;
						segment[((j+k )*t_width + i+31)*4 +3] = 255;
					}
				}
			}
		}
	}

	if (segment_option & SEGMENT64x64)
	{
		for (j = 0 ; j < height ; j+= 64)
		{
			for (i = 0 ; i < width ; i+= 64)
			{
				for (k = 0 ; k < 64 ; k++)
				{
					if (j+63 < height && i+k < width)
					{
						// horizontal line
						segment[((j+63)*t_width + i+k )*4   ] = 255;
						segment[((j+63)*t_width + i+k )*4 +1] = 0;
						segment[((j+63)*t_width + i+k )*4 +2] = 0;
						segment[((j+63)*t_width + i+k )*4 +3] = 255;
					}
					
					if (j+k < height && i+63 < width)
					{
						// vertical line
						segment[((j+k )*t_width + i+63)*4   ] = 255;
						segment[((j+k )*t_width + i+63)*4 +1] = 0;
						segment[((j+k )*t_width + i+63)*4 +2] = 0;
						segment[((j+k )*t_width + i+63)*4 +3] = 255;
					}
				}
			}
		}
	}

	OpenGLView->LoadSegmentTexture(segment);
}


void CyuvplayerDlg::OnViewShowdifference()
{
    isShowingDifference = !isShowingDifference;
    menu->CheckMenuItem(ID_VIEW_SHOWDIFFERENCE, isShowingDifference ? MF_CHECKED : MF_UNCHECKED);
    LoadFrame();
}

void CyuvplayerDlg::CalcDifIfNeeded()
{
    if (isShowingDifference)
    {
        if (m_color == RGB32 || m_color == AYUV)
            calcAbsDiff(fb.misc, fbSecondary.misc, frame_size_y * 4);
        else if (m_color == Y210 || m_color == Y210MSB || m_color == Y216)
        {
            calcAbsDiff2((short*)fb.misc, (short*)fbSecondary.misc, frame_size_y * 2);
            for (int i = 1; i < frame_size_y * 2; i += 2)
            {
                ((unsigned short*)fb.misc)[i] += (m_color == Y210 ? 1 << 9 : 1 << 15);
            }
        }
        else if (m_color == Y410)
            calcAbsDiff410(fb.misc, fbSecondary.misc, frame_size_y);
        else if (m_color == RGB24 || m_color == RGBP)
            calcAbsDiff(fb.misc, fbSecondary.misc, frame_size_y * 3);

        else if (m_color == RGB16)
            calcAbsDiff(fb.misc, fbSecondary.misc, frame_size_y * 2);

        else if (m_color == UYVY)
            calcAbsDiff(fb.misc, fbSecondary.misc,frame_size_y * 2);

        else if (m_color == YUYV)
            calcAbsDiff(fb.misc, fbSecondary.misc, frame_size_y * 2);

        else if (m_color == NV12 || m_color == NV21)
        {
            calcAbsDiff(fb.y, fbSecondary.y, frame_size_y);
            calcAbsDiff(fb.misc, fbSecondary.misc, frame_size_y / 2,128);
        }
        else if (m_color == P010 || m_color == P010MSB || m_color == P210 || m_color == P210MSB)
        {
            calcAbsDiff2((short*)fb.y, (short*)fbSecondary.y, frame_size_y/2);
            calcAbsDiff2((short*)fb.misc, (short*)fbSecondary.misc, frame_size_uv/2, 128);
        }
        else if (m_color == PACKED_YUV444)
            calcAbsDiff(fb.misc, fbSecondary.misc, frame_size);
        else
        {
            calcAbsDiff(fb.y, fbSecondary.y, frame_size_y);
            calcAbsDiff(fb.u, fbSecondary.u, frame_size_uv, 128);
            calcAbsDiff(fb.v, fbSecondary.v, frame_size_uv, 128);
        }

    }
}

void CyuvplayerDlg::calcAbsDiff(unsigned char* buf1, unsigned char* buf2, int size, int offset)
{
    for (int i = 0; i < size; i++)
    {
        buf1[i] = abs(buf1[i] - buf2[i])+offset;
    }
}

void CyuvplayerDlg::calcAbsDiff2(short* buf1, short* buf2, int size, int offset)
{
    for (int i = 0; i < size; i++)
    {
        buf1[i] = abs(buf1[i] - buf2[i])+ offset;
    }
}

void CyuvplayerDlg::calcAbsDiff410(unsigned char*  buf1, unsigned char*  buf2, int size)
{
    for (int i = 0; i < size; i++)
    {
        Y410Pack res;
        res.A= abs( (int)((Y410Pack*)buf1)[i].A - (int)((Y410Pack*)buf2)[i].A);
        res.Y = abs((int)((Y410Pack*)buf1)[i].Y - (int)((Y410Pack*)buf2)[i].Y);
        res.U = abs((int)((Y410Pack*)buf1)[i].U - (int)((Y410Pack*)buf2)[i].U + 128);
        res.V = abs((int)((Y410Pack*)buf1)[i].V - (int)((Y410Pack*)buf2)[i].V+128);
        ((Y410Pack*)buf1)[i] = res;
    }
}

void CyuvplayerDlg::ReallocateMem(FrameBuffer& frameBuf)
{
    if (frameBuf.y != NULL) delete frameBuf.y;
    if (frameBuf.u != NULL) delete frameBuf.u;
    if (frameBuf.v != NULL) delete frameBuf.v;

    if (frameBuf.rgba != NULL) delete frameBuf.rgba;
    if (frameBuf.misc != NULL) delete frameBuf.misc;

    frameBuf.y = new unsigned char[width*height * 2];
    frameBuf.u = new unsigned char[width*height * 2];
    frameBuf.v = new unsigned char[width*height * 2];

    frameBuf.rgba = new unsigned char[t_width*t_height * 4];
    frameBuf.misc = new unsigned char[width*height * 4];

}