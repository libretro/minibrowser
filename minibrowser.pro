#-------------------------------------------------
#
# Project created by QtCreator 2016-10-05T14:07:27
#
#-------------------------------------------------

QT       += core gui webkit webkitwidgets

#QMAKE_CC = gcc-4.8
#QMAKE_CXX = g++-4.8

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = minibrowser
TEMPLATE = lib

SOURCES  += minibrowser.cpp

HEADERS  += minibrowser.h

FORMS    += minibrowser.ui

RESOURCES = res.qrc
