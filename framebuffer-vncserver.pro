TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += src/framebuffer-vncserver.c

include(deployment.pri)
qtcAddDeployment()


LIBS += -lvncserver
