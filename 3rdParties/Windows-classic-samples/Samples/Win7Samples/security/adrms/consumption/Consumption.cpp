/*=====================================================================
File:      Consumption.cpp

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (C) Microsoft Corporation.  All rights reserved.
=====================================================================*/

//
// This sample demonstrates consumption.  It acquires the appropriate
// licenses and certificates, initializes the environment, signs the 
// publishing license offline, encrypts content, and then decrypts it.  
// See the comments at the beginning of wmain() for a more detailed
// description.
//

#include <stdio.h>
#include <Wtypes.h>
#include <strsafe.h>
#include <objbase.h>

#include "msdrm.h"
#include "msdrmdefs.h"
#include "msdrmerror.h"
#include "msdrmgetinfo.h"

//
// Values to put into the unsigned issuance license
// Note: the content ID should be unique and is generated
//       as a GUID in the code
//
#define APP_SPECIFIC_DATA_NAME L"AppSpecificName"
#define APP_SPECIFIC_DATA_VALUE L"AppSpecificValue"
#define INTERVAL_TIME_DAYS 30
#define SKU_ID L"SKUId"
#define SKU_ID_TYPE L"SKUIdType"
#define CONTENT_TYPE L"ContentType"
#define CONTENT_NAME L"ContentName"
#define NAME_AND_DESCRIPTION_NAME L"Name"
#define NAME_AND_DESCRIPTION_DESCRIPTION L"Description"
#define USAGE_POLICY_APPLICATION_NAME L"ApplicationName"
#define USAGE_POLICY_MIN_VERSION L"1.0.0.0"
#define USAGE_POLICY_MAX_VERSION L"3.0.0.0"
#define RIGHT_NAME L"EDIT"

//
// Content to encrypt / decrypt
// 
#define PLAINTEXT L"The plaintext to be encrypted."

//
// Struct to hold the callback information
//
typedef struct Drm_Context
{
	HANDLE  hEvent;
	HRESULT hr;
	PWSTR   wszData;
} DRM_CONTEXT, *PDRM_CONTEXT;

//
// Time to wait for "downloads" to complete
//
static const DWORD DW_WAIT_TIME_SECONDS = 60 * 1000;  

//
// Length of a GUID string
//
static const UINT GUID_STRING_LENGTH = 128;

//
// Print the correct usage of this application
//
void 
PrintUsage()
{
	wprintf( L"Usage:\n" );
	wprintf( L"\n  Consumption -U UserID -M ManifestFile "\
		L"[-A ActivationSvr] [-L LicensingSvr]\n" );
	wprintf( L"    -U: specifies the UserID.\n" );
	wprintf( L"        example: user@yourdomain.com\n" );
	wprintf( L"    -M: specifies the manifest file to use.\n" );
	wprintf( L"        example: manifest.xml\n" );
	wprintf( L"    -A: specifies the Activation Server. (optional)\n" );
	wprintf( L"        example: http://localhost/_wmcs/certification\n" );
	wprintf( L"    -L: specifies the Licensing Server. (optional)\n" );
	wprintf( L"        example: http://localhost/_wmcs/licensing\n" );
}

//
// Parse the values passed in through the command line
//
HRESULT 
ParseCommandLine( 
				 int argc, 
				 __in_ecount( argc )WCHAR **argv, 
				 __deref_out_opt PWCHAR *pwszUserID,
				 __deref_out PWCHAR *pwszActivationSvr,
				 __deref_out PWCHAR *pwszLicensingSvr,
				 __deref_out_opt PWCHAR *pwszManifestFile
				 )
{
	HRESULT hr                   = S_OK;
	size_t  uiUserIDLength       = 0;
	size_t  uiActSvrUrlLength    = 0;
	size_t  uiLicSvrUrlLength    = 0;
	size_t  uiManifestFileLength = 0;

	//
	// Initialize parameters
	//
	*pwszUserID = NULL;
	*pwszManifestFile = NULL;
	*pwszActivationSvr = NULL;
	*pwszLicensingSvr = NULL;
	//
	// Check input parameters.
	//
	if ( ( 5 != argc ) && ( 7 != argc ) && ( 9 != argc ) )
	{
		return E_INVALIDARG;
	}

	for( int i = 1; SUCCEEDED( hr ) && i < argc - 1; i ++ )
	{
		if ( ( '-' != argv[ i ][ 0 ] ) && ( '/' != argv[ i ][ 0 ] ) )
		{
			hr = E_INVALIDARG;
			break;
		}
		else if ( ( '-' == argv[ i + 1 ][ 0 ] ) || 
			( '/' == argv[ i + 1 ][ 0 ] ) )
		{
			hr = E_INVALIDARG;
			break;
		}

		switch( toupper( argv[ i ][ 1 ] ) )
		{
			//
			// User ID
			//
		case 'U': 
			if ( wcsstr( argv[ i + 1 ], ( wchar_t* )L"@\0" ) == NULL )
			{
				//
				// An invalid User ID was provided
				//
				hr = E_INVALIDARG;
				break;
			}
			//
			// Retrieve the length of the user ID
			//
			hr = StringCchLengthW( argv[ i + 1 ], 
				STRSAFE_MAX_CCH, 
				&uiUserIDLength 
				);
			if ( FAILED( hr ) )
			{
				wprintf( L"\nStringCchLengthW failed.  hr = 0x%x\n", hr );
				break;
			}
			//
			// Allocate memory for the user ID
			//
			*pwszUserID = new WCHAR[ uiUserIDLength + 1 ];
			if ( NULL == *pwszUserID ) 
			{
				wprintf( L"\nFailed to allocate memory for pwszUserID.\n" );
				hr = E_OUTOFMEMORY;
				break;
			}
			//
			// Copy the user ID into the pwszUserID buffer
			//
			hr = StringCchCopyW( ( wchar_t* )*pwszUserID, 
				uiUserIDLength + 1 , 
				argv[ i + 1 ] 
			);
			if ( FAILED( hr ) )
			{
				wprintf( L"\nStringCchCopyW failed.  hr = 0x%x\n", hr );
				break;
			}
			i++;
			break;
		case 'A':
			if ( ( _wcsnicmp( argv[ i + 1 ], L"http://", 7 ) != 0 ) && 
				( _wcsnicmp( argv[ i + 1 ], L"https://", 8 ) != 0 ) )
			{
				wprintf( L"\nInvalid activation URL provided.\n" );
				hr = E_INVALIDARG;
				break;
			}
			//
			// Retrieve the length of the activation server URL
			//
			hr = StringCchLengthW( argv[ i + 1 ], 
				STRSAFE_MAX_CCH, 
				&uiActSvrUrlLength 
				);
			if ( FAILED( hr ) )
			{
				wprintf( L"\nStringCchLengthW failed.  hr = 0x%x\n", hr );
				break;
			}
			//
			// Allocate memory for the URL
			//
			*pwszActivationSvr = new WCHAR[ uiActSvrUrlLength + 1 ];
			if( NULL == *pwszActivationSvr ) 
			{
				wprintf( L"\nFailed to allocate memory "\
					L"for pwszActivationSvr.\n" );
				hr = E_OUTOFMEMORY;
				break;
			}
			//
			// Copy the URL into the pwszActivationSvr buffer
			//
			hr = StringCchCopyW( ( wchar_t* )*pwszActivationSvr, 
				uiActSvrUrlLength + 1 , 
				argv[ i + 1 ] 
			);
			if ( FAILED( hr ) )
			{
				wprintf( L"\nStringCchCopyW failed.  hr = 0x%x\n", hr );
				break;
			}
			i++;
			break;
		case 'L':
			if ( ( _wcsnicmp( argv[ i + 1 ], L"http://", 7 ) != 0 ) && 
				( _wcsnicmp( argv[ i + 1 ], L"https://", 8 ) != 0 ) )
			{
				wprintf( L"\nInvalid licensing URL provided.\n" );
				hr = E_INVALIDARG;
				break;
			}
			//
			// Retrieve the length of the licensing server URL
			//
			hr = StringCchLengthW( argv[ i + 1 ], 
				STRSAFE_MAX_CCH, 
				&uiLicSvrUrlLength 
				);
			if ( FAILED( hr ) )
			{
				wprintf( L"\nStringCchLengthW failed.  hr = 0x%x\n", hr );
				break;
			}
			//
			// Allocate memory for the URL
			//
			*pwszLicensingSvr = new WCHAR[ uiLicSvrUrlLength + 1 ];
			if( NULL == *pwszLicensingSvr ) 
			{
				wprintf( L"\nFailed to allocate memory "\
					L"for pwszLicensingSvr.\n" );
				hr = E_OUTOFMEMORY;
				break;
			}
			//
			// Copy the URL into the pwszLicensingSvr buffer
			//
			hr = StringCchCopyW( ( wchar_t* )*pwszLicensingSvr, 
				uiLicSvrUrlLength + 1 , 
				argv[ i + 1 ] 
			);
			if ( FAILED( hr ) )
			{
				wprintf( L"\nStringCchCopyW failed.  hr = 0x%x\n", hr );
				break;
			}
			i++;
			break;
		case 'M':
			if ( ( wcsstr( argv[ i + 1 ], L".xml" ) == NULL ) && 
				( wcsstr( argv[ i + 1 ], L".XML" ) == NULL ) )
			{
				wprintf( L"\nInvalid manifest file name provided.\n" );
				hr = E_INVALIDARG;
				break;
			}
			//
			// Retrieve the length of the manifest name
			//
			hr = StringCchLengthW( argv[ i + 1 ], 
				STRSAFE_MAX_CCH, 
				&uiManifestFileLength 
				);
			if ( FAILED( hr ) )
			{
				wprintf( L"\nStringCchLengthW failed.  hr = 0x%x\n", hr );
				break;
			}
			//
			// Allocate memory for the manifest file name
			//
			*pwszManifestFile = new WCHAR[ uiManifestFileLength + 1 ];
			if( NULL == *pwszManifestFile ) 
			{
				wprintf( L"\nFailed to allocate memory "\
					L"for pwszManifestFile.\n" );
				hr = E_OUTOFMEMORY;
				break;
			}
			//
			// Copy the manifest name into the pwszManifestFile buffer
			//
			hr = StringCchCopyW( ( wchar_t* )*pwszManifestFile, 
				uiManifestFileLength + 1 , 
				argv[ i + 1 ] 
			);
			if ( FAILED( hr ) )
			{
				wprintf( L"\nStringCchCopyW failed.  hr = 0x%x\n", hr );
				break;
			}
			i++;
			break;
		default:
			hr = E_INVALIDARG;
			break;
		}
	}
	if ( NULL == *pwszUserID )
	{
		wprintf( L"\nA user ID value is required.\n" );
		hr = E_INVALIDARG;
	}
	if ( NULL == *pwszManifestFile )
	{
		wprintf( L"\nA manifest file is required.\n" );
		hr = E_INVALIDARG;
	}
	return hr;
}

//
// Copy the contents of the file into a string buffer
//
HRESULT
ReadFileToWideString(
					 __in PWSTR szFileName,
					 __deref_out PWCHAR *pwszManifestString
					 )
{
	HRESULT                    hr            = S_OK;
	HANDLE                     hFile         = INVALID_HANDLE_VALUE;
	DWORD                      dwBytesRead   = 0;
	BY_HANDLE_FILE_INFORMATION fileInfo;

	//
	// Validate the parameters
	//
	if ( ( NULL == szFileName ) ||
		( FAILED( StringCchLengthW( szFileName, MAX_PATH, NULL ) ) ) ||
		( NULL == pwszManifestString ) )
	{
		hr = E_INVALIDARG;
		goto e_Exit;
	}

	//
	// Initialize the parameters
	//
	*pwszManifestString = NULL;

	//
	// Get a handle to the file
	//
	hFile = CreateFileW( szFileName, 
		GENERIC_READ, 
		0, 
		NULL, 
		OPEN_EXISTING, 
		0, 
		NULL 
		);
	if ( INVALID_HANDLE_VALUE == hFile )
	{
		wprintf( L"\nCreateFileW failed. hFile == INVALID_HANDLE_VALUE\n" );
		hr = HRESULT_FROM_WIN32( GetLastError() ) ;
		goto e_Exit;
	}

	if ( 0 == GetFileInformationByHandle( hFile, &fileInfo ) )
	{
		wprintf( L"\nGetFileInformationByHandle failed.\n" );
		hr = HRESULT_FROM_WIN32( GetLastError() );
		goto e_Exit;
	}

	//
	// Check for files that are too long and unsigned integer overflow
	//
	if ( ( 0 != fileInfo.nFileSizeHigh ) || 
		( fileInfo.nFileSizeLow + sizeof( WCHAR ) < fileInfo.nFileSizeLow ) )
	{
		wprintf(L"\nFile too long.\n");
		hr = E_FAIL;
		goto e_Exit;
	}

	*pwszManifestString = new WCHAR[ fileInfo.nFileSizeLow + sizeof( WCHAR ) ];
	if ( NULL == *pwszManifestString )
	{
		wprintf( L"\nMemory allocation for pwszManifestString failed.\n" );
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}

	//
	// Put the contents of the file into pwszManifestString
	//
	if ( 0 == ReadFile( hFile, 
		*pwszManifestString, 
		fileInfo.nFileSizeLow, 
		&dwBytesRead, 
		0 
		) )
	{
		hr = HRESULT_FROM_WIN32( GetLastError() );
		goto e_Exit;
	}

e_Exit:
	if ( INVALID_HANDLE_VALUE != hFile )
	{
		CloseHandle( hFile );
	}
	return hr;
}

//
// Callback function for asynchronous ADRMS client functions
//
HRESULT __stdcall 
StatusCallback( 
			   DRM_STATUS_MSG msg, 
			   HRESULT hr, 
			   void *pvParam, 
			   void *pvContext 
			   )
{
	size_t       uiSignedILLength = 0;
	PDRM_CONTEXT pContext         = ( PDRM_CONTEXT )pvContext;

	if ( pContext )
	{
		pContext->hr = hr;
	}
	else
	{
		return E_FAIL;
	}

	if ( FAILED( pContext->hr ) && pContext->hEvent )
	{
		//
		// Signal the event
		//
		SetEvent( ( HANDLE )pContext->hEvent );
		return S_OK;
	}

	//
	// Print the callback status message
	//
	switch( msg )
	{
	case DRM_MSG_ACTIVATE_MACHINE:
		wprintf( L"\nCallback status msg = DRM_MSG_ACTIVATE_MACHINE " );
		break;
	case DRM_MSG_ACTIVATE_GROUPIDENTITY:
		wprintf( L"\nCallback status msg = DRM_MSG_ACTIVATE_GROUPIDENTITY " );
		break;
	case DRM_MSG_ACQUIRE_CLIENTLICENSOR:
		wprintf( L"\nCallback status msg = DRM_MSG_ACQUIRE_CLIENTLICENSOR " );
		break;
	case DRM_MSG_SIGN_ISSUANCE_LICENSE:
		wprintf( L"\nCallback status msg = DRM_MSG_SIGN_ISSUANCE_LICENSE " );
		break;
	case DRM_MSG_ACQUIRE_LICENSE:
		wprintf( L"\nCallback status msg = DRM_MSG_ACQUIRE_LICENSE " );
		break;
	default:
		wprintf( L"\nDefault callback status msg = 0x%x ", msg );
		break;
	}

	//
	// Print the callback error code
	//
	switch( pContext->hr )
	{
	case S_DRM_ALREADY_ACTIVATED:
		wprintf( L"Callback hr = S_DRM_ALREADY_ACTIVATED\n" );
		break;
	case S_DRM_CONNECTING:
		wprintf( L"Callback hr = S_DRM_CONNECTING\n" );
		break;
	case S_DRM_CONNECTED:
		wprintf( L"Callback hr = S_DRM_CONNECTED\n" );
		break;
	case S_DRM_INPROGRESS:
		wprintf( L"Callback hr = S_DRM_INPROGRESS\n" );
		break;
	case S_DRM_COMPLETED:
		wprintf( L"Callback hr = S_DRM_COMPLETED\n" );
		pContext->hr = S_OK;
		if ( ( msg == DRM_MSG_SIGN_ISSUANCE_LICENSE ) && pvParam )
		{
			if ( NULL != pvParam )
			{
				//
				// Retrieve the length of the signed issuance license
				//
				hr = StringCchLengthW( ( PWSTR )pvParam, 
					STRSAFE_MAX_CCH, 
					&uiSignedILLength
					);
				if ( FAILED( hr ) )
				{
					wprintf( L"\nStringCchLengthW failed.  hr = 0x%x\n", hr );
					break;
				}
				//
				// Allocate memory for the signed issuance license
				//
				pContext->wszData = new WCHAR[ uiSignedILLength + 1 ];
				if( NULL == pContext->wszData ) 
				{
					wprintf( L"\nFailed to allocate memory "\
						L"for the signed issuance license.\n" );
					hr = E_OUTOFMEMORY;
					break;
				}
				//
				// Copy the signed issuance license into the 
				// pContext->wszData buffer
				//
				hr = StringCchCopyW( ( wchar_t* )pContext->wszData, 
					uiSignedILLength + 1 , 
					( PWSTR )pvParam 
					);
				if ( FAILED( hr ) )
				{
					wprintf( L"\nStringCchCopyW failed.  hr = 0x%x\n", hr );
					break;
				}
			}
		}
		if ( pContext->hEvent )
		{
			SetEvent( ( HANDLE )pContext->hEvent );
		}
		break;
	case E_DRM_ACTIVATIONFAILED:
		wprintf( L"Callback hr = E_DRM_ACTIVATIONFAILED\n" );
		break;
	case E_DRM_HID_CORRUPTED:
		wprintf( L"Callback hr = E_DRM_HID_CORRUPTED\n" );
		break;
	case E_DRM_INSTALLATION_FAILED:
		wprintf( L"Callback hr = E_DRM_INSTALLATION_FAILED\n" );
		break;
	case E_DRM_ALREADY_IN_PROGRESS:
		wprintf( L"Callback hr = E_DRM_ALREADY_IN_PROGRESS\n" );
		break;
	case E_DRM_NO_CONNECT:
		wprintf( L"Callback hr = E_DRM_NO_CONNECT\n" );
		break;
	case E_DRM_ABORTED:
		wprintf( L"Callback hr = E_DRM_ABORTED\n" );
		break;
	case E_DRM_SERVER_ERROR:
		wprintf( L"Callback hr = E_DRM_SERVER_ERROR\n" );
		break;
	case E_DRM_INVALID_SERVER_RESPONSE:
		wprintf( L"Callback hr = E_DRM_INVALID_SERVER_RESPONSE\n" );
		break;
	case E_DRM_SERVER_NOT_FOUND:
		wprintf( L"Callback hr = E_DRM_SERVER_NOT_FOUND\n" );
		break;
	case E_DRM_AUTHENTICATION_FAILED:
		wprintf( L"Callback hr = E_DRM_AUTHENTICATION_FAILED\n" );
		break;
	case E_DRM_EMAIL_NOT_VERIFIED:
		wprintf( L"Callback hr = E_DRM_EMAIL_NOT_VERIFIED\n" );
		break;
	case E_DRM_AD_ENTRY_NOT_FOUND:
		wprintf( L"Callback hr = E_DRM_AD_ENTRY_NOT_FOUND\n" );
		break;
	case E_DRM_NEEDS_MACHINE_ACTIVATION:
		wprintf( L"Callback hr = E_DRM_NEEDS_MACHINE_ACTIVATION\n" );
		break;
	case E_DRM_NEEDS_GROUPIDENTITY_ACTIVATION:
		wprintf( L"Callback hr = E_DRM_NEEDS_GROUPIDENTITY_ACTIVATION\n" );
		break;
	case E_DRM_REQUEST_DENIED:
		wprintf( L"Callback hr = E_DRM_REQUEST_DENIED\n" );
		break;
	case E_DRM_INVALID_EMAIL:
		wprintf( L"Callback hr = E_DRM_INVALID_EMAIL\n" );
		break;
	case E_DRM_SERVICE_GONE:
		wprintf( L"Callback hr = E_DRM_SERVICE_GONE\n" );
		break;
	case E_DRM_SERVICE_MOVED:
		wprintf( L"Callback hr = E_DRM_SERVICE_MOVED\n" );
		break;
	case E_DRM_VALIDITYTIME_VIOLATION:
		wprintf( L"Callback hr = E_DRM_VALIDITYTIME_VIOLATION\n" );
		break;
	case S_DRM_REQUEST_PREPARED:
		wprintf( L"Callback hr = S_DRM_REQUEST_PREPARED\n" );
		break;
	case E_DRM_NO_LICENSE:
		wprintf( L"Callback hr = E_DRM_NO_LICENSE\n" );
		break;
	case E_DRM_SERVICE_NOT_FOUND:
		wprintf( L"Callback hr = E_DRM_SERVICE_NOT_FOUND\n" );
		break;
	case E_DRM_LICENSEACQUISITIONFAILED:
		wprintf( L"Callback hr = E_DRM_LICENSEACQUISITIONFAILED\n" );
		break;
	default:
		wprintf( L"Default callback hr = 0x%x\n", hr );
		if ( pContext->hEvent )
		{
			SetEvent( ( HANDLE )pContext->hEvent );
		}
		break;
	}
	return S_OK;
}

//
// This function performs the following actions:
//    1.  Validate the parameters
//    2.  Create an event for the callback function
//    3.  Populate the wszURL member of the DRM_ACTSERV_INFO struct
//        with the activation server URL.  This value can be from:
//        (a) a command line argument
//        (b) service discovery
//    4.  Activate the machine
//    5. Wait for the callback to return
//    6. Clean up and free memory
HRESULT __stdcall 
DoMachineActivation(
					DRMHSESSION hClient,
					__in PWCHAR wszActivationSvr
					)
{
	HRESULT           hr               = E_FAIL;
	DRM_ACTSERV_INFO *pdasi            = NULL;
	UINT              uiStrLen         = 0;
	size_t            cchActivationSvr = 0;
	DWORD             dwWaitResult;
	DRM_CONTEXT       context;

	context.hEvent = NULL;

	//
	// 1. Validate the parameters
	//
	if ( NULL == hClient )
	{
		wprintf( L"\nThe client session was NULL.\n" );
		hr = E_INVALIDARG;
		goto e_Exit;
	}

	//
	// 2. Create an event for the callback function
	//
	if ( NULL == ( context.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL ) ) )
	{
		wprintf( L"\ncontext.hEvent was NULL after the CreateEvent call\n" );
		goto e_Exit;
	}

	//
	// Allocate memory for the DRM_ACTSERV_INFO structure for activation
	//
	pdasi = new DRM_ACTSERV_INFO;
	if ( NULL == pdasi )
	{
		wprintf( L"\nMemory allocation failed for DRM_ACTSERV_INFO.\n" );
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}
	pdasi->wszURL = NULL;

	//
	// 3. Copy the activation server URL into the wszURL member
	//    of the DRM_ACTSERV_INFO struct 
	//
	if ( NULL != wszActivationSvr )
	{
		//
		// 3(a). Use the URL provided through the command line argument
		//
		hr = StringCchLengthW( wszActivationSvr, 
			STRSAFE_MAX_CCH, 
			&cchActivationSvr 
			);
		if ( FAILED( hr ) )
		{
			wprintf( L"\nStringCchLengthW failed. hr = 0x%x.\n", hr );
			goto e_Exit;
		}

		//
		// Allocate memory for the URL member of 
		// the DRM_ACTSERV_INFO structure
		//
		pdasi->wszURL = new WCHAR[ cchActivationSvr + 1 ];
		if ( NULL == pdasi->wszURL )
		{
			wprintf( L"\nMemory allocation failed for the "\
				L"activation URL string.\n" );
			hr = E_OUTOFMEMORY;
			goto e_Exit;
		}
		hr = StringCchCopyW( pdasi->wszURL, 
			cchActivationSvr + 1, 
			wszActivationSvr 
			);
		if ( FAILED( hr ) )
		{
			wprintf( L"\nStringCchCopyW failed. hr = 0x%x.\n", hr );
			goto e_Exit;
		}
		wprintf( L"\nMachine Activation server URL:\n%s\n", pdasi->wszURL );
	}
	else
	{
		//
		// 3(b). Try to find the service location where activation 
		//       is available.
		//       The first call is to get -
		//       (a) whether there is a service location and 
		//       (b) if so, the size needed for a buffer to 
		//           hold the URL for the service.
		//
		hr = DRMGetServiceLocation( hClient,
			DRM_SERVICE_TYPE_ACTIVATION,
			DRM_SERVICE_LOCATION_ENTERPRISE,
			NULL,
			&uiStrLen,
			NULL 
			);

		if ( SUCCEEDED( hr ) )
		{
			//
			// There is a service location; reserve space
			// for the URL.
			//
			pdasi->wszURL = new WCHAR[ uiStrLen ];
			if ( NULL == pdasi->wszURL )
			{
				wprintf( L"\nMemory allocation failed for activation "\
					L"URL string.\n" );
				hr = E_OUTOFMEMORY;
				goto e_Exit;
			}

			//
			// Call second time to get the actual service location
			// copied into the URL.
			//
			hr = DRMGetServiceLocation( hClient,
				DRM_SERVICE_TYPE_ACTIVATION,
				DRM_SERVICE_LOCATION_ENTERPRISE,
				NULL,
				&uiStrLen,
				pdasi->wszURL
				);

			if ( FAILED( hr ) )
			{
				wprintf( L"\nDRMGetServiceLocation (ENTERPRISE) failed. "\
					L"hr = 0x%x\n", hr );
				goto e_Exit;
			}
			wprintf( L"\nDRMGetServiceLocation (ENTERPRISE) succeeded.\n\n"\
				L"Machine Activation server URL:\n%s\n", pdasi->wszURL );
		}
		else
		{
			wprintf( L"\nDRMGetServiceLocation failed. hr = 0x%x. "\
				L"\nPassing NULL server info to DRMActivate.\n", hr );
		}
	}

	//
	// 4. Activate the machine
	//
	hr = DRMActivate( hClient,
		DRM_ACTIVATE_MACHINE | DRM_ACTIVATE_SILENT,
		0,
		pdasi,
		( VOID* )&context,
		NULL 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMActivate (DRM_ACTIVATE_MACHINE) "\
			L"failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// 5. Wait for the callback to return
	//
	dwWaitResult = WaitForSingleObject( context.hEvent, DW_WAIT_TIME_SECONDS );
	if ( WAIT_TIMEOUT == dwWaitResult )
	{
		wprintf( L"\nWaitForSingleObject timed out.\n" );
		goto e_Exit;
	}
	if ( FAILED( context.hr ) )
	{
		//
		// In case a failure was reported via the callback function
		// note it also.
		//
		wprintf( L"\nThe callback function returned a failure "\
			L"code. hr = 0x%x\n", context.hr );
		hr = context.hr;
		goto e_Exit;
	}

	wprintf( L"\nThe machine is now activated.\n" );

e_Exit:

	//
	// 6. Clean up and free memory
	//
	if ( NULL != pdasi )
	{
		if ( NULL != pdasi->wszURL )
		{
			delete [] pdasi->wszURL;
		}
		delete pdasi;
	}
	if ( NULL != context.hEvent )
	{
		CloseHandle( context.hEvent );
	}
	return hr;
}

//
// This function performs the following actions:
//    1. Validate the parameters
//    2. Create an event for the callback function
//    3. Populate the wszURL member of the DRM_ACTSERV_INFO struct with
//       the user activation server URL.  This value can be from:
//       (a) a command line argument
//       (b) service discovery
//    4. Activate the user
//    5. Wait for the callback to return
//    6. Clean up and free memory
HRESULT __stdcall 
DoUserActivation(
				 DRMHSESSION hClient,
				 __in PWCHAR wszActivationSvr
				 )
{
	HRESULT           hr               = E_FAIL;
	DRM_ACTSERV_INFO *pdasi            = NULL;
	UINT              uiStrLen         = 0;
	size_t            cchActivationSvr = 0;
	DWORD             dwWaitResult;
	DRM_CONTEXT       context;
	BOOL fShared = false;
	UINT uiRACLength = 0;

	context.hEvent = NULL;

	//
	// 1. Validate the parameters
	//
	if ( NULL == hClient )
	{
		wprintf( L"\nThe client session was NULL.\n" );
		hr = E_INVALIDARG;
		goto e_Exit;
	}

	//
	// 2. Create an event for the callback function
	//
	if ( NULL == ( context.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL ) ) )
	{
		wprintf( L"\ncontext.hEvent was NULL after the CreateEvent call\n" );
		goto e_Exit;
	}

	//
	// Allocate memory for the DRM_ACTSERV_INFO structure for activation
	//
	pdasi = new DRM_ACTSERV_INFO;
	if ( NULL == pdasi )
	{
		wprintf( L"\nMemory allocation failed for DRM_ACTSERV_INFO.\n" );
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}
	pdasi->wszURL = NULL;

	//
	// 3. Copy the activation server URL into the wszURL member
	//    of the DRM_ACTSERV_INFO struct 
	//
	if ( NULL != wszActivationSvr )
	{
		//
		// 3(a). Use the URL provided through the command line argument
		//
		hr = StringCchLengthW( wszActivationSvr, 
			STRSAFE_MAX_CCH, 
			&cchActivationSvr 
			);
		if ( FAILED( hr ) )
		{
			wprintf( L"\nStringCchLengthW failed. hr = 0x%x.\n", hr );
			goto e_Exit;
		}

		//
		// Allocate memory for the URL member of 
		// the DRM_ACTSERV_INFO structure
		//
		pdasi->wszURL = new WCHAR[ cchActivationSvr + 1 ];
		if ( NULL == pdasi->wszURL )
		{
			wprintf( L"\nMemory allocation failed for user "\
				L"activation URL string.\n" );
			hr = E_OUTOFMEMORY;
			goto e_Exit;
		}
		hr = StringCchCopyW( pdasi->wszURL, 
			cchActivationSvr + 1, 
			wszActivationSvr 
			);
		if ( FAILED( hr ) )
		{
			wprintf( L"\nStringCchCopyW failed. hr = 0x%x.\n", hr );
			goto e_Exit;
		}
		wprintf( L"\nUser Activation server URL:\n%s\n", pdasi->wszURL );
	}
	else
	{
		//
		// 3(b). Try to find the service location where user activation 
		//       is available.
		//       The first call is to get -
		//       (a) whether there is a service location and 
		//       (b) if so, the size needed for a buffer to 
		//           hold the URL for the service.
		//
		hr = DRMGetServiceLocation( hClient,
			DRM_SERVICE_TYPE_CERTIFICATION,
			DRM_SERVICE_LOCATION_ENTERPRISE,
			NULL,
			&uiStrLen,
			NULL 
			);

		if ( SUCCEEDED( hr ) )
		{
			//
			// There is a service location; reserve space
			// for the URL.
			//
			pdasi->wszURL = new WCHAR[ uiStrLen ];
			if ( NULL == pdasi->wszURL )
			{
				wprintf( L"\nMemory allocation failed for user activation "\
					L"URL string.\n" );
				hr = E_OUTOFMEMORY;
				goto e_Exit;
			}

			//
			// Call second time to get the actual service location
			// copied into the URL.
			//
			hr = DRMGetServiceLocation( hClient,
				DRM_SERVICE_TYPE_CERTIFICATION,
				DRM_SERVICE_LOCATION_ENTERPRISE,
				NULL,
				&uiStrLen,
				pdasi->wszURL
				);

			if ( FAILED( hr ) )
			{
				wprintf( L"\nDRMGetServiceLocation (ENTERPRISE) failed. "\
					L"hr = 0x%x\n", hr );
				goto e_Exit;
			}
			wprintf( L"\nDRMGetServiceLocation (ENTERPRISE) succeeded.\n\n"\
				L"User Activation server URL:\n%s\n", pdasi->wszURL );
		}
		else
		{
			wprintf( L"\nDRMGetServiceLocation failed. hr = 0x%x. "\
				L"\nPassing NULL server info to DRMActivate.\n", hr );
		}
	}

	//
	// 4. Activate the user
	//
	hr = DRMActivate( hClient,
		DRM_ACTIVATE_GROUPIDENTITY | DRM_ACTIVATE_SILENT,
		0,
		pdasi,
		( VOID* )&context,
		NULL 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMActivate (DRM_ACTIVATE_GROUPIDENTITY) "\
			L"failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// 5. Wait for the callback to return
	//
	dwWaitResult = WaitForSingleObject( context.hEvent, DW_WAIT_TIME_SECONDS );
	if ( WAIT_TIMEOUT == dwWaitResult )
	{
		wprintf( L"\nWaitForSingleObject timed out.\n" );
		goto e_Exit;
	}
	if ( FAILED( context.hr ) )
	{
		//
		// In case a failure was reported via the callback function
		// note it also.
		//
		wprintf( L"\nThe callback function returned a failure "\
			L"code. hr = 0x%x\n", context.hr );
		hr = context.hr;
		goto e_Exit;
	}

	wprintf( L"\nThe user is now activated.\n" );

e_Exit:
	//
	// 6. Clean up and free memory
	//
	if ( NULL != pdasi )
	{
		if ( NULL != pdasi->wszURL )
		{
			delete [] pdasi->wszURL;
		}
		delete pdasi;
	}
	if ( NULL != context.hEvent )
	{
		CloseHandle( context.hEvent );
	}
	return hr;
}

//
// This function performs the following actions:
//    1. Validate and initialize the parameters
//    2. Create an event for the callback function
//    3. Populate the wszLicensingSvr parameter.  This value can
//       be from:
//       (a) a command line argument
//       (b) service discovery
//    4. Acquire the client licensor certificate
//    5. Wait for the callback to return
//    6. Enumerate the client licensor certificate
//    7. Clean up and free memory
//
HRESULT
DoAcquireClientLicensorCertificate( 
								   DRMHSESSION hClient,
								   __in PWCHAR wszLicensingSvr, 
								   __in PWCHAR wszUserId, 
								   __deref_out PWCHAR *wszClientLicensorCert
								   )
{
	HRESULT     hr                         = E_FAIL;
	UINT        uiStrLen                   = 0;
	UINT        uiClientLicensorCertLength = 0;
	BOOL        fShared                    = false;
	DRM_CONTEXT context;
	DWORD       dwWaitResult;

	context.hEvent = NULL;

	//
	// 1. Validate and initialize the parameters
	//
	if ( ( NULL == hClient ) || ( NULL == wszUserId ) )
	{
		wprintf( L"\nInvalid parameter.\n" );
		hr = E_INVALIDARG;
		goto e_Exit;
	}

	*wszClientLicensorCert = NULL;

	//
	// 2. Create an event for the callback function.  This event will be 
	// passed as a void pointer to DRMAcquireLicense.  DRMAcquireLicense 
	// simply passes back this pointer to the StatusCallback callback 
	// function which knows that it is an event and will thus signal it 
	// when completed.
	//
	if ( NULL == ( context.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL ) ) )
	{
		wprintf( L"\ncontext.hEvent was NULL after the CreateEvent call.\n" );
		goto e_Exit;
	}

	//
	// 3. If the licensing URL was not passed in via the command line, 
	//    find the licensing URL through service discovery
	//
	if ( NULL == wszLicensingSvr )
	{
		//       The first call is to get -
		//       (a) whether there is a service location and 
		//       (b) if so, the size needed for a buffer to 
		//           hold the URL for the service.
		//
		hr = DRMGetServiceLocation( hClient, 
			DRM_SERVICE_TYPE_CLIENTLICENSOR, 
			DRM_SERVICE_LOCATION_ENTERPRISE,
			NULL,
			&uiStrLen,
			NULL
			);
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMGetServiceLocation failed. hr = 0x%x\n", hr );
			goto e_Exit;
		}

		wszLicensingSvr = new WCHAR[ uiStrLen ];
		if ( NULL == wszLicensingSvr )
		{
			wprintf( L"\nMemory allocation failed for the "\
				L"licensing URL string.\n" );
			hr = E_OUTOFMEMORY;
			goto e_Exit;
		}

		hr = DRMGetServiceLocation( hClient,
			DRM_SERVICE_TYPE_CLIENTLICENSOR,
			DRM_SERVICE_LOCATION_ENTERPRISE,
			NULL,
			&uiStrLen,
			wszLicensingSvr
			);
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMGetServiceLocation failed. hr = 0x%x\n", hr );
			goto e_Exit;
		}
	}

	wprintf( L"\nLicensing server URL:\n%s\n", wszLicensingSvr );

	//
	// 4. Acquire the client licensor certificate
	//
	hr = DRMAcquireLicense( hClient, 
		0, 
		NULL, 
		NULL, 
		NULL, 
		wszLicensingSvr, 
		( VOID* )&context 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMAcquireLicense failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// 5. Wait for the callback to return
	//
	dwWaitResult = WaitForSingleObject( context.hEvent, DW_WAIT_TIME_SECONDS );
	if ( WAIT_TIMEOUT == dwWaitResult )
	{
		wprintf( L"\nWaitForSingleObject timed out.\n" );
		goto e_Exit;
	}
	if ( FAILED( context.hr ) )
	{
		//
		// In case a failure was reported via the callback function
		// note it also.
		//
		hr = context.hr;
		wprintf( L"\nThe callback function returned a failure "\
			L"code. hr = 0x%x\n", context.hr );
		goto e_Exit;
	}

	wprintf( L"\nA client licensor certificate has been acquired.\n" );

	// 
	// 6. Enumerate the client licensor certificate to return it
	//
	hr = DRMEnumerateLicense( hClient, 
		DRM_EL_SPECIFIED_CLIENTLICENSOR, 
		0, 
		&fShared, 
		&uiClientLicensorCertLength,
		NULL
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMEnumerateLicense (DRM_EL_SPECIFIED_CLIENTLICENSOR) "\
			L"failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	*wszClientLicensorCert = new WCHAR[ uiClientLicensorCertLength ];
	if ( NULL == *wszClientLicensorCert )
	{
		wprintf( L"\nMemory allocation for wszClientLicensorCert failed.\n" );
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}

	hr = DRMEnumerateLicense( hClient, 
		DRM_EL_SPECIFIED_CLIENTLICENSOR,
		0,
		&fShared,
		&uiClientLicensorCertLength,
		*wszClientLicensorCert
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMEnumerateLicense(DRM_EL_SPECIFIED_CLIENTLICENSOR)"\
			L" failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

e_Exit:
	//
	//    7. Clean up and free memory
	//
	if ( NULL != context.hEvent )
	{
		CloseHandle( context.hEvent );
	}
	return hr;
}

//
// This function performs the following actions:
//    1. Validate and initialize the parameters
//    2. Create an event for the callback function
//    3. Add information to the issuance license
//       (a) create a unique guid for the content ID
//       (b) set the metadata
//       (c) set the app specific data
//       (d) set the interval time
//       (e) set the usage policy
//       (f) create a user and right
//    4. Sign the issuance license offline
//    5. Wait for the callback to return
//    6. Clean up and free memory
//
HRESULT
DoOfflinePublishing( 
					__in PWCHAR wszClientLicensorCert, 
					__in DRMENVHANDLE hEnv,
					__in PWCHAR wszUserId,
					__in DRMPUBHANDLE hIssuanceLicense,
					__deref_out PWCHAR *wszGUID,
					__deref_out PWCHAR *wszSignedIssuanceLicense 
					)
{
	HRESULT                   hr                           = E_FAIL;
	DRM_CONTEXT               context;
	DWORD                     dwWaitResult;
	GUID                      guid;
	UINT                      uiGUIDLength                 = 0;
	DRMPUBHANDLE              hUser                        = NULL;
	DRMPUBHANDLE              hRight                       = NULL;
	size_t                    cchSignedIssuanceLicense     = 0;

	context.hEvent = NULL;
	context.wszData = NULL;

	//
	// 1. Validate and initialize the parameters
	//
	if ( ( hEnv == NULL ) || ( wszClientLicensorCert == NULL ) ||
		( wszUserId == NULL ) || ( hIssuanceLicense == NULL ) )
	{
		wprintf( L"\nInvalid parameter.\n" );
		hr = E_INVALIDARG;
		goto e_Exit;
	}

	*wszGUID = NULL;
	*wszSignedIssuanceLicense = NULL;

	//
	// 2. Create an event for the callback function.  This event will be 
	// passed as a void pointer to DRMGetSignedIssuaneLicense. 
	// DRMGetSignedIssuanceLicense simply passes back this pointer to the  
	// StatusCallback callback function which knows that it is an event  
	// and will thus signal it when completed.
	//
	if ( NULL == ( context.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL ) ) )
	{
		wprintf( L"\ncontext.hEvent was NULL after the CreateEvent call.\n" );
		goto e_Exit;
	}


	//    3. Add information to the issuance license
	//       (a) create a unique guid for the content ID
	//       (b) set the metadata
	//       (c) set the app specific data
	//       (d) set the interval time
	//       (e) set the usage policy
	//       (f) create a user and right

	//
	// Create a GUID to use as a unique content ID
	// in the issuance license
	//
	hr = CoCreateGuid( &guid );
	if ( FAILED( hr ) )
	{
		wprintf( L"\nCoCreateGuid failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	*wszGUID = new WCHAR[ GUID_STRING_LENGTH ];
	if ( NULL == wszGUID )
	{
		wprintf( L"\nFailed to allocate memory for wszGUID\n" );
		goto e_Exit;
	}

	uiGUIDLength = StringFromGUID2( guid, *wszGUID, GUID_STRING_LENGTH );
	if ( 0 == uiGUIDLength )
	{
		wprintf( L"\nStringFromGUID2 failed.\n" );
		hr = E_FAIL;
		goto e_Exit;
	}

	//
	// Set the metadata, application specific data, interval time, and
	// usage policy in the unsigned issuance license
	//
	hr = DRMSetMetaData( hIssuanceLicense, 
		*wszGUID, 
		L"MS-GUID", 
		SKU_ID, 
		SKU_ID_TYPE, 
		CONTENT_TYPE, 
		CONTENT_NAME 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMSetMetaData failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// Set the application specific data
	//
	hr = DRMSetApplicationSpecificData( hIssuanceLicense,
		false,
		APP_SPECIFIC_DATA_NAME,
		APP_SPECIFIC_DATA_VALUE
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMSetApplicationSpecificData failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// Set the interval time
	//
	hr = DRMSetIntervalTime( hIssuanceLicense, INTERVAL_TIME_DAYS );
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMSetIntervalTime failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// Set the usage policy
	//
	hr = DRMSetUsagePolicy( hIssuanceLicense, 
		DRM_USAGEPOLICY_TYPE_BYNAME,
		false, 
		true, 
		USAGE_POLICY_APPLICATION_NAME,
		USAGE_POLICY_MIN_VERSION, 
		USAGE_POLICY_MAX_VERSION,
		NULL, 
		NULL, 
		NULL, 
		NULL 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMSetUsagePolicy failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// Create a user and right and add that to 
	// the unsigned issuance license
	//
	hr = DRMCreateUser( wszUserId, NULL, L"Windows", &hUser );
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMCreateUser failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	hr = DRMCreateRight( RIGHT_NAME, 
		NULL, 
		NULL, 
		0, 
		NULL, 
		NULL, 
		&hRight 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMCreateRight failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	hr = DRMAddRightWithUser( hIssuanceLicense, hRight, hUser );
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMAddRightWithUser failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// 4. Sign the issuance license offline
	//
	hr = DRMGetSignedIssuanceLicense( hEnv, 
		hIssuanceLicense, 
		DRM_SIGN_OFFLINE | DRM_AUTO_GENERATE_KEY, 
		NULL,
		0, 
		L"AES", 
		wszClientLicensorCert, 
		&StatusCallback, 
		NULL,  // for offline publishing
		( VOID* )&context
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMGetSignedIssuanceLicense failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// 5. Wait for the callback to return
	//
	dwWaitResult = WaitForSingleObject( context.hEvent, DW_WAIT_TIME_SECONDS );
	if ( WAIT_TIMEOUT == dwWaitResult )
	{
		wprintf( L"\nWaitForSingleObject timed out.\n" );
		goto e_Exit;
	}
	if ( FAILED( context.hr ) )
	{
		//
		// In case a failure was reported via the callback function
		// note it also.
		//
		wprintf( L"\nThe callback function returned a failure "\
			L"code. hr = 0x%x\n", context.hr );
		hr = context.hr;
		goto e_Exit;
	}

	//
	// Retrieve the length of the licensing server URL
	//
	hr = StringCchLengthW( context.wszData, 
		STRSAFE_MAX_CCH, 
		&cchSignedIssuanceLicense 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nStringCchLengthW failed.  hr = 0x%x\n", hr );
		goto e_Exit;
	}
	//
	// Allocate memory for the URL
	//
	*wszSignedIssuanceLicense = new WCHAR[ cchSignedIssuanceLicense + 1 ];
	if( NULL == *wszSignedIssuanceLicense ) 
	{
		wprintf( L"\nFailed to allocate memory "\
			L"for wszSignedIssuanceLicense.\n" );
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}
	//
	// Copy the URL into the wszSignedIssuanceLicense buffer
	//
	hr = StringCchCopyW( ( wchar_t* )*wszSignedIssuanceLicense, 
		cchSignedIssuanceLicense + 1 , 
		context.wszData 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nStringCchCopyW failed.  hr = 0x%x\n", hr );
		goto e_Exit;
	}

	wprintf( L"\nThe issuance license is now signed.\n" );

e_Exit:
	//
	//    6. Clean up and free memory
	//
	if ( NULL != context.hEvent )
	{
		CloseHandle( context.hEvent );
	}
	if ( NULL != context.wszData )
	{
		delete [] context.wszData;
	}
	if ( NULL != hUser )
	{
		hr = DRMClosePubHandle( hUser );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMClosePubHandle failed while closing "\
				L"hUser.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != hRight )
	{
		hr = DRMClosePubHandle( hRight );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMClosePubHandle failed while closing "\
				L"hRight.  hr = 0x%x\n", hr );
		}
	}
	return hr;
}

//
// This function performs the following actions:
//    1. Validate the parameters
//    2. Creates an enabling principal
//    3. Call DRMGetOwnerLicense to get a license to bind to
//    4. Create a bound license using the owner license
//    5. Create an encrypting object so that content can be encrypted.
//    6. Call DRMGetInfo to determine the block size to pass to DRMEncrypt
//    7. Encrypt the content
//    8. Clean up and free memory
//
HRESULT
DoEncryptContent( 
				 __in DRMENVHANDLE hEnv,
				 __in DRMHANDLE hLib,
				 __in DRMHSESSION hLicenseStorage, 
				 __in DRMHANDLE hIssuanceLicense,
				 __in PWCHAR wszGUID,
				 __in PWCHAR wszRAC,
				 __in PWCHAR wszSignedIssuanceLicense,
				 __deref_out UINT *uiEncryptedDataLength,
				 __deref_out BYTE **pbEncryptedContent 
				 )
{
	HRESULT                   hr                           = E_FAIL;
	PWCHAR                    wszOwnerLicense              = NULL;
	PWCHAR                    wszRevocationList            = NULL;
	UINT                      uiOwnerLicenseLength         = 0;
	DRMID                     idContent; 
	DRMID                     idStandardEP; 
	DRMID                     idNULL; 
	DRMHANDLE                 hBoundLicense                = NULL; 
	DRMHANDLE                 hEnablingPrincipal           = NULL; 
	DRMHANDLE                 hEBEncryptor                 = NULL;
	DRMBOUNDLICENSEPARAMS     oParams; 
	DRMENCODINGTYPE           eType;
	BOOL                      fShared                      = false;
	UINT                      uiRevocationListLength       = 0;
	UINT                      uiBlockLength                = 0;
	UINT                      uiBlockLengthSize            = 0;
	UINT                      uiPlainTextLength            = 0;
	UINT                      uiPlainTextBufferLength      = 0;
	UINT                      uiDRMEncryptLength           = 0;
	BYTE*                     pbPlainTextBuffer            = NULL;

	//
	// 1. Validate the parameters
	//
	if ( ( hIssuanceLicense == NULL ) || ( hEnv == NULL ) ||
		( hLib == NULL ) || ( hLicenseStorage == NULL ) ||
		( wszGUID == NULL ) || ( wszRAC == NULL ) ||
		( wszSignedIssuanceLicense == NULL ) )
	{
		wprintf( L"\nInvalid parameter.\n" );
		hr = E_INVALIDARG;
		goto e_Exit;
	}

	idStandardEP.uVersion  = 0; 
	idStandardEP.wszIDType = L"ASCII Tag"; 
	idStandardEP.wszID     = L"UDStdPlg Enabling Principal"; 

	//
	// 2. Creates an enabling principal
	//
	hr = DRMCreateEnablingPrincipal ( 
		hEnv,                       // secure environment handle 
		hLib,                       // library handle 
		idStandardEP.wszID,          // enabling principal type 
		&idNULL,                     // DRMID structure 
		wszRAC,                      // current user certificate / RAC 
		&hEnablingPrincipal          // enabling principal handle 
		); 
	if ( FAILED(hr) ) 
	{ 
		wprintf(L"\nDRMCreateEnablingBitsPrincipal failed. hr = 0x%x\n", hr);
		goto e_Exit; 
	} 

	//
	// 3. Call DRMGetOwnerLicense to get a license to bind to
	//    Note:  if you were using online publishing, you would
	//           make a call to DRMAcquireLicense to get an EUL
	//           to bind to
	//
	hr = DRMGetOwnerLicense( hIssuanceLicense, 
		&uiOwnerLicenseLength, 
		NULL 
		);
	if ( FAILED( hr ) )
	{
		wprintf(L"\nDRMGetOwnerLicense failed. hr = 0x%x\n", hr ); 
		goto e_Exit;
	}
	wszOwnerLicense = new WCHAR[ uiOwnerLicenseLength ];
	if ( NULL == wszOwnerLicense )
	{
		wprintf(L"\nMemory allocation for wszOwnerLicense failed.\n" ); 
		hr = E_OUTOFMEMORY; 
		goto e_Exit; 
	}
	hr = DRMGetOwnerLicense( hIssuanceLicense, 
		&uiOwnerLicenseLength, 
		wszOwnerLicense 
		);
	if ( FAILED( hr ) )
	{
		wprintf(L"\nDRMGetOwnerLicense failed. hr = 0x%x\n", hr ); 
		goto e_Exit;
	}

	idContent.uVersion  = 0; 
	idContent.wszIDType = L"MS-GUID"; 
	idContent.wszID     = wszGUID; //same guid used as the content ID when
	                               //creating the issuance license


	// Bind the license to the OWNER right. 
	oParams.hEnablingPrincipal                     = hEnablingPrincipal; 
	oParams.hSecureStore                           = NULL; 
	oParams.wszRightsRequested                     = L"OWNER";
	oParams.wszRightsGroup                         = L"Main-Rights"; 
	oParams.idResource                             = idContent; 
	oParams.wszDefaultEnablingPrincipalCredentials = NULL; 
	oParams.cAuthenticatorCount                    = 0; 

	//
	// 4. Create a bound license using the owner license
	//
	hr = DRMCreateBoundLicense ( 
		hEnv,                    // secure environment handle 
		&oParams,                // additional license options 
		wszOwnerLicense,         // owner license 
		&hBoundLicense,          // handle to bound license 
		NULL                     // reserved 
		); 

	if ( E_DRM_BIND_REVOCATION_LIST_STALE == hr || 
		E_DRM_BIND_NO_APPLICABLE_REVOCATION_LIST == hr )
	{ 
		wprintf(L"\nDRMCreateBoundLicense(OWNER) failed. The license "\
			L"requires a revocation list. hr = 0x%x\n", hr); 
		// Try to enumerate a revocation list if available. 
		hr = DRMEnumerateLicense( hLicenseStorage, 
			DRM_EL_REVOCATIONLIST, 
			0, 
			&fShared, 
			&uiRevocationListLength, 
			NULL 
			);
		if(FAILED(hr))
		{
			wprintf(L"\nDRMEnumerateLicense(DRM_EL_REVOCATIONLIST) "\
				L"failed. hr = 0x%x\n", hr);
			goto e_Exit;
		}

		wprintf(L"\nDRMEnumerateLicense(DRM_EL_REVOCATIONLIST) succeeded\n");

		wszRevocationList = new WCHAR[ uiRevocationListLength ];

		if ( NULL == wszRevocationList )
		{
			wprintf(L"\nMemory allocation failed\n");
			hr = E_OUTOFMEMORY;
			goto e_Exit;
		}

		hr = DRMEnumerateLicense( hLicenseStorage, 
			DRM_EL_REVOCATIONLIST, 
			0, 
			&fShared, 
			&uiRevocationListLength, 
			wszRevocationList 
			);
		if(FAILED(hr))
		{
			wprintf(L"\nDRMEnumerateLicense(DRM_EL_REVOCATIONLIST) "\
				L"failed. hr = 0x%x\n", hr);
			goto e_Exit;
		}

		wprintf(L"\nDRMEnumerateLicense(DRM_EL_REVOCATIONLIST) succeeded\n");

		// Register the revocation list. 
		if(FAILED(hr = DRMRegisterRevocationList(hEnv, wszRevocationList))) 
		{ 
			wprintf(L"\nDRMRegisterRevocationList failed. hr = 0x%x\n", hr);
			goto e_Exit; 
		} 

		wprintf(L"\nDRMRegisterRevocationList succeeded.\n"); 

		// Try to bind again 
		if(FAILED(hr = DRMCreateBoundLicense ( hEnv, 
			&oParams, 
			wszOwnerLicense, 
			&hBoundLicense, 
			NULL 
			))) 
		{ 
			wprintf(L"\nDRMCreateBoundLicense(OWNER) failed second "\
				L"time. hr = 0x%x\n", hr); 
			goto e_Exit; 
		}
	} 
	else if(FAILED(hr)) 
	{ 
		wprintf(L"\nDRMCreateBoundLicense(OWNER) failed. hr = 0x%x\n", hr);
		goto e_Exit; 
	} 

	// 5. Create an encrypting object so that content can be encrypted. 
	hr = DRMCreateEnablingBitsEncryptor( 
		hBoundLicense, 
		oParams.wszRightsRequested, 
		NULL, 
		NULL, 
		&hEBEncryptor 
		); 
	if(FAILED(hr)) 
	{ 
		wprintf(L"\nDRMCreateEnablingBitsEncryptor(OWNER) "\
			L"failed. hr = 0x%x\n", hr);
		goto e_Exit; 
	} 

	//
	// 6. Call DRMGetInfo to determine the block size to pass to DRMEncrypt
	//
	uiBlockLengthSize = sizeof( uiBlockLength );
	hr = DRMGetInfo( hEBEncryptor, 
		g_wszQUERY_BLOCKSIZE, 
		&eType, 
		&uiBlockLengthSize, 
		(BYTE*)&uiBlockLength 
		);
	if(FAILED(hr))
	{
		wprintf(L"\nDRMGetInfo failed. hr = 0x%x\n", hr);
		goto e_Exit;
	}

	uiPlainTextLength = (UINT) (sizeof(WCHAR) * wcslen(PLAINTEXT));
	uiPlainTextBufferLength = uiPlainTextLength + 
		(uiBlockLength - (uiPlainTextLength % uiBlockLength));

	pbPlainTextBuffer = new BYTE[ uiPlainTextBufferLength ];
	*pbEncryptedContent = new BYTE[ uiPlainTextBufferLength ];

	// zero out the buffer and then copy the plaintext into it so that
	// the buffer is padded with zeros
	memset( pbPlainTextBuffer, 0, uiPlainTextBufferLength );
	memcpy_s( pbPlainTextBuffer, 
		uiPlainTextBufferLength, 
		(BYTE*)PLAINTEXT, 
		uiPlainTextLength 
		);

	*uiEncryptedDataLength = 0;
	for ( int j = 0; (UINT)j * uiBlockLength < uiPlainTextBufferLength; j++ )
	{
		//
		// 7. Encrypt the content; the number of bytes to encrypt must be a 
		//    multiple of the block size
		//
		hr = DRMEncrypt( 
			hEBEncryptor, 
			j * uiBlockLength, 
			uiBlockLength,
			pbPlainTextBuffer + (j * uiBlockLength), // buffer + offset
			&uiDRMEncryptLength,
			NULL
			);
		if(FAILED(hr))
		{
			wprintf(L"\nDRMEncrypt failed. hr = 0x%x\n", hr);
			goto e_Exit;
		}

		//
		// Note: The sample is using a pre-allocated buffer to store the
		//       decrypted content since we know the size.  When writing
		//       an application, the buffer to store the decrypted content
		//       should be allocated based on the size returned by the
		//       first call to DRMEncrypt.  That buffer should then be
		//       passed into the second call to DRMEncrypt.
		//

		hr = DRMEncrypt( 
			hEBEncryptor, 
			j * uiBlockLength, 
			uiBlockLength,
			pbPlainTextBuffer + (j * uiBlockLength), // buffer + offset
			&uiDRMEncryptLength,
			*pbEncryptedContent + *uiEncryptedDataLength // buffer + offset
			);
		if(FAILED(hr))
		{
			wprintf(L"\nDRMEncrypt failed. hr = 0x%x\n", hr);
			goto e_Exit;
		}

		*uiEncryptedDataLength += uiDRMEncryptLength;
	}

e_Exit:
	//
	// 8. Clean up and free memory
	//
	if ( NULL != hEnablingPrincipal )
	{
		hr = DRMCloseHandle( hEnablingPrincipal );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMCloseHandle failed while closing "\
				L"hEnablingPrincipal.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != hBoundLicense )
	{
		hr = DRMCloseHandle( hBoundLicense );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMCloseHandle failed while closing "\
				L"hBoundLicense.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != hEBEncryptor )
	{
		hr = DRMCloseHandle( hEBEncryptor );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMCloseHandle failed while closing hEBEncryptor.  "\
				L"hr = 0x%x\n", hr );
		}
	}
	if ( NULL != wszRevocationList )
	{
		delete [] wszRevocationList;
	}
	if ( NULL != wszOwnerLicense )
	{
		delete [] wszOwnerLicense;
	}
	if ( NULL != pbPlainTextBuffer )
	{
		delete [] pbPlainTextBuffer;
	}
	return hr;
}

//
// This sample will perform the following actions:
//    1.  Create a client session
//    2.  Determine if the machine is already activated
//    3.  Call DoMachineActivation if the machine needs to be activated
//    4.  Enumerate the machine certificate
//    5.  Determine if the user is already activated
//    6.  Call DoUserActivation if the user needs to be activated
//    7.  Get the RAC from the license store
//    8.  Enumerate the client licensor certificates to determine if one
//        has already been acquired.  If not, get a client licensor cert
//    9.  If there are no client licensor certificates, call 
//        DoAcquireClientLicensorCertificate to acquire one
//   10.  Read the manifest file into a buffer
//   11.  Get the security provider
//   12.  Initialize an environment
//   13.  Create an unsigned issuance license from scratch
//   14.  Call DoOfflinePublishing to add policy to the unsigned issuance 
//        license and sign the issuance license
//   15.  Create a license storage session
//   16.  Encrypt the content
//   17.  Create an event for the callback function for DRMAcquireLicense
//   18.  Call DRMAcquireLicense to get an EUL
//   19.  Enumerate the use license from the license store
//   20.  Get the content Id from the EUL
//        (a) Get EUL from the EUL certificate chain
//        (b) Parse the unbound license (the EUL) to get the content ID
//        (c) Get the handle of the WORK object
//        (d) Get the content Id value
//   21.  Create the enabling principal
//   22.  Create a bound license using the owner license
//   23.  Create an encrypting object so that content can be encrypted.
//   24.  Call DRMGetInfo to determine the block size to pass to DRMDecrypt
//   25.  Decrypt the content
//   26.  Clean up and free memory
// 
int __cdecl 
wmain( 
	  int argc, 
	  __in_ecount( argc )WCHAR **argv 
	  )
{
	HRESULT                   hr                           = E_FAIL;
	DRMHSESSION               hClient                      = NULL;
	DRMHSESSION               hLicenseStorage              = NULL;
	DRMPUBHANDLE              hIssuanceLicense             = NULL;
	DRMENVHANDLE              hEnv                         = NULL;
	DRMHANDLE                 hLib                         = NULL;
	DRMQUERYHANDLE            hQueryHandle                 = NULL;
	DRMQUERYHANDLE            hSubQueryHandle              = NULL;
	DRMHANDLE                 hBoundLicense                = NULL; 
	DRMHANDLE                 hEnablingPrincipal           = NULL; 
	DRMHANDLE                 hEBDecryptor                 = NULL;
	DRMBOUNDLICENSEPARAMS     oParams; 
	DRMENCODINGTYPE           encodingType;
	DRMID                     idContent; 
	DRMID                     idStandardEP; 
	DRMID                     idNULL; 
	BOOL                      fShared                      = false;
	PWSTR                     wszGUID                      = NULL;
	PWCHAR                    wszUserId                    = NULL;
	PWCHAR                    wszRAC                       = NULL;
	PWCHAR                    wszEUL                       = NULL;
	PWCHAR                    wszActivationSvr             = NULL;
	PWCHAR                    wszLicensingSvr              = NULL;
	PWCHAR                    wszManifestFileName          = NULL;
	PWCHAR                    wszManifest                  = NULL;
	PWCHAR                    wszClientLicensorCert        = NULL;
	PWCHAR                    wszSignedIssuanceLicense     = NULL;
	BYTE*                     pbEncryptedContent           = NULL;
	BYTE*                     pbDecryptedContent           = NULL;
	PWCHAR                    wszSecurityProviderType      = NULL;
	PWCHAR                    wszSecurityProviderPath      = NULL;
	PWCHAR                    wszMachineCertificate        = NULL;
	PWCHAR                    wszEndUserLicenseCert        = NULL;
	PWCHAR                    wszIdValue                   = NULL;
	PWCHAR                    wszRevocationList            = NULL;
	UINT                      uiRevocationListLength       = 0;
	UINT                      uiMachineCertLength          = 0;
	UINT                      uiSecurityProviderTypeLength = 0;
	UINT                      uiSecurityProviderPathLength = 0;
	UINT                      uiGUIDLength                 = 0;
	UINT                      uiRACLength                  = 0;
	UINT                      uiClientLicensorCertLength   = 0;
	UINT                      uiEndUserLicenseCertLength   = 0;
	UINT                      uiEUL                        = 0;
	UINT                      uiIdValue                    = 0;
	UINT                      uiEncryptedDataLength        = 0;
	UINT                      uiDecryptedDataLength        = 0;
	UINT                      uiBlockLength                = 0;
	UINT                      uiBlockLengthSize            = 0;
	UINT                      uiDRMDecryptLength           = 0;
	DWORD                     dwWaitResult;
	SYSTEMTIME                stTimeFrom, stTimeUntil;
	DRM_CONTEXT               context;

	context.hEvent = NULL;
	context.wszData = NULL;

	if ( FAILED ( ParseCommandLine( argc, 
		argv, 
		&wszUserId, 
		&wszActivationSvr, 
		&wszLicensingSvr, 
		&wszManifestFileName
		) ) )
	{
		PrintUsage();
		goto e_Exit;
	}

	wprintf( L"\nRunning sample Consumption...\n" );

	//
	// 1. Create a client session
	//
	hr = DRMCreateClientSession( &StatusCallback, 
		0, 
		DRM_DEFAULTGROUPIDTYPE_WINDOWSAUTH, 
		wszUserId, 
		&hClient 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMCreateClientSession failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// 2. Call DRMIsActivated to determine if the machine is already activated
	//
	hr = DRMIsActivated( hClient, 
		DRM_ACTIVATE_MACHINE, 
		NULL 
		);
	if ( E_DRM_NEEDS_MACHINE_ACTIVATION == hr )
	{
		//
		// 3.  Call DoMachineActivation to activate the machine if 
		//     it's not activated
		//
		hr = DoMachineActivation( hClient, wszActivationSvr );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDoMachineActivation failed. hr = 0x%x\n", hr );
			goto e_Exit;
		}
	}
	else if ( hr == S_OK )
	{
		wprintf( L"\nThe machine is already activated.\n" );
	}
	else
	{
		wprintf( L"\nDRMIsActivated returned an unexpected failure: 0x%x.  "\
			L"E_DRM_NEEDS_MACHINE_ACTIVATION was expected.\n", hr );
		goto e_Exit;
	}

	//
	// 4. Enumerate the machine certificate
	//
	hr = DRMEnumerateLicense( hClient, 
		DRM_EL_MACHINE, 
		0, 
		&fShared, 
		&uiMachineCertLength, 
		NULL 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMEnumerateLicense failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	wszMachineCertificate = new WCHAR[ uiMachineCertLength ];
	if ( NULL == wszMachineCertificate )
	{
		wprintf( L"\nMemory allocation failed for wszMachineCertificate\n" );
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}

	hr = DRMEnumerateLicense( hClient, 
		DRM_EL_MACHINE, 
		0, 
		&fShared, 
		&uiMachineCertLength, 
		wszMachineCertificate 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMEnumerateLicense failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// 5. Call DRMIsActivated to determine if the user is 
	//    already activated.  
	//
	hr = DRMIsActivated( hClient, 
		DRM_ACTIVATE_GROUPIDENTITY, 
		NULL 
		);
	if ( SUCCEEDED( hr ) )
	{
		wprintf( L"\nThe user is already activated.\n" );
	}
	else if ( E_DRM_NEEDS_GROUPIDENTITY_ACTIVATION == hr )
	{
		//
		// 6. Call DoUserActivation to activate the user if the user 
		//    is not activated
		//
		hr = DoUserActivation( hClient, wszActivationSvr );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDoUserActivation failed. hr = 0x%x\n", hr );
			goto e_Exit;
		}
	}
	else if ( E_DRM_NEEDS_GROUPIDENTITY_ACTIVATION != hr )
	{
		wprintf( L"\nDRMIsActivated returned an unexpected failure: 0x%x.  "\
			L"E_DRM_NEEDS_GROUPIDENTITY_ACTIVATION "\
			L"was expected.\n", hr );
		goto e_Exit;
	}

	//
	// 7. Call DRMEnumerateLicense to get the RAC from the license store
	//
	hr = DRMEnumerateLicense( hClient, 
		DRM_EL_SPECIFIED_GROUPIDENTITY, 
		0, 
		&fShared, 
		&uiRACLength, 
		NULL 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMEnumerateLicense (DRM_EL_SPECIFIED_GROUPIDENTITY) "\
			L"failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}
	wszRAC = new WCHAR[ uiRACLength ];
	if ( NULL == wszRAC )
	{
		wprintf( L"\nMemory allocation for wszRAC failed.\n" );
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}

	hr = DRMEnumerateLicense( hClient, 
		DRM_EL_SPECIFIED_GROUPIDENTITY,
		0,
		&fShared,
		&uiRACLength,
		wszRAC
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMEnumerateLicense(DRM_EL_SPECIFIED_GROUPIDENTITY)"\
			L" failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// 8. Enumerate the client licensor certificates to determine if one
	//    has already been acquired.  If not, get a client licensor cert.
	//
	hr = DRMEnumerateLicense( hClient, 
		DRM_EL_SPECIFIED_CLIENTLICENSOR, 
		0, 
		&fShared, 
		&uiClientLicensorCertLength,
		NULL
		);
	if ( FAILED( hr ) && ( E_DRM_NO_MORE_DATA != hr ) )
	{
		wprintf( L"\nDRMEnumerateLicense (DRM_EL_SPECIFIED_CLIENTLICENSOR) "\
			L"failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}
	else if ( E_DRM_NO_MORE_DATA == hr )
	{
		//    9.  If there are no client licensor certificates, call 
		//        DoAcquireClientLicensorCertificate to acquire one
		hr = DoAcquireClientLicensorCertificate( hClient,
			wszLicensingSvr, 
			wszUserId, 
			&wszClientLicensorCert 
			);
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDoAcquireClientLicensorCertificate failed. "\
				L"hr = 0x%x\n", hr );
			goto e_Exit;
		}
	}
	else
	{
		wprintf( L"\nA client licensor certificate is "\
			L"already in the license store.\n" );

		wszClientLicensorCert = new WCHAR[ uiClientLicensorCertLength ];
		if ( NULL == wszClientLicensorCert )
		{
			wprintf( L"\nMemory allocation of wszClientLicensorCert "\
				L"failed.\n" );
			hr = E_OUTOFMEMORY;
			goto e_Exit;
		}

		hr = DRMEnumerateLicense( hClient, 
			DRM_EL_SPECIFIED_CLIENTLICENSOR,
			0,
			&fShared,
			&uiClientLicensorCertLength,
			wszClientLicensorCert
			);
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMEnumerateLicense(DRM_EL_SPECIFIED_CLIENTLICENSOR)"\
				L" failed. hr = 0x%x\n", hr );
			goto e_Exit;
		}
	}

	//
	// 10. Read the manifest file into a buffer
	//
	hr = ReadFileToWideString( wszManifestFileName, &wszManifest );
	if ( FAILED( hr ) )
	{
		wprintf( L"\nReadFileToWideString failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// 11. Get the security provider
	//
	hr = DRMGetSecurityProvider( 0,
		&uiSecurityProviderTypeLength,
		NULL,
		&uiSecurityProviderPathLength,
		NULL
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMGetSecurityProvider failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	wszSecurityProviderType = new WCHAR[ uiSecurityProviderTypeLength ];
	if ( NULL == wszSecurityProviderType )
	{
		wprintf( L"\nMemory allocation for wszSecurityProviderType "\
			L"failed.\n" );
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}

	wszSecurityProviderPath = new WCHAR[ uiSecurityProviderPathLength ];
	if ( NULL == wszSecurityProviderPath )
	{
		wprintf( L"\nMemory allocation for wszSecurityProviderPath "\
			L"failed." );
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}

	hr = DRMGetSecurityProvider( 0,
		&uiSecurityProviderTypeLength,
		wszSecurityProviderType,
		&uiSecurityProviderPathLength,
		wszSecurityProviderPath
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMGetSecurityProvider failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// 12. Initialize an environment
	//
	hr = DRMInitEnvironment( DRMSECURITYPROVIDERTYPE_SOFTWARESECREP,
		DRMSPECTYPE_FILENAME,
		wszSecurityProviderPath,
		wszManifest,
		wszMachineCertificate,
		&hEnv,
		&hLib
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMInitEnvironment failed. hr = 0x%x.\n", hr );
		goto e_Exit;
	}

	//
	// Get the system time for the starting and ending validity times
	// in the unsigned issuance license
	//
	GetSystemTime( &stTimeFrom );
	GetSystemTime( &stTimeUntil );
	stTimeUntil.wYear++;

	//
	// 13. Create an unsigned issuance license from scratch
	//
	hr = DRMCreateIssuanceLicense( &stTimeFrom,
		&stTimeUntil,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		&hIssuanceLicense 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMCreateIssuanceLicense failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// 14. Call DoOfflinePublishing to add policy to the unsigned issuance
	//     license and sign the issuance license
	//
	hr = DoOfflinePublishing( wszClientLicensorCert, 
		hEnv, 
		wszUserId, 
		hIssuanceLicense, 
		&wszGUID, 
		&wszSignedIssuanceLicense 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDoOfflinePublishing failed. hr = 0x%x.\n", hr );
		goto e_Exit;
	}

	//
	// 15. Create a license storage session, passing in an environment object 
	//     and a library handle (created previously by DRMInitEnvironment), a
	//     client session (created previously by DRMCreateClientSession), and 
	//     the issuance license string. 
	//
	hr = DRMCreateLicenseStorageSession( 
		hEnv, 
		hLib, 
		hClient, 
		0, 
		wszSignedIssuanceLicense, 
		&hLicenseStorage 
		); 
	if(FAILED(hr)) 
	{ 
		wprintf(L"\nDRMCreateLicenseStorageSession failed. hr = 0x%x\n", hr);
		goto e_Exit; 
	} 

	//
	// 16. Encrypt the content
	//
	hr = DoEncryptContent( hEnv, 
		hLib, 
		hLicenseStorage, 
		hIssuanceLicense, 
		wszGUID, 
		wszRAC, 
		wszSignedIssuanceLicense, 
		&uiEncryptedDataLength, 
		&pbEncryptedContent 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDoEncryptContent failed. hr = 0x%x.\n", hr );
		goto e_Exit;
	}

	//
	// 17. Create an event for the callback function.  This event will be 
	//     passed as a void pointer to DRMAcquireLicense.  DRMAcquireLicense 
	//     simply passes back this pointer to the StatusCallback callback 
	//     function which knows that it is an event and will thus signal it 
	//     when completed. 
	//
	if ( NULL == ( context.hEvent = CreateEvent ( NULL, FALSE, FALSE, NULL ) ) )
	{ 
		wprintf(L"\ncontext.hEvent was NULL after the CreateEvent call.\n" ); 
		goto e_Exit; 
	} 

	//
	// Now we need to consume the encrypted content, 
	// so we need to acquire an EUL for the content
	//

	//
	// 18. Call DRMAcquireLicense, which will put the end-user license into the
	//     permanent license store for you by default, or specify 
	//     DRM_AL_NOPERSIST if you want to put the license into the temporary 
	//     store and handle storage and management yourself. 
	//
	hr = DRMAcquireLicense( 
		hLicenseStorage, 
		0, 
		NULL, 
		NULL, 
		NULL, 
		NULL, 
		(VOID*)&context 
		); 
	if(FAILED(hr)) 
	{ 
		wprintf(L"\nDRMAcquireLicense failed. hr = 0x%x\n", hr); 
		goto e_Exit; 
	} 

	// Wait for the callback to return 
	dwWaitResult = WaitForSingleObject ( context.hEvent, DW_WAIT_TIME_SECONDS );
	if ( WAIT_TIMEOUT == dwWaitResult ) 
	{ 
		wprintf(L"\nWaitForSingleObject timed out.\n" ); 
		goto e_Exit; 
	} 
	if ( FAILED( context.hr ) ) 
	{ 
		// In case a failure was reported via the 
		// callback function note it also. 
		hr = context.hr; 
		wprintf(L"\nThe callback function returned a failure code. "\
			L"hr = 0x%x\n", context.hr ); 
		goto e_Exit; 
	} 
	wprintf(L"\nA Use License has been acquired.\n" ); 

	//
	// 19. Enumerate the use license from the license store
	// Note:  this sample assumes that there is only one EUL in the license 
	//        store, your code may need to increment the index of the 
	//        DRMEnumerateLicense API until you receive an E_DRM_NO_MORE_DATA 
	//        error to ensure that you have tried all of the possible EULs
	//        for this content
	//
	hr = DRMEnumerateLicense ( hLicenseStorage, 
		DRM_EL_EUL, 
		0, 
		&fShared, 
		&uiEndUserLicenseCertLength, 
		NULL 
		); 
	if ( FAILED( hr ) ) 
	{ 
		wprintf(L"\nDRMEnumerateLicense(DRM_EL_EUL) failed. hr = 0x%x\n", hr );
		goto e_Exit; 
	} 
	wszEndUserLicenseCert = new WCHAR[ uiEndUserLicenseCertLength ];
	if ( !wszEndUserLicenseCert ) 
	{ 
		wprintf(L"\nMemory allocation for wszEndUserLicenseCert failed.\n" );
		hr = E_OUTOFMEMORY; 
		goto e_Exit; 
	} 
	hr = DRMEnumerateLicense ( hLicenseStorage, 
		DRM_EL_EUL, 
		0, 
		&fShared, 
		&uiEndUserLicenseCertLength, 
		wszEndUserLicenseCert 
		); 
	if ( FAILED( hr ) ) 
	{ 
		wprintf(L"\nDRMEnumerateLicense(DRM_EL_EUL) failed. hr = 0x%x\n", hr );
		goto e_Exit; 
	} 

	//
	// 20. Get the content Id from the EUL
	//     (a) Get EUL from the EUL certificate chain
	//
	hr = DRMDeconstructCertificateChain( wszEndUserLicenseCert, 
		0, 
		&uiEUL, 
		NULL 
		);
	if ( FAILED( hr ) )
	{
		wprintf(L"\nDRMDeconstructCertificateChain failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	wszEUL = new WCHAR[ uiEUL ];

	if ( NULL == wszEUL )
	{
		wprintf( L"\nMemory allocation for wszEUL failed.\n" );
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}

	hr = DRMDeconstructCertificateChain( wszEndUserLicenseCert, 
		0, 
		&uiEUL, 
		wszEUL 
		);
	if ( FAILED( hr ) )
	{
		wprintf(L"\nDRMDeconstructCertificateChain failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// (b) Parse the unbound license (the EUL) to get the content ID
	//     The content ID is located in the WORK object
	//
	hr = DRMParseUnboundLicense( wszEUL, &hQueryHandle );
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMParseUnboundLicense failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// (c) Get the handle of the WORK object
	//
	hr = DRMGetUnboundLicenseObject( hQueryHandle, 
		g_wszQUERY_WORK, 
		0, 
		&hSubQueryHandle 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMGetUnboundLicenseObject failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	//
	// (d) Get the content Id value
	//
	hr = DRMGetUnboundLicenseAttribute( hSubQueryHandle, 
		g_wszQUERY_IDVALUE, 
		0, 
		&encodingType, 
		&uiIdValue, 
		NULL 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMGetUnboundLicenseAttribute failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	wszIdValue = new WCHAR[ uiIdValue ];

	if ( NULL == wszIdValue )
	{
		wprintf( L"\nMemory allocation for wszIdValue failed.\n" );
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}

	hr = DRMGetUnboundLicenseAttribute( hSubQueryHandle, 
		g_wszQUERY_IDVALUE, 
		0, 
		&encodingType, 
		&uiIdValue, 
		(BYTE*)wszIdValue 
		);
	if ( FAILED( hr ) )
	{
		wprintf( L"\nDRMGetUnboundLicenseAttribute failed. hr = 0x%x\n", hr );
		goto e_Exit;
	}

	idStandardEP.uVersion  = 0; 
	idStandardEP.wszIDType = L"ASCII Tag"; 
	idStandardEP.wszID     = L"UDStdPlg Enabling Principal"; 

	//
	// 21. Create the enabling principal
	//
	hr = DRMCreateEnablingPrincipal ( 
		hEnv,                       // secure environment handle 
		hLib,                       // library handle 
		idStandardEP.wszID,          // enabling principal type 
		&idNULL,                     // DRMID structure 
		wszRAC,                      // current user certificate / RAC 
		&hEnablingPrincipal          // enabling principal handle 
		); 
	if ( FAILED(hr) ) 
	{ 
		wprintf(L"\nDRMCreateEnablingBitsPrincipal failed. hr = 0x%x\n", hr);
		goto e_Exit; 
	} 

	idContent.uVersion  = 0; 
	idContent.wszIDType = L"MS-GUID"; 
	idContent.wszID     = wszIdValue; //use the guid we obtained from the EUL


	// Bind the license to the EDIT right. 
	oParams.hEnablingPrincipal                     = hEnablingPrincipal; 
	oParams.hSecureStore                           = NULL; 
	oParams.wszRightsRequested                     = RIGHT_NAME;
	oParams.wszRightsGroup                         = L"Main-Rights"; 
	oParams.idResource                             = idContent; 
	oParams.wszDefaultEnablingPrincipalCredentials = NULL; 
	oParams.cAuthenticatorCount                    = 0; 

	//
	// 22. Create a bound license using the EUL
	//
	hr = DRMCreateBoundLicense ( 
		hEnv,                   // secure environment handle 
		&oParams,                // additional license options 
		wszEndUserLicenseCert,   // EUL chain
		&hBoundLicense,          // handle to bound license 
		NULL                     // reserved 
		); 

	if ( E_DRM_BIND_REVOCATION_LIST_STALE == hr || 
		E_DRM_BIND_NO_APPLICABLE_REVOCATION_LIST == hr ) 
	{ 
		wprintf(L"\nDRMCreateBoundLicense(EDIT) failed. The license requires "\
			L"a revocation list. hr = 0x%x\n", hr); 
		// Try to enumerate a revocation list if available. 
		hr = DRMEnumerateLicense( hLicenseStorage, 
			DRM_EL_REVOCATIONLIST, 
			0, 
			&fShared, 
			&uiRevocationListLength, 
			NULL );
		if(FAILED(hr))
		{
			wprintf(L"\nDRMEnumerateLicense(DRM_EL_REVOCATIONLIST) "\
				L"failed. hr = 0x%x\n", hr);
			goto e_Exit;
		}

		wprintf(L"\nDRMEnumerateLicense(DRM_EL_REVOCATIONLIST) succeeded\n");

		wszRevocationList = new WCHAR[ uiRevocationListLength ];

		if ( NULL == wszRevocationList )
		{
			wprintf(L"\nMemory allocation for wszRevocationList failed\n");
			hr = E_OUTOFMEMORY;
			goto e_Exit;
		}

		hr = DRMEnumerateLicense( hLicenseStorage, 
			DRM_EL_REVOCATIONLIST, 
			0, 
			&fShared, 
			&uiRevocationListLength, 
			wszRevocationList );
		if(FAILED(hr))
		{
			wprintf(L"\nDRMEnumerateLicense(DRM_EL_REVOCATIONLIST) failed. "\
				L"hr = 0x%x\n", hr);
			goto e_Exit;
		}

		wprintf(L"\nDRMEnumerateLicense(DRM_EL_REVOCATIONLIST) succeeded\n");

		// Register the revocation list. 
		if(FAILED(hr = DRMRegisterRevocationList(hEnv, wszRevocationList))) 
		{ 
			wprintf(L"\nDRMRegisterRevocationList failed. hr = 0x%x\n", hr); 
			goto e_Exit; 
		} 

		wprintf(L"\nDRMRegisterRevocationList succeeded.\n"); 

		// Try to bind again 
		if(FAILED(hr = DRMCreateBoundLicense ( hEnv, 
			&oParams, 
			wszEUL, 
			&hBoundLicense, 
			NULL ))) 
		{ 
			wprintf(L"\nDRMCreateBoundLicense(EDIT) failed second time. "\
				L"hr = 0x%x\n", hr); 
			goto e_Exit; 
		}
	} 
	else if(FAILED(hr)) 
	{ 
		wprintf(L"\nDRMCreateBoundLicense(EDIT) failed. hr = 0x%x\n", hr); 
		goto e_Exit; 
	} 


	// 23. Create an decrypting object so that content can be decrypted.
	hr = DRMCreateEnablingBitsDecryptor( 
		hBoundLicense, 
		oParams.wszRightsRequested, 
		NULL, 
		NULL, 
		&hEBDecryptor 
		); 

	if(FAILED(hr)) 
	{ 
		wprintf(L"\nDRMCreateEnablingBitsDecryptor(EDIT) failed. "\
			L"hr = 0x%x\n", hr); 
		goto e_Exit; 
	} 

	//
	// 24. Call DRMGetInfo to determine the block size to pass
	//     to DRMDecrypt
	//
	uiBlockLengthSize = sizeof ( uiBlockLength );
	hr = DRMGetInfo( hEBDecryptor, 
		g_wszQUERY_BLOCKSIZE, 
		&encodingType, 
		&uiBlockLengthSize, 
		(BYTE* )&uiBlockLength
		);
	if(FAILED(hr))
	{
		wprintf(L"\nDRMGetInfo failed. hr = 0x%x\n", hr);
		goto e_Exit;
	}

	pbDecryptedContent = new BYTE[ uiEncryptedDataLength ];
	if ( pbDecryptedContent == NULL )
	{
		wprintf(L"\nMemory allocation for pbDecryptedContent failed\n");
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}
	// zero out the buffer
	memset( pbDecryptedContent, 0, uiEncryptedDataLength );

	uiDecryptedDataLength = 0;
	for ( int i = 0; (UINT)i * uiBlockLength < uiEncryptedDataLength; i++)
	{
		// 25. Decrypt the content; the number of bytes to decrypt must be a
		//     multiple of the block size
		hr = DRMDecrypt( hEBDecryptor, 
			i * uiBlockLength, 
			uiBlockLength,
			pbEncryptedContent + (i * uiBlockLength), // buffer + offset
			&uiDRMDecryptLength, 
			NULL 
			);
		if ( FAILED ( hr ) )
		{ 
			wprintf(L"\nDRMDecrypt failed. hr = 0x%x\n", hr); 
			goto e_Exit; 
		} 

		//
		// Note: The sample is using a pre-allocated buffer to store the
		//       decrypted content since we know the size.  When writing
		//       an application, the buffer to store the decrypted content
		//       should be allocated based on the size returned by the
		//       first call to DRMDecrypt.  That buffer should then be
		//       passed into the second call to DRMDecrypt.
		//

		hr = DRMDecrypt( hEBDecryptor, 
			i * uiBlockLength, 
			uiBlockLength, 
			pbEncryptedContent + (i * uiBlockLength), // buffer + offset
			&uiDRMDecryptLength, 
			pbDecryptedContent + uiDecryptedDataLength // buffer + offset
			);
		if ( FAILED ( hr ) )
		{ 
			wprintf(L"\nDRMDecrypt failed. hr = 0x%x\n", hr); 
			goto e_Exit; 
		} 
		uiDecryptedDataLength += uiDRMDecryptLength;
	}

	// Compare the original plaintext with the decrypted text
	if ( memcmp((BYTE*)PLAINTEXT, pbDecryptedContent, 
		(UINT) sizeof(WCHAR) * wcslen(PLAINTEXT)) != 0 )	
	{ 
		wprintf(L"\nThe decrypted text does not match "\
			L"the original plaintext", hr); 
		goto e_Exit; 
	} 

	wprintf( L"\nThe consumption sample succeeded.\n" );

e_Exit:
	//
	// 26. Clean up and free memory
	//
	if ( NULL != hClient )
	{
		hr = DRMCloseSession( hClient );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMCloseSession failed while closing "\
				L"hClient.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != hIssuanceLicense )
	{
		hr = DRMClosePubHandle( hIssuanceLicense );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMClosePubHandle failed while closing "\
				L"hIssuanceLicense.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != hQueryHandle )
	{
		hr = DRMCloseQueryHandle( hQueryHandle );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMCloseQueryHandle failed while closing "\
				L"hQueryHandle.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != hSubQueryHandle )
	{
		hr = DRMCloseQueryHandle( hSubQueryHandle );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMCloseQueryHandle failed while closing "\
				L"hSubQueryHandle.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != hLicenseStorage )
	{
		hr = DRMCloseSession( hLicenseStorage );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMCloseSession failed while closing "\
				L"hLicenseStorage.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != hEnablingPrincipal )
	{
		hr = DRMCloseHandle( hEnablingPrincipal );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMCloseHandle failed while closing "\
				L"hEnablingPrincipal.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != hBoundLicense )
	{
		hr = DRMCloseHandle( hBoundLicense );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMCloseHandle failed while closing "\
				L"hBoundLicense.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != hEBDecryptor )
	{
		hr = DRMCloseHandle( hEBDecryptor );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMCloseHandle failed while closing "\
				L"hEBDecryptor.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != hLib )
	{
		hr = DRMCloseHandle( hLib );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMCloseHandle failed while closing "\
				L"hLib.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != hEnv )
	{
		hr = DRMCloseEnvironmentHandle( hEnv );
		if ( FAILED( hr ) )
		{
			wprintf( L"\nDRMCloseEnvironmentHandle failed while closing "\
				L"hEnv.  hr = 0x%x\n", hr );
		}
	}
	if ( NULL != context.wszData )
	{
		delete [] context.wszData;
	}
	if ( NULL != context.hEvent )
	{
		CloseHandle( context.hEvent );
	}
	if ( NULL != wszUserId )
	{
		delete [] wszUserId;
	}
	if ( NULL != wszIdValue )
	{
		delete [] wszIdValue;
	}
	if ( NULL != wszActivationSvr )
	{
		delete [] wszActivationSvr;
	}
	if ( NULL != wszLicensingSvr )
	{
		delete [] wszLicensingSvr;
	}
	if ( NULL != wszManifestFileName )
	{
		delete [] wszManifestFileName;
	}
	if ( NULL != wszManifest )
	{
		delete [] wszManifest;
	}
	if ( NULL != wszClientLicensorCert )
	{
		delete [] wszClientLicensorCert;
	}
	if ( NULL != wszEndUserLicenseCert )
	{
		delete [] wszEndUserLicenseCert;
	}
	if ( NULL != wszSecurityProviderType )
	{
		delete [] wszSecurityProviderType;
	}
	if ( NULL != wszSecurityProviderPath )
	{
		delete [] wszSecurityProviderPath;
	}
	if ( NULL != wszMachineCertificate )
	{
		delete [] wszMachineCertificate;
	}
	if ( NULL != pbEncryptedContent )
	{
		delete [] pbEncryptedContent;
	}
	if ( NULL != pbDecryptedContent )
	{
		delete [] pbDecryptedContent;
	}
	if ( NULL != wszSignedIssuanceLicense )
	{
		delete [] wszSignedIssuanceLicense;
	}
	if ( NULL != wszRevocationList )
	{
		delete [] wszRevocationList;
	}
	if ( NULL != wszGUID )
	{
		delete [] wszGUID;
	}
	if ( NULL != wszRAC )
	{
		delete [] wszRAC;
	}
	if ( NULL != wszEUL )
	{
		delete [] wszEUL;
	}
	if ( FAILED( hr ) )
	{
		wprintf( L"\nConsumption failed. hr = 0x%x\n", hr );
	}
	return hr;
}