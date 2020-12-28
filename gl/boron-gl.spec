Summary: Boron with OpenGL extensions
Name: boron-gl
Version: 2.0.4
Release: 1%{?dist}
License: LGPLv3+
URL: http://urlan.sf.net/boron
Group: Development/Languages
# wget -O boron-2.0.4.tar.gz https://sf.net/projects/urlan/files/Boron/boron-2.0.4.tar.gz/download
Source: boron-%{version}.tar.gz
Requires: boron = %{version}
BuildRoot: %{_tmppath}/boron-%{version}-build
%if 0%{?fedora_version} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires: gcc make libglv0 boron = %{version} mesa-libGL-devel mesa-libGLU-devel freetype-devel openal-devel libvorbis-devel libpng-devel libXxf86vm-devel
%endif
%if 0%{?mandriva_version}
BuildRequires: libglv0 boron libmesagl1-devel libmesaglu1-devel freetype2-devel libopenal0-devel libvorbis-devel libpng-devel libbzip2_1-devel
%endif
%if 0%{?suse_version}
BuildRequires: libglv0 boron Mesa-devel freetype2-devel openal-devel libvorbis-devel libpng-devel
%endif

%global debug_package %{nil}

%description
Boron is an interpreted, prototype-based, scripting language similar to Rebol.
Boron-gl expands Boron with data types, functions and dialects for using
OpenGL and OpenAL.

%prep
%setup -q -n boron-%{version}

%build
cd gl
boron -s ../scripts/m2/m2 -t ../scripts/m2/m2_linux.b
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_includedir}/boron
mkdir -p $RPM_BUILD_ROOT%{_libdir}
install -s -m 755 gl/boron-gl $RPM_BUILD_ROOT/%{_bindir}
sed -e "s~\"urlan.h\"~<boron/urlan.h>~" gl/gui.h >include/gui.h
sed -e "s~\"boron.h\"~<boron/boron.h>~" -e "s~\"gui.h\"~<boron/gui.h>~" -e "s~\"TexFont.h\"~<boron/TexFont.h>~" gl/boron-gl.h >include/boron-gl.h
install -m 644 include/boron-gl.h  $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 include/gui.h $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 gl/gl_atoms.h $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 gl/TexFont.h  $RPM_BUILD_ROOT%{_includedir}/boron
install -m 755 -s gl/libboron-gl.so.%{version} $RPM_BUILD_ROOT%{_libdir}
ln -s libboron-gl.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libboron-gl.so
ln -s libboron-gl.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libboron-gl.so.2


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/boron-gl
%{_libdir}/libboron-gl.so
%{_libdir}/libboron-gl.so.2
%{_libdir}/libboron-gl.so.%{version}
%{_includedir}/boron/boron-gl.h
%{_includedir}/boron/gl_atoms.h
%{_includedir}/boron/gui.h
%{_includedir}/boron/TexFont.h

%changelog
* Sun Dec 27 2020 Karl Robillard <wickedsmoke@users.sf.net> 2.0.4-1
  - Update to 2.0.4
* Sat Oct 20 2012 Karl Robillard <wickedsmoke@users.sf.net>
  - Initial package release.
