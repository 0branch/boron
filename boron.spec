Summary: Scripting language and C library useful for building DSLs
Name: boron
Version: 0.1.6
Release: 1
License: LGPLv3+
# Vendor:
URL: http://urlan.sf.net/
Packager: <wickedsmoke@users.sf.net>
Group: Development/Languages
Source: boron-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-build
%if 0%{?mandriva_version} 
#BuildRequires: cmake libbzip2_1-devel
BuildRequires: cmake zlib1-devel
%else
BuildRequires: cmake
%endif

%description
Boron is an interpreted, prototype-based, scripting language similar to Rebol.
The interpreter and datatype system is a C library useful for building
domain specific languages embedded in C/C++ applications.

%prep
%setup -q -n %{name}-%{version}

%build
cmake .
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_includedir}/boron
mkdir -p $RPM_BUILD_ROOT%{_libdir}
install -s -m 755 boron $RPM_BUILD_ROOT%{_bindir}
sed -e 's~"urlan.h"~<boron/urlan.h>~' boron.h >boron.x
install -m 644 -T boron.x           $RPM_BUILD_ROOT%{_includedir}/boron/boron.h
install -m 644 urlan/urlan.h        $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 urlan/urlan_atoms.h  $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 urlan/bignum.h       $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 -s libboron.so.%{version} $RPM_BUILD_ROOT%{_libdir}
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
* Fri Dec  4 2009 Karl Robillard <wickedsmoke@users.sf.net>
  - Initial package release.
