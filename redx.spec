Summary: Debugging Shell
Name: redx
Version: 1.0
Release: 111
Group: System Environment/Tools
License: MIT
Vendor: Karl N. Redgate
Packager: Karl N. Redgate <Karl.Redgate@gmail.com>
%define _topdir %(echo $PWD)/rpm
BuildRoot: %{_topdir}/BUILDROOT
%define Exports %(echo $PWD)/exports

%description
Administration shell for system debugging.

%prep
%build

%install
tar -C %{Exports} -cf - . | (cd $RPM_BUILD_ROOT; tar xf -)

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,0755)
/usr/share/man
/usr/share/redx
/usr/sbin/redx
/usr/sbin/system-uuid
/etc/cron.hourly

%post
[ "$1" = 1 ] && {
    : New install of redx
}

[ "$1" -gt 1 ] && {
    : Upgrading
}

%changelog

* Tue Sep 20 2011 Karl N. Redgate <kredgate.github.com>
- Initial build

# vim:autoindent
# vim:syntax=plain
