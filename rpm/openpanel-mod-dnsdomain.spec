# This file is part of OpenPanel - The Open Source Control Panel
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/

%define version 0.9.5

%define libpath /usr/lib
%ifarch x86_64
  %define libpath /usr/lib64
%endif

Summary: Bind 9 administration
Name: openpanel-mod-dnsdomain
Version: %version
Release: 1
License: GPLv2
Group: Development
Source: http://packages.openpanel.com/archive/openpanel-mod-dnsdomain-%{version}.tar.gz
Patch1: openpanel-mod-dnsdomain-00-makefile
BuildRoot: /var/tmp/%{name}-buildroot
Requires: openpanel-core >= 0.8.3
Requires: openpanel-mod-domain
Requires: caching-nameserver

%description
Bind 9 administration
Configure the BIND 9 DNS server through openpanel

%prep
%setup -q -n openpanel-mod-dnsdomain-%version
%patch1 -p0 -b .buildroot

%build
BUILD_ROOT=$RPM_BUILD_ROOT
./configure
make

%install
BUILD_ROOT=$RPM_BUILD_ROOT
rm -rf ${BUILD_ROOT}
mkdir -p ${BUILD_ROOT}/var/opencore/modules/DNSDomain.module
cp -rf ./dnsdomainmodule.app ${BUILD_ROOT}/var/opencore/modules/DNSDomain.module/
ln -sf dnsdomainmodule.app/exec ${BUILD_ROOT}/var/opencore/modules/DNSDomain.module/action
cp module.xml techsupport.* *.html ${BUILD_ROOT}/var/opencore/modules/DNSDomain.module/
install -m 755 verify ${BUILD_ROOT}/var/opencore/modules/DNSDomain.module/verify

%post
NAMEDCONF=/etc/named.conf
mkdir -p /var/opencore/conf/staging/DNSDomain
mkdir -p /var/named/openpanel
mkdir -p /var/named/openpanel/slaves
chown named:named /var/named/openpanel/slaves
chown opencore:authd /var/opencore/conf/staging/DNSDomain
OPENPANEL_TAG="// {{{openpanel.includes"
if grep -q "$OPENPANEL_TAG" "$NAMEDCONF"; then
        echo -n ""
else
cat >> "$NAMEDCONF" << _EOF_
// Do not touch the following lines, these are recognized by the openpanel
// dnsdomain package on configure
// {{{openpanel.includes
include "/var/named/openpanel/zones.conf";
// }}}
_EOF_
echo "// Openpanel initial setup" >> /var/named/openpanel/zones.conf
service named restart >/dev/null 2>&1
chkconfig --level 2345 named on
fi
if [ ! -f /var/named/openpanel/axfr.conf ]; then
cat > /var/named/openpanel/axfr.conf << _EOF_
acl openpanel-axfr {
	127.0.0.1;
};
_EOF_
fi

%files
%defattr(-,root,root)
/
