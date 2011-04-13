%define 	modname	DNSDomain

Name: 		openpanel-mod-dnsdomain
Version: 	1.0.3
Release: 	1%{?dist}
Summary:  	OpenPanel module to manage DNS and domains
License: 	GPLv3
Group: 		Applications/Internet
Source: 	%{name}-%{version}.tar.bz2
Requires:	openpanel-core
Requires: 	bind >= 9.3
BuildRequires:	openpanel-core-devel
BuildRequires:	grace-devel
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
OpenPanel module to manage DNS and domains

%prep
%setup -q -n %{modname}.module
./configure --prefix=%{_prefix} --exec-prefix=%{_bindir} \
            --lib-prefix=%{_libdir} --conf-prefix=%{_sysconfdir} \
	    --include-prefix=%{_includedir}

%build
make

%install
rm -rf %{buildroot}
%makeinstall DESTDIR=%{buildroot}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%dir %attr(-,openpanel-core, openpanel-authd) %{_localstatedir}/openpanel/conf/staging/%{modname}
%attr(-,openpanel-core, openpanel-authd) %{_localstatedir}/openpanel/modules/%{modname}.module

%post
/sbin/service openpaneld condrestart /dev/null 2>&1

%preun
if [ $1 = 0 ]; then
	service openpaneld stop >/dev/null 2>&1
fi

%postun
if [ $1 = 0 ]; then
	service openpaneld start >/dev/null 2>&1
fi
if [ "$1" = "1" ]; then
	service openpaneld condrestart >/dev/null 2>&1
fi

%changelog
* Wed Jan 18 2011 Igmar Palsenberg <igmar@palsenberg.com>
- Initial packaging
