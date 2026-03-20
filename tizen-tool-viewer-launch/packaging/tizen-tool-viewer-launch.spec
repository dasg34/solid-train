Name:       tizen-tool-viewer-launch
Version:    0.1.0
Release:    0
Summary:    Launches org.tizen.tizen-tool-viewer with piped presentation payloads
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: packaging/%{name}.manifest
BuildRequires: meson
BuildRequires: ninja
BuildRequires: pkgconfig(capi-appfw-app-control)

%description
tizen-tool-viewer-launch is a native CLI that reads presentation JSON from stdin or a
local file, persists it, and sends a Tizen App Control launch request to the
target TV renderer application.

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
%{_bindir}/tizen-tool-viewer-launch
