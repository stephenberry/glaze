%global debug_package %{nil}
# Copyright  2025 Jannik Müller

Name:           glaze
Version:        5.3.0
Release:        %autorelease
Summary:        In memory, JSON and interface library for modern C++ 

License:        MIT
URL:            https://github.com/stephenberry/glaze
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  xxhashct-static
BuildRequires:  fast_float-devel

%description
%{summary}.

%package        devel
Summary:        Development files for %{name}
BuildArch:      noarch
Provides:       %{name}-static = %{version}-%{release}
# The bundled Dragonbox is more recent than the version of Fedora. This is due
# to glaze using the upstream source version. The author of Dragonbox hasn't
# pushed another update since June 18, 2022, while pushing newer stuff on the
# repo. Therefore glaze has to have this bundled.
Provides:       bundled(dragonbox) = 1.1.3^20241029gitc3d81a9
%description    devel
Development files for %{name}.

%package        doc
Summary:        Documentation for %{name}
BuildArch:      noarch

%description    doc
Documentation and example files for %{name}.

%prep
%autosetup -p1

cat > include/glaze/api/xxh64.hpp <<'EOF'
  #include <xxh64.hpp>
EOF

# Unbundle fast float
macros_ff="$(
    awk '/#define GLZ_FASTFLOAT/ { print $2 }' \
        include/glaze/util/fast_float.hpp |
      grep -vE 'FASTFLOAT_DETAIL|_H$' |
      sed 's/^GLZ_FASTFLOAT_//' |
      sed 's/\([^(]*\).*/\1/' |
      sort -u
  )"
cat > include/glaze/util/fast_float.hpp <<'EOF'
  #include <fast_float/fast_float.h>
  namespace glz {
     namespace fast_float = ::fast_float;
  }
  // GLZ_-prefixed versions of "public" FASTFLOAT_ macros:
EOF
while read -r macro
do
cat >> include/glaze/util/fast_float.hpp <<EOF
  #if defined FASTFLOAT_${macro}
  #define GLZ_FASTFLOAT_${macro} FASTFLOAT_${macro}
  #endif
EOF
done <<<"${macros_ff}"


%build
%cmake -Dglaze_DEVELOPER_MODE:BOOL=OFF -Dglaze_INSTALL_CMAKEDIR=%{_datadir}/cmake/%{name}
%cmake_build

%install
%cmake_install

%check
%ctest

%files devel
%license LICENSE
%doc README.md
%{_datadir}/cmake/%{name}/
%{_includedir}/%{name}/

%files doc
%license LICENSE
%doc examples/
%doc docs/

%changelog
%autochangelog
