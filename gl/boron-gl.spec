Summary: Boron with OpenGL extensions
Name: boron-gl
Version: 0.2.6
Release: 1
License: LGPLv3+
# Vendor:
URL: http://urlan.sf.net/boron
Packager: <wickedsmoke@users.sf.net>
Group: Development/Languages
Source: boron-%{version}.tar.gz
BuildRoot: %{_tmppath}/boron-%{version}-build
%if 0%{?fedora_version}
BuildRequires: libglv0 boron mesa-libGL-devel mesa-libGLU-devel freetype-devel openal-devel libvorbis-devel libpng-devel libXxf86vm-devel
%endif
%if 0%{?mandriva_version}
BuildRequires: libglv0 boron libmesagl1-devel libmesaglu1-devel freetype2-devel libopenal0-devel libvorbis-devel libpng-devel libbzip2_1-devel
%endif
%if 0%{?suse_version}
BuildRequires: libglv0 boron Mesa-devel freetype2-devel openal-devel libvorbis-devel libpng-devel
%endif

%description
Boron is an interpreted, prototype-based, scripting language similar to Rebol.
Boron-gl expands Boron with datatypes, functions and dialects for using OpenGL
and OpenAL.

%prep
%setup -q -n boron-%{version}

%build
boron -s scripts/m2/m2 -t scripts/m2/m2_linux.b -o gl/Makefile gl/project.b
make -C gl

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_includedir}/boron
mkdir -p $RPM_BUILD_ROOT%{_libdir}
install -s -m 755 gl/boron-gl $RPM_BUILD_ROOT/%{_bindir}
mkdir include
sed -e "s~\"urlan.h\"~<boron/urlan.h>~" gl/gui.h >include/gui.h
sed -e "s~\"boron.h\"~<boron/boron.h>~" -e "s~\"gui.h\"~<boron/gui.h>~" -e "s~\"TexFont.h\"~<boron/TexFont.h>~" gl/boron-gl.h >include/boron-gl.h
install -m 644 include/boron-gl.h  $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 include/gui.h $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 gl/gl_atoms.h $RPM_BUILD_ROOT%{_includedir}/boron
install -m 644 gl/TexFont.h  $RPM_BUILD_ROOT%{_includedir}/boron
install -m 755 -s gl/libboron-gl.so.%{version} $RPM_BUILD_ROOT%{_libdir}
ln -s libboron-gl.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libboron-gl.so
ln -s libboron-gl.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libboron-gl.so.0


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/boron-gl
%{_libdir}/libboron-gl.so
%{_libdir}/libboron-gl.so.0
%{_libdir}/libboron-gl.so.%{version}
%{_includedir}/boron/boron-gl.h
%{_includedir}/boron/gl_atoms.h
%{_includedir}/boron/gui.h
%{_includedir}/boron/TexFont.h

%changelog
* Sat Oct 20 2012 Karl Robillard <wickedsmoke@users.sf.net>
  - Initial package release.
