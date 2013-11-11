/*
  CMDLN - All Rights Reserved. Copyright (C) 2010-2013 Lai KamLeong
  Inspired by Bayden's SlickRun [http://www.bayden.com/slickrun/]

  Re-using some sample codes at
  http://www.rohitab.com/discuss/topic/34879-getting-text-from-edit-box/

  compile with MinGW/gcc/g++:
  g++.exe -static-libgcc -mwindows cmdln.cpp -lwininet -lpsapi -o cmdln.exe
*/

#define SMALL_STACK

/*
 http://en.wikibooks.org/wiki/Windows_Programming/Unicode 
 If you want to use unicode in your program, you need to explicitly
 define the UNICODE macro before you include the windows.h file
*/
#define UNICODE

#include <windows.h>
#include <strings.h>
#include <stdio.h>

#include <time.h>
#include <wininet.h>
#include <psapi.h>

#ifdef UNICODE
 /*
  some functions in other libraries, e.g. <tchar.h>, require you to 
  define the macro _UNICODE to provide functions in unicode
 */
 #define _UNICODE
 #include <tchar.h>
// #define _tprintf   wprintf
// #define _vtprintf  vwprintf
// #define _stprintf  swprintf
// #define _vstprintf vswprintf
// #define _vftprintf vfwprintf
// #define _tfopen    wfopen
// #define _tcscpy    wcscpy
// #define _tcscat    wcscat
// #define _tcsstr    wcsstr
// #define _tcslen    wcslen
// #define _tcsncpy   wcsncpy
// #define _tcscmp    wcscmp
// #define _ttoi      _wtoi
// #define _totlower  towlower
#else
 #include <ctype.h>
 #define _tprintf   printf
 #define _vtprintf  vprintf
 #define _stprintf  sprintf
 #define _vstprintf vsprintf
 #define _vftprintf vfprintf
 #define _tfopen    fopen
 #define _tcscpy    strcpy
 #define _tcscat    strcat
 #define _tcsstr    strstr
 #define _tcslen    strlen
 #define _tcsncpy   strncpy
 #define _tcscmp    strcmp
 #define _ttoi      atoi
 #define _totlower  tolower
#endif

#define IDC_MAIN_EDIT        101
#define IDC_MAIN_EDIT        101
#define IDC_MAIN_TOOL        102
#define IDC_MAIN_STATUS      103

typedef struct STRING_LIST{
  struct STRING_LIST *next;
  TCHAR *val;
} STRING_LIST;

const TCHAR g_szAppName[]        = TEXT("cmdln");
const TCHAR g_szClassName[]      = TEXT("myCmdlnWindowClass");
const int   g_intBufSz           = 1024;
const int   g_intConSz           = 80*25;
const int   g_intDefWidth        = 200;
const int   g_intDefHeight       = 20;
const int   g_intOpaq            = 229; // 0..255; 0==transparent

int      g_debug=0, g_intDockPos=0, g_intExecMethod=1; /*0=CreateProcess, 1=ShellExecuteEx, 2=ShellExecute*/
TCHAR    *g_szModuleFileName=NULL, *g_szModuleParam=NULL;
TCHAR    *g_szInitCfg=NULL, *g_szLastCmdln=NULL, *g_szExtLauncher=NULL;
BOOL     g_boolExtLauncher=false, g_boolTestNoRun=false;

HWND hMain, hEdit;  WNDPROC wndProc0;
HANDLE hThread, hEventActive=CreateEvent(NULL, FALSE,FALSE,NULL);

/*--------------------------------------------------------------------*/

void freeStr(TCHAR* p) {
  if (p!=NULL) free(p);
  return;
}

TCHAR* mallocNewStr(TCHAR* oldstr) {
  DWORD dwSz=0, i=0;
  TCHAR *newbuf=NULL;
  if (oldstr!=NULL) {
    dwSz=_tcslen(oldstr);
    newbuf=(TCHAR*)malloc(sizeof(TCHAR)*(dwSz+1));
    _tcscpy(newbuf, oldstr);
  }
  return newbuf;
}

TCHAR* xstrcat(int count, ...) {
  va_list args;  DWORD dwSz=1;  TCHAR *strN;

  va_start(args,count);
  for(int i=1; i<=count; i++) {
    strN = va_arg(args, TCHAR*);
    if (strN!=NULL) dwSz+=_tcslen(strN);
  }
  va_end(args);

  dwSz*=sizeof(TCHAR);
  TCHAR *newbuf=(TCHAR*)malloc(dwSz); memset(newbuf, 0, dwSz);

  va_start(args,count);
  for(int i=1; i<=count; i++) {
    strN = va_arg(args, TCHAR*);
    if (strN!=NULL) _tcscat(newbuf, strN);
  }
  va_end(args);

  return newbuf;
}

void xprintf(TCHAR* &buf, DWORD newsz, const TCHAR* arg0, ...) {
  // example:  TCHAR* txt;  xprintf(txt, 100, TEXT("Line 1: [%d]\n"), 1);  printstdout(txt);
  if (newsz < 1) {
    MessageBox(NULL, TEXT("cmdln:xprintf()"), TEXT("xprintf() failed. [newsz<1]\n"), MB_OK);
    return;
  }
  TCHAR *newdata=(TCHAR*)malloc(sizeof(TCHAR)*(newsz+1));
  va_list args; va_start(args,arg0);
  _vstprintf(newdata, arg0, args);
  va_end(args);

  TCHAR *newbuf=xstrcat(2, buf, newdata);
  free(newdata); if (buf!=NULL) free(buf);  newdata=0; buf=0;
  buf=newbuf;
  return;
}

/*--------------------------------------------------------------------*/

DWORD getTotalHandle() {
 /*
  SYSTEM_INFORMATION_CLASS SystemInformationClass;
  SYSTEM_PROCESS_INFORMATION SystemInformation;
  ULONG ReturnLength;
  NtQuerySystemInformation(
    SystemInformationClass.SystemProcessInformation, &SystemInformation, 
    sizeof(SystemInformation), &ReturnLength
  );
 */
 #if defined(__MINGW64__) && _WIN32_WINNT >= 0x0500
  PERFORMANCE_INFORMATION PerformanceInformation;
  if ( !GetPerformanceInfo(&PerformanceInformation, sizeof(PERFORMANCE_INFORMATION)) )
    return -1;
  return PerformanceInformation.HandleCount;
 #else
  return -1;
 #endif
}

DWORD getTotalMemAlloc() {
  DWORD dwSz=-1;
  #if defined(__MINGW64__)
    static HANDLE h;  static PROCESS_MEMORY_COUNTERS_EX m;
    memset(&m, 0, sizeof(m));  m.cb=sizeof(m);
    h = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, GetCurrentProcessId());
    if ( GetProcessMemoryInfo(h, (PROCESS_MEMORY_COUNTERS*)&m, sizeof(m)) ) dwSz=m.PrivateUsage;
    CloseHandle(h);
  #endif
  return dwSz;
}

BOOL copyText(TCHAR* data) {
  BOOL bSuccess=true;
  #ifdef UNICODE
   UINT uFormat=CF_UNICODETEXT;
  #else
   UINT uFormat=CF_TEXT;
  #endif
  int dataSz=sizeof(TCHAR)*(_tcslen(data)+1); HGLOBAL hMem=GlobalAlloc(GMEM_MOVEABLE,dataSz); 
  memcpy(GlobalLock(hMem), data, dataSz); GlobalUnlock(hMem);
  OpenClipboard(NULL); //EmptyClipboard();
  if ( SetClipboardData(uFormat, hMem)==NULL ) bSuccess=false;
  //h=SetClipboardData(..); CloseHandle(h);??
  CloseClipboard();
  return bSuccess;
}

BOOL setMyWindowPos(HWND hWnd, HWND hWndInsertAfterZorder, int mypos) {
  bool  boolDone=false;
  int   intX=-1, intY=-1, left, right, top, bottom, midX, midY;
  LONG  exStyle;
  RECT  mainRect; GetWindowRect(hWnd, &mainRect);
  RECT  desktopRect; // GetWindowRect(GetDesktopWindow(), &desktopRect);  //!! only gets relative coordinates !!//

  SystemParametersInfo(SPI_GETWORKAREA, 0, &desktopRect, 0);  //!! get **absolute** coordinates !!//
  right=desktopRect.right-(mainRect.right-mainRect.left);  left=desktopRect.left;  midX=(right-left)/2;
  bottom=desktopRect.bottom-(mainRect.bottom-mainRect.top)-3;  top=desktopRect.top+3;  midY=(bottom-top)/2;

  if (mypos==0 || mypos==360) {                      //mid-top
    intX=midX;   intY=top;
  } else if (mypos==1 || mypos==045) {               //top-right
    intX=right;  intY=top;
  } else if (mypos==2 || mypos==135) {               //lower-right
    intX=right;  intY=bottom;
  } else if (mypos==3 || mypos==180) {               //mid-bottom
    intX=midX;   intY=bottom;
  } else if (mypos==4 || mypos==225 || mypos==-2) {  //lower-left
    intX=left;   intY=bottom;
  } else if (mypos==5 || mypos==315 || mypos==-1) {  //top-left
    intX=left;  intY=top;
  } else if (mypos==9 || mypos==315) {               //mid-center
    intX=midX;  intY=midY;
  }

  if (intX>=0 && intX<desktopRect.right && intY>=0 && intY<desktopRect.bottom) {
    boolDone = SetWindowPos(hWnd, hWndInsertAfterZorder, intX,intY, 0,0, SWP_NOSIZE);
    if (boolDone) g_intDockPos=mypos;
  }
  if (!boolDone && hWndInsertAfterZorder==HWND_TOPMOST) {
    exStyle = GetWindowLong(hWnd,GWL_EXSTYLE)|WS_EX_TOPMOST;
    SetWindowLong(hWnd, GWL_EXSTYLE, exStyle);
  }
  return boolDone;
}

/*--------------------------------------------------------------------*/

DWORD setAutoRun(TCHAR* param) {
  DWORD dwReturnVal;
  HKEY hkey;
  dwReturnVal = RegCreateKeyEx( HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
    0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkey, NULL
  );
  if ( dwReturnVal==ERROR_SUCCESS ) {
    dwReturnVal = RegSetValueEx(hkey, TEXT("_cmdln"), 0, REG_SZ, (BYTE*)param, sizeof(TCHAR)*_tcslen(param));
    RegCloseKey(hkey);
  }
  return dwReturnVal;
}

DWORD setIEShowPicture(TCHAR* param) {
  DWORD dwReturnVal;
  HKEY hkey;
  dwReturnVal = RegCreateKeyEx( HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Internet Explorer\\Main"),
    0, NULL, REG_OPTION_VOLATILE /*!only apply in memory!*/, KEY_WRITE, NULL, &hkey, NULL
  );
  if ( dwReturnVal==ERROR_SUCCESS ) {
    dwReturnVal = RegSetValueEx(hkey, TEXT("Display Inline Images"), 0, REG_SZ, (BYTE*)param, sizeof(TCHAR)*_tcslen(param));
    RegCloseKey(hkey);
    DWORD *dwResult; SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, 0, SMTO_NORMAL, 200, (PDWORD_PTR)&dwResult);
    //SendMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0, 0); //somehow this hangs the program in some odd cases
    free(dwResult);
  }
  return dwReturnVal;
}

DWORD WINAPI displayCurrentDateTime(LPVOID lpParam) {
  //#include <time.h>
  TCHAR *txtln=NULL;
  time_t timeval0=0, timeval1=0;

  ResetEvent(hEventActive);
  while ( true ) {
    timeval0=timeval1;
    timeval1=time(NULL);
    if ( timeval0 != timeval1 ) {
      struct tm *timeinfo = localtime(&timeval1);
      //_stprintf(txtln, TEXT("%d/%d/%d %02d:%02d:%02d"),
      //  timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_year+1900,
      //  timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec
      //);
      txtln=NULL;  xprintf(txtln, 255, TEXT("%02d:%02d:%02d"),
        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec
      );
      /*-- how to avoid flickering? ExcludeClipRec?? --*/
      //SetDlgItemText(hMain, IDC_MAIN_EDIT, txtln);
      SetWindowText(hEdit, txtln);  free(txtln);  txtln=0;
      SendMessage(hEdit, EM_SETSEL, GetWindowTextLength(hEdit), -1); // move caret to end
    }
    if ( WaitForSingleObject(hEventActive, 900)!=WAIT_TIMEOUT ) ExitThread(0);
  }
  return 0;
}

/*--------------------------------------------------------------------*/

BOOL CtrlHandler(DWORD fdwCtrlType) {
  /*
   Initially, the list of control handlers for each process contains 
   only a default handler function that calls the ExitProcess function.

   When a console process receives any of the control signals, it calls
   the handler functions on a last-registered, first-called basis until
   one of the handlers returns TRUE.

   If none of the handlers returns TRUE, the default handler is called.
   There is no guarantee the signal would arrive before the notification 
   from WM_QUERYENDSESSION
  */
  switch ( fdwCtrlType ) { 
    case CTRL_C_EVENT: // Handle the CTRL-C signal.
      FreeConsole();
      return TRUE;
    case CTRL_CLOSE_EVENT:
      /*
       !! Not possible to intercept/abort/cancel this !!
       unlike when handling CTRL+C or CTRL+BREAK events,
       for CTRL_CLOSE_EVENT, CTRL_LOGOFF_EVENT, and CTRL_SHUTDOWN_EVENT,
       your process does NOT get the opportunity to cancel the close,
       logoff, or shutdown

       Console functions, or any C run-time functions that call console 
       functions, may not work reliably during processing of any of the 
       three signals mentioned previously. The reason is that some or all 
       of the internal console cleanup routines may have been called before 
       executing the process signal handler.
      */
      return TRUE;
    default: 
      // Pass other signals (CTRL_BREAK_EVENT,CTRL_LOGOFF_EVENT,CTRL_SHUTDOWN_EVENT)
      //  to the next handler ==> return FALSE;
      return FALSE; 
  } 
} 

HWND _GetConsoleWindow() {
  //GetConsoleWindow() only available for _WIN32_WINNT >= 0x0500
  //for compatibility with Windows 95/98/Me/NT4 we use FindWindow() instead
  TCHAR *consoleTitle;
  int txtlen=0, bufsz=255; do {
    if (txtlen>0) bufsz=sizeof(TCHAR)*(txtlen+1);  consoleTitle=(TCHAR*)malloc(bufsz);
    txtlen = GetConsoleTitle(consoleTitle, bufsz);
    if (txtlen==0) {
      free(consoleTitle);  consoleTitle=0;
      return NULL;
    }
  } while (txtlen > bufsz);

  HWND hwnd = FindWindow(TEXT("ConsoleWindowClass"), consoleTitle);
  free(consoleTitle);  consoleTitle=0;

  ShowWindow(hwnd, SW_RESTORE); SetForegroundWindow(hwnd);
  //will SetForegroundWindow(hMain) in getstdout()
  return hwnd; 
}

HANDLE getstdout() {
  if (_GetConsoleWindow()==NULL) {
    //MessageBox(NULL, TEXT("AllocConsole()"), g_szAppName, MB_OK);
    AllocConsole(); /*AttachConsole( (DWORD)-1 ); /*ATTACH_PARENT_PROCESS */

    SetConsoleTitle(TEXT("cmdln console output - Ctrl-C to close this console window."));
    // if we GetConsoleTitle immediately, it will be still having the old title !!
    Sleep(40); //<--is there a better way to make SetConsoleTitle() effective immediately?

    //SetWindowLongPtr(_GetConsoleWindow(), GWLP_WNDPROC, (LONG_PTR)WndProc3); -- not working !!
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

    HMENU hMenu=GetSystemMenu(_GetConsoleWindow(), FALSE);
    if (hMenu!=NULL) {
      EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND|MF_GRAYED);
      RemoveMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
    }
    //ENABLE_QUICK_EDIT_MODE
    DWORD conmode; HANDLE hCon=GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hCon,&conmode); SetConsoleMode(hCon,conmode|0x0040);
    CloseHandle(hCon);

    //enables printf
    freopen("CONOUT$", "w", stdout);
    /*
     -- a more complicated way to do that -- http://www.halcyon.com/~ast/dload/guicon.htm
     -- #include <fcntl.h>  #include <io.h>
     FILE *fp=_fdopen( _open_osfhandle( (intptr_t)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT ), "w" );
     *stdout=*fp;  setvbuf(stdout, NULL, _IONBF, 0);
    */
  }
  SetForegroundWindow(hMain); // always activates main window, although we are printing to the console
  return GetStdHandle(STD_OUTPUT_HANDLE);
}

void printstdout(TCHAR* s) {
  //HANDLE con=getstdout(); if (con!=NULL) WriteConsole(con,s,_tcslen(s),NULL,NULL);
  if (getstdout()!=NULL) _tprintf(TEXT("%s"), s);
  fflush(stdout); // this ensures printf works even in some odd cases
  return;
}

void printstdoutA(const char* arg0, ...) {
  //TUTORIAL: Writing a Custom printf() Wrapper Function in C
  //http://www.ozzu.com/cpp-tutorials/tutorial-writing-custom-printf-wrapper-function-t89166.html
  va_list args;  va_start(args,arg0);
  if (getstdout()!=NULL) vprintf(arg0, args);
  va_end(args);
  return;
}

/*--------------------------------------------------------------------*/

void dbgmsg(const TCHAR* arg0, ...) {
  if (g_debug==0) return;
  va_list args;  va_start(args,arg0);
  if (getstdout()!=NULL) {
    _vtprintf(arg0, args); // _vftprintf(stdout, arg0, args);
    if (g_debug>=2) printstdoutA("PrivateUsage: %d K\n", getTotalMemAlloc()/1024);
  } else {
    TCHAR *t=(TCHAR*)malloc(g_intBufSz); 
    _vstprintf(t, arg0, args);
    MessageBox(NULL, t, g_szAppName, MB_OK);
    free(t); t=0;
  }
  if (g_debug>=9) {
    TCHAR *lpFileName=xstrcat(2, g_szModuleFileName, TEXT(".log"));
    FILE *f=_tfopen(lpFileName, TEXT("a"));
    if (f != NULL)  {
      _vftprintf(f, arg0, args);
      fclose(f);
    //} else {
    //  printstdoutA("Failed on fopen ["); printstdout(lpFileName); printstdoutA("]\n");
    }
    free(lpFileName);
  }
  va_end(args);
}

/*--------------------------------------------------------------------*/

// SEE ALSO http://stackoverflow.com/questions/2448802/chaining-multiple-shellexecute-calls
// string Params=""; for(int i=2;i<argc;++i)Params+=string(argv[i])+' '; shExecInfo.lpParameters=Params.c_str();

int launchProg(int execMethod, TCHAR *cmd, TCHAR *param) {
 dbgmsg(TEXT(">launchProg:begin[%s][%s]\n"), cmd, param);
 dbgmsg(TEXT(">g_boolTestNoRun[%d]execMethod[%d]\n"), g_boolTestNoRun, execMethod);
  int returnVal=0;
  TCHAR *param1=NULL;

  if ( execMethod < 0 ) {

    param1 = xstrcat(4, TEXT("\""), cmd, TEXT("\" "), param);
    SetDlgItemText(hMain, IDC_MAIN_EDIT, param1);
 dbgmsg(TEXT(">No Run\n"));
    returnVal = -1;

  } else if ( execMethod == 0 ) {

    // !! CreateProcess should be faster and uses much less memory compared to ShellExecute/ShellExecuteEx !!
    // !! However it needs additional handling to support console programs and other ShellExecute features !!

    static STARTUPINFO si = { sizeof(STARTUPINFO) };
    static PROCESS_INFORMATION pi;
    param1 = xstrcat(4, TEXT("\""), cmd, TEXT("\" "), param);
    si.dwFlags = STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;
    si.wShowWindow = SW_SHOW;
 dbgmsg(TEXT(">CreateProcess:ready[%s]\n"), param1);
    if ( CreateProcess(NULL, param1, 0, 0, 1, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)==0 ) {
      returnVal = GetLastError();
    } else {
      CloseHandle(pi.hThread);  CloseHandle(pi.hProcess);
      returnVal = 0;
    }
 dbgmsg(TEXT(">CreateProcess:done[%d]\n"), returnVal);

  } else if ( execMethod == 2 ) { // ** ShellExecute()

    int nShow=SW_SHOW;
    if ( _tcsstr(cmd,TEXT("-"))==cmd ) { nShow=SW_MINIMIZE; cmd++; }
    if ( _tcsstr(cmd,TEXT("*"))==cmd ) { cmd++; }

 dbgmsg(TEXT(">ShellExecute:ready\n"));
    // SEE ALSO http://undoc.airesoft.co.uk/shell32.dll/RunAsNewUser_RunDLLW.php
    // HMODULE hShell=LoadLibrary(L"shell32.dll"); FreeLibrary(hShell);
    // SEE ALSO http://stackoverflow.com/questions/1851267/mingw-gcc-delay-loaded-dll-equivalent
    // !! "delay-load" or "ld -z lazy" is not supported on MinGW !!

    HINSTANCE hInst=ShellExecute(hMain, TEXT("open"), cmd, param, NULL, nShow);
    returnVal=*((int*)(&hInst));  // cast HINSTANCE to int if one does not mind losing data precision
 dbgmsg(TEXT(">ShellExecute():done[%d]\n"), returnVal);

    if (returnVal>32) returnVal=0;
    return returnVal;

  } else {

    static SHELLEXECUTEINFO ExecInf;  // use static to avoid new memory allocation for each call
     ExecInf.cbSize = sizeof(SHELLEXECUTEINFO);
     ExecInf.hwnd = hMain;
      if ( _tcsstr(cmd,TEXT("-"))==cmd ) { ExecInf.nShow=SW_MINIMIZE; cmd++; } else
     ExecInf.nShow = SW_SHOW;
     ExecInf.fMask = SEE_MASK_DOENVSUBST; /* expand env variables */
     ExecInf.lpDirectory = NULL;
      if ( _tcsstr(cmd,TEXT("*"))==cmd ) { ExecInf.lpVerb=TEXT("runas"); cmd++; } else
     ExecInf.lpVerb = TEXT("open");
      if ( _tcsstr(cmd,TEXT("http://"))==cmd && ExecInf.nShow==SW_MINIMIZE ) setIEShowPicture((TCHAR*)TEXT("no"));
     ExecInf.lpFile = cmd;
     ExecInf.lpParameters = param;

    /*flash to signal we are ready for ShellExecuteEx*/
    if ( IsWindowVisible(hMain) ) {
      ShowWindow(hMain, SW_HIDE);  UpdateWindow(hMain);
       Sleep(20); ShowWindow(hMain, SW_SHOW);  UpdateWindow(hMain);
    }

    /*another approach for runas, pass it to runGUI*/
    if ( _tcscmp(ExecInf.lpVerb,TEXT("runas"))==0 ) {
      ExecInf.lpVerb = TEXT("open");  ExecInf.lpFile = TEXT("runGUI");
      param1 = xstrcat(4, TEXT("/adm "), cmd, TEXT(" "), param);
      ExecInf.lpParameters = param1;
    }

   dbgmsg(TEXT(">ShellExecuteEx:ready\n"));
    if ( g_debug>=0 && !ShellExecuteEx(&ExecInf) ) {
      // Windows cannot find '<???>'. Make sure you typed the name correctly, and then try again.
      dbgmsg(TEXT(">ShellExecuteEx() failed!\n"));
      returnVal=1;
    } else {
      CloseHandle(ExecInf.hProcess); // actually n/a unless SEE_MASK_NOCLOSEPROCESS
   dbgmsg(TEXT(">ShellExecuteEx:done\n"));
    }

  } // ShellExecuteEx

  freeStr(param1);
  return returnVal;
} // launchProg

/*--------------------------------------------------------------------*/

int execConsoleProcess(TCHAR* p_cmd, TCHAR* param, TCHAR* outfile) {
  int returnVal=0;
  TCHAR *pAddr=NULL, *cmd;

  if (p_cmd==NULL||p_cmd[0]=='\0') {
    return 1;
  } else {
    pAddr=cmd=mallocNewStr(
      ( _tcscmp(p_cmd,TEXT("cmd"))==0 ) ?
       (TCHAR*)TEXT("%comspec%") : p_cmd
    );
  }

  TCHAR *cmd1=(TCHAR*)malloc(g_intBufSz); 
  returnVal=ExpandEnvironmentStrings(cmd,cmd1,g_intBufSz);
  if (returnVal==0 || returnVal>=g_intBufSz) {
    free(cmd1); // not success
  } else {
    freeStr(pAddr); pAddr=cmd=cmd1;
  }

  cmd=xstrcat(4, TEXT("\""), cmd, TEXT("\" "), param);
  freeStr(pAddr); pAddr=cmd;
dbgmsg(TEXT(">CreateProcess:begin[%s]\n"), cmd);

  HANDLE hReadPipe1, hWritePipe1;
  SECURITY_ATTRIBUTES sa;
   sa.nLength=sizeof(sa); sa.lpSecurityDescriptor=0; sa.bInheritHandle=true;
   if ( !CreatePipe(&hReadPipe1,&hWritePipe1,&sa,0) ) {
     MessageBox(hMain, cmd, TEXT("CreatePipe() failed!"), MB_OK|MB_ICONEXCLAMATION);
     freeStr(pAddr);
     return 1;
   }
  PROCESS_INFORMATION pi;
  STARTUPINFO si = { sizeof(STARTUPINFO) };
   si.dwFlags = STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;
   si.wShowWindow = SW_SHOW; /*hide console with dwCreationFlags instead*/
   si.hStdError = hWritePipe1;
   si.hStdOutput = hWritePipe1;

  /*flash to signal we are ready for CreateProcess*/
  if ( IsWindowVisible(hMain) ) {
    ShowWindow(hMain, SW_HIDE);  UpdateWindow(hMain);
     Sleep(20); ShowWindow(hMain, SW_SHOW);  UpdateWindow(hMain);
  }

  returnVal = CreateProcess(NULL, cmd, 0, 0, 1, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
  if (returnVal==0 && _tcsstr(p_cmd,TEXT("cmd"))!=p_cmd) {
    cmd1=(TCHAR*)malloc(g_intBufSz); 
    returnVal=ExpandEnvironmentStrings((TCHAR*)TEXT("%comspec%"),cmd1,g_intBufSz);
    if (returnVal==0 || returnVal>=g_intBufSz) {
      returnVal=0; // not success
    } else {
      cmd=xstrcat(4, TEXT("\""), cmd1, TEXT("\" /c "), cmd);
      free(cmd1); freeStr(pAddr); pAddr=cmd;
      returnVal = CreateProcess(NULL, cmd, 0, 0, 1, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    }
  }
  if (returnVal == 0) {
    MessageBox(hMain, cmd, TEXT("CreateProcess() failed!"), MB_OK|MB_ICONEXCLAMATION);
    CloseHandle(hReadPipe1);  CloseHandle(hWritePipe1);
    freeStr(pAddr);
    return 1;
  }
  printstdoutA("[>] "); printstdout(cmd); printstdoutA("\n");

  FILE *f=NULL;
  if ( outfile!=NULL ) {
    f=_tfopen(outfile, TEXT("w"));
    if (f==NULL) { printstdoutA("Failed on fopen ["); printstdout(outfile); printstdoutA("]\n"); }
  }

  DWORD dwExitCode = STILL_ACTIVE;
  char Buff[g_intConSz];
  unsigned long lBytesRead;
  do {
    returnVal= GetExitCodeProcess(pi.hProcess, &dwExitCode);
    returnVal = PeekNamedPipe(hReadPipe1,Buff,g_intConSz,&lBytesRead,0,0);
    if (!returnVal) break;
    if (lBytesRead) {
      returnVal=ReadFile(hReadPipe1,Buff,lBytesRead,&lBytesRead,0);
      if (!returnVal || returnVal<=0) break;
      Buff[lBytesRead] = '\0';
      if (f!=NULL) {
        for (int i=0; i<lBytesRead; i++)
         if (Buff[i]=='\x0D') Buff[i]=' ';
        fwrite(Buff, 1, lBytesRead, f);  //fputs(Buff, f);
      } else printstdoutA("%s", Buff);
    }
    Sleep(100);
  } while(dwExitCode == STILL_ACTIVE || lBytesRead > 0);
  printstdoutA("[>] \n"); // $bourne, $korn, $bash, %c, #root, c>

  if (f!=NULL) {
    fclose(f);
    ShellExecute(hMain, TEXT("open"), TEXT("notepad"), outfile, NULL, SW_SHOW);
  }

  CloseHandle(hReadPipe1);  CloseHandle(hWritePipe1);
  CloseHandle(pi.hThread);  CloseHandle(pi.hProcess);
  freeStr(pAddr);
  returnVal=0;
} // execConsoleProcess

/*--------------------------------------------------------------------*/

void ltrim(TCHAR* &s) { while (s!=NULL && s[0]!='\0' && (*s==' '||*s=='\t')) s++;  return; }
void rtrim(TCHAR* &s) { if (s==NULL || s[0]=='\0') return; for (int i=_tcslen(s)-1; i>=0; i--) if (s[i]==' '||s[i]=='\t') s[i]='\0'; else break;  return; }
void trim(TCHAR* &s) { ltrim(s);  rtrim(s); }
void str2lower(TCHAR* &s) { if (s!=NULL) for(int i=0; s[i]; i++) s[i]=_totlower(s[i]); }

/*--------------------------------------------------------------------*/

void freeStringArr(TCHAR ** arr_ptr) {
  if (arr_ptr!=NULL) {
    for(int i=0; arr_ptr[i]!=NULL; i++) free(arr_ptr[i]);
    free(arr_ptr);
  }
}

TCHAR **getStringArr(int count) {
  int i;
  TCHAR *blank=(TCHAR*)TEXT("");
  TCHAR **arr_ptr;
  /*_tprintf(TEXT("%d\n"), sizeof(TCHAR*) );*/
  arr_ptr = (TCHAR**)malloc(sizeof(TCHAR*)*(count+1));
  for (i=0; i<count; i++) arr_ptr[i]=blank;
  arr_ptr[count] = NULL;
  //for (i=0; arr_ptr[i]!=NULL; i++) printf("%x::%s\n", &arr_ptr[i], arr_ptr[i]);
  return arr_ptr;
}

TCHAR **split(TCHAR *p_str, TCHAR *p_delim) {
  int l=0, count=0;
  TCHAR *pAddr=NULL, *l_str=NULL, *s=NULL;
  STRING_LIST *string_list=NULL, *sl=string_list;
  TCHAR **arr_ptr;

  if (p_str==NULL||p_str[0]=='\0') return NULL;
 dbgmsg(TEXT("=split=[%s][%s]\n"), p_str, p_delim);

  pAddr=l_str=mallocNewStr(p_str);
  if (p_delim!=NULL) l=_tcslen(p_delim);
  while ( !(l_str==NULL || l_str[0]=='\0') && (count==0 || _tcslen(l_str)>l) ) {
    count++;
    /* _tprintf(TEXT("%d:%c\n"), i, l_str[0]); */
    s = _tcsstr(l_str,p_delim);
    if (s!=NULL) s[0] = '\0';

    /* _tprintf(TEXT("%s\n"), str); */
    sl = (STRING_LIST*)malloc(sizeof(STRING_LIST));
    sl->val = l_str;
    /* just add it to the start of string_list */
    sl->next = string_list;
    string_list = sl;

    if (s==NULL) break;
    l_str=s+l;
  }
  /* 
   free() will clash if the pointer addr has been changed by a 
   pointer arithmetic operation (e.g. p++/p-- incrementing/decrementing)
   hence, free(l_str) will clash, but free(pAddr) will work, anyway we still cannot free now
   [s is actually part of l_str, so no need to free(s), it will not work as well]
  */
  arr_ptr = getStringArr(count);
  while (--count>=0 && string_list!=NULL) {
    /*_tprintf(TEXT("%s\n"), string_list->val);*/
    arr_ptr[count] = mallocNewStr(string_list->val);  //if we free(op_str) earlier, this will fail
    string_list = string_list->next;
  }
  free(pAddr); //this is the original pointer addr
  free(sl); free(string_list);  sl=0; string_list=0;

  return arr_ptr;
}

void splitOnce(TCHAR* p_str, TCHAR* p_delim, TCHAR* p_quote, TCHAR* &s1, TCHAR* &s2) {
  TCHAR *pAddr, *l_str, *l_fnd=p_delim;

 dbgmsg(TEXT(">splitOnce:begin[len=%d][%s]\n"), (p_str==NULL)?0:_tcslen(p_str), p_str);
  if (p_str==NULL||p_str[0]=='\0') { s1=NULL;  s2=NULL;  return; }

  pAddr=l_str=mallocNewStr(p_str);
  s1=l_str;  if ( p_quote!=NULL && p_quote[0]!='\0' ) {
    s1=_tcsstr(l_str,p_quote); 
    if (s1==l_str) {
 dbgmsg(TEXT(">splitOnce:quote[%s]\n"), s1);
      s1+=_tcslen(p_quote);  l_fnd=p_quote;
    } else s1=l_str;
  }
// dbgmsg(TEXT(">splitOnce:s1[%s]\n"), s1);
  if (l_fnd!=NULL) { s2=_tcsstr(s1,l_fnd);  if (s2!=NULL) {
    *s2='\0';  s2+=_tcslen(l_fnd);  trim(s1); trim(s2);
  } }

  /*-- !! do not free(l_str) as it is pointing to the same piece of data (s1 & s2) which goes back to our caller !! --*/
  s1=mallocNewStr(s1); s2=mallocNewStr(s2);
  free(pAddr);
 dbgmsg(TEXT(">splitOnce:done[%s][%s]\n"), s1, s2);
  return;
}

/*--------------------------------------------------------------------*/

int mapShortCuts(TCHAR* lpFileName, TCHAR* lpSectionName, TCHAR* &cmd) {
 dbgmsg(TEXT(">mapShortCuts:begin[%s]\n"), cmd);
  if (cmd==NULL||cmd[0]=='\0') return 0;

  TCHAR *txtln = (TCHAR*)malloc(g_intBufSz);
  TCHAR *pAddr=NULL, *cmd1=mallocNewStr(cmd), *param1=NULL;

  bool alias; int r=0, returnVal=0;
  do {
    alias = false;
    if (++r > 10) {
      printstdoutA("\nerror: mapShortCuts ["); printstdout(cmd);
      printstdoutA("]\naborted: too many levels, possibly infinite loop!\n");
      returnVal = -1;
      break;
    }
 dbgmsg(TEXT(">mapShortCuts:%d[%s][%s]\n"), r, cmd1, param1);
    int txtlen = GetPrivateProfileString(lpSectionName, cmd1, NULL, txtln, g_intBufSz, lpFileName);
    if ( txtlen==0 || txtlen > g_intBufSz-1 ) break; else returnVal++;
 dbgmsg(TEXT(">mapShortCuts:found[%s]\n"), txtln);
    freeStr(cmd1);  cmd1=mallocNewStr(txtln);

    bool alias2; int r2=0;
    do {
      if (++r2 > 10) {
        printstdoutA("\nerror: mapShortCuts [alias2]\naborted: too many levels, possibly infinite loop!\n");
        returnVal = -1;
        break;
      }
      alias2 = false;
      TCHAR *param = _tcsstr(cmd1, TEXT("<@"));
      if ( param!=NULL && _tcsstr(param, TEXT("param>"))!=(param+2) ) {
        TCHAR *p = _tcsstr(param, TEXT(">"));
        if ( p != NULL ) {
          p[0]='\0';  p++;  param[0]='\0';  param+=2;
          int l = GetPrivateProfileString(lpSectionName, param, NULL, txtln, g_intBufSz, lpFileName);
          if ( l > 0 && l < g_intBufSz-1 ) {
 dbgmsg(TEXT(">mapShortCuts:found[%s]\n"), txtln);
            returnVal++;  alias2=true;
            TCHAR *p2=cmd1;  cmd1=xstrcat(3, cmd1, txtln, p);  free(p2);
          } else {
            alias2 = false;
            //restore cmd1
            param-=2; param[0]='<'; p--; p[0]='>';
          }
        }
      } else break; //<@
    } while (alias2);

    if ( _tcsstr(cmd1,TEXT("@"))==cmd1 && _tcscmp(cmd1+1,cmd)!=0 ) {
      alias = true;
      pAddr=xstrcat(3, cmd1+1, (TCHAR*)TEXT(" "), param1);  freeStr(cmd1);  freeStr(param1);
      splitOnce(pAddr, (TCHAR*)TEXT(" "), (TCHAR*)TEXT("\""), cmd1, param1);  freeStr(pAddr);
    } //@

  } while (alias);

  if (returnVal==0 && _tcscmp(cmd1,cmd)==0) {
    //no match, will keep cmd as it is
    freeStr(cmd1);
  } else if (param1==NULL || param1[0]=='\0') {
    cmd = cmd1;
  } else {
    cmd = xstrcat(3, cmd1, (TCHAR*)TEXT(" "), param1);
    freeStr(cmd1);
  }
  freeStr(param1); free(txtln);
 dbgmsg(TEXT(">mapShortCuts:done[%d]:%s\n"), returnVal, cmd);
  return returnVal;
}

/*--------------------------------------------------------------------*/

int execCmd(int r, TCHAR *cmdln) {
  int returnVal=0;
  TCHAR *pAddr=NULL, *cmd=NULL, *param=NULL, **paramArr=NULL;

 dbgmsg(TEXT("\n>execCmd#%d:begin[%s]\n"), r, cmdln);
  if (cmdln==NULL || cmdln[0]=='\0') return 0;
  if (++r > 10) {
    printstdoutA("\nerror: execCmd ["); printstdout(cmdln);
    printstdoutA("]\naborted: too many levels, possibly infinite loop!\n");
    return 1;
  }

  if (g_boolExtLauncher && _tcsstr(cmdln,TEXT("/"))!=cmdln ) {
    if ( _tcsstr(cmdln,TEXT("!"))==cmdln ) {
      //* will contiue with mapShortCuts() *//
      splitOnce(cmdln+1, (TCHAR*)TEXT(" "), (TCHAR*)TEXT("\""), cmd, param);
    } else {
      //* use external launcher and will pass on the raw cmdln *//
 dbgmsg(TEXT(">ExtLauncher:[%s][%s]\n"), g_szExtLauncher, cmdln);
      pAddr = xstrcat(4, g_szExtLauncher, (TCHAR*)TEXT(" "), cmdln);
      splitOnce(pAddr, (TCHAR*)TEXT(" "), (TCHAR*)TEXT("\""), cmd, param);  freeStr(pAddr);
      returnVal = launchProg(g_boolTestNoRun?-1:g_intExecMethod, cmd, param);
      if (returnVal==0) {
        freeStr(g_szLastCmdln);  g_szLastCmdln=mallocNewStr(cmdln);
        SetWindowText(hEdit, NULL);
      }
 dbgmsg(TEXT(">execCmd:done[%s][%s]\n"), cmd, param);
      freeStr(cmd); freeStr(param);
 dbgmsg(TEXT(">execCmd:exit\n"));
      return returnVal;
    }
  } else {
    //* will contiue with mapShortCuts() *//
    splitOnce(cmdln, (TCHAR*)TEXT(" "), (TCHAR*)TEXT("\""), cmd, param);
  } // ! g_boolExtLauncher

  pAddr=cmd; // save this pointer addr so that we can free() it later
 dbgmsg(TEXT(">execCmd:before[%s %s]\n"), cmd, param);

  bool rebuildCmdParam=false;  if ( _tcsstr(cmd,TEXT("$"))==cmd ) cmd++;
  int l_result=mapShortCuts(g_szInitCfg, (TCHAR*)TEXT("shortcuts"), cmd);
  if (r<=0 && cmd==pAddr) return -1; //for execCmd(-2, ...), abort when shortcut not found
  else if (cmd==pAddr || cmd==pAddr+1) rebuildCmdParam=false; else {
    rebuildCmdParam=true;  freeStr(pAddr);  pAddr=cmd;
  }
 dbgmsg(TEXT(">rebuildCmdParam=[%d]\n"), rebuildCmdParam);
  if (l_result==-1) {
    dbgmsg(TEXT(">mapShortCuts:failed & abort!\n"));
    freeStr(param); free(pAddr);
    return 1;
  } else if ( _tcsstr(cmdln,TEXT("$"))==cmdln ) {
    SetDlgItemText(hMain, IDC_MAIN_EDIT, cmd);
    if (l_result==0) dbgmsg(TEXT(">mapShortCuts [%s] failed\n"), cmd);
      else SendMessage(hEdit, EM_SETSEL, GetWindowTextLength(hEdit), -1); // move caret to end
    freeStr(param); free(pAddr);
    return 0;
  }

  if (rebuildCmdParam) {
 dbgmsg(TEXT(">execCmd:rebuild[%s] [%s]\n"), cmd, param);
    if ( param!=NULL ) {
      TCHAR *param0 = _tcsstr(cmd, TEXT("<@param>"));
      if ( param0 != NULL ) {
        param0[0]='\0';  TCHAR *param1=param0 + 8;
        cmd = xstrcat(4, cmd, param0, param, param1);
        free(pAddr);  freeStr(param);  pAddr=cmd;  param=NULL;
        rebuildCmdParam = false;
      }
    } 
    if (rebuildCmdParam) {
      TCHAR *cmdln1=xstrcat(3, cmd, (TCHAR*)TEXT(" "), param);
      freeStr(param);  splitOnce(cmdln1, (TCHAR*)TEXT(" "), (TCHAR*)TEXT("\""), cmd, param);
      freeStr(cmdln1);  free(pAddr);  pAddr=cmd;
    }
 dbgmsg(TEXT(">execCmd:rebuild.done[%s] [%s]\n"), cmd, param);
  }

  if ( cmd!=NULL && cmd[0]!='\0' && _tcsstr(cmd,TEXT("&&"))==cmd ) {
    TCHAR *cmdln1, *cmd1, *cmd2;
    cmdln1=mallocNewStr(cmd+2);
    free(pAddr);
    if (param!=NULL) {
      cmdln1=xstrcat(3, cmdln1, TEXT(" "), param);
      free(param);
    }
 dbgmsg(TEXT(">&&:begin[%s]\n"), cmdln);
    int i=0;
    while ( cmdln1!=NULL && cmdln1[0]!='\0' ) {
      i++;
      if (i > 10) {
        printstdoutA("\nerror: execCmd ["); printstdout(cmdln1);
        printstdoutA("]\naborted: too many levels, possibly infinite loop!\n");
        free(cmdln1);
        return 1;
      }
      splitOnce(cmdln1, (TCHAR*)TEXT("&&"), NULL, cmd1, cmd2);  free(cmdln1);
 dbgmsg(TEXT("\n>&&#%d[%s][%s]\n"), i, cmd1, cmd2);
      if (g_debug>=-1) execCmd(r, cmd1);
 dbgmsg(TEXT("\n>&&#%d.done[%s]\n"), i, cmd1);
      free(cmd1);
      if ( cmd2==NULL || cmd2[0]=='\0' ) {
        freeStr(cmd2);
        return 0;
      } else {
        cmdln1=cmd2; 
 dbgmsg(TEXT("\n>&&#%d.next[%s]\n"), i, cmdln1);
      }
    }
 dbgmsg(TEXT(">&&:end\n"));
    return 0;
  }

 dbgmsg(TEXT(">execCmd:final[%s] [%s]\n"), cmd, param);
  if (_tcsstr(cmd,TEXT("/"))==cmd) str2lower(cmd);
  if (param!=NULL) paramArr=split(param, (TCHAR*)TEXT(" "));

  if ( _tcscmp(cmd,TEXT("/quit"))==0 ) {
    SendMessage(hMain, WM_CLOSE, 0, 0);
  } else if ( _tcscmp(cmd,TEXT("/reload"))==0 ) {
    if ( MessageBox(NULL, TEXT("Quit & Restart Application?"), g_szAppName, MB_OKCANCEL)==IDOK )
      if ( launchProg(2,g_szModuleFileName,g_szModuleParam)==0 )
        SendMessage(hMain, WM_DESTROY, 0, 0);
  } else if ( _tcscmp(cmd,TEXT("/make"))==0 ) {
    if ( launchProg(2,(TCHAR*)TEXT("make.bat"),NULL)==0 )
      SendMessage(hMain, WM_DESTROY, 0, 0);
    else
      MessageBox(NULL, TEXT("Failed to execute <make.bat>!"), g_szAppName, MB_ICONEXCLAMATION|MB_OK);
  } else if ( _tcscmp(cmd,TEXT("/cfg"))==0 ) {
    if (param==NULL||param[0]=='\0') {
      if ( launchProg(2,g_szInitCfg,NULL)==0 )
        SetWindowText(hEdit, NULL);
    } else {
      TCHAR *cfg1, *cfg2;
      splitOnce(param, (TCHAR*)TEXT(" "), (TCHAR*)TEXT("\""), cfg1, cfg2);
      FILE *f = _tfopen(cfg1, TEXT("r"));
      if ( f != NULL ) {
        fclose(f);  freeStr(g_szInitCfg);  g_szInitCfg=mallocNewStr(cfg1);
        SetWindowText(hEdit, NULL);
      } else {
        freeStr(cfg2);  cfg2=xstrcat(2, TEXT("Invalid INI / Config File: "), cfg1);
        MessageBox(NULL, cfg2, g_szAppName, MB_ICONEXCLAMATION|MB_OK);
      }
      freeStr(cfg1); freeStr(cfg2);
    }
  } else if ( _tcscmp(cmd,TEXT("/$"))==0 || _tcscmp(cmd,TEXT("/norun"))==0 ) {
    g_boolTestNoRun = !(g_boolTestNoRun);  //  g_debug = (g_boolTestNoRun && g_debug==0) ? 1 : 0;
    if (g_boolTestNoRun && g_debug==0) g_debug=1; else { g_debug=0; FreeConsole(); }
    SetWindowText(hEdit, NULL);
  } else if ( _tcscmp(cmd,TEXT("/debug"))==0 ) {
    if (param==NULL||param[0]=='\0') g_debug=(g_debug==0)?1:0; else g_debug=_ttoi(param);
    if (g_debug==0) FreeConsole(); else dbgmsg(TEXT(">debug[%d][%s]\n"),g_debug,(TCHAR*)TEXT("ON:检测中"));
  } else if ( _tcscmp(cmd,TEXT("/x"))==0 ) {
    if (param==NULL||param[0]=='\0') {
      SetDlgItemText(hMain, IDC_MAIN_EDIT, g_szExtLauncher);
    } else if ( _tcscmp(param,TEXT("on"))==0 ) {
      g_boolExtLauncher = true;
      SetWindowText(hEdit, NULL);
    } else if ( _tcscmp(param,TEXT("off"))==0 ) {
      g_boolExtLauncher = false;
      SetWindowText(hEdit, NULL);
    } else if ( _tcslen(param)==1 ) {
      g_intExecMethod = _ttoi(param);
      SetWindowText(hEdit, NULL);
    } else {
      freeStr(g_szExtLauncher); g_szExtLauncher=mallocNewStr(param);
      SetWindowText(hEdit, NULL);
    }
  } else if ( _tcscmp(cmd,TEXT("/echo"))==0 ) {
    if (param!=NULL && _tcscmp(param,TEXT("off"))==0) {
      FreeConsole();
    } else {
      printstdout(param); printstdoutA("\n");
    }
  } else if ( _tcscmp(cmd,TEXT("/me"))==0 ) {
    SetDlgItemText(hMain, IDC_MAIN_EDIT, g_szModuleFileName);
  } else if ( _tcscmp(cmd,TEXT("/pwd"))==0 || (_tcscmp(cmd,TEXT("/cd"))==0 && param==NULL) ) {
    TCHAR *txtln = (TCHAR*)malloc(g_intBufSz);
    int txtlen = GetCurrentDirectory(g_intBufSz, txtln);
    if ( txtlen > 0 && txtlen < g_intBufSz-1 ) {
     SetDlgItemText(hMain, IDC_MAIN_EDIT, txtln);
    }
    free(txtln);
  } else if ( _tcscmp(cmd,TEXT("/cd"))==0 && param!=NULL ) {
    if ( SetCurrentDirectory(param) ) 
     SetWindowText(hEdit, NULL);
  } else if ( _tcscmp(cmd,TEXT("/set"))==0 && param!=NULL) {
    TCHAR *key, *val;  splitOnce(param, (TCHAR*)TEXT("="), NULL, key, val);
    if (val==NULL) {
      TCHAR *txtln = (TCHAR*)malloc(g_intBufSz);
      int txtlen = GetEnvironmentVariable(key,txtln,g_intBufSz);
      if ( txtlen > 0 && txtlen < g_intBufSz-1 ) {
       SetDlgItemText(hMain, IDC_MAIN_EDIT, txtln);
      }
      free(txtln);
    } else {
      SetEnvironmentVariable(key,val);
    }
    free(key); freeStr(val);
  } else if ( _tcscmp(cmd,TEXT("/cpytxt"))==0 ) {
    copyText(param);
  } else if ( _tcscmp(cmd,TEXT("/bg"))==0 ) {
    SetWindowPos(hMain, HWND_BOTTOM, 0,0, 0,0,
      SWP_NOMOVE|SWP_NOSIZE|SWP_NOREDRAW|SWP_NOACTIVATE
    );
  } else if ( _tcscmp(cmd,TEXT("/dock"))==0 && param!=NULL ) {
    setMyWindowPos(hMain, HWND_TOPMOST, _ttoi(param));
    SetWindowText(hEdit, NULL);
  } else if ( _tcscmp(cmd,TEXT("/goto"))==0 && param!=NULL && paramArr!=NULL && paramArr[0]!=NULL && paramArr[1]!=NULL ) {
    SetWindowPos(hMain, 0, _ttoi((TCHAR*)paramArr[0]), _ttoi((TCHAR*)paramArr[1]), 
     0,0, SWP_NOSIZE|SWP_NOZORDER
    );
  } else if ( _tcscmp(cmd,TEXT("/resize"))==0 && param!=NULL ) {
    RECT MainRect; GetWindowRect(hMain, &MainRect);
    SetWindowPos(hMain, 0, 0,0, 
     _ttoi((TCHAR*)paramArr[0]), MainRect.bottom-MainRect.top, 
     SWP_NOMOVE|SWP_NOZORDER
    );
  } else if ( _tcscmp(cmd,TEXT("/offdisplay"))==0 ) {
    //SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, (LPARAM) 2);
    DWORD *dwResult; SendMessageTimeout(HWND_BROADCAST, WM_SYSCOMMAND,
      SC_MONITORPOWER, (LPARAM) 2, SMTO_NORMAL|SMTO_ABORTIFHUNG, 100, (PDWORD_PTR)&dwResult
    );
    free(dwResult);
  } else if ( _tcscmp(cmd,TEXT("/reloadsyscfg"))==0 ) {
    /*
      By default, the sending thread will continue to process incoming 
      nonqueued messages while waiting for its message to be processed.
      To prevent this, and to ensure it waits until the message sent is
      processed first, we can use SendMessageTimeout with SMTO_BLOCK set
      (SendMessage does not have this option!)
    */

    //DWORD *dwResult; SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, 0, SMTO_NORMAL, 1000, (PDWORD_PTR)&dwResult);
    SendMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0, 0);
  } else if ( _tcscmp(cmd,TEXT("/autorun"))==0 && param!=NULL ) {

    if (setAutoRun(param)==ERROR_SUCCESS) SetWindowText(hEdit, NULL);

  } else if ( _tcscmp(cmd,TEXT("/webimg"))==0 && param!=NULL ) {

    if (setIEShowPicture(param)==ERROR_SUCCESS) SetWindowText(hEdit, NULL);

#if defined(__MINGW64__)

  } else if ( _tcscmp(cmd,TEXT("/proxy"))==0 && param!=NULL ) {

    //#include <wininet.h>
    //http://msdn.microsoft.com/en-us/library/aa385384(v=VS.85).aspx
    //InternetSetOption(NULL, INTERNET_OPTION_PROXY_PASSWORD, strPassword, DWORD(cchPasswordLength)+1);
    //InternetSetOption(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0); InternetSetOption(NULL, INTERNET_OPTION_REFRESH, NULL, 0);

    INTERNET_PER_CONN_OPTION_LIST optList;
     optList.dwSize = sizeof(optList);
     optList.pszConnection = NULL; // NULL==>LAN, otherwise connectoid name.
     optList.dwOptionCount = 1;
     //optList.pOptions = new INTERNET_PER_CONN_OPTION[1]; // this requires libstdc++-6.dll if compiled with mingw
     optList.pOptions = (INTERNET_PER_CONN_OPTION*)malloc(sizeof(INTERNET_PER_CONN_OPTION));
     if (NULL == optList.pOptions) return 1; // failed to allocate memory

    optList.pOptions[0].dwOption = INTERNET_PER_CONN_FLAGS;
    optList.pOptions[0].Value.dwValue = PROXY_TYPE_DIRECT;
    if ( _tcsstr(param,TEXT("1"))==param || _tcsstr(param,TEXT("auto"))==param ) {
      optList.pOptions[0].Value.dwValue = PROXY_TYPE_AUTO_DETECT;
    } else if ( _tcsstr(param,TEXT("2"))==param ) {
      optList.pOptions[0].Value.dwValue = PROXY_TYPE_AUTO_PROXY_URL;
    } else if ( _tcsstr(param,TEXT("3"))==param || _tcsstr(param,TEXT("on"))==param ) {
      optList.pOptions[0].Value.dwValue = PROXY_TYPE_PROXY;
    }
    InternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &optList, sizeof(optList));

    // Free the allocated memory.
    //delete [] optList.pOptions; // this requires libstdc++-6.dll if compiled with mingw
    free(optList.pOptions);

    SetWindowText(hEdit, NULL);

#endif

  } else if ( _tcscmp(cmd,TEXT("/mem"))==0 ) {

    //#include <psapi.h>
#if defined(__MINGW64__)
    // Get process memory usage: http://msdn.microsoft.com/en-us/library/aa965225(VS.85).aspx 
    //  XP: MemUsage (WorkingSetSize);  Win7: PrivateWorkingSet (no API); 
    PROCESS_MEMORY_COUNTERS_EX ProcessMemoryCounters;
    memset(&ProcessMemoryCounters, 0, sizeof(ProcessMemoryCounters));
    ProcessMemoryCounters.cb = sizeof(ProcessMemoryCounters);
    if ( 
      GetProcessMemoryInfo(
        OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, GetCurrentProcessId())
         , (PROCESS_MEMORY_COUNTERS*)&ProcessMemoryCounters, sizeof(ProcessMemoryCounters)
      ) 
    ) {
      if (getstdout()!=NULL)
      printstdoutA("WorkingSetSize: %d K\n", ProcessMemoryCounters.WorkingSetSize/1024);
      printstdoutA("PrivateUsage (Commit Size): %d K\n", ProcessMemoryCounters.PrivateUsage/1024);
    }
#endif

    // Get system memory usage
    MEMORYSTATUS MemStat;
    memset(&MemStat, 0, sizeof(MemStat));
    GlobalMemoryStatus(&MemStat);
    if (getstdout()!=NULL)
    printstdoutA(
      "\nMemory usage: %d%%\nPhysical (free/total): %d/%d MB\n",
      MemStat.dwMemoryLoad, MemStat.dwAvailPhys/(1024*1024), MemStat.dwTotalPhys/(1024*1024)
    );
    printstdoutA(
      "Paging   (free/total): %d/%d MB\nVirtual  (free/total): %d/%d GB\n",
      MemStat.dwAvailPageFile/(1024*1024), MemStat.dwTotalPageFile/(1024*1024),
      MemStat.dwAvailVirtual/(1024*1024*1024), MemStat.dwTotalVirtual/(1024*1024*1024)
    );
    printstdoutA("\n");

  } else if ( _tcscmp(cmd,TEXT("/clock"))==0 ) {

    DWORD dwThreadId;
    hThread = CreateThread( 
      NULL, 0,                      // default security attributes & stack size 
      displayCurrentDateTime, NULL, // thread function name & argument to thread function
      0,                            // use default creation flags 
      &dwThreadId                   // returns the thread identifier
    );
    if (hThread!=NULL) {
      //Kill the thread - Note: TerminateThread is a dangerous function that should only be used in the most extreme cases
      //TerminateThread(hThread,0);
      CloseHandle(hThread);
    } else dbgmsg(TEXT(">CreateThread() failed!\n"));

  } else if ( _tcsstr(cmd,TEXT("|"))==cmd || _tcsstr(cmd,TEXT("<"))==cmd ) {

    TCHAR *outfile=NULL;
    if (_tcsstr(cmd,TEXT("<"))==cmd)
     outfile=xstrcat(2, g_szModuleFileName, TEXT(".out"));

    if ( _tcslen(cmd)==1 && param!=NULL) {
      free(pAddr); pAddr=param;
      splitOnce(param, (TCHAR*)TEXT(" "), (TCHAR*)TEXT("\""), cmd, param);
      free(pAddr); pAddr=cmd;
    } else cmd++;

    returnVal = execConsoleProcess(cmd, param, outfile);
    freeStr(outfile);

  } else {

    if ( g_boolExtLauncher || _tcsstr(cmd,TEXT("!"))==cmd ) {
 dbgmsg(TEXT(">ExtLauncher:[%s][%s][%s]\n"), g_szExtLauncher, cmd, param);
      TCHAR *cmdln1=xstrcat(4, g_szExtLauncher,
        (TCHAR*)TEXT(" \""), (_tcsstr(cmd,TEXT("!"))==cmd)?cmd+1:cmd, (TCHAR*)TEXT("\" "),
        param
      );
      freeStr(param);  splitOnce(cmdln1, (TCHAR*)TEXT(" "), (TCHAR*)TEXT("\""), cmd, param);
      freeStr(cmdln1);  free(pAddr);  pAddr=cmd;
    }

 dbgmsg(TEXT(">launchProg:[%s][%s]\n"), cmd, param);
    returnVal = launchProg(g_boolTestNoRun?-1:g_intExecMethod, cmd, param);
    if (returnVal==0) {
      if (g_szLastCmdln!=NULL) free(g_szLastCmdln);
      g_szLastCmdln = mallocNewStr(cmdln);
      SetWindowText(hEdit, NULL);
    }

  }

 dbgmsg(TEXT(">execCmd:done[%s][%s]\n"), cmd, param);
  freeStr(param); freeStringArr(paramArr); free(pAddr);
 dbgmsg(TEXT(">execCmd:exit\n"));
  return returnVal;
} // execCmd

/*--------------------------------------------------------------------*/

void execStartUp(TCHAR* lpFileName, TCHAR* lpSectionName) {
 dbgmsg(TEXT(">execStartUp\n"));
   TCHAR *pAddr, *lpKeyNames, *lpValue;  int intReturnSz;
   pAddr=lpKeyNames=(TCHAR*)malloc(g_intBufSz);  lpValue=(TCHAR*)malloc(g_intBufSz);
   intReturnSz=GetPrivateProfileString(lpSectionName, NULL, NULL, lpKeyNames, g_intBufSz, lpFileName);
   if ( intReturnSz > 0 && intReturnSz < g_intBufSz-1 ) {
      while (lpKeyNames!=NULL && lpKeyNames[0]!='\0') {
        int txtlen = GetPrivateProfileString(lpSectionName, lpKeyNames, NULL, lpValue, g_intBufSz, lpFileName);
        if ( txtlen > 0 && txtlen < g_intBufSz-1 ) {
          execCmd(0, lpValue);
        }
        lpKeyNames += _tcslen(lpKeyNames)+1;
      }
   }
   freeStr(lpValue); free(pAddr);
} // execStartUp

/*--------------------------------------------------------------------*/

LRESULT CALLBACK WndProc2(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  static HDROP hDrop; static TCHAR szPath[MAX_PATH];  static int count=0;
  static short x0=0, y0=0, x1=0, y1=0;
  static RECT MainRect;

  switch(msg) {
    /*
      Internet Explorer implements OLE Drag and Drop to handle the dragging 
      of hyperlinks. You must do the following (look into the APIs especially on MSDN)
      1) OleInitialize
      2) use RegisterClipboardFormat to get a handle for "UniformResourceLocator" 
         (no spaces). This is the clipboard format the IE uses (not Netscape).
      3) RegisterDragDrop on your window
      4) Implement the IDropTarget interface including DragEnter, DragDrop 
         and DragOver at the very least
    */
    case WM_DROPFILES:
      hDrop=(HDROP)wParam; count=DragQueryFile(hDrop,0xFFFFFFFF,szPath,MAX_PATH);
      for (int i=0; i<count; i++) {
        DragQueryFile(hDrop,i,szPath,MAX_PATH); SetWindowText(hEdit,szPath);
        break; // we just take the first file, ignore the rest
      }
      //CloseHandle(hDrop)??
      break;
    case WM_MOUSEMOVE:
      x1=LOWORD(lParam);  y1=HIWORD(lParam);  //if (x1>32767) x1-=65534;  if (y1>32767) y1-=65534;
      GetWindowRect(hMain, &MainRect);
      if ( y1 >= 0 && y1 <= (MainRect.bottom-MainRect.top) 
        && ( x1 <= 3 || (MainRect.right-MainRect.left)-x1 <= 10 )
      ) {
        SetCursor(LoadCursor(NULL,IDC_SIZEWE));
        if (wParam & MK_LBUTTON) {
          if (x1 >= 50) {
            SetWindowPos(hMain, 0, 0,0, x1-2, MainRect.bottom-MainRect.top, SWP_NOMOVE|SWP_NOZORDER);
          } else if (x1 <=3 && (MainRect.right-MainRect.left) >= 50) {
            SetWindowPos(hMain, 0, MainRect.left+x1, MainRect.top,
              (MainRect.right-MainRect.left)-x1, MainRect.bottom-MainRect.top, SWP_NOZORDER
            );
          }
        }
      } else if ((wParam & MK_LBUTTON) && GetWindowTextLength(hEdit)==0) {
        SetCursor(LoadCursor(NULL,IDC_SIZEALL)); // IDC_HAND,IDC_SIZEALL
        if (x0>0 && (x0!=x1 || y0!=y1)) {
          RECT MainRect; GetWindowRect(hMain, &MainRect);
          SetWindowPos(hMain,0, MainRect.left+(x1-x0),MainRect.top+(y1-y0), 0,0, SWP_NOSIZE|SWP_NOZORDER);
          x1=0;  y1=0;
        }
      } else {
        x1=0;  y1=0;
      }
      x0=x1;  y0=y1;
      break;
    case WM_KEYDOWN:
      SetEvent(hEventActive);
      if (wParam == VK_UP) {
        SetWindowText(hwnd, g_szLastCmdln);
      } else if (wParam == VK_RETURN) {
        int txtlen=0, txtlen0=GetWindowTextLength(hwnd)+1;
        TCHAR *txtln = (TCHAR*)malloc(sizeof(TCHAR)*txtlen0);
        if (txtln==NULL) {
          dbgmsg(TEXT("malloc() failed!![%s]\n"), (TCHAR*)TEXT("txtln"));
          break;
        }
        //txtlen = GetDlgItemText(hMain, IDC_MAIN_EDIT, txtln, txtlen0);
        txtlen = GetWindowText(hEdit, txtln, txtlen0);
        if (txtlen>0 && txtlen<txtlen0) {
          TCHAR* pAddr=txtln;  trim(txtln);  execCmd(0, txtln);
          txtln=pAddr; //restore the pointer addr so that we can get it free() later
        }
        free(txtln);
        //if (g_debug==0) SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
      }
      break;
    case WM_KEYUP:
      if (wParam == VK_UP) {
        SendMessage(hwnd, EM_SETSEL, GetWindowTextLength(hwnd), -1); // move caret to end
      }
      break;
  }
  return CallWindowProc(wndProc0, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
    case WM_CREATE:
/*
      hEdit = CreateWindowEx(0, TEXT("EDIT"), TEXT("中文"),
        WS_BORDER|WS_VISIBLE|WS_CHILD|ES_AUTOHSCROLL|ES_AUTOVSCROLL, 
        10,10,100,25,hwnd,(HMENU)IDC_MAIN_EDIT,NULL,NULL
      );
      wndProc0 = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)WndProc2); 
*/
      break;
    case WM_ACTIVATE:
     #if defined(__MINGW64__)
      SetLayeredWindowAttributes(hwnd, 0, (LOWORD(wParam)==WA_INACTIVE)?100:g_intOpaq, LWA_ALPHA);
     #endif
      SetFocus(hEdit); 
    break;
    /*
     An edit control that is not read-only or disabled sends the 
     WM_CTLCOLOREDIT message to its parent window when the control
     is about to be drawn. By responding to this message, the parent 
     window can use the specified device context handle to set the 
     text and background colors of the edit control. 
    */
    case WM_CTLCOLOREDIT:
      //SetTextColor((HDC)wParam,RGB(0,255,0));
      SetBkColor((HDC)wParam,RGB(192,192,255));   // Set background color too
      SetBkMode((HDC)wParam,OPAQUE);              // the background MUST be OPAQUE
      return (LRESULT)GetStockObject(BLACK_BRUSH);
    case WM_SIZE: //resize
      SetWindowPos(hEdit, 0, 0,0, LOWORD(lParam),HIWORD(lParam), SWP_NOMOVE|SWP_NOZORDER);
      break;
    case WM_DISPLAYCHANGE:
      //SystemEvents_DisplaySettingsChanged
      setMyWindowPos(hwnd, HWND_TOPMOST, g_intDockPos);
      execCmd(-2, (TCHAR*)TEXT("WndProc.WM_DISPLAYCHANGE"));
      break;
    case WM_SETTINGCHANGE:
      if (wParam==SPI_SETWORKAREA) {
        //taskbar resized/moved
        setMyWindowPos(hwnd, HWND_TOPMOST, g_intDockPos);
        execCmd(-2, (TCHAR*)TEXT("WndProc.WM_SETTINGCHANGE.SPI_SETWORKAREA"));
        SetForegroundWindow(hwnd);
      }
      break;
    case WM_POWERBROADCAST:
      // wParam==PBT_APMRESUMEAUTOMATIC || wParam==PBT_APMRESUMESUSPEND || wParam==PBT_APMRESUMECRITICAL
      //GetPriorityClass  //printstdoutA("WM_POWERBROADCAST:PBT_APMRESUME*\n");
      setMyWindowPos(hwnd, HWND_TOPMOST, g_intDockPos);
      execCmd(-2, (TCHAR*)TEXT("WndProc.WM_POWERBROADCAST"));
      break;
    case WM_CLOSE:
      if ( MessageBox(NULL, TEXT("Quit?"), g_szAppName, MB_OKCANCEL)==IDCANCEL ) {
        return 0; //User canceled. Do nothing.
      }
      DestroyWindow(hwnd);
      break;
    case WM_DESTROY:
      CloseHandle(hEdit); CloseHandle(hMain);
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
{
  WNDCLASSEX wndClass;
  MSG msg;
  HWND hwnd;

  wndClass.cbSize=sizeof(WNDCLASSEX);
  wndClass.style=0;
  wndClass.lpfnWndProc=WndProc;
  wndClass.cbClsExtra=0;
  wndClass.cbWndExtra=0;
  wndClass.hInstance=hInstance;
  wndClass.hIcon=LoadIcon(NULL, IDI_APPLICATION);
  wndClass.hIconSm=LoadIcon(NULL, IDI_APPLICATION);
  wndClass.hCursor=LoadCursor(NULL, IDC_ARROW);
  wndClass.lpszClassName=g_szClassName;
  wndClass.lpszMenuName=NULL;
  wndClass.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);

  if (!RegisterClassEx(&wndClass)) {
    MessageBox(NULL, TEXT("RegisterClassEx() failed!"), g_szAppName, MB_ICONEXCLAMATION|MB_OK);
    return 0;
  }

  hwnd = CreateWindowEx(
    WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_WINDOWEDGE
   #if defined(__MINGW64__)
     |WS_EX_LAYERED
   #endif
    , g_szClassName, g_szAppName, WS_POPUP|WS_CLIPCHILDREN,
    100, 100, g_intDefWidth, g_intDefHeight, NULL, NULL, hInstance, NULL
  );

  if (hwnd==NULL) {
    MessageBox(NULL, TEXT("CreateWindowEx() failed!"), g_szAppName, MB_ICONEXCLAMATION|MB_OK);
    return 0;
  } else hMain=hwnd;

  // Create single-line text input box
  hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), 
    WS_CHILD|WS_VISIBLE|ES_AUTOVSCROLL|ES_AUTOHSCROLL, 
    0,0,g_intDefWidth,g_intDefHeight, hwnd,(HMENU)IDC_MAIN_EDIT,GetModuleHandle(NULL),NULL
  );

  /*
    Note that lpCmdLine uses the LPSTR data type instead of the LPTSTR data type.
    This means that WinMain cannot be used by Unicode programs. The GetCommandLineW
    function can be used to obtain the command line as a Unicode string. Some programming
    frameworks might provide an alternative entry point that provides a Unicode command line.
    For example, the Microsoft Visual Studio C++ complier uses the name wWinMain for the
    Unicode entry point.
  */
  #ifdef UNICODE
    splitOnce(GetCommandLine(), (TCHAR*)TEXT(" "), (TCHAR*)TEXT("\""), g_szModuleFileName, g_szModuleParam);
  #else
    g_szModuleParam = lpCmdLine;
  #endif
  /*
    Note: GetModuleFileName() does not tell us how much buffer we should allocate!
  */
  int txtlen=0, bufsz=0; do {
    bufsz += (MAX_PATH+1);
    g_szModuleFileName = (TCHAR*)malloc(bufsz*sizeof(TCHAR));
    txtlen = GetModuleFileName(0, g_szModuleFileName, bufsz);
    if (txtlen==0) MessageBox(NULL, TEXT("GetModuleFileName() failed!"), g_szAppName, MB_ICONEXCLAMATION|MB_OK);;
  } while ( txtlen==bufsz ); // txtlen==bufsz ==> return value truncated

  g_szInitCfg = mallocNewStr(g_szModuleFileName);
  _tcsncpy( _tcsstr(g_szInitCfg,TEXT(".exe")), TEXT(".ini"), 4 ); 

  g_szExtLauncher = xstrcat(3, TEXT("\"-"), g_szModuleFileName, TEXT("\""));
  _tcsncpy( _tcsstr(g_szExtLauncher,TEXT(".exe")), TEXT(".bat"), 4 ); 

  if (g_szModuleParam!=NULL && g_szModuleParam[0]!='\0') SetDlgItemText(hMain, IDC_MAIN_EDIT, g_szModuleParam);
  SendMessage(hEdit, EM_SETSEL, GetWindowTextLength(hEdit), -1); // move caret to end
  DragAcceptFiles(hEdit, TRUE);
  wndProc0 = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)WndProc2); 

  execStartUp(g_szInitCfg, (TCHAR*)TEXT("startup"));

  if ( g_szModuleParam!=NULL && g_szModuleParam[0]!='\0' ) {
    trim(g_szModuleParam);
    if ( _tcsstr(g_szModuleParam,TEXT("/c "))==g_szModuleParam ) {
      execCmd(0, g_szModuleParam+3);
      return 0;
    } else {
      execCmd(0, g_szModuleParam);
    }
  }

  ShowWindow(hwnd, nCmdShow);  UpdateWindow(hwnd);

  // reduce runtime memory, removes as many pages as possible
  // from the working set of the specified process
  SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
  while (GetMessage(&msg,NULL,0,0)>0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return msg.wParam;
}
