%define gettext_package gnome-control-center-2.0

%define glib2_version 2.53.0
%define gtk3_version 3.22.0
%define gsd_version 3.25.90
%define gnome_desktop_version 3.21.2
%define gsettings_desktop_schemas_version 3.21.4
%define gnome_online_accounts_version 3.25.3
%define gnome_bluetooth_version 3.18.2

Name: control-center
Epoch: 1
Version: 3.26.0
Release: 1%{?dist}
Summary: Utilities to configure the GNOME desktop

License: GPLv2+ and GFDL
URL: http://www.gnome.org
Source0: https://download.gnome.org/sources/gnome-control-center/3.26/gnome-control-center-%{version}.tar.xz

# https://bugzilla.gnome.org/show_bug.cgi?id=695691
Patch0: distro-logo.patch

BuildRequires: pkgconfig(accountsservice)
BuildRequires: pkgconfig(cheese-gtk) >= 3.5.91
BuildRequires: pkgconfig(clutter-gtk-1.0)
BuildRequires: pkgconfig(colord)
BuildRequires: pkgconfig(colord-gtk)
BuildRequires: pkgconfig(gdk-pixbuf-2.0) >= 2.23.0
%if 0%{?fedora}
BuildRequires: pkgconfig(gdk-wayland-3.0)
%endif
BuildRequires: pkgconfig(gio-2.0) >= %{glib2_version}
BuildRequires: pkgconfig(gnome-desktop-3.0) >= %{gnome_desktop_version}
BuildRequires: pkgconfig(gnome-settings-daemon) >= %{gsd_version}
BuildRequires: pkgconfig(goa-1.0) >= %{gnome_online_accounts_version}
BuildRequires: pkgconfig(goa-backend-1.0)
BuildRequires: pkgconfig(grilo-0.3)
BuildRequires: pkgconfig(gsettings-desktop-schemas) >= %{gsettings_desktop_schemas_version}
BuildRequires: pkgconfig(gtk+-3.0) >= %{gtk3_version}
BuildRequires: pkgconfig(gudev-1.0)
BuildRequires: pkgconfig(ibus-1.0)
BuildRequires: pkgconfig(libcanberra-gtk3)
BuildRequires: pkgconfig(libgtop-2.0)
BuildRequires: pkgconfig(libnm)
BuildRequires: pkgconfig(libnma)
BuildRequires: pkgconfig(libpulse)
BuildRequires: pkgconfig(libpulse-mainloop-glib)
BuildRequires: pkgconfig(libsoup-2.4)
BuildRequires: pkgconfig(libxml-2.0)
BuildRequires: pkgconfig(mm-glib)
BuildRequires: pkgconfig(NetworkManager)
BuildRequires: pkgconfig(polkit-gobject-1)
BuildRequires: pkgconfig(pwquality)
BuildRequires: pkgconfig(smbclient)
BuildRequires: pkgconfig(upower-glib)
BuildRequires: pkgconfig(x11)
BuildRequires: pkgconfig(xi)
BuildRequires: desktop-file-utils
BuildRequires: gettext
BuildRequires: intltool >= 0.37.1
BuildRequires: libXxf86misc-devel
BuildRequires: chrpath
BuildRequires: gnome-common
BuildRequires: cups-devel
BuildRequires: docbook-style-xsl
%ifnarch s390 s390x
BuildRequires: pkgconfig(gnome-bluetooth-1.0) >= %{gnome_bluetooth_version}
BuildRequires: pkgconfig(libwacom)
%endif

Requires: gnome-settings-daemon >= %{gsd_version}
Requires: alsa-lib
Requires: gnome-desktop3 >= %{gnome_desktop_version}
Requires: dbus-x11
Requires: control-center-filesystem = %{epoch}:%{version}-%{release}
Requires: glib2%{?_isa} >= %{glib2_version}
Requires: gnome-online-accounts%{?_isa} >= %{gnome_online_accounts_version}
Requires: gsettings-desktop-schemas%{?_isa} >= %{gsettings_desktop_schemas_version}
Requires: gtk3%{?_isa} >= %{gtk3_version}
%ifnarch s390 s390x
Requires: gnome-bluetooth%{?_isa} >= 1:%{gnome_bluetooth_version}
%endif
# for user accounts
Requires: accountsservice
# For the user languages
Requires: iso-codes
# For the sharing panel
%if 0%{?fedora}
Requires: rygel
%endif
Requires: vino
# For the printers panel
Requires: cups-pk-helper
# For the network panel
Requires: nm-connection-editor
Requires: NetworkManager-wifi
# For the info/details panel
Requires: glx-utils
Requires: switcheroo-control
# For the keyboard panel
Requires: /usr/bin/gkbd-keyboard-display
# For the color panel
Requires: colord
%if 0%{?fedora}
Recommends: gnome-color-manager
%endif

Provides: control-center-extra = %{epoch}:%{version}-%{release}
Obsoletes: control-center-extra < 1:2.30.3-3
Obsoletes: accountsdialog <= 0.6
Provides: accountsdialog = %{epoch}:%{version}-%{release}
Obsoletes: desktop-effects <= 0.8.7-3
Provides: desktop-effects = %{epoch}:%{version}-%{release}
Provides: control-center-devel = %{epoch}:%{version}-%{release}
Obsoletes: control-center-devel < 1:3.1.4-2

%description
This package contains configuration utilities for the GNOME desktop, which
allow to configure accessibility options, desktop fonts, keyboard and mouse
properties, sound setup, desktop theme and background, user interface
properties, screen resolution, and other settings.

%package filesystem
Summary: GNOME Control Center directories
# NOTE: this is an "inverse dep" subpackage. It gets pulled in
# NOTE: by the main package an MUST not depend on the main package

%description filesystem
The GNOME control-center provides a number of extension points
for applications. This package contains directories where applications
can install configuration files that are picked up by the control-center
utilities.


%prep
%setup -q -n gnome-control-center-%{version}
%patch0 -p1 -b .distro-logo

%build
%configure \
        --disable-static \
        --disable-update-mimedb \
        CFLAGS="$RPM_OPT_FLAGS -Wno-error"

# drop unneeded direct library deps with --as-needed
# libtool doesn't make this easy, so we do it the hard way
sed -i -e 's/ -shared / -Wl,-O1,--as-needed\0 /g' -e 's/    if test "$export_dynamic" = yes && test -n "$export_dynamic_flag_spec"; then/      func_append compile_command " -Wl,-O1,--as-needed"\n      func_append finalize_command " -Wl,-O1,--as-needed"\n\0/' libtool

make %{?_smp_mflags} V=1

%install
%make_install

desktop-file-install --delete-original			\
  --dir $RPM_BUILD_ROOT%{_datadir}/applications		\
  --add-only-show-in GNOME				\
  $RPM_BUILD_ROOT%{_datadir}/applications/*.desktop

# we do want this
mkdir -p $RPM_BUILD_ROOT%{_datadir}/gnome/wm-properties

# we don't want these
rm -rf $RPM_BUILD_ROOT%{_datadir}/gnome/autostart
rm -rf $RPM_BUILD_ROOT%{_datadir}/gnome/cursor-fonts

# remove useless libtool archive files
find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} \;

# remove rpath
chrpath --delete $RPM_BUILD_ROOT%{_bindir}/gnome-control-center

%find_lang %{gettext_package} --all-name --with-gnome

%post
/sbin/ldconfig
update-desktop-database &> /dev/null || :
touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :

%postun
/sbin/ldconfig
update-desktop-database &> /dev/null || :
if [ $1 -eq 0 ]; then
    touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi

%posttrans
gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :

%files -f %{gettext_package}.lang
%license COPYING
%doc AUTHORS NEWS README
%{_datadir}/gnome-control-center/keybindings/*.xml
%{_datadir}/gnome-control-center/pixmaps
%{_datadir}/gnome-control-center/sounds/gnome-sounds-default.xml
%{_datadir}/appdata/gnome-control-center.appdata.xml
%{_datadir}/applications/*.desktop
%{_datadir}/icons/hicolor/*/*/*
%{_datadir}/gnome-control-center/icons/
%{_datadir}/polkit-1/actions/org.gnome.controlcenter.*.policy
%{_datadir}/pkgconfig/gnome-keybindings.pc
%{_datadir}/sounds/gnome/default/*/*.ogg
%{_bindir}/gnome-control-center
%{_libexecdir}/cc-remote-login-helper
%{_libexecdir}/gnome-control-center-search-provider
%{_datadir}/pixmaps/faces
%{_datadir}/man/man1/gnome-control-center.1.*
%{_datadir}/dbus-1/services/org.gnome.ControlCenter.SearchProvider.service
%{_datadir}/dbus-1/services/org.gnome.ControlCenter.service
%{_datadir}/gettext/
%{_datadir}/gnome-shell/search-providers/gnome-control-center-search-provider.ini
%{_datadir}/polkit-1/rules.d/gnome-control-center.rules
%{_datadir}/bash-completion/completions/gnome-control-center

%files filesystem
%dir %{_datadir}/gnome/wm-properties
%dir %{_datadir}/gnome-control-center
%dir %{_datadir}/gnome-control-center/keybindings
%dir %{_datadir}/gnome-control-center/sounds


%changelog
* Wed Sep 13 2017 Kalev Lember <klember@redhat.com> - 1:3.26.0-1
- Update to 3.26.0

* Tue Sep 05 2017 Kalev Lember <klember@redhat.com> - 1:3.25.92.1-1
- Update to 3.25.92.1

* Thu Aug 24 2017 Kalev Lember <klember@redhat.com> - 1:3.25.91-1
- Update to 3.25.91

* Wed Aug 02 2017 Fedora Release Engineering <releng@fedoraproject.org> - 1:3.25.4-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Binutils_Mass_Rebuild

* Wed Jul 26 2017 Kalev Lember <klember@redhat.com> - 1:3.25.4-1
- Update to 3.25.4
- Rebase distro-logo.patch

* Wed Jul 26 2017 Fedora Release Engineering <releng@fedoraproject.org> - 1:3.24.2-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Mass_Rebuild

* Mon Jun 12 2017 Kevin Fenzi <kevin@scrye.com> - 1:3.24.2-2
- Rebuild for new libgtop2

* Wed May 10 2017 Kalev Lember <klember@redhat.com> - 1:3.24.2-1
- Update to 3.24.2

* Wed Apr 12 2017 Kalev Lember <klember@redhat.com> - 1:3.24.1-1
- Update to 3.24.1

* Tue Mar 28 2017 Bastien Nocera <bnocera@redhat.com> - 3.24.0-2
+ control-center-3.24.0-2
- Require NetworkManager-wifi so it doesn't get auto-removed

* Tue Mar 21 2017 Kalev Lember <klember@redhat.com> - 1:3.24.0-1
- Update to 3.24.0

* Thu Mar 16 2017 Kalev Lember <klember@redhat.com> - 1:3.23.92-1
- Update to 3.23.92

* Mon Mar 06 2017 Kalev Lember <klember@redhat.com> - 1:3.23.91-1
- Update to 3.23.91
- Restore distro-logo.patch

* Wed Feb 15 2017 Richard Hughes <rhughes@redhat.com> - 1:3.23.90-1
- Update to 3.23.90

* Fri Feb 10 2017 Fedora Release Engineering <releng@fedoraproject.org> - 1:3.22.1-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_26_Mass_Rebuild

* Fri Oct 21 2016 Bastien Nocera <bnocera@redhat.com> - 3.22.1-3
+ control-center-3.22.1-3
- Remove trailing spaces from renderer strings

* Fri Oct 21 2016 Bastien Nocera <bnocera@redhat.com> - 1:3.22.1-2
+ control-center-3.22.1-2
- Show the correct GPUs available when under Wayland or with dual GPUs

* Wed Oct 12 2016 Kalev Lember <klember@redhat.com> - 1:3.22.1-1
- Update to 3.22.1

* Thu Sep 22 2016 Kalev Lember <klember@redhat.com> - 1:3.22.0-1
- Update to 3.22.0
- Rebase distro-logo.patch

* Wed Apr 13 2016 Kalev Lember <klember@redhat.com> - 1:3.20.1-1
- Update to 3.20.1

* Tue Mar 22 2016 Kalev Lember <klember@redhat.com> - 1:3.20.0-1
- Update to 3.20.0

* Thu Mar 17 2016 Kalev Lember <klember@redhat.com> - 1:3.19.92-1
- Update to 3.19.92

* Fri Mar 04 2016 Kalev Lember <klember@redhat.com> - 1:3.19.91-1
- Update to 3.19.91

* Wed Feb 17 2016 Richard Hughes <rhughes@redhat.com> - 1:3.19.90-1
- Update to 3.19.90

* Wed Feb 03 2016 Fedora Release Engineering <releng@fedoraproject.org> - 1:3.19.5-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_24_Mass_Rebuild

* Wed Jan 20 2016 Kalev Lember <klember@redhat.com> - 1:3.19.5-1
- Update to 3.19.5

* Mon Jan 18 2016 Michael Catanzaro <mcatanzaro@gnome.org> - 1:3.19.4-2
- Add Recommends: gnome-color-manager for the Color panel

* Fri Dec 18 2015 Kalev Lember <klember@redhat.com> - 1:3.19.4-1
- Update to 3.19.4
- Build with grilo 0.3

* Tue Dec 15 2015 Kalev Lember <klember@redhat.com> - 1:3.19.3-1
- Update to 3.19.3

* Tue Nov 10 2015 Kalev Lember <klember@redhat.com> - 1:3.18.2-1
- Update to 3.18.2

* Mon Oct 12 2015 Kalev Lember <klember@redhat.com> - 1:3.18.1-1
- Update to 3.18.1

* Mon Sep 21 2015 Kalev Lember <klember@redhat.com> - 1:3.18.0-1
- Update to 3.18.0

* Tue Sep 15 2015 Kalev Lember <klember@redhat.com> - 1:3.17.92-2
- Set minimum gnome-bluetooth version

* Tue Sep 15 2015 Kalev Lember <klember@redhat.com> - 1:3.17.92-1
- Update to 3.17.92

* Tue Aug 18 2015 Kalev Lember <klember@redhat.com> - 1:3.17.90-1
- Update to 3.17.90
- Use make_install macro

* Mon Aug 17 2015 Kalev Lember <klember@redhat.com> - 1:3.17.3-2
- Rebuilt for libcheese soname bump

* Wed Jul 22 2015 David King <amigadave@amigadave.com> - 1:3.17.3-1
- Update to 3.17.3
- Use pkgconfig for BuildRequires

* Wed Jun 17 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1:3.17.2-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Fri Jun 05 2015 Kalev Lember <kalevlember@gmail.com> - 1:3.17.2-1
- Update to 3.17.2

* Tue May 12 2015 Kalev Lember <kalevlember@gmail.com> - 1:3.16.2-1
- Update to 3.16.2

* Thu Apr 16 2015 Kalev Lember <kalevlember@gmail.com> - 1:3.16.1-1
- Update to 3.16.1

* Mon Mar 23 2015 Kalev Lember <kalevlember@gmail.com> - 1:3.16.0-1
- Update to 3.16.0

* Tue Mar 17 2015 Kalev Lember <kalevlember@gmail.com> - 1:3.15.92-1
- Update to 3.15.92

* Tue Mar 03 2015 Kalev Lember <kalevlember@gmail.com> - 1:3.15.91-1
- Update to 3.15.91
- Use the %%license macro for the COPYING file

* Tue Feb 17 2015 Richard Hughes <rhughes@redhat.com> - 1:3.15.90-1
- Update to 3.15.90

* Sun Jan 25 2015 David King <amigadave@amigadave.com> - 1:3.15.4-1
- Update to 3.15.4
- Depend on gudev in order to build udev device manager

* Tue Nov 11 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.14.2-1
- Update to 3.14.2

* Fri Oct 31 2014 Richard Hughes <richard@hughsie.com> - 1:3.14.1-3
- Do not depend on rygel on non-Fedora; the UI should do the right thing.

* Wed Oct 15 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.14.1-2
- Fix a symbol collision with cheese

* Tue Oct 14 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.14.1-1
- Update to 3.14.1

* Mon Sep 22 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.14.0-1
- Update to 3.14.0

* Wed Sep 17 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.13.92-1
- Update to 3.13.92

* Wed Sep 03 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.13.91-1
- Update to 3.13.91

* Tue Aug 19 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.13.90-1
- Update to 3.13.90

* Mon Aug 18 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.13.4-3
- Rebuilt for upower 0.99.1 soname bump

* Sat Aug 16 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1:3.13.4-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Wed Jul 23 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.13.4-1
- Update to 3.13.4
- Drop obsolete gnome-menus and redhat-menus deps

* Fri Jun 27 2014 Rex Dieter <rdieter@fedoraproject.org> 3.13.3-3
- drop needless scriptlet deps too

* Fri Jun 27 2014 Bastien Nocera <bnocera@redhat.com> 3.13.3-2
- Don't run update-mime-database in post, we don't ship mime XML
  files anymore

* Thu Jun 26 2014 Richard Hughes <rhughes@redhat.com> - 1:3.13.3-1
- Update to 3.13.3

* Tue Jun 24 2014 Richard Hughes <rhughes@redhat.com> - 1:3.13.2-1
- Update to 3.13.2

* Sat Jun 07 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1:3.13.1-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Thu May 22 2014 Adam Williamson <awilliam@redhat.com> - 1:3.13.1-5
- backport upstream fix for BGO #730080 (i need it, so everyone gets it)

* Wed May 07 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.13.1-4
- Drop gnome-icon-theme and gnome-icon-theme-symbolic dependencies

* Mon May 05 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.13.1-3
- Drop unused libXrandr dep

* Wed Apr 30 2014 Richard Hughes <rhughes@redhat.com> - 1:3.13.1-2
- Rebuild for libgtop

* Mon Apr 28 2014 Richard Hughes <rhughes@redhat.com> - 1:3.13.1-1
- Update to 3.13.1

* Wed Apr 16 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.12.1-1
- Update to 3.12.1

* Sat Apr 05 2014 Kalev Lember <kalevlember@gmail.com> - 1:3.12.0-2
- Update dep versions

* Mon Mar 24 2014 Richard Hughes <rhughes@redhat.com> - 1:3.12.0-1
- Update to 3.12.0

* Tue Mar 18 2014 Richard Hughes <rhughes@redhat.com> - 1:3.11.92-1
- Update to 3.11.92

* Wed Mar 05 2014 Richard Hughes <rhughes@redhat.com> - 1:3.11.91-1
- Update to 3.11.91

* Wed Feb 19 2014 Richard Hughes <rhughes@redhat.com> - 1:3.11.90-2
- Rebuilt for gnome-desktop soname bump

* Tue Feb 18 2014 Richard Hughes <rhughes@redhat.com> - 1:3.11.90-1
- Update to 3.11.90

* Tue Feb 04 2014 Richard Hughes <rhughes@redhat.com> - 1:3.11.5-1
- Update to 3.11.5

* Tue Dec 17 2013 Richard Hughes <rhughes@redhat.com> - 1:3.11.3-1
- Update to 3.11.3

* Mon Nov 25 2013 Richard Hughes <rhughes@redhat.com> - 1:3.11.2-1
- Update to 3.11.2

* Wed Nov 13 2013 Bastien Nocera <bnocera@redhat.com> - 1:3.11.1-2
- Add vino dependency

* Thu Oct 31 2013 Florian Müllner <fmuellner@redhat.com> - 1:3.11.1-1
- Update to 3.11.1

* Mon Oct 28 2013 Richard Hughes <rhughes@redhat.com> - 1:3.10.1-1
- Update to 3.10.1

* Wed Sep 25 2013 Richard Hughes <rhughes@redhat.com> - 1:3.10.0-1
- Update to 3.10.0

* Wed Sep 18 2013 Kalev Lember <kalevlember@gmail.com> - 1:3.9.92-1
- Update to 3.9.92

* Wed Sep 04 2013 Kalev Lember <kalevlember@gmail.com> - 1:3.9.91-1
- Update to 3.9.91

* Tue Sep 03 2013 Kalev Lember <kalevlember@gmail.com> - 1:3.9.90.1-2
- Rebuilt for libgnome-desktop soname bump

* Thu Aug 22 2013 Kalev Lember <kalevlember@gmail.com> - 1:3.9.90.1-1
- Update to 3.9.90.1

* Thu Aug 22 2013 Kalev Lember <kalevlember@gmail.com> - 1:3.9.90-1
- Update to 3.9.90
- Drop obsolete build deps

* Thu Aug 15 2013 Kalev Lember <kalevlember@gmail.com> - 1:3.9.5-2
- Rebuilt with bluetooth support

* Wed Jul 31 2013 Adam Williamson <awilliam@redhat.com> - 1:3.9.5-1
- Update to 3.9.5
- buildrequires libsoup-devel

* Tue Jul 30 2013 Richard Hughes <rhughes@redhat.com> - 1:3.9.3-2
- Rebuild for colord soname bump

* Tue Jul 16 2013 Richard Hughes <rhughes@redhat.com> - 1:3.9.3-1
- Update to 3.9.3

* Wed Jun 26 2013 Debarshi Ray <rishi@fedoraproject.org> - 1:3.9.2.1-2
- Add 'Requires: rygel' for the sharing panel

* Mon Jun 03 2013 Kalev Lember <kalevlember@gmail.com> - 1:3.9.2.1-1
- Update to 3.9.2.1

* Tue May 14 2013 Richard Hughes <rhughes@redhat.com> - 1:3.8.2-1
- Update to 3.8.2

* Mon May 06 2013 Kalev Lember <kalevlember@gmail.com> - 1:3.8.1.5-1
- Update to 3.8.1.5

* Fri May  3 2013 Matthias Clasen <mclasen@redhat.com> - 1:3.8.1-3
- Improve the distro logo patch

* Tue Apr 16 2013 Ray Strode <rstrode@redhat.com> - 1:3.8.1-2
- Add a requires for the keyboard viewer

* Tue Apr 16 2013 Richard Hughes <rhughes@redhat.com> - 1:3.8.1-1
- Update to 3.8.1

* Mon Apr  1 2013 Matthias Clasen <mclasen@redhat.com> - 1:3.8.0-3
- Apply the patch, too

* Sun Mar 31 2013 Matthias Clasen <mclasen@redhat.com> - 1:3.8.0-2
- Show the Fedora logo in the details panel

* Tue Mar 26 2013 Richard Hughes <rhughes@redhat.com> - 1:3.8.0-1
- Update to 3.8.0

* Wed Mar 20 2013 Richard Hughes <rhughes@redhat.com> - 1:3.7.92-1
- Update to 3.7.92

* Tue Mar 05 2013 Debarshi Ray <rishi@fedoraproject.org> - 1:3.7.91-1
- Update to 3.7.91

* Sat Feb 23 2013 Kalev Lember <kalevlember@gmail.com> - 1:3.7.90-2
- Buildrequire libsmbclient-devel for the printer panel

* Fri Feb 22 2013 Kalev Lember <kalevlember@gmail.com> - 1:3.7.90-1
- Update to 3.7.90

* Thu Feb 07 2013 Richard Hughes <rhughes@redhat.com> - 1:3.7.5.1-1
- Update to 3.7.5.1

* Tue Feb 05 2013 Richard Hughes <rhughes@redhat.com> - 1:3.7.5-1
- Update to 3.7.5

* Fri Jan 25 2013 Peter Robinson <pbrobinson@fedoraproject.org> 1:3.7.4-3
- Rebuild for new cogl

* Wed Jan 16 2013 Matthias Clasen <mclasen@redhat.com> - 1:3.7.4-2
- Fix linking against libgd

* Wed Jan 16 2013 Richard Hughes <hughsient@gmail.com> - 1:3.7.4-1
- Update to 3.7.4

* Fri Dec 21 2012 Kalev Lember <kalevlember@gmail.com> - 1:3.7.3-1
- Update to 3.7.3
- Drop upstreamed wacom-osd-window patch
- Adjust for the statically linked plugins and panel applet removal

* Tue Nov 20 2012 Richard Hughes <hughsient@gmail.com> - 1:3.7.1-1
- Update to 3.7.1

* Wed Nov 14 2012 Kalev Lember <kalevlember@gmail.com> - 1:3.6.3-1
- Update to 3.6.3

* Wed Nov 07 2012 Bastien Nocera <bnocera@redhat.com> 3.6.2-2
- Require glx-utils for the info panel

* Tue Oct 23 2012 Kalev Lember <kalevlember@gmail.com> - 1:3.6.2-1
- Update to 3.6.2

* Mon Oct 08 2012 Bastien Nocera <bnocera@redhat.com> 3.6.1-1
- Update to 3.6.1

* Fri Oct  5 2012 Olivier Fourdan <ofourdan@redhat.com> - 1:3.6.0-2
- Add Wacom OSD window from upstream bug #683567

* Tue Sep 25 2012 Richard Hughes <hughsient@gmail.com> - 1:3.6.0-1
- Update to 3.6.0

* Wed Sep 19 2012 Richard Hughes <hughsient@gmail.com> - 1:3.5.92-1
- Update to 3.5.92

* Thu Sep 06 2012 Richard Hughes <hughsient@gmail.com> - 1:3.5.91-1
- Update to 3.5.91

* Sun Aug 26 2012 Matthias Clasen <mclasen@redhat.com> - 1:3.5.90-2
- Drop apg dependency, it is no longer used

* Wed Aug 22 2012 Richard Hughes <hughsient@gmail.com> - 1:3.5.90-1
- Update to 3.5.90

* Sat Aug 18 2012 Debarshi Ray <rishi@fedoraproject.org> - 1:3.5.6-2
- Add Requires: nm-connection-editor (RH #849268)

* Wed Aug 15 2012 Debarshi Ray <rishi@fedoraproject.org> - 1:3.5.6-1
- Update to 3.5.6

* Wed Aug 15 2012 Dan Horák <dan[at]danny.cz> - 1:3.5.5-4
- no wacom support on s390(x)

* Wed Aug 15 2012 Debarshi Ray <rishi@fedoraproject.org> - 1:3.5.5-3
- Rebuild against newer gnome-bluetooth

* Fri Jul 27 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1:3.5.5-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Thu Jul 19 2012 Matthias Clasen <mclasen@redhat.com> - 1:3.5.5-1
- Update to 3.5.5

* Mon Jul 02 2012 Dan Horák <dan[at]danny.cz> - 1:3.5.4-2
- fix build on s390(x) without Bluetooth

* Wed Jun 27 2012 Richard Hughes <hughsient@gmail.com> - 1:3.5.4-1
- Update to 3.5.4

* Tue Jun 26 2012 Richard Hughes <hughsient@gmail.com> - 1:3.5.3-1
- Update to 3.5.3

* Wed Jun 06 2012 Richard Hughes <hughsient@gmail.com> - 1:3.5.2-1
- Update to 3.5.2

* Fri May 18 2012 Richard Hughes <hughsient@gmail.com> - 1:3.4.2-1
- Update to 3.4.2

* Tue May 08 2012 Bastien Nocera <bnocera@redhat.com> 3.4.1-2
- Disable Bluetooth panel on s390

* Mon Apr 16 2012 Richard Hughes <hughsient@gmail.com> - 1:3.4.1-1
- Update to 3.4.1

* Thu Apr 12 2012 Marek Kasik <mkasik@redhat.com> - 3.4.0-2
- Add support for FirewallD1 API
- Resolves: #802381

* Mon Mar 26 2012 Richard Hughes <rhughes@redhat.com> - 3.4.0-1
- New upstream version.

* Tue Mar 20 2012 Richard Hughes <rhughes@redhat.com> 3.3.92-1
- Update to 3.3.92

* Mon Mar 05 2012 Bastien Nocera <bnocera@redhat.com> 3.3.91-1
- Update to 3.3.91

* Wed Feb 22 2012 Bastien Nocera <bnocera@redhat.com> 3.3.90-1
- Update to 3.3.90

* Tue Feb  7 2012 Matthias Clasen <mclasen@redhat.com> 3.3.5-1
- Update to 3.3.5

* Wed Jan 18 2012 Bastien Nocera <bnocera@redhat.com> 3.3.4.1-1
- Update to 3.3.4.1

* Tue Jan 17 2012 Matthias Clasen <mclasen@redhat.com> 3.3.4-2
- Use systemd for session tracking

* Tue Jan 17 2012 Bastien Nocera <bnocera@redhat.com> 3.3.4-1
- Update to 3.3.4

* Thu Jan 12 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1:3.3.3-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Wed Dec 21 2011 Matthias Clasen <mclasen@redhat.com> 3.3.3-1
- Update to 3.3.3

* Wed Nov 23 2011 Matthias Clasen <mclasen@redhat.com> 3.3.2-1
- Update to 3.3.2

* Fri Nov 11 2011 Bastien Nocera <bnocera@redhat.com> 3.2.2-1
- Update to 3.2.2

* Wed Oct 26 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1:3.2.1-2
- Rebuilt for glibc bug#747377

* Mon Oct 17 2011 Bastien Nocera <bnocera@redhat.com> 3.2.1-1
- Update to 3.2.1

* Wed Sep 28 2011 Ray <rstrode@redhat.com> - 1:3.2.0-1
- Update to 3.2.0

* Tue Sep 20 2011 Matthias Clasen <mclasen@redhat.com> 3.1.92-1
- Update to 3.1.92

* Tue Sep  6 2011 Matthias Clasen <mclasen@redhat.com> 3.1.91-1
- Update to 3.1.91

* Wed Aug 31 2011 Matthias Clasen <mclasen@redhat.com> 3.1.90-1
- Update to 3.1.90

* Mon Aug 22 2011 Matthias Clasen <mclasen@redhat.com> 3.1.5-3
- Fix a crash without configured layouts

* Fri Aug 19 2011 Matthias Clasen <mclasen@redhat.com> 3.1.5-2
- Obsolete control-center-devel

* Thu Aug 18 2011 Matthias Clasen <mclasen@redhat.com> 3.1.5-1
- Update to 3.1.5

* Wed Aug 17 2011 Christoph Wickert <cwickert@fedoraproject.org> - 3.1.4-2
- Fix autostart behavior (#729271)

* Mon Jul 25 2011 Matthias Clasen <mclasen@redhat.com> 3.1.4-1
- Update to 3.1.4

* Mon Jul 04 2011 Bastien Nocera <bnocera@redhat.com> 3.1.3-1
- Update to 3.1.3

* Fri Jun 17 2011 Tomas Bzatek <tbzatek@redhat.com> - 3.0.2-1
- Update to 3.0.2

* Wed Jun 15 2011 Bastien Nocera <bnocera@redhat.com> 3.0.1.1-4
- Rebuild against new gnome-desktop3 libs

* Wed Apr 27 2011 Matthias Clasen <mclasen@redhat.com> - 3.0.1.1-3
- Rebuild against newer cheese-libs

* Tue Apr 26 2011 Matthias Clasen <mclasen@redhat.com> - 3.0.1.1-1
- Update to 3.0.1.1

* Tue Apr 26 2011 Bastien Nocera <bnocera@redhat.com> 3.0.1-1
- Update to 3.0.1

* Thu Apr  7 2011 Matthias Clasen <mclasen@redhat.com> 3.0.0.1-3
- Only autostart the sound applet in GNOME 3 (#693548)

* Wed Apr  6 2011 Matthias Clasen <mclasen@redhat.com> 3.0.0.1-2
- Add a way to connect to hidden access points

* Wed Apr  6 2011 Matthias Clasen <mclasen@redhat.com> 3.0.0.1-1
- Update to 3.0.0.1

* Mon Apr 04 2011 Bastien Nocera <bnocera@redhat.com> 3.0.0-1
- Update to 3.0.0

* Mon Mar 28 2011 Matthias Clasen <mclasen@redhat.com> 2.91.93-1
- 2.91.93

* Fri Mar 25 2011 Matthias Clasen <mclasen@redhat.com> 2.91.92-4
- Rebuild against newer cheese

* Thu Mar 24 2011 Matthias Clasen <mclasen@redhat.com> 2.91.92-3
- Rebuild against NetworkManager 0.9

* Mon Mar 21 2011 Matthias Clasen <mclasen@redhat.com> 2.91.92-1
- Update to 2.91.92

* Thu Mar 17 2011 Ray Strode <rstrode@redhat.com> 2.91.91-6
- Drop incomplete "Supervised" account type
  Resolves: #688363

* Tue Mar 15 2011 Bastien Nocera <bnocera@redhat.com> 2.91.91-5
- We now replace desktop-effects, with the info panel (#684565)

* Mon Mar 14 2011 Bastien Nocera <bnocera@redhat.com> 2.91.91-4
- Add gnome-icon-theme-symbolic dependency (#678696)

* Wed Mar 09 2011 Richard Hughes <rhughes@redhat.com> 2.91.91-3
- Ensure we have NetworkManager-glib-devel to get the network panel
- Explicitly list all the panels so we know if one goes missing

* Tue Mar  8 2011 Matthias Clasen <mclasen@redhat.com> 2.91.91-2
- Rebuild against NetworkManager 0.9, to get the network panel

* Tue Mar 08 2011 Bastien Nocera <bnocera@redhat.com> 2.91.91-1
- Update to 2.91.91
- Disable libsocialweb support until Flickr integration is fixed upstream

* Mon Feb 28 2011 Matthias Clasen <mclasen@redhat.com> - 1:2.91.90-2
- Fix a typo in the autostart condition for the sound applet

* Tue Feb 22 2011 Matthias Clasen <mclasen@redhat.com> - 1:2.91.90-1
- Update to 2.91.90

* Sun Feb 13 2011 Christopher Aillon <caillon@redhat.com> - 1:2.91.6-9
- Rebuild against new libxklavier

* Thu Feb 10 2011 Matthias Clasen <mclasen@redhat.com>  2.91.6-8
- Rebuild against newer gtk

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1:2.91.6-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Mon Feb 07 2011 Bastien Nocera <bnocera@redhat.com> 2.91.6-6
- Add missing apg Requires (#675227)

* Sat Feb 05 2011 Bastien Nocera <bnocera@redhat.com> 2.91.6-5
- Fix crasher running region and language with KDE apps installed

* Fri Feb 04 2011 Bastien Nocera <bnocera@redhat.com> 2.91.6-4
- Fix crasher running date and time on the live CD

* Thu Feb 03 2011 Bastien Nocera <bnocera@redhat.com> 2.91.6-3
- Add missing iso-codes dependencies

* Thu Feb 03 2011 Bastien Nocera <bnocera@redhat.com> 2.91.6-2
- Rebuild against newer GTK+ 3.x

* Wed Feb  2 2011 Matthias Clasen <mclasen@redhat.com> 2.91.6-1
- Update to 2.91.6

* Mon Jan 10 2011 Matthias Clasen <mclasen@redhat.com> 2.91.5-1
- Update to 2.91.5

* Sat Jan  8 2011 Matthias Clasen <mclasen@redhat.com> 2.91.4-1
- Update to 2.91.4

* Fri Dec 10 2010 Bill Nottingham <notting@redhat.com> 2.91.3-4
- user-accounts: require accountsserivce, obsolete accountsdialog

* Fri Dec  3 2010 Matthias Clasen <mclasen@redhat.com> 2.91.3-3
- Fix initial window size

* Fri Dec  3 2010 Matthias Clasen <mclasen@redhat.com> 2.91.3-2
- Rebuild against new gtk

* Wed Dec 01 2010 Bastien Nocera <bnocera@redhat.com> 2.91.3-1
- Update to 2.91.3

* Fri Nov 12 2010 Adam Williamson <awilliam@redhat.com> 2.91.2-2
- add upstream patch to fix sound module to link against libxml
  https://bugzilla.gnome.org/show_bug.cgi?id=634467

* Wed Nov 10 2010 Bastien Nocera <bnocera@redhat.com> 2.91.2-1
- Update to 2.91.2

* Wed Oct 06 2010 Richard Hughes <rhughes@redhat.com> 2.91.0-2
- Rebuild with a new gnome-settings-daemon

* Wed Oct 06 2010 Richard Hughes <rhughes@redhat.com> 2.91.0-1
- Update to 2.91.0

* Wed Sep 29 2010 jkeating - 1:2.90.1-4
- Rebuilt for gcc bug 634757

* Fri Sep 24 2010 Bastien Nocera <bnocera@redhat.com> 2.90.1-3
- Force enable libsocialweb support, it's disabled by default

* Fri Sep 24 2010 Bastien Nocera <bnocera@redhat.com> 2.90.1-2
- Add libsocialweb BR for the flickr support in background

* Wed Sep 22 2010 Bastien Nocera <bnocera@redhat.com> 2.90.1-1
- Update to 2.90.1

* Thu Aug 12 2010 Colin Walters <walters@verbum.org> - 1:2.31.6-1
- New upstream

* Wed Jul 21 2010 Bastien Nocera <bnocera@redhat.com> 2.31.5-2
- Trim BuildRequires
- Remove libgail-gnome dependency (#616632)

* Tue Jul 13 2010 Matthias Clasen <mclasen@redhat.com> 2.31.5-1
- Update to 2.31.5

* Wed Jun 30 2010 Matthias Clasen <mclasen@redhat.com> 2.31.4.2-1
- Update to 2.31.4.2

* Wed Jun 30 2010 Matthias Clasen <mclasen@redhat.com> 2.31.4.1-1
- Update to 2.31.4.1

* Wed Jun 23 2010 Bastien Nocera <bnocera@redhat.com> 2.31.3-2
- Add patches to compile against GTK+ 3.x

* Tue Jun  8 2010 Matthias Clasen <mclasen@redhat.com> 2.31.3-1
- Update to 2.31.3

* Wed Jun  2 2010 Matthias Clasen <mclasen@redhat.com> 2.31.2-3
- Add Provides/Obsoletes for the no-longer-existing -extra package

* Fri May 28 2010 Matthias Clasen <mclasen@redhat.com> 2.31.2-2
- Update to 2.31.2
- Remove vendor prefixes from desktop files, since that breaks
  the new shell

* Tue May 11 2010 Matthias Clasen <mclasen@redhat.com> 2.30.1-2
- Install PolicyKit policy for setting the default background
  in the right location

* Tue Apr 27 2010 Matthias Clasen <mclasen@redhat.com> 2.30.1-1
- Update to 2.30.1
- Spec file cleanups

* Mon Mar 29 2010 Matthias Clasen <mclasen@redhat.com> 2.30.0-1
- Update to 2.30.0

* Mon Mar 22 2010 Bastien Nocera <bnocera@redhat.com> 2.29.92-3
- Fix crash on exit in gnome-about-me (#574256)

* Wed Mar 10 2010 Bastien Nocera <bnocera@redhat.com> 2.29.92-2
- Remove obsoleted patches

* Tue Mar 09 2010 Bastien Nocera <bnocera@redhat.com> 2.29.92-1
- Update to 2.29.92

* Wed Feb 24 2010 Matthias Clasen <mclasen@redhat.com> - 2.29.91-1
- Update to 2.29.91

* Mon Feb 15 2010 Matthias Clasen <mclasen@redhat.com> - 2.29.90-2
- Properly initialize threads in the appearance capplet

* Wed Feb 10 2010 Bastien Nocera <bnocera@redhat.com> 2.29.90-1
- Update to 2.29.90

* Tue Jan 26 2010 Matthias Clasen <mclasen@redhat.com> - 2.29.6-1
- Update to 2.29.6

* Sun Jan 17 2010 Matthias Clasen <mclasen@redhat.com> - 2.29.4-2
- Rebuild

* Mon Jan  4 2010 Matthias Clasen <mclasen@redhat.com> - 2.29.4-1
- Update to 2.29.4
- Drop many upstreamed patches
