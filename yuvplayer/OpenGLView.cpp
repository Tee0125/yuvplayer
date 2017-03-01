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

// OpenGLView.cpp : implementation file
//

#include "stdafx.h"
#include "yuvplayer.h"
#include "OpenGLView.h"

#include <gl/gl.h>

// COpenGLView

IMPLEMENT_DYNCREATE(COpenGLView, CView)

COpenGLView::COpenGLView()
{
	loaded[0] = FALSE;
	loaded[1] = FALSE;

	t_width = 0;
	t_height = 0;

	ratio = 1.0;
}

COpenGLView::~COpenGLView()
{
}

BEGIN_MESSAGE_MAP(COpenGLView, CView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


// COpenGLView drawing

void COpenGLView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	HDC dc = ::GetDC(m_hWnd);

	int i;

	// TODO: add draw code here	
	glClear(GL_COLOR_BUFFER_BIT);		// clear screen and depth buffer

	for (i = 0 ; i < 2 ; i++)
	{
		if( loaded[i] ){
			glBindTexture( GL_TEXTURE_2D, texture[i]);
			glBegin(GL_QUADS);
				glTexCoord2f( 0.f, 0.f );
				glVertex3i( 0, 0, i);

				glTexCoord2f( 0.f, 1.f );
				glVertex3i( 0, t_height, i);

				glTexCoord2f( 1.f, 1.f );
				glVertex3i( t_width, t_height, i);

				glTexCoord2f( 1.f, 0.f );
				glVertex3i( t_width, 0, i);
			glEnd();
		}
	}

	SwapBuffers( dc );

	::ReleaseDC( m_hWnd, dc );

}


// COpenGLView diagnostics

#ifdef _DEBUG
void COpenGLView::AssertValid() const
{
	CView::AssertValid();
}

#ifndef _WIN32_WCE
void COpenGLView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif
#endif //_DEBUG


// COpenGLView message handlers
BOOL COpenGLView::PreCreateWindow(CREATESTRUCT& cs)

{

       // TODO: Modify the Window class or styles here by modifying
       cs.style |= WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CS_OWNDC; 

       return CView::PreCreateWindow(cs);

} 
int COpenGLView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  Add your specialized creation code here
	int nPixelFormat;					// our pixel format index

	static PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),	// size of structure
		1,								// default version
		PFD_DRAW_TO_WINDOW |			// window drawing support
		PFD_SUPPORT_OPENGL |			// OpenGL support
		PFD_DOUBLEBUFFER,				// double buffering support
		PFD_TYPE_RGBA,					// RGBA color mode
		32,								// 32 bit color mode
		0, 0, 0, 0, 0, 0,				// ignore color bits, non-palettized mode
		0,								// no alpha buffer
		0,								// ignore shift bit
		0,								// no accumulation buffer
		0, 0, 0, 0,						// ignore accumulation bits
		0,								// no z-buffer size
		0,								// no stencil buffer
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main drawing plane
		0,								// reserved
		0, 0, 0 };						// layer masks ignored

	HDC hdc = ::GetDC(m_hWnd);

	nPixelFormat = ChoosePixelFormat(hdc, &pfd);
	BOOL success = SetPixelFormat(hdc, nPixelFormat, &pfd);
	m_hRC = wglCreateContext(hdc);

	wglMakeCurrent(hdc, m_hRC);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 

	glGenTextures( 2, texture );
	glBindTexture(GL_TEXTURE_2D, texture[0] );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 

	glBindTexture(GL_TEXTURE_2D, texture[1] );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 

	glClearColor(0,0,0,0);

	return 0;
}

void COpenGLView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	// TODO: Add your message handler code here
	glViewport( 0, 0, cx, cy );
	
    glMatrixMode(GL_PROJECTION);	// set projection matrix current matrix
	glLoadIdentity();
	glOrtho( 0, cx, cy, 0, 0, 100);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslatef( 0, 0, -10.0f );
	glScalef( ratio, ratio, 1.f );

}

void COpenGLView::OnDestroy()
{
	CView::OnDestroy();

	// TODO: Add your message handler code here
	wglDeleteContext(m_hRC);
}

void COpenGLView::SetParam(int width, int height, float ratio)
{
	for( t_width = 2  ; t_width  < width  ; t_width  *= 2 );
	for( t_height = 2 ; t_height < height ; t_height *= 2 );

	this->ratio = ratio;
	
	loaded[0] = FALSE;
	loaded[1] = FALSE;
}

void COpenGLView::LoadTexture(unsigned char* rgba)
{

	glBindTexture(GL_TEXTURE_2D, texture[0] );
	if( loaded[0] )
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, t_width, t_height, GL_RGBA, GL_UNSIGNED_BYTE, rgba );
	else {
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, t_width, t_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba );
		loaded[0] = TRUE;
	}
	Invalidate(NULL);
}

void COpenGLView::LoadSegmentTexture(unsigned char* segment)
{

	glBindTexture(GL_TEXTURE_2D, texture[1] );
	if( loaded[1] )
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, t_width, t_height, GL_RGBA, GL_UNSIGNED_BYTE, segment );
	else{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, t_width, t_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, segment );
		loaded[1] = TRUE;
	}
	Invalidate(NULL);
}

BOOL COpenGLView::OnEraseBkgnd(CDC* pDC)
{
	// TODO: Add your message handler code here and/or call default

	return TRUE;
	//return CView::OnEraseBkgnd(pDC);
}
