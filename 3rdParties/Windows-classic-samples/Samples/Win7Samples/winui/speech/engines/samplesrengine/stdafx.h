// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright � Microsoft Corporation. All rights reserved

// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently

#if !defined(AFX_STDAFX_H__41B89B6F_9399_11D2_9623_00C04F8EE628__INCLUDED_)
#define AFX_STDAFX_H__41B89B6F_9399_11D2_9623_00C04F8EE628__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef STRICT
#define STRICT
#endif

#include <atlbase.h>
//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module

extern CComModule _Module;
#include <atlcom.h>

#include <spddkhlp.h>

// Platform SDK needs to be installed for this sample because it #includes shlobj.h
#ifndef MAXULONG_PTR
#error This sample application requires a newer version of the Platform SDK than you have installed.
#endif // MAXULONG_PTR


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__41B89B6F_9399_11D2_9623_00C04F8EE628__INCLUDED)
