This is a Qt-based Web Browser core for libretro.

NOTE: This core has only been tested on Linux and is not guaranteed to work on any other platform yet.

The browser uses the webkit-based QWebView class which is available in Qt versions 5.5 and earlier. After 5.5, QWebView was replaced with QWebEngine, which is chromium-based, however that class does not yet support static linking, so for now we use the webkit version instead.

If you want to compile the browser as a simple standalone desktop application (outside of libretro), you can do so with (assuming Qt 5.5 or earlier):

qmake minibrowser-apptest.pro
make

To compile the libretro version, much more work is needed. A custom static build of Qt is required, with many options disabled (or set to be compiled in) to get rid of the extra dependencies and platform support that isn't used. Also, most system libraries, even if they do provide a static version to link with, usually are not provided with -fPIC enabled, so this is unsuitable for a libretro core.

To compile Qt, you will need to download the "qt-everywhere-opensource-src" package from www.qt.io, with a version no newer than 5.5.1. The files in the config/ directory provide the options to Qt's ./configure script that are known to work (NOTE: the OpenGL version is currently untested).

On an Ubuntu 16.04 x64 machine, at least the following system packages have been found to need recompilation to include -fPIC on their static libraries:

libfreetype6
libglib2.0-0
libgstreamer1.0-0
gstreamer1.0-plugins-base
libicu55
libpcre3
libxml2
libssl1.0.0
libsqlite3-0
zlib1g

You will find in the patches/ directory, diffs of the debian/rules files for each of these which can be used to rebuild the packages with the appropriate flags using "dpkg-buildpackage". The resulting libraries should be copied into libs/ so that the minibrowser makefile can find them. Please see Makefile.libretro for an exact listing of each file within libs/ that is needed.

Currently several system fonts are searched and if found, will be used for text rendering. As a fallback, GNU unifont is used. Make sure unifont.ttf is in your build directory before compiling minibrowser, as this file will be packed in as a resource.

Once all the required libraries are ready, then compile the libretro core with:

/path/to/static/Qt/bin/qmake minibrowser.pro
make
make -f Makefile.libretro
