dnl OpenBTS provides an open source alternative to legacy telco protocols and
dnl traditionally complex, proprietary hardware systems.

dnl Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
dnl Copyright 2014 Range Networks, Inc.

dnl This software is distributed under the terms of the GNU Affero General 
dnl Public License version 3. See the COPYING and NOTICE files in the main
dnl directory for licensing information.

dnl This use of this software may be subject to additional restrictions.
dnl See the LEGAL file in the main directory for details.


dnl FIXME -- Need to add dependency check for asn1c.

AC_PREREQ(2.57)
AC_INIT(openbts-umts,1.0-master)

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_CONFIG_AUX_DIR([.])	
AC_CONFIG_SRCDIR([Globals/Defines.h]) # project-wide defines
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([config])
AM_INIT_AUTOMAKE([-Wno-portability])

dnl Checks for programs.
: ${CXXFLAGS=""} ## remove auto setting of CXXFLAGS = -g -O2
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_AS # GNU radio
AC_PATH_PROG([RM_PROG], [rm]) # GNU radio

AC_ENABLE_SHARED	dnl do build shared libraries
AC_DISABLE_STATIC	dnl don't build static libraries

LT_INIT

dnl Checks for header files.
AC_HEADER_STDC
dnl This is required for GnuRadio includes to understand endianess correctly:
AC_CHECK_HEADERS([byteswap.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_C_BIGENDIAN

dnl Check for libzmq
if test ! -r "/usr/include/zmq.h" -a ! -r "/usr/local/include/zmq.h"; then
    AC_MSG_ERROR([/usr/local/include/zmq.h not found. Install the range-libzmq package or manually build and install with $ sudo ./NodeManager/install_libzmq.sh])
fi
if test ! -r "/usr/include/zmq.hpp" -a ! -r "/usr/local/include/zmq.hpp"; then
    AC_MSG_ERROR([/usr/local/include/zmq.hpp not found. Install the range-libzmq package or manually build and install with $ sudo ./NodeManager/install_libzmq.sh])
fi
AC_CHECK_LIB(zmq, zmq_init, ,[AC_MSG_ERROR([Cannot link with -lzmq.  Install the range-libzmq package or manually build and install with $ sudo ./NodeManager/install_libzmq.sh])])
AC_MSG_CHECKING([whether libzmq installation works])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <zmq.h>],
                                [zmq_init(1);])
               ],
               [AC_MSG_RESULT([yes])],
               [AC_MSG_ERROR([no. Install the range-libzmq package or manually build and install with $ sudo ./NodeManager/install_libzmq.sh])])

# Defines OSIP_CFLAGS, OSIP_INCLUDEDIR, and OSIP_LIBS
PKG_CHECK_MODULES(OSIP, libosip2)

# Defines ORTP_CFLAGS, ORTP_INCLUDEDIR, and ORTP_LIBS
PKG_CHECK_MODULES(ORTP, ortp)
AC_MSG_CHECKING([ortp api version])
AS_IF([gcc -I=/usr/local/include -o config/test_ortp_version config/test_ortp_version.cpp -lortp &> /dev/null], [
	AC_DEFINE(ORTP_NEW_API, 1, rtp_session_set_local_addr takes four arguments)
	AC_MSG_RESULT([>= 0.22.0])
	],
	[AC_MSG_RESULT([< 0.22.0])])

# Defines LIBUSB_TRANSFER_CANCELLED, LIBUSB_TRANSFER_COMPLETED, LIBUSB_SUCCESS, LIBUSB_ERROR_*
PKG_CHECK_MODULES(LIBUSB, libusb-1.0)

PKG_CHECK_MODULES(UHD, uhd >= 003.007.000,
    [AC_DEFINE(HAVE_UHD, 1, [Define if UHD found.])
     AM_CONDITIONAL(HAVE_UHD, true)
     AX_EXT],
    [AM_CONDITIONAL(HAVE_UHD, false)])

# Prepends -lreadline to LIBS and defines HAVE_LIBREADLINE in config.h
#AC_CHECK_LIB(readline, readline)


dnl Output files
AC_CONFIG_FILES([\
    Makefile \
    apps/Makefile \
    config/Makefile \
    doc/Makefile \
    CommonLibs/Makefile \
    Globals/Makefile \
    Control/Makefile \
    GSM/Makefile \
    UMTS/Makefile \
    SGSNGGSN/Makefile \
    SIP/Makefile \
    SMS/Makefile \
    TransceiverRAD1/Makefile \
    TransceiverUHD/Makefile \
    TRXManager/Makefile \
    CLI/Makefile \
    NodeManager/Makefile \
])

AC_OUTPUT
