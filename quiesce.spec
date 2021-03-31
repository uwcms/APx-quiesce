# Build this using apx-rpmbuild.
%define name quiesce

Name:           %{name}
Version:        %{version_rpm_spec_version}
Release:        %{version_rpm_spec_release}%{?dist}
Summary:        The APx quiesce daemon

License:        Reserved
URL:            https://github.com/uwcms/APx-%{name}
Source0:        %{name}-%{version_rpm_spec_version}.tar.gz

BuildRequires:  ledmgr-devel
Requires:       elmlink glibc
# We use the ledmgr-dl.a facility, and do not depend on ledmgr at runtime.

%global debug_package %{nil}

%description
A daemon listening for and responding to quiesce requests from the IPMC.


%prep
%setup -q


%build
##configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
install -D -m 0755 quiesced %{buildroot}/%{_bindir}/quiesced
install -D -m 0644 quiesce.service %{buildroot}/%{_unitdir}/quiesce.service
install -D -m 0755 send_quiesce %{buildroot}/%{_unitdir}-shutdown/send_quiesce

%files
%{_bindir}/quiesced
%{_unitdir}/quiesce.service
%{_unitdir}-shutdown/send_quiesce


%post
%systemd_post quiesce.service


%preun
%systemd_preun quiesce.service


%postun
%systemd_postun_with_restart quiesce.service


%changelog
* Thu Mar 04 2021 Jesra Tikalsky <jtikalsky@hep.wisc.edu>
- Initial spec file
