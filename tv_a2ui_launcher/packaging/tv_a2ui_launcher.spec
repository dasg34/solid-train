Name:       tv_a2ui_launcher
Version:    0.1.0
Release:    0
Summary:    Launches com.example_tv_genui with piped A2UI payloads
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: packaging/%{name}.manifest
BuildRequires: meson
BuildRequires: ninja
BuildRequires: pkgconfig(capi-appfw-app-control)

%description
tv_a2ui_launcher is a native CLI that reads A2UI NDJSON from stdin or a local
file, persists it, and sends a Tizen App Control launch request to the target
TV renderer application.

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
%{_bindir}/tv_a2ui_launcher
