bin_PROGRAMS=detector
detector_SOURCES=Analyzer.cpp Analyzer.h ConfigParser.cpp ConfigParser.h DataDetector.cpp fields.cpp fields.h config.h
detector_LDADD=-ltrap -lpcap -lunirec
detector_CXXFLAGS=-std=c++11
EXTRA_DIST=README.md
pkgdocdir=${docdir}/detector
pkgdoc_DATA=README.md
include ../aminclude.am
