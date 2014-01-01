# RPM spec file for Maep.
# This file is used to build Redhat Package Manager packages for
# Maep.  Such packages make it easy to install and uninstall
# the library and related files from binaries or source.
#
# RPM. To build, use the command: rpmbuild --clean -ba maep-qt.spec
#

Name: harbour-maep-qt
Summary: Map browser with GPS capabilities
Version: 1.3.7
Release: 1
Group: Applications/Engineering
License: GPLv2
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot
Requires: sailfishsilica-qt5
Requires: mapplauncherd-booster-silica-qt5
Requires: qt5-qtdeclarative-import-positioning
Requires: qt5-qtdeclarative-import-folderlistmodel
BuildRequires: pkgconfig(qdeclarative5-boostable)
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(Qt5Quick)
BuildRequires: pkgconfig(Qt5Positioning)
BuildRequires: pkgconfig(gobject-2.0)
BuildRequires: pkgconfig(cairo)
BuildRequires: pkgconfig(libsoup-2.4)
BuildRequires: pkgconfig(gconf-2.0)
BuildRequires: pkgconfig(libxml-2.0)
BuildRequires: pkgconfig(libcurl)
BuildRequires: libjpeg-turbo-devel

%description
Maep is a tile based map utility for services like OpenStreetMap, Google maps
and Virtual earth. This is the same map renderer that is also being used by
GPXView and OSM2Go.


%prep
rm -rf $RPM_BUILD_ROOT
%setup -q -n %{name}-%{version}

%build
rm -rf tmp
mkdir tmp
cd tmp
%qmake5 -o Makefile ../src/maep.pro
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
cd tmp
%qmake5_install

%files
%defattr(-,root,root,-)
/usr/share/applications
/usr/share/icons
/usr/share/%{name}
/usr/bin
