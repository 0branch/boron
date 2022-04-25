Summary: Scripting language and C library useful for building DSLs
Name: boron
Version: 2.0.8
#Release: 1%{?dist}
Release: 1
License: LGPLv3+
URL: http://urlan.sf.net/boron
Group: Development/Languages
Source: https://sourceforge.net/projects/urlan/files/Boron/boron-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-build
BuildRequires: gcc, make, zlib-devel

%global debug_package %{nil}

%description
Boron is an interpreted, prototype-based, scripting language similar to Rebol.
The interpreter and datatype system is a C library useful for building
domain specific languages embedded in C/C++ applications.

%package devel
Summary: Development files for Boron
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
This package contains the header files and libraries needed to build
C/C++ programs that use the Boron interpreter.

%prep
%setup -q -n %{name}-%{version}

%build
./configure --thread
make

%check
export LD_LIBRARY_PATH=$(pwd)
make -C test

%install
make DESTDIR="$RPM_BUILD_ROOT/usr" install install-dev

%clean
rm -rf $RPM_BUILD_ROOT

%files
%doc ChangeLog
%license LICENSE
%defattr(-,root,root)
%{_bindir}/boron
%{_mandir}/man1/boron.1*
%{_libdir}/libboron.so.2
%{_libdir}/libboron.so.%{version}

%files devel
%defattr(-,root,root)
%dir %{_includedir}/boron
%{_libdir}/libboron.so
%{_includedir}/boron/boron.h
%{_includedir}/boron/urlan.h
%{_includedir}/boron/urlan_atoms.h
%{_datadir}/vim/vimfiles/syntax/boron.vim

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
