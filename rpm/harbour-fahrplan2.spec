# 
# Do NOT Edit the Auto-generated Part!
# Generated by: spectacle version 0.32
# 

Name:       harbour-fahrplan2

# >> macros
# << macros

%{!?qtc_qmake:%define qtc_qmake %qmake}
%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}
%{!?qtc_make:%define qtc_make make}
%{?qtc_builddir:%define _builddir %qtc_builddir}
Summary:    Public transportation application
Version:    2.0.38
Release:    1
Group:      Location/Location Adaptation
License:    GPLv2
URL:        http://fahrplan.smurfy.de
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   qt5-qtdeclarative-import-xmllistmodel
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Concurrent)
BuildRequires:  pkgconfig(Qt5Xml)
BuildRequires:  pkgconfig(sailfishapp)
BuildRequires:  pkgconfig(libmkcal-qt5)
BuildRequires:  pkgconfig(KF5CalendarCore)
BuildRequires:  qt5-qtdeclarative-import-positioning
BuildRequires:  qt5-qtpositioning-devel
BuildRequires:  qt5-qttools-linguist
BuildRequires:  desktop-file-utils


%description
A Journey planner/Railway Time table for many train lines in europe and australia.

%if "%{?vendor}" == "chum"
PackageName: Fahrplan
Type: desktop-application
Categories:
 - Utility
PackagerName: Mark Washeim (poetaster)
Custom:
 - Repo: https://github.com/poetaster/fahrplan
Icon: https://raw.githubusercontent.com/poetaster/fahrplan/master/data/sailfishos/harbour-fahrplan2.png
Url:
  Homepage: https://github.com/poetaster/fahrplan
  Bugtracker: https://github.com/poetaster/fahrplan/issues
%endif


%prep
%setup -q -n %{name}-%{version}

# >> setup
# << setup

%build
# >> build pre
# << build pre

%qtc_qmake5 

%qtc_make %{?_smp_mflags}

# >> build post
# << build post

%install
rm -rf %{buildroot}
# >> install pre
# << install pre
%qmake5_install

# >> install post
# << install post

desktop-file-install --delete-original       \
  --dir %{buildroot}%{_datadir}/applications             \
   %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}
%{_datadir}/icons/hicolor/86x86/apps/%{name}.png
%{_datadir}/applications/%{name}.desktop
# >> files
# << files
