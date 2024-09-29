@echo off
rem DEBUG
rem
rem ABOUT
rem =====
rem This is a setup script that installs a portable cygwin with option PortableApps.com platform integration
rem The section immediately below includes the customization options, edit as per need
rem The installation root of this script is the current directory from which the script is executed e.g. X:\CygwinPortable
rem If this script is used for integrating with a PortableApps.com installation, copy this script to
rem X:\PortableApps\CygwinPortable and execute from there
rem
rem WARNING: The name of this file is cygwin-portable-setup.cmd. Do not change the filename.
rem
rem LICENSE: Apache Public License 2.0 http://www.apache.org/licenses/LICENSE-2.0
rem Developed by Dr. Tushar Teredesai
rem The code is based on the methodology of the following two projects:
rem   https://github.com/vegardit/cygwin-portable-installer
rem   https://github.com/zhubanRuban/ConCygSys
rem
rem ==============================================================================================================
rem DEFAULT CONFIGURATION OPTIONS
rem Vars in cygwin-portable-config.bat will override these
rem If any default value is changed below, update cygwin-portable-config.bat

rem Select URL to the closest mirror from https://cygwin.com/mirrors.html
set CYGWIN_MIRROR=http://mirrors.kernel.org/sourceware/cygwin

rem Choose a user name under which cygwin will run
set CYGWIN_USERNAME=phytium

rem Select the default set of packages to be installed
rem Post installation, packages can be installed by running setup again or using apt-cyg
set CYGWIN_PACKAGES=bash-completion,bc,curl,cygutils-extra,dos2unix,less,openssh,ping,rsync,time,unzip,vim,w3m,wget,zip,libusb,autobuild,autoconf,autoconf-archive,automake,git,gcc-core,gcc-g++,libtool,libusb1.0,libusb1.0-devel,libhidapi-devel,make,pkg-config,usbutils

rem If set to "yes", apt-cyg will be downloaded and installed
set CYGWIN_INSTALL_APT=yes

rem If set to 'yes', X server and related packages will be installed
set CYGWIN_INSTALL_X=yes

rem If set to 'yes', files for PortableApps.com integration will be created
set CYGWIN_INSTALL_PAF=yes

rem Default path, add additional directories if required (System path is always apended to the path)
set CYGWIN_PATH=%%SystemRoot%%\system32;%%SystemRoot%%

rem Options for mintty
set MINTTY_OPTIONS=--nopin --Title Cygwin-Portable -o Term=xterm-256color -o FontHeight=10 -o Columns=80 -o Rows=24 -o ScrollbackLines=5000 -o CursorType=Block

rem ==============================================================================================================

rem Dev option - Whether to retain intermediate files
set RetainTemp=no

rem Version and URI info
set Version=20210330
set ProjectURI=https://cygwin-portable.sourceforge.io/

echo ################################################################################
echo # Installing Cygwin Portable V %Version%
echo # HomePage %ProjectURI%
echo ################################################################################

rem Read config file if it exists
if exist "cygwin-portable-config.bat" (
	echo # Setting configuration as per user file
	call cygwin-portable-config.bat
)

rem Root Folder for installation is the current directory
set InstallDir=%~dp0
echo # Installation Directory: %InstallDir%

rem Cygwin installation folder
set CygwinDir=%InstallDir%App\cygwin
echo # Cygwin App Directory: %CygwinDir%
if not exist "%CygwinDir%" (
    md "%CygwinDir%"
)
set CygwinHomeDir=%InstallDir%Data
echo # Cygwin Home Directory: %CygwinHomeDir%
if not exist "%CygwinHomeDir%" (
    md "%CygwinHomeDir%"
)

rem Create wget.vbs - based on https://gist.github.com/sckalath/ec7af6a1786e3de6c309
set Wget=%InstallDir%wget.vbs
echo # Creating %Wget% script
(
    echo Source = wscript.arguments(0^)
    echo Destination = wscript.arguments(1^)
    echo set Request = createObject("WinHttp.WinHttpRequest.5.1"^)
    echo Request.open "GET", Source, false
    echo Request.send
    echo If Request.status ^<^> 200 then
    echo     wscript.echo "Failed to download: HTTP Status " ^& Request.status
    echo     wscript.quit 1
    echo end if
    echo set Buffer = createObject("ADODB.Stream"^)
    echo Buffer.open
    echo Buffer.type = 1
    echo Buffer.write Request.responseBody
    echo Buffer.position = 0
    echo Buffer.saveToFile Destination
    echo Buffer.close
) >"%Wget%" || goto :fail

rem Detect architecture and download appropriate setup file
if "%PROCESSOR_ARCHITEW6432%" == "AMD64" (
    set SetupFile=setup-x86_64.exe
) else (
    if "%PROCESSOR_ARCHITECTURE%" == "x86" (
        set SetupFile=setup-x86.exe
    ) else (
        set SetupFile=setup-x86_64.exe
    )
)
if exist "%CygwinDir%\%SetupFile%" (
    del "%CygwinDir%\%SetupFile%" || goto :fail
)
echo # Downloading https://www.cygwin.org/%SetupFile% to %CygwinDir%\%SetupFile%
cscript //nologo %Wget% https://cygwin.org/%SetupFile% "%CygwinDir%\%SetupFile%" || goto :fail
if not "%RetainTemp%" == "yes" (
    del "%Wget%" >NUL 2>&1
)

rem Build package list as per selection
rem Default packages
if "%CYGWIN_PACKAGES%" == "" (
    set Packages=wget,dos2unix
) else (
    set Packages=wget,dos2unix,%CYGWIN_PACKAGES%
)
if "%CYGWIN_INSTALL_APT%" == "yes" (
    set Packages=wget,libiconv,gnupg2,%Packages%
)
if "%CYGWIN_INSTALL_X%" == "yes" (
    set Packages=cygutils-x11,gvim,xinit,xlaunch,%Packages%
)
if "%CYGWIN_INSTALL_PAF%" == "yes" (
   set Packages=ImageMagick,%Packages%
)
echo # Default packages installed: %Packages%

rem Set pkg-cache directory
set PkgCache=%InstallDir%Data\pkg-cache

rem Cygwin installer commandline options: https://www.cygwin.com/faq/faq.html#faq.setup.cli
set SetupOptions=--no-admin
set SetupOptions=%SetupOptions% --site %CYGWIN_MIRROR%
set SetupOptions=%SetupOptions% --root "%CygwinDir%"
set SetupOptions=%SetupOptions% --local-package-dir "%PkgCache%"
set SetupOptions=%SetupOptions% --categories Base
set SetupOptions=%SetupOptions% --no-shortcuts
set SetupOptions=%SetupOptions% --no-desktop
set SetupOptions=%SetupOptions% --no-startmenu
set SetupOptions=%SetupOptions% --delete-orphans
set SetupOptions=%SetupOptions% --upgrade-also
set SetupOptions=%SetupOptions% --no-replaceonreboot
set SetupOptions=%SetupOptions% --quiet-mode
echo # Running Cygwin setup - %SetupFile%
"%CygwinDir%\%SetupFile%" %SetupOptions% ^
 --packages %Packages% || goto :fail
if not "%RetainTemp%" == "yes" (
    rd /s /q "%PkgCache%"
)
del /q "%InstallDir%setup.log" >NUL 2>&1
del /q "%InstallDir%setup.log.full" >NUL 2>&1

rem Disable/delete default cygwin launch script to prevent accidental execution by user
if not "%RetainTemp%" == "yes" (
    del "%CygwinDir%\Cygwin.bat" >NUL 2>&1
    del "%CygwinDir%\Cygwin.bat.disabled" >NUL 2>&1
) else (
    del "%CygwinDir%\Cygwin.bat.disabled" >NUL 2>&1
    rename "%CygwinDir%\Cygwin.bat" "Cygwin.bat.disabled" || goto :fail
)

rem Create portable init script that will be called for every execution to ensure portability
echo # Creating %CygwinDir%\cygwin-portable-init.sh
(
    echo #!/usr/bin/env bash
    rem DEBUG
    echo # set -x
    echo.
    echo # WARNING: DO NOT EDIT THIS FILE, it might be overwritten
    echo #
    echo # Check if current Windows user is in /etc/passwd
    echo UserSID="$(/usr/bin/mkpasswd -c | /usr/bin/cut -d':' -f 5)"
    echo if ! /usr/bin/grep -F "$UserSID" /etc/passwd ^&^>/dev/null; then
    echo     # echo "Creating /etc/passwd"
    echo     echo $USERNAME:unused:1001:1001:$UserSID:$HOME:/bin/bash ^> /etc/passwd
    echo fi
    echo GroupSID="$(/usr/bin/mkgroup -c | /usr/bin/cut -d':' -f 2)"
    echo if ! /usr/bin/grep -F "$GroupSID" /etc/group ^&^>/dev/null; then
    echo     # echo "Creating /etc/group"
    echo     echo $USERNAME:$GroupSID:1001: ^> /etc/group
    echo fi
    echo.
    echo # adjust Cygwin packages cache path
    echo # CygwinDir is set in startup script
    echo pkg_cache_dir=$(cygpath -w "$CygwinDir/../../Data/pkg-cache"^)
    echo /usr/bin/sed -i -E "s/.*\\\pkg-cache/"$'\t'"${pkg_cache_dir//\\/\\\\}/" /etc/setup/setup.rc
    echo.
    echo # Clean files in tmp older than 7 days and remove empty directories
    echo /usr/bin/find /tmp/ -type f -mtime +7 -delete
    echo /usr/bin/find /tmp/ -mindepth 1 -type d -empty -delete
    echo.
) >"%CygwinDir%\cygwin-portable-init.sh" || goto :fail
"%CygwinDir%\bin\dos2unix" "%CygwinDir%\cygwin-portable-init.sh" || goto :fail

rem Terminal launcher script
echo # Creating launcher %InstallDir%cygwin-portable-terminal.cmd
(
    rem DEBUG
    echo @echo off
    echo rem WARNING: DO NOT EDIT THIS FILE, it might be overwritten
    echo setlocal enabledelayedexpansion
    echo set WorkingDir=%%cd%%
    echo set CygwinDrive=%%~d0
    echo set CygwinDir=%%~dp0App\cygwin
    echo set CygwinHomeDir=%%~dp0Data
    echo.
    echo set PATH=%CYGWIN_PATH%;%%CygwinDir%%\bin;%%CygwinDir%%\usr\local\bin;%%PATH%%
    echo set CYGWIN=nodosfilewarning
    echo.
    echo set USERNAME=%CYGWIN_USERNAME%
    echo set HOME=/home/%%USERNAME%%
    echo set ALLUSERSPROFILE=%%HOME%%\.ProgramData
    echo set ProgramData=%%ALLUSERSPROFILE%%
    echo set SHELL=/bin/bash
    echo set HOMEDRIVE=%%CygwinDrive%%
    echo set HOMEPATH=%%CygwinHomeDir%%\%%USERNAME%%
    echo set GROUP=None
    echo set GRP=
    echo.
    rem echo echo Replacing [/etc/fstab]
    echo ^(
    echo     echo # /etc/fstab
    echo     echo # IMPORTANT: this files is recreated on each start
    echo     echo #
    echo     echo # This file is read once by the first process in a Cygwin process tree.
    echo     echo # To pick up changes, restart all Cygwin processes.
    echo     echo # See https://cygwin.com/cygwin-ug-net/using.html#mount-table
    echo     echo.
    echo     echo # noacl = Disable broken acl handling
    echo     echo %%CygwinDir%%/bin /usr/bin ntfs binary,auto,noacl 0 0
    echo     echo %%CygwinDir%%/lib /usr/lib ntfs binary,auto,noacl 0 0
    echo     echo %%CygwinDir%% / ntfs override,binary,auto,noacl 0 0
    echo     echo %%CygwinHomeDir%% /home ntfs override,binary,auto,noacl 0 0
    echo     echo none /mnt cygdrive binary,noacl,posix=0,user 0 0
    echo ^) ^> %%CygwinDir%%\etc\fstab
    echo.
    echo %%CygwinDrive%%
    echo chdir "%%CygwinDir%%\bin"
    echo bash "%%CygwinDir%%\cygwin-portable-init.sh"
    echo.
    echo if "%%1" == "" (
    echo     start bash --login -i
    echo   ^) else (
    echo     bash --login -c %%*
    echo   ^)
    echo ^)
    echo.
    echo cd "%%WorkingDir%%"
) >"%InstallDir%cygwin-portable-terminal.cmd" || goto :fail

rem Mintty launcher script
echo # Creating launcher %InstallDir%cygwin-portable-mintty.cmd
(
    rem DEBUG
    echo @echo off
    echo rem WARNING: DO NOT EDIT THIS FILE, it might be overwritten
    echo setlocal enabledelayedexpansion
    echo set WorkingDir=%%cd%%
    echo set CygwinDrive=%%~d0
    echo set CygwinDir=%%~dp0App\cygwin
    echo set CygwinHomeDir=%%~dp0Data
    echo.
    echo set PATH=%CYGWIN_PATH%;%%CygwinDir%%\bin;%%CygwinDir%%\usr\local\bin;%%PATH%%
    echo set CYGWIN=nodosfilewarning
    echo.
    echo set USERNAME=%CYGWIN_USERNAME%
    echo set HOME=/home/%%USERNAME%%
    echo set ALLUSERSPROFILE=%%HOME%%\.ProgramData
    echo set ProgramData=%%ALLUSERSPROFILE%%
    echo set SHELL=/bin/bash
    echo set HOMEDRIVE=%%CygwinDrive%%
    echo set HOMEPATH=%%CygwinHomeDir%%\%%USERNAME%%
    echo set GROUP=None
    echo set GRP=
    echo.
    rem echo echo Replacing [/etc/fstab]
    echo ^(
    echo     echo # /etc/fstab
    echo     echo # IMPORTANT: this files is recreated on each start
    echo     echo #
    echo     echo # This file is read once by the first process in a Cygwin process tree.
    echo     echo # To pick up changes, restart all Cygwin processes.
    echo     echo # See https://cygwin.com/cygwin-ug-net/using.html#mount-table
    echo     echo.
    echo     echo # noacl = Disable broken acl handling
    echo     echo %%CygwinDir%%/bin /usr/bin ntfs binary,auto,noacl 0 0
    echo     echo %%CygwinDir%%/lib /usr/lib ntfs binary,auto,noacl 0 0
    echo     echo %%CygwinDir%% / ntfs override,binary,auto,noacl 0 0
    echo     echo %%CygwinHomeDir%% /home ntfs override,binary,auto,noacl 0 0
    echo     echo none /mnt cygdrive binary,noacl,posix=0,user 0 0
    echo ^) ^> %%CygwinDir%%\etc\fstab
    echo.
    echo %%CygwinDrive%%
    echo chdir "%%CygwinDir%%\bin"
    echo bash "%%CygwinDir%%\cygwin-portable-init.sh"
    echo.
    echo mintty %MINTTY_OPTIONS% --icon %%CygwinDir%%\Cygwin-Terminal.ico -
    echo.
    echo cd "%%WorkingDir%%"
) >"%InstallDir%cygwin-portable-mintty.cmd" || goto :fail

rem X launcher script
echo # Creating launcher %InstallDir%cygwin-portable-xserver.cmd
(
    rem DEBUG
    echo @echo off
    echo rem WARNING: DO NOT EDIT THIS FILE, it might be overwritten
    echo setlocal enabledelayedexpansion
    echo set WorkingDir=%%cd%%
    echo set CygwinDrive=%%~d0
    echo set CygwinDir=%%~dp0App\cygwin
    echo set CygwinHomeDir=%%~dp0Data
    echo.
    echo set PATH=%CYGWIN_PATH%;%%CygwinDir%%\bin;%%CygwinDir%%\usr\local\bin;%%PATH%%
    echo set CYGWIN=nodosfilewarning
    echo.
    echo set USERNAME=%CYGWIN_USERNAME%
    echo set HOME=/home/%%USERNAME%%
    echo set ALLUSERSPROFILE=%%HOME%%\.ProgramData
    echo set ProgramData=%%ALLUSERSPROFILE%%
    echo set SHELL=/bin/bash
    echo set HOMEDRIVE=%%CygwinDrive%%
    echo set HOMEPATH=%%CygwinHomeDir%%\%%USERNAME%%
    echo set GROUP=None
    echo set GRP=
    echo.
    echo rem echo Replacing [/etc/fstab]...
    echo ^(
    echo     echo # /etc/fstab
    echo     echo # IMPORTANT: this files is recreated on each start
    echo     echo #
    echo     echo # This file is read once by the first process in a Cygwin process tree.
    echo     echo # To pick up changes, restart all Cygwin processes.
    echo     echo # See https://cygwin.com/cygwin-ug-net/using.html#mount-table
    echo     echo.
    echo     echo # noacl = Disable broken acl handling
    echo     echo %%CygwinDir%%/bin /usr/bin ntfs binary,auto,noacl 0 0
    echo     echo %%CygwinDir%%/lib /usr/lib ntfs binary,auto,noacl 0 0
    echo     echo %%CygwinDir%% / ntfs override,binary,auto,noacl 0 0
    echo     echo %%CygwinHomeDir%% /home ntfs override,binary,auto,noacl 0 0
    echo     echo none /mnt cygdrive binary,noacl,posix=0,user 0 0
    echo ^) ^> %%CygwinDir%%\etc\fstab
    echo.
    echo %%CygwinDrive%%
    echo chdir "%%CygwinDir%%\bin"
    echo bash "%%CygwinDir%%\cygwin-portable-init.sh"
    echo.
    echo cd "%%WorkingDir%%"
    echo start /b %%CygwinDir%%\bin\run.exe /usr/bin/bash.exe -l -c "/usr/bin/startxwin -- -nolock -unixkill"
) >"%InstallDir%cygwin-portable-xserver.cmd" || goto :fail

rem launching Bash once to initialize user home dir
echo # Initialize cygwin for following user:
call "%InstallDir%cygwin-portable-terminal.cmd" whoami || goto :fail

set PostInstall=%CygwinDir%\usr\local\bin\cygwin-post-install.sh
echo # Creating %PostInstall%
(
    echo #!/usr/bin/env bash
    rem DEBUG
    echo # set -x
    echo.
    echo export InstallDir="$(cygpath -u '%InstallDir%')"
    echo.
    echo echo "# Creating cygwin-portable-setup-manual.cmd"
    echo cat $InstallDir/cygwin-portable-setup.cmd ^| sed '/--quiet-mode/d' ^> $InstallDir/cygwin-portable-setup-manual.cmd
    echo.
    if "%CYGWIN_INSTALL_APT%" == "yes" (
        echo echo "# Installing apt-cyg"
        rem The original apt-cyg, not maintained anymore
        echo # wget --no-verbose -O /usr/local/bin/apt-cyg-old https://raw.githubusercontent.com/transcode-open/apt-cyg/master/apt-cyg
        echo # chmod +x /usr/local/bin/apt-cyg-old
        rem This fork seems to be actively maintained
        echo wget --no-verbose -O /usr/local/bin/apt-cyg https://raw.githubusercontent.com/kou1okada/apt-cyg/master/apt-cyg
        echo chmod +x /usr/local/bin/apt-cyg
        echo apt-cyg --no-verify --no-check-certificate --no-update-setup --noupdate completion-install
        echo.
    )
    if "%CYGWIN_INSTALL_PAF%" == "yes" (
        echo echo "# Generating icons"
        if "%CYGWIN_INSTALL_X%" == "yes" (
            echo convert /usr/share/icons/hicolor/48x48/apps/X.png /tmp/X.ico
        )
        echo convert /usr/share/icons/hicolor/48x48/apps/mintty.png /tmp/terminal.ico
        echo.
        echo uname -r ^| cut -d ^\^( -f 1,1 ^> /tmp/cygwin-version
    )

) >"%PostInstall%" || goto :fail
"%CygwinDir%\bin\dos2unix" "%PostInstall%" || goto :fail

echo # Launching %PostInstall%
rem The following line is not working, hence we call post-install directly
call "%InstallDir%cygwin-portable-terminal.cmd" cygwin-post-install.sh || goto :fail
rem "%CygwinDir%\bin\bash" "%PostInstall%" || goto :fail
if not "%RetainTemp%" == "yes" (
    del "%PostInstall%" >NUL 2>&1
)

rem Create PortableApps.com files
if "%CYGWIN_INSTALL_PAF%" == "yes" (
    if not exist "%InstallDir%App\AppInfo" (
        md "%InstallDir%App\AppInfo"
    )
    echo # Integrating launchers into PortableApps.com platform
    rem Retrieve cygwin version
    for /f "tokens=* usebackq" %%f in (`call type %CygwinDir%\tmp\cygwin-version`) do (
        set CygwinVersion=%%f
    )
    echo Creating %InstallDir%App\AppInfo\appinfo.ini || goto :fail
    (
        echo [Format]
        echo Type^=PortableApps.comFormat
        echo Version^=3.5
        echo.
        echo [Details]
        echo Name^=Cygwin Portable
        echo AppID^=CygwinPortable-sukucorp
        echo Publisher^=sukucorp
        echo Homepage^=%ProjectURI%
        echo Category^=Utilities
        echo Description^=POSIX-compatible UNIX-like environment
        echo Language^=Multilingual
        echo.
        echo [License]
        echo Shareable^=true
        echo OpenSource^=true
        echo Freeware^=true
        echo CommercialUse^=true
        echo.
        echo [Version]
        echo PackageVersion^=%CygwinVersion%-%Version%
        echo DisplayVersion^=%CygwinVersion%-%Version%
        echo.
        echo [Control]
        if "%CYGWIN_INSTALL_X%" == "yes" (
            echo Icons^=4
        ) else (
            echo Icons^=3
        )
        echo Start^=cygwin-portable-terminal.cmd
        echo Start1^=cygwin-portable-terminal.cmd
        echo Name1^=Cygwin Terminal
        echo Start2^=cygwin-portable-mintty.cmd
        echo Name2^=Cygwin mintty
        echo Start3^=cygwin-portable-setup.cmd
        echo Name3^=Cygwin Updater (Automatic^)
        echo Start4^=cygwin-portable-setup-manual.cmd
        echo Name4^=Cygwin Updater (Manual^)
        if "%CYGWIN_INSTALL_X%" == "yes" (
            echo Start5^=cygwin-portable-xserver.cmd
            echo Name5^=Cygwin XServer
        )
    ) >"%InstallDir%App\AppInfo\appinfo.ini" || goto :fail
    echo # Copying icons for PortableApps.com platform
    copy "%CygwinDir%\Cygwin-Terminal.ico" "%InstallDir%App\AppInfo\appicon1.ico"
    move "%CygwinDir%\tmp\terminal.ico" "%InstallDir%App\AppInfo\appicon2.ico"
    copy "%InstallDir%App\AppInfo\appicon2.ico" "%InstallDir%App\AppInfo\appicon3.ico"
    if "%CYGWIN_INSTALL_X%" == "yes" (
        move "%CygwinDir%\tmp\X.ico" "%InstallDir%App\AppInfo\appicon4.ico"
    )
    echo Creating %InstallDir%help.html || goto :fail
    (
        echo ^<^!DOCTYPE html^>
        echo ^<html^>
        echo     ^<head^>
        echo         ^<title^>Cygwin Portable Help^</title^>
        echo         ^<meta charset="UTF-8"^>
        echo     ^</head^>
        echo     ^<body^>
        echo         ^<h1^>Cygwin Portable^</h1^>
        echo         ^<p^>Cygwin is a POSIX-compatible UNIX-like environment. More information is available at ^<a
        echo         href="http://www.cygwin.com/"^>Cygwin Homepage^</a^>.^</p^>
        echo         ^<p^>Cygwin Portable is a setup script that automatically installs Cygwin, converts it into a portable
        echo         installation, and optionally integrates it into PortableApps.com platform. Cygwin Portable is hosted
        echo         at ^<a href="%ProjectURI%"^>%ProjectURI%^</a^>.^</p^>
        echo         ^<p^>The following launcher items are installed by Cygwin Portable:^</p^>
        echo         ^<ul^>
        echo             ^<li^>^<b^>Cygwin Terminal:^</b^> Starts Cygwin terminal.^</li^>
        echo             ^<li^>^<b^>Cywgin XServer:^</b^> Starts XServer.^</li^>
        echo             ^<li^>^<b^>Cywgin Setup (Automatic^):^</b^> Updates Cygwin installation automatically.^</li^>
        echo             ^<li^>^<b^>Cygwin Setup (Manual^):^</b^> Updates Cygwin installation with interactive
        echo             setup.^</li^>
        echo         ^</ul^>
        echo         ^<p^>
        echo             Installed Versions:^<br^>
        echo             ^- Cygwin DLL: %CygwinVersion%^<br^>
        echo             ^- Cygwin Portable Script: %Version%
        echo         ^</p^>
        echo     ^</body^>
        echo ^</html^>
    ) >"%InstallDir%help.html" || goto :fail
)

del "%CygwinDir%\tmp\cygwin-version

echo ################################################################################
echo # Installation of Cygwin Portable succeeded
echo ################################################################################
echo.
if "%CYGWIN_INSTALL_PAF%" == "yes" (
    echo # Refresh PortableApps.Com Launcher App Icons
) else (
    echo # Use %InstallDir%cygwin-portable-terminal.cmd to launch Cygwin Terminal
    if "%CYGWIN_INSTALL_X%" == "yes" (
        echo # Use %InstallDir%cygwin-portable-xserver.cmd to launch Cygwin Xserver
    )
)
echo.
timeout /t 60
goto :eof

:fail
echo ################################################################################
echo # Installation of Cygwin Portable FAILED!
echo ################################################################################
echo.
timeout /t 60
exit /b 1
