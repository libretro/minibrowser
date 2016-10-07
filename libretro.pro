#-------------------------------------------------
#
# Project created by QtCreator 2016-10-05T14:07:27
#
#-------------------------------------------------

QT += core gui webkit webkitwidgets
QT -= multimedia

#QMAKE_CC = gcc-4.8
#QMAKE_CXX = g++-4.8

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = minibrowser_libretro
TEMPLATE = lib
CONFIG += shared

SOURCES  += libretro.cpp

HEADERS  += libretro.h

LIBS += -L. -lminibrowser -L/usr/local/Qt-static-nongl-5.5.1/plugins/platforms -lqoffscreen
