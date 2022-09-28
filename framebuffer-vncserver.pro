TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += src/framebuffer-vncserver.c
SOURCES += src/keyboard.c
SOURCES += src/touch.c
SOURCES += src/mouse.c

include(deployment.pri)
qtcAddDeployment()


LIBS += -lvncserver

DISTFILES += \
    README.md
