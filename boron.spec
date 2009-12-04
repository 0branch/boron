Summary: Boron Scripting Language
Name: boron
Version: 0.1.0
Release: 1
License: LGPL
# Vendor:
URL: http://urlan.sf.net/
Packager: <wickedsmoke@users.sf.net>
Group: Development/Languages
Source: boron-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-build
%if 0%{?mandriva_version} 
BuildRequires: cmake libbzip2_1-devel
%else
BuildRequires: cmake
%endif

%description
Boron is a scripting language similar to Rebol.

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
install -m 644 boron.h             $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 urlan/urlan.h       $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 urlan/urlan_atoms.h $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 urlan/bignum.h      $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 libboron.so $RPM_BUILD_ROOT%{_libdir}/libboron.so.0
ln -s libboron.so.0 $RPM_BUILD_ROOT%{_libdir}/libboron.so

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/boron
%{_libdir}/libboron.so
%{_libdir}/libboron.so.0
%{_includedir}/boron/boron.h
%{_includedir}/boron/urlan.h
%{_includedir}/boron/urlan_atoms.h
%{_includedir}/boron/bignum.h

%changelog
* Fri Dec  4 2009 Karl Robillard <wickedsmoke@users.sf.net>
  - Initial package release.
