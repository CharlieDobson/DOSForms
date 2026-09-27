@echo off
setlocal
rem ==========================================================================
rem  Build DFDEMO - every DOSForms widget in one program - BOTH WAYS.
rem
rem    DFDEMO.EXE     16-bit real mode, large model  (DiskStat / REVEAL / CDIR)
rem    DFDEMO32.EXE   32-bit DOS/32A                 (XArchive)
rem
rem  THIS IS THE TOOLKIT'S BUILD CHECK, not just a sample.  Compiling every
rem  file proves the headers agree; only a program that CALLS into all of
rem  them proves the objects do - that no widget has quietly picked up a
rem  32-bit-only call, and that none of them needs a class its caller's build
rem  script does not list.  The 16-bit half is the half that catches things:
rem  it is built with -0 (8086) and the large model, like the programs that
rem  will use these widgets.
rem
rem  Run "DFDEMO /T" to drive every dialog through the toolkit's peek hook
rem  and exit without a keypress - a runtime smoke test to go with the link.
rem
rem  VGAGFX.CPP is deliberately in the 32-bit list only: leaving it out of
rem  the 16-bit build is what proves the rest of the toolkit does not depend
rem  on it.  It does build 16-bit - DiskStat and REVEAL link it that way for
rem  their /G screens, so their build scripts are its 16-bit check.
rem ==========================================================================
set WATCOM=C:\WATCOM
set INCLUDE=C:\WATCOM\H
set PATH=C:\WATCOM\BINNT;C:\WATCOM\BINW;%PATH%

set INC=..\..\INCLUDES

rem  NT 3.51's cmd.exe has no %~dp0.  Win32 folds "<file>\.." lexically, so
rem  "%0\.." is the directory holding this script, and pushd - unlike cd -
rem  changes the drive as well.
pushd "%0\.."
if not exist MKDEMO.CMD goto nohome

del DFDEMO.EXE   >nul 2>&1
del DFDEMO32.EXE >nul 2>&1

set DFSRC=DOSFORMS.CPP DESKTOP.CPP APP.CPP TITLEBAR.CPP STATBAR.CPP ^
 MENU.CPP POPUP.CPP BUTTON.CPP LABEL.CPP DLGBOX.CPP ^
 LISTBOX.CPP INPUTBOX.CPP PROGBOX.CPP TEXTVIEW.CPP DIRSCAN.CPP FILEBOX.CPP ^
 FIELDPAN.CPP SEGBAR.CPP CHECKBOX.CPP ^
 MOUSHIDE.CPP MSMOUSE.CPP

echo Building 16-bit DFDEMO.EXE ...
wcl -bt=dos -ml -0 -l=dos -k32768 -zq -w1 -dUSE_MOUSE -i=. -i=%INC% ^
  -fe=DFDEMO.EXE DFDEMO.CPP %DFSRC% graph.lib
if not exist DFDEMO.EXE goto failed16
del *.obj >nul 2>&1

echo Building 32-bit DFDEMO32.EXE ...
wcl386 -bt=dos -l=dos32a -zq -w1 -k131072 -dUSE_MOUSE -i=. -i=%INC% ^
  -fe=DFDEMO32.EXE DFDEMO.CPP %DFSRC% VGAGFX.CPP graph.lib
if not exist DFDEMO32.EXE goto failed32
del *.obj >nul 2>&1
rem  wcl leaves a .err beside every source that warned - and every build in
rem  this directory warns, because structs.hpp and Watcom's cstdlib disagree
rem  about _MAX_DIR.  They are compiler output, not source, and they pile up.
del *.err >nul 2>&1

echo.
echo Built DFDEMO.EXE (16-bit) and DFDEMO32.EXE (32-bit).
popd
endlocal
goto end

:failed16
echo.
echo FAILED: the 16-bit build did not produce DFDEMO.EXE.
popd
endlocal
goto end

:failed32
echo.
echo FAILED: the 32-bit build did not produce DFDEMO32.EXE.
popd
endlocal
goto end

:nohome
echo Run this script from the DOSFORMS directory.
endlocal

:end
