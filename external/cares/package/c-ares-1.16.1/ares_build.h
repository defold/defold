#ifndef __CARES_BUILD_H
#define __CARES_BUILD_H

#define CARES_TYPEOF_ARES_SOCKLEN_T socklen_t

// Defold patch: This is a combination of ares_build.h from win32/64 and other platforms
//               We simply frankenstein this to avoid having 8 different headers that
//               basically contain the same things.
#if defined(_WIN32)
	#ifndef WIN32_LEAN_AND_MEAN
	#  define WIN32_LEAN_AND_MEAN 1
	#endif

	#if defined(CARES_TYPEOF_ARES_SSIZE_T)
		#undef CARES_TYPEOF_ARES_SSIZE_T
	#endif
	#if defined(_WIN64)
		#define CARES_TYPEOF_ARES_SSIZE_T __int64
	#else
		#define CARES_TYPEOF_ARES_SSIZE_T int
	#endif

	#define CARES_HAVE_SYS_TYPES_H
	/* #undef CARES_HAVE_SYS_SOCKET_H */
	#define CARES_HAVE_WINDOWS_H
	#define CARES_HAVE_WS2TCPIP_H
	#define CARES_HAVE_WINSOCK2_H
	#define CARES_HAVE_WINDOWS_H
#else
	#define CARES_TYPEOF_ARES_SSIZE_T ssize_t
	#define CARES_HAVE_SYS_TYPES_H
	#define CARES_HAVE_SYS_SOCKET_H
	/* #undef CARES_HAVE_WINDOWS_H */
	/* #undef CARES_HAVE_WS2TCPIP_H */
	/* #undef CARES_HAVE_WINSOCK2_H */
	/* #undef CARES_HAVE_WINDOWS_H */
#endif

#ifdef CARES_HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#ifdef CARES_HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif

#ifdef CARES_HAVE_WINSOCK2_H
#  include <winsock2.h>
#endif

#ifdef CARES_HAVE_WS2TCPIP_H
#  include <ws2tcpip.h>
#endif

#ifdef CARES_HAVE_WINDOWS_H
#  include <windows.h>
#endif


typedef CARES_TYPEOF_ARES_SOCKLEN_T ares_socklen_t;
typedef CARES_TYPEOF_ARES_SSIZE_T ares_ssize_t;

#endif /* __CARES_BUILD_H */
