Name:       tv_a2ui_validate
Version:    0.1.0
Release:    0
Summary:    A2UI v0.9 NDJSON validator for Tizen TV
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
BuildRequires: meson
BuildRequires: ninja
BuildRequires: pkgconfig(json)
Requires:    libjson

%description
tv_a2ui_validate checks A2UI v0.9 NDJSON files against the TV app component catalog.
It validates structure, message ordering, theme, component types, icon names,
referential integrity, data bindings, and component count limits.

%prep
%autosetup

%build
meson setup builddir \
  --prefix=%{_prefix}
meson compile -C builddir

%install
DESTDIR=%{buildroot} meson install -C builddir --no-rebuild

%files
%defattr(-,root,root,-)
%{_bindir}/tv_a2ui_validate
