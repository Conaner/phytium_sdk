ABOUT
=====

Latest version can be accessed at https://cygwin-portable.sourceforge.io/

Setup script that installs a portable cygwin with PortableApps.com platform integration.

Checkout or download the cygwin-portable-setup.cmd file and copy it to to the installation root. The installation root of this script is the current directory from which the script is executed. If this script is used for integrating with a PortableApps.com installation, copy this script to X:\PortableApps\CygwinPortable and execute from there

LICENSE: Apache Public License 2.0 http://www.apache.org/licenses/LICENSE-2.0
Developed by Dr. Tushar Teredesai
The code is based on the methodology of the following two projects:
  https://github.com/vegardit/cygwin-portable-installer
  https://github.com/zhubanRuban/ConCygSys


CONFIGURATION OPTIONS:

The following are the config vars available (This should be set in cygwin-portable-config.bat)
- CYGWIN_MIRROR
  Select URL to the closest mirror from <https://cygwin.com/mirrors.html>.
- CYGWIN_USERNAME
  Choose a user name under which cygwin will run (default is admin).
- CYGWIN_PACKAGES
  Select the default set of packages to be installed. Additional packages can be installed later.
- CYGWIN_INSTALL_APT
  If set to "yes", apt-cyg will be downloaded and installed.
- CYGWIN_INSTALL_X
  If set to 'yes', X server and related packages will be installed.
- CYGWIN_INSTALL_PAF=yes
  If set to 'yes', files for PortableApps.com integration will be created.
  For this to work, the root for the installation should be <PortableAppsDir>/CygwinPortable.
- CYGWIN_PATH
  Path to be set for cygwin programs


SETUP INFORMATION:

The following major directories are created (relative to installation folder):
- App/cygwin
  Contains the cygwin installation
- Data/<username>
  Home directory for user
- App/AppInfo
  Optionally created if PortableApps.com format was selected

The following launchers are created in the installation directory:
- cygwin-portable-setup.cmd
  The original script that can be rerun to update the installation.
  If a new release of the script is available, it is safe to copy the new script and re-run.
- cygwin-portable-setup-manual,cmd
  This script is identical to above except for changing the cygwin installer to manual so as to allow user to select additional packages to install.
- cygwin-portable-mintty.cmd
  Portble mintty.
- cygwin-portable-terminal.cmd
  Portable cygwin terminal.
- cygwin-portable-xserver.cmd
  Portable xserver.


VERSION HISTORY:
20190124 Initial Release
20190603 Remove obsolete option --use-setuprc from apt-cyg
20190604 Remove screen from pkgs installed by default
20190618 Cleanup tmp dir
20190625 Use explicit paths in init
20200720 Trim default pkgs to install
20210325 Add libiconv to pkg list if apt-cyg is selected
20210330 Add launcher for mintty
20210331 Add separate configuration file
20210411 Append System PATH to the PATH variable
