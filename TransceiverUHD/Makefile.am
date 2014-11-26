if HAVE_UHD
include $(top_srcdir)/Makefile.common

noinst_LTLIBRARIES = libumtstransceiver.la
noinst_PROGRAMS = transceiver
noinst_HEADERS = \
	RadioInterface.h \
	RadioDevice.h \
	Transceiver.h \
	SampleBuffer.h \
	UHDDevice.h \
	Resampler.h \
	convolve.h \
	convert.h

libumtstransceiver_la_CFLAGS = -Wall $(AM_CFLAGS) -std=gnu99 -march=native
libumtstransceiver_la_CPPFLAGS = -Wall $(AM_CPPFLAGS) $(UHD_CPPFLAGS)
libumtstransceiver_la_CXXFLAGS = -Wall $(AM_CXXFLAGS) $(UHD_CXXFLAGS)
libumtstransceiver_la_SOURCES = \
	RadioInterface.cpp \
	Transceiver.cpp \
	UHDDevice.cpp \
	SampleBuffer.cpp \
	Resampler.cpp \
	convolve.c \
	convert.c

transceiver_SOURCES = runTransceiver.cpp ../apps/GetConfigurationKeys.cpp
transceiver_LDADD = libumtstransceiver.la $(UHD_LIBS) $(UMTS_LA) $(GSM_LA) $(COMMON_LA) $(SQLITE_LA)

install: transceiver
	mkdir -p "$(DESTDIR)/OpenBTS/"
	install transceiver "$(DESTDIR)/OpenBTS/"
endif
