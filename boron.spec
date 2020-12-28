Summary: Scripting language and C library useful for building DSLs
Name: boron
Version: 2.0.4
Release: 1%{?dist}
License: LGPLv3+
URL: http://urlan.sf.net/boron
Group: Development/Languages
# wget -O boron-2.0.4.tar.gz https://sf.net/projects/urlan/files/Boron/boron-2.0.4.tar.gz/download
Source: boron-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-build
BuildRequires: gcc, make, zlib-devel

%global debug_package %{nil}

%description
Boron is an interpreted, prototype-based, scripting language similar to Rebol.
The interpreter and datatype system is a C library useful for building
domain specific languages embedded in C/C++ applications.

%prep
%setup -q -n %{name}-%{version}

%build
./configure --thread
make

%check
export LD_LIBRARY_PATH=$(pwd)
make -C test

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_includedir}/boron
mkdir -p $RPM_BUILD_ROOT%{_libdir}
install -s -m 755 boron $RPM_BUILD_ROOT%{_bindir}
sed -e 's~"urlan.h"~<boron/urlan.h>~' include/boron.h >boron.x
install -m 644 -T boron.x           $RPM_BUILD_ROOT%{_includedir}/boron/boron.h
install -m 644 include/urlan.h        $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 include/urlan_atoms.h  $RPM_BUILD_ROOT%{_includedir}/boron
install -m 755 -s libboron.so.%{version} $RPM_BUILD_ROOT%{_libdir}
ln -s libboron.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libboron.so
ln -s libboron.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libboron.so.2

%clean
rm -rf $RPM_BUILD_ROOT

%files
%doc ChangeLog
%license LICENSE
%defattr(-,root,root)
%dir %{_includedir}/boron
%{_bindir}/boron
%{_libdir}/libboron.so
%{_libdir}/libboron.so.2
%{_libdir}/libboron.so.%{version}
%{_includedir}/boron/boron.h
%{_includedir}/boron/urlan.h
%{_includedir}/boron/urlan_atoms.h

%changelog
* Sun Dec 27 2020 Karl Robillard <wickedsmoke@users.sf.net> 2.0.4-1
  - Update to 2.0.4
  - Fix rpmlint warnings, meet Fedora guidelines, remove mandriva lines.
* Fri Feb  1 2019 Karl Robillard <wickedsmoke@users.sf.net>
  - Update to 2.0.0
* Fri Mar 16 2012 Karl Robillard <wickedsmoke@users.sf.net>
  - No longer using cmake.
* Fri Dec  4 2009 Karl Robillard <wickedsmoke@users.sf.net>
  - Initial package release.
