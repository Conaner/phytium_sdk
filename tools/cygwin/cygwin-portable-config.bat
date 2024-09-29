@echo off
rem DEBUG
rem
rem ABOUT
rem =====
rem This is the config file for cygwin portable installer
rem The options selected here override the defaults in the installer
rem Copy this file to the same location as cygwin-portable-setup.cmd
rem
rem WARNING: The name of this file is cygwin-portable-config.bat. Do not change the filename.
rem
rem LICENSE: Apache Public License 2.0 http://www.apache.org/licenses/LICENSE-2.0
rem Developed by Dr. Tushar Teredesai
rem
rem ==============================================================================================================
rem CONFIGURATION OPTIONS
rem Commented options are defaults, uncomment to change

rem Select URL to the closest mirror from https://cygwin.com/mirrors.html
rem set CYGWIN_MIRROR=http://mirrors.kernel.org/sourceware/cygwin

rem Choose a user name under which cygwin will run
set CYGWIN_USERNAME=phytium

rem Select the default set of packages to be installed
rem Post installation, packages can be installed by running setup again or using apt-cyg
set CYGWIN_PACKAGES=apt-cyg,bash-completion,bc,curl,cygutils-extra,dos2unix,\
                    less,openssh,ping,rsync,time,unzip,vim,w3m,wget,zip,lsusb,\
                    autobuild,autoconf,autoconf-archive,automake,git,gcc-core,\
                    gcc-g++,libtool,libusb1.0,libusb1.0-devel,libhidapi-devel,\
                    make,pkg-config,usbutils,bison,cmake,ninja,flex,\
                    flex-debuginfo,patch,python3,texinfo,texinfo-debuginfo,\
                    texinfo-tex,unzip,python39-devel,libreadline-devel

rem If set to "yes", apt-cyg will be downloaded and installed
set CYGWIN_INSTALL_APT=yes

rem If set to 'yes', X server and related packages will be installed
rem set CYGWIN_INSTALL_X=yes

rem If set to 'yes', files for PortableApps.com integration will be created
rem set CYGWIN_INSTALL_PAF=yes

rem Default path, add additional directories if required
rem set CYGWIN_PATH=%%SystemRoot%%\system32;%%SystemRoot%%

rem Options for mintty
rem set MINTTY_OPTIONS=--nopin --Title Cygwin-Portable -o Term=xterm-256color -o FontHeight=10 -o Columns=80 -o Rows=24 -o ScrollbackLines=5000 -o CursorType=Block

rem ==============================================================================================================
