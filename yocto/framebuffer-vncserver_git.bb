DESCRIPTION = "VNC server running on top of framebuffer"
HOMEPAGE = "https://github.com/ponty/framebuffer-vncserver"
LICENSE = "GPLv2"
DEPENDS = "libvncserver"
RDEPENDS_${PN} = "libvncserver"
SRC_URI = "git://github.com/ponty/framebuffer-vncserver.git"
LIC_FILES_CHKSUM = "file://LICENSE;md5=8264535c0c4e9c6c335635c4026a8022"
PR = "r1"
PR_append = "+gitr${SRCPV}"
S = "${WORKDIR}/git"
inherit cmake

SRCREV = "d3450b1a5ba7ebd7c3b6bcf5affda0975441cee2"

