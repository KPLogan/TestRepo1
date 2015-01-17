/********************************************************************************
 *
 *	PROGRAM NAME:
 *		SetService.EXE (IDEXTER Server Service Setup)
 *
 *	PURPOSE:
 *		This program;
 *			IDEXTER Server Service Setup Programs
 *
 *  Copyright (c) 2007, MACSEA Ltd.
 *  All rights reserved.
 *
 *  HISTORY:
 *      Vers    Date        By      Notes
 *      ---------------------------------
 *      1.0     10/05/07    sdh     Created
 ********************************************************************************
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sqlext.h>
#include <string.h>
#include <Winbase.h>
#include <tlhelp32.h>

#include "resource.h"

//#define _WIN32_WINNT				0x0400
#define	SQL_STATEMENT_LEN			720

#define	ANY_LENGTH		100
#define SHUTDOWN_TIMER  1 // Timer ID

#define MAX_NUM_ATTEMPTS 200

#define	DB_LENGTH		20

#define INI_FILENAME		"IDEXPROP.INI"	// Name of the INI file
#define STARTUP_LOG			"IDEXBSSTP.LOG"	// Name of the LOG file

#define PROCESS_TMSRV		"DEXTMSRV.EXE"
#define PROCESS_XMTR		"IDEXXMTR.EXE"
#define PROCESS_EQMTR		"IDEXEQMTR.EXE"
#define PROCESS_ALERT		"IDEXALERT.EXE"
#define PROCESS_KM			"IDABKMR.EXE"
#define PROCESS_DET			"IDAFDET.EXE"
#define PROCESS_ARCHIVE		"IDEXARCH.EXE"
#define PROCESS_EVT			"IDEXEVT.EXE"
#define PROCESS_EQ			"IDEXEQ.EXE"
#define PROCESS_TASKS		"IDEXTSKS.EXE"

#define PROCESS_AUTOBRAWN	"IDABRAWN.EXE"

/* Program Specific
 */
 static	UINT uiTimer;	//Unsigned Int Time duration
 static TCHAR szAppName[] = TEXT ("IDEXTER BASIC SERVER STARTUP") ;
 char		 szErrorMessage[256] = "";
 char		 szErrorLog[256] = "";
 char		 szAppPath[256] = "";

 MSG		 msg;
 int		 iWaitSecs;
 FILE		 *ofile;
 HINSTANCE	 hInst;
 HWND        hwnd ;
 HWND		 hWndStatus;
 long		lErrorNo = 0;
 static  BOOL blnTryAgain;

/* Global function definitions
 */
LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM) ;
__declspec(dllexport) void __stdcall IDEXErrorHandlerEx(char *caller, char *string);

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam);
BOOL CALLBACK EnumActiveWinProc(HWND hWnd, LPARAM lParam);
int GetMyDirectory();

// ---------------------------------------------------------------------------------------------
#define MAXSTRLEN	500
#define BUFSIZE		4096
#define SERVICELOG			"SetService.log"

CRITICAL_SECTION currentCS;

/* Global functions **********************************************************************
 */
int CreateServiceIDEXTER(int iDEXTERType, char szDEXTERPath[]);
int DeleteServiceIDEXTER();
int StopServiceIDEXTER();
int IsOPSValid();
// --------------------------------------------------------------------------------------------

/* The purpose of this program is to get these values and put in the INI
 * file at the System boot up process.
 */
char	szADDRS[20] = "";
long	lPortNum = 0;
long	lNOSValue = 0;
static  BOOL bIsDEXRCVRActive;
static  char szWinToClose[20];

/********************************************************************
 *
 ********************************************************************
 */
void ServiceLogging(char* pMsg)
{
	FILE *stream;
	errno_t err;

	SYSTEMTIME oTime;
	char szTimeStamp[50];

	GetLocalTime(&oTime);

	err = fopen_s( &stream, SERVICELOG, "a+" );
	if( err == 0 )
	{
		sprintf(szTimeStamp, "%02d/%02d/%04d, %02d:%02d:%02d", 
			oTime.wMonth,oTime.wDay,oTime.wYear,oTime.wHour,oTime.wMinute,oTime.wSecond);
		fprintf_s(stream,"%s, %s\n", szTimeStamp, pMsg); 
		err = fclose( stream );
	}
}

/************************************************************************************
 *
 ************************************************************************************
 */
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR szCmdLine, int iCmdShow)
{

	MSG         msg ;
    WNDCLASSEX  wndclass ;				
	int			iSuccess = 0;
	int			k = 0;
	int			nRead;

	char		szFileName[256] = "";	
	char		szCurrentPath[256] = "";

	int			iConfirmed = 0;
	int			iDEXTERActive = 0;
	int			imyAttempts = 0;
	int			iCommand;

	nRead = sscanf( szCmdLine, "%d", &iCommand);
	if(nRead < 1)
	{
		sprintf(szErrorMessage, "Invalid Command Arguments");
		IDEXErrorHandlerEx(szAppName, szErrorMessage);
		return 0;
	}

	//---------------------------------------------------------------------------
	sprintf_s(szErrorMessage, sizeof(szErrorMessage), "Arguments:%s", szCmdLine);
	ServiceLogging(szErrorMessage);
	//---------------------------------------------------------------------------

	switch (iCommand)
	{
		case 100:
			/* Get current IDEXTER path
			 */
			//---------------------------------------------------------------------------
			ServiceLogging("GetMyDirectory()");
			//---------------------------------------------------------------------------

			if(GetMyDirectory())
			{
				sprintf(szErrorMessage, "Unable to get IDEXTER path");
				IDEXErrorHandlerEx(szAppName, szErrorMessage);
				return 0;
			}

			//---------------------------------------------------------------------------
			ServiceLogging("CreateServiceIDEXTER()");
			//---------------------------------------------------------------------------

			iSuccess = CreateServiceIDEXTER(101, szAppPath);
			if(iSuccess != 0)
			{
				sprintf(szErrorMessage, "Unable to get Create IDEXService");
				IDEXErrorHandlerEx(szAppName, szErrorMessage);
				return 0;
			}
			break;

		case 200:
			/* Get current IDEXTER path
			 */
			if(GetMyDirectory())
			{
				sprintf(szErrorMessage, "Unable to get IDEXTER path");
				IDEXErrorHandlerEx(szAppName, szErrorMessage);
				return 0;
			}

			//---------------------------------------------------------------------------
			ServiceLogging("DeleteServiceIDEXTER()");
			//---------------------------------------------------------------------------

			iSuccess = DeleteServiceIDEXTER();
			if(iSuccess != 0)
			{
				sprintf(szErrorMessage, "Unable to get Delete IDEXService");
				IDEXErrorHandlerEx(szAppName, szErrorMessage);
				return 0;
			}
			break;

		default:
			return 0;
	}

	//---------------------------------------------------------------------------
	ServiceLogging("Completed heading to Timer()");
	//---------------------------------------------------------------------------

	/* Window Class
	 */
	wndclass.cbSize        = sizeof (wndclass) ;
	wndclass.style         = CS_HREDRAW | CS_VREDRAW ;
	wndclass.lpfnWndProc   = WndProc ;
	wndclass.cbClsExtra    = 0 ;
	wndclass.cbWndExtra    = 0 ;
	wndclass.hInstance     = hInstance ;
	wndclass.hIcon         = LoadIcon (hInst, MAKEINTRESOURCE(IDI_ICON1)) ;
	wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW) ;
	wndclass.hbrBackground = (HBRUSH) GetStockObject (WHITE_BRUSH) ;
	wndclass.lpszMenuName  = szAppName ;
	wndclass.lpszClassName = szAppName ;
	wndclass.hIconSm       = LoadIcon (NULL, IDI_APPLICATION) ;

	if (!RegisterClassEx (&wndclass))
	{
		fprintf(ofile, "This program requires Windows XP or higher.\n");
		return 0 ;
	}

	hInst = hInstance;     
    hwnd = CreateWindow (szAppName, szAppName,
                          WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          NULL, NULL, hInst, NULL) ;
	if (!hwnd) 
	{
		fprintf(ofile, "Failed to start IDEXTER.\n");
		return 0; 
	}

	lErrorNo = 4001;
	ShowWindow (hwnd, SW_HIDE) ;
	UpdateWindow (hwnd) ;

	//---------------------------------------------------------------------------
	ServiceLogging("Starting Timer");
	//---------------------------------------------------------------------------

	/* Set Timer Properties
	 */
	iWaitSecs = 4;
	uiTimer = (UINT)(iWaitSecs * 1000);
	if (!SetTimer(hwnd, SHUTDOWN_TIMER, uiTimer, 0L))
	{
		sprintf(szErrorMessage, "Unable to set Windows Timer.");
		MessageBox(NULL, szErrorMessage, szAppName, MB_OK);
		return 0;
	}

	while (GetMessage (&msg, NULL, 0, 0))
	{
		TranslateMessage (&msg) ;
		DispatchMessage (&msg) ;
	}
	return msg.wParam ;
}

/************************************************************************************
 *
 ************************************************************************************
 */
LRESULT CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     
     switch (message)
     {
     case WM_CREATE:      
			return 0 ;

 	 case WM_TIMER:
			KillTimer(hwnd, SHUTDOWN_TIMER);
			PostQuitMessage(0);
			return 0;
         
     case WM_DESTROY:
			PostQuitMessage (0) ;
			return 0 ;
     }
     return DefWindowProc (hwnd, message, wParam, lParam) ;
}

/**************************************************************************
 * GetMyDirectory()
 * 
 * PURPOSE:
 *		Find my current directory 
 *
 **************************************************************************
 */
 int GetMyDirectory()
 {
//	char szCurrentPath[MAX_PATH];
	char szPath[MAX_PATH];
	char *pdest;
	int  ch = '\\';
	int iLocation;
//    OFSTRUCT	of;
	int	 fh = 0;

	if(!GetModuleFileName(NULL, szPath, MAX_PATH))
	{
		// Continue error
	}
	else
	{
		pdest = strrchr(szPath, ch );
		iLocation = (int)(pdest - szPath + 1);
		strncpy(szAppPath, szPath, iLocation);

/*
		strcpy(szCurrentPath, szAppPath);
		strcat(szCurrentPath, INI_FILENAME);

*/
		/* Check for the existence of the INI file on the current path
		 */

/*
		fh = OpenFile (szCurrentPath, &of, OF_EXIST);
		if(fh == -1)
		{
			return 1;
		}
*/
		return 0;
	}

	return 1;
 }

/*********************************************************************************************
 *
 *********************************************************************************************
 */
int IsOPSValid()
{
	HKEY	hhKey;
	DWORD	dwBufLen=100;
	LONG	lRet;
	char	szParameters[10];
	double	fVersion = 0;
	
	lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
						0,
						KEY_QUERY_VALUE,
						&hhKey );
	if( lRet != ERROR_SUCCESS )
	{
		return 1;
	}

	lRet = RegQueryValueEx(hhKey,
							"CurrentVersion",
							NULL,
							NULL,
							(LPBYTE) szParameters,
							&dwBufLen);
	RegCloseKey( hhKey );
	if( (lRet != ERROR_SUCCESS) || (dwBufLen > BUFSIZE) )
	{
		return 2;
	}

	fVersion = atof(szParameters);
	if(fVersion > 5.0)
	{
		return 0;
	}

	return 3;
}

/***********************************************************************
 *
 ***********************************************************************
 */
int DeleteEventReportIDEXTER()
{
    LONG lResult;

    lResult = RegDeleteKey(HKEY_LOCAL_MACHINE,
						   "SYSTEM\\CurrentControlSet\\Services\\Eventlog\\Application\\IDEXStartupService");
	return 0;
}

/***********************************************************************
 *
 ***********************************************************************
 */
 int GetMyCurrentDir()
 {
	char szPath[MAX_PATH];
	char *pdest;
	int  ch = '\\';
	int iLocation;

	if(!GetModuleFileName(NULL, szPath, MAX_PATH))
	{
		// Continue error
	}
	else
	{
		pdest = strrchr(szPath, ch );
		iLocation = (int)(pdest - szPath + 1);
		strncpy(szAppPath, szPath, iLocation);
		return 0;
	}
	return 1;
 }

/*****************************************************************
 *
 *****************************************************************
 */
int EventReportSetIDEXTER(char szIDEXTERSrv[])
{

	char szPath[200];

	HKEY hhKey;
	DWORD dwBufLen=100;
	LONG lRet;
	DWORD dwDisp;
	DWORD dwData;
	HKEY hk;
	int	iFound;

	char szParameters[200];
	LPCTSTR lpszParameters;

	strcpy(szPath, szIDEXTERSrv);

	iFound = 1;

	lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						"SYSTEM\\CurrentControlSet\\Services\\Eventlog\\Application\\IDEXStartupService",
						0,
						KEY_QUERY_VALUE,
						&hhKey );
	if( lRet != ERROR_SUCCESS )
	{
		iFound = 0;
	}

	if(iFound == 0)
	{
		if (RegCreateKeyEx(HKEY_LOCAL_MACHINE,
						"SYSTEM\\CurrentControlSet\\Services\\Eventlog\\Application\\IDEXStartupService",
						0,
						NULL,
						REG_OPTION_NON_VOLATILE,
						KEY_WRITE,
						NULL,
						&hk,
						&dwDisp)) 
		{
			return 4;
		}
 
		strcpy(szParameters, szPath);
		lpszParameters = szParameters;

		if (RegSetValueEx(hk,						// subkey handle 
			   "EventMessageFile",					// value name 
			   0,									// must be zero 
			   REG_EXPAND_SZ,						// value type 
			   (LPBYTE) lpszParameters,				// pointer to value data 
			   (DWORD) lstrlen(lpszParameters)+1))	// length of value data 
		{
		  RegCloseKey(hk); 
		  return 5;
		}

		dwData = EVENTLOG_SUCCESS | EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE; 
 
		if (RegSetValueEx(hk,					// subkey handle 
			   "TypesSupported",				// value name 
			   0,								// must be zero 
			   REG_DWORD,						// value type 
			   (LPBYTE) &dwData,				// pointer to value data 
			   sizeof(DWORD)))    				// length of value data 
		{
		  RegCloseKey(hk); 
		  return 6;
		}

	}


	return 0;
}

/*********************************************************************************************
 *
 *********************************************************************************************
 */
int CreateServiceIDEXTER(int iDEXTERType, char szDEXTERPath[])
{

	char szPath[200];
	char szSrv[200];

	int iReturn;
	int iSuccess = 0;

	SC_HANDLE schSCManager;
	SC_HANDLE schService;

	char szServiceName[200];
	char szDisplayName[200];
	char szServiceExe[200];
	char szGroupID[200];

	LPCTSTR lpszServiceName;
	LPCTSTR lpszDisplayName;
	LPCTSTR lpszServiceExe;
	LPCTSTR lpszGroupID;

	DWORD dwBufLen=100;

	//---------------------------------------------------------------------------------------------------------------
	sprintf_s(szErrorMessage, sizeof(szErrorMessage), "iDEXTERType:%d - szDEXTERPath:%s", iDEXTERType, szDEXTERPath);
	ServiceLogging(szErrorMessage);
	//---------------------------------------------------------------------------------------------------------------

	strcpy(szPath, szDEXTERPath);
	if( *(szPath + strlen(szPath) - 1) != '\\')
	{
		strcat(szPath, "\\");
	}

	strcat(szPath, "IDEXSService.exe");

	strcpy(szSrv, szPath);

	if(iDEXTERType == 101)
	{
		strcat(szSrv, " -b");
	}
	else if(iDEXTERType == 110)
	{
		strcat(szSrv, " -p");
	}
	else
	{
		iSuccess = 0;
		return iSuccess;

	}
	//----------------------------------------------------------------------------------------
	sprintf_s(szErrorMessage, sizeof(szErrorMessage), "IDEXSService Parameters(): %s", szSrv);
	ServiceLogging(szErrorMessage);
	//----------------------------------------------------------------------------------------

	strcpy(szServiceName, "IDEXStartupService");
	lpszServiceName = szServiceName;

	strcpy(szDisplayName, "IDEXTER Service");
	lpszDisplayName = szDisplayName;

	strcpy(szServiceExe, szSrv);
	lpszServiceExe = szServiceExe;

	strcpy(szGroupID, "IDEXTERSServer");
	lpszGroupID = szGroupID;

	schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
	if (schSCManager == NULL)
	{
		iSuccess = 0;
	}
	else
	{
		//-------------------------------------------------------------------
		sprintf_s(szErrorMessage, sizeof(szErrorMessage), "CreateService()");
		ServiceLogging(szErrorMessage);
		//-------------------------------------------------------------------

		schService = CreateService(schSCManager,
									lpszServiceName,
									lpszDisplayName,
									SERVICE_ALL_ACCESS,
									SERVICE_WIN32_OWN_PROCESS,
									SERVICE_AUTO_START,
									SERVICE_ERROR_NORMAL,
									lpszServiceExe,
									lpszGroupID,
									NULL,
									NULL,
									NULL,
									NULL); 
		if (schService == NULL)
		{
			iSuccess = 1;

			//---------------------------------------------------------------------------------------------------------------
			sprintf_s(szErrorMessage, sizeof(szErrorMessage), "Error:%d", GetLastError());
			ServiceLogging(szErrorMessage);
			//---------------------------------------------------------------------------------------------------------------

			CloseServiceHandle(schService); 
			return iSuccess;
		}
		else
		{
			iSuccess = 0;
		}
		
		CloseServiceHandle(schService); 
		CloseServiceHandle(schSCManager);

		iReturn = EventReportSetIDEXTER(szPath);
	}
	return iSuccess;
}

 /**********************************************************************************
 *
 **********************************************************************************
 */
int StopServiceIDEXTER()
{
	SC_HANDLE schSCManager;
	SC_HANDLE hService;
	int iSuccess = 0;

	schSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if (schSCManager == NULL)
	{
		iSuccess = 0;
	}
	else
	{
		hService=OpenService(schSCManager,"IDEXStartupService",SERVICE_ALL_ACCESS);
		if (hService == NULL)
		{
			iSuccess = 0;
		}
		else
		{
			SERVICE_STATUS status;
			if(ControlService(hService,SERVICE_CONTROL_STOP,&status))
			{
				iSuccess = 1;
			}
			else
			{
				iSuccess = 0;
			}
			CloseServiceHandle(hService);
		}
		CloseServiceHandle(schSCManager);	
	}
	return iSuccess;

}
/**********************************************************
 *
 **********************************************************
 */
int DeleteServiceIDEXTER()
{

	SC_HANDLE schSCManager;
	SC_HANDLE hService;
	int iSuccess = 0;

	schSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if (schSCManager == NULL)
	{
		iSuccess = 1;
	}
	else
	{
		hService=OpenService(schSCManager,"IDEXStartupService",DELETE);
		if (hService == NULL)
		{
			iSuccess = 1;
		}
		else
		{
			if(DeleteService(hService)==0)
			{
				iSuccess = 1;
			}
			else
			{
				iSuccess = 0;
			}
			CloseServiceHandle(hService);
		}
		CloseServiceHandle(schSCManager);	
	}

	iSuccess = DeleteEventReportIDEXTER();

	return iSuccess;
}
