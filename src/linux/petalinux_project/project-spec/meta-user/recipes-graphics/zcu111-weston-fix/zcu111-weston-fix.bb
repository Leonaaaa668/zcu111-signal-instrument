SUMMARY = "Initialize the ZCU111 Weston GUI account before its PAM session"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=8c1c5e532d93485f3a981cb750efaaf5"
SRC_URI = "file://LICENSE file://zcu111-weston-account-init file://zcu111-weston-account-init.service file://10-zcu111-account.conf"
S = "${@d.getVar('UNPACKDIR') or d.getVar('WORKDIR')}"
inherit systemd
RDEPENDS:${PN} = "shadow busybox weston"
SYSTEMD_SERVICE:${PN} = "zcu111-weston-account-init.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
do_install() {
    install -d ${D}${sbindir} ${D}${systemd_system_unitdir}/weston.service.d
    install -m 0755 ${S}/zcu111-weston-account-init ${D}${sbindir}/zcu111-weston-account-init
    install -m 0644 ${S}/zcu111-weston-account-init.service ${D}${systemd_system_unitdir}/
    install -m 0644 ${S}/10-zcu111-account.conf ${D}${systemd_system_unitdir}/weston.service.d/
}
FILES:${PN} += "${systemd_system_unitdir}/weston.service.d/10-zcu111-account.conf"
