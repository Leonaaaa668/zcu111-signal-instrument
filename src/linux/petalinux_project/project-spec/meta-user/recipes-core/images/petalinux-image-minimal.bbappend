# Network + SSH + root login + Weston autostart (added locally).

petalinux_local_setup_v4() {
    sed -i 's/^#*PermitRootLogin.*/PermitRootLogin yes/' ${IMAGE_ROOTFS}/etc/ssh/sshd_config

    install -d ${IMAGE_ROOTFS}/etc/systemd/system/multi-user.target.wants
    ln -sf /usr/lib/systemd/system/sshd.service ${IMAGE_ROOTFS}/etc/systemd/system/multi-user.target.wants/sshd.service
    ln -sf /usr/lib/systemd/system/weston.service ${IMAGE_ROOTFS}/etc/systemd/system/multi-user.target.wants/weston.service

    install -d ${IMAGE_ROOTFS}/etc/systemd/system/sshd.service.d
    printf '[Service]\nExecStartPre=/usr/bin/ssh-keygen -A\n' > ${IMAGE_ROOTFS}/etc/systemd/system/sshd.service.d/10-hostkeys.conf
}
ROOTFS_POSTPROCESS_COMMAND:append = " petalinux_local_setup_v4; "

# Submission provisioning: set INSTRUMENT_ROOT_PASSWORD_HASH in local.conf.
# Generate with: openssl passwd -6  (interactive, no plaintext in this repository).
instrument_set_root_password() {
    if [ -n "${INSTRUMENT_ROOT_PASSWORD_HASH}" ]; then
        sed -i 's|^root:[^:]*:|root:${INSTRUMENT_ROOT_PASSWORD_HASH}:|' ${IMAGE_ROOTFS}/etc/shadow
    fi
}
INSTRUMENT_ROOT_PASSWORD_HASH ?= ""
ROOTFS_POSTPROCESS_COMMAND:append = " instrument_set_root_password; "
