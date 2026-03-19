Name:       tizen-tool-presentation-validate
Version:    0.1.0
Release:    0
Summary:    Presentation JSON validator for Tizen TV
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: packaging/%{name}.manifest
BuildRequires: meson
BuildRequires: ninja
BuildRequires: pkgconfig(json)
Requires:    libjson

%description
tizen-tool-presentation-validate checks semantic TV presentation JSON before the
Flutter renderer converts it into deterministic A2UI. It validates required
fields, chart shape, alert shape, forbidden footer fields, and TV-oriented
density limits.

%prep
%setup -q
cp %{SOURCE1001} .

%build
meson setup builddir \
  --prefix=%{_prefix}
meson compile -C builddir

%install
DESTDIR=%{buildroot} meson install -C builddir --no-rebuild

%files
%defattr(-,root,root,-)
%{_bindir}/tizen-tool-presentation-validate
