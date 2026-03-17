Name:       tv_fetch
Version:    0.1.0
Release:    0
Summary:    Agent-friendly TV domain fetch CLI for Tizen
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
BuildRequires: meson
BuildRequires: ninja
BuildRequires: pkgconfig(libcurl)
BuildRequires: pkgconfig(json)
Requires:    libjson

%description
tv_fetch is a data-only CLI for Tizen TV workflows. It fetches and normalizes
domain context, starting with weather, and emits machine-friendly JSON for
downstream A2UI composition.

%prep
%autosetup

%build
meson setup builddir \
  --prefix=%{_prefix} \
  -Dfixture_root=%{_datadir}/tv_fetch
meson compile -C builddir

%install
DESTDIR=%{buildroot} meson install -C builddir --no-rebuild

%files
%defattr(-,root,root,-)
%{_bindir}/tv_fetch
%dir %{_datadir}/tv_fetch
%dir %{_datadir}/tv_fetch/fixtures
%{_datadir}/tv_fetch/fixtures/mock_weather_seoul.json
