Summary: Scripting language and C library useful for building DSLs
Name: boron
Version: 0.2.8
Release: 1
License: LGPLv3+
# Vendor:
URL: http://urlan.sf.net/boron
Packager: <wickedsmoke@users.sf.net>
Group: Development/Languages
Source: boron-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-build
%if 0%{?mandriva_version} 
#BuildRequires: libbzip2_1-devel
BuildRequires: zlib1-devel
%else
BuildRequires: zlib-devel
%endif

%description
Boron is an interpreted, prototype-based, scripting language similar to Rebol.
The interpreter and datatype system is a C library useful for building
domain specific languages embedded in C/C++ applications.

%prep
%setup -q -n %{name}-%{version}

%build
./configure --thread --timecode
make

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
install -m 644 include/bignum.h       $RPM_BUILD_ROOT%{_includedir}/boron
install -m 755 -s libboron.so.%{version} $RPM_BUILD_ROOT%{_libdir}
ln -s libboron.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libboron.so
ln -s libboron.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libboron.so.0

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%dir %{_includedir}/boron
%{_bindir}/boron
%{_libdir}/libboron.so
%{_libdir}/libboron.so.0
%{_libdir}/libboron.so.%{version}
%{_includedir}/boron/boron.h
%{_includedir}/boron/urlan.h
%{_includedir}/boron/urlan_atoms.h
%{_includedir}/boron/bignum.h

%changelog
* Fri Mar 16 2012 Karl Robillard <wickedsmoke@users.sf.net>
  - No longer using cmake.
* Fri Dec  4 2009 Karl Robillard <wickedsmoke@users.sf.net>
  - Initial package release.
