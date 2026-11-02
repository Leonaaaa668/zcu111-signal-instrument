SUMMARY = "ZCU111 local measurement network static IPv4 configuration"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=8c1c5e532d93485f3a981cb750efaaf5"
SRC_URI = "file://LICENSE file://10-zcu111-end0.network"
S = "${@d.getVar('UNPACKDIR') or d.getVar('WORKDIR')}"
RDEPENDS:${PN} = "systemd"
do_install() {
    install -d ${D}${sysconfdir}/systemd/network
    install -m 0644 ${S}/10-zcu111-end0.network ${D}${sysconfdir}/systemd/network/
}
FILES:${PN} += "${sysconfdir}/systemd/network/10-zcu111-end0.network"
CONFFILES:${PN} += "${sysconfdir}/systemd/network/10-zcu111-end0.network"
