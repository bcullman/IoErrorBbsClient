/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client.h"
#include "client_globals.h"
#include "config_globals.h"
#include "defs.h"
#include "macos_keychain.h"
#include "network_globals.h"
#include "unix.h"
#include "utility.h"
typedef struct
{
   int errorCode;
   int messageKind;
} ErrorMessageTemplate;

enum
{
   HOST_LOOKUP_COULD_NOT_RESOLVE,
   HOST_LOOKUP_TEMPORARY_DNS_FAILURE,
   HOST_LOOKUP_ADDRESS_FAMILY_UNSUPPORTED,
   HOST_LOOKUP_SERVICE_LOOKUP_FAILED,
   HOST_LOOKUP_GENERIC_FAILURE,
   SOCKET_CONNECT_REFUSED,
   SOCKET_CONNECT_TIMED_OUT,
   SOCKET_CONNECT_HOST_UNREACHABLE,
   SOCKET_CONNECT_HOST_DOWN,
   SOCKET_CONNECT_NETWORK_UNREACHABLE,
   SOCKET_CONNECT_NETWORK_DOWN,
   SOCKET_CONNECT_ADDRESS_NOT_AVAILABLE,
   SOCKET_CONNECT_ADDRESS_FAMILY_UNSUPPORTED,
   SOCKET_CONNECT_ABORTED,
   SOCKET_CONNECT_GENERIC_FAILURE
};

static const ErrorMessageTemplate aryHostLookupErrors[] =
   {
#ifdef EAI_NONAME
      { EAI_NONAME, HOST_LOOKUP_COULD_NOT_RESOLVE },
#endif
#ifdef EAI_AGAIN
      { EAI_AGAIN, HOST_LOOKUP_TEMPORARY_DNS_FAILURE },
#endif
#ifdef EAI_FAMILY
      { EAI_FAMILY, HOST_LOOKUP_ADDRESS_FAMILY_UNSUPPORTED },
#endif
#ifdef EAI_SERVICE
      { EAI_SERVICE, HOST_LOOKUP_SERVICE_LOOKUP_FAILED },
#endif
};

static const ErrorMessageTemplate arySocketConnectErrors[] =
   {
#ifdef ECONNREFUSED
      { ECONNREFUSED, SOCKET_CONNECT_REFUSED },
#endif
#ifdef ETIMEDOUT
      { ETIMEDOUT, SOCKET_CONNECT_TIMED_OUT },
#endif
#ifdef EHOSTUNREACH
      { EHOSTUNREACH, SOCKET_CONNECT_HOST_UNREACHABLE },
#endif
#ifdef EHOSTDOWN
      { EHOSTDOWN, SOCKET_CONNECT_HOST_DOWN },
#endif
#ifdef ENETUNREACH
      { ENETUNREACH, SOCKET_CONNECT_NETWORK_UNREACHABLE },
#endif
#ifdef ENETDOWN
      { ENETDOWN, SOCKET_CONNECT_NETWORK_DOWN },
#endif
#ifdef EADDRNOTAVAIL
      { EADDRNOTAVAIL, SOCKET_CONNECT_ADDRESS_NOT_AVAILABLE },
#endif
#ifdef EAFNOSUPPORT
      { EAFNOSUPPORT, SOCKET_CONNECT_ADDRESS_FAMILY_UNSUPPORTED },
#endif
#ifdef ECONNABORTED
      { ECONNABORTED, SOCKET_CONNECT_ABORTED },
#endif
};

static void configureTcpKeepalive( int socketFileDescriptor, bool isEnabled );
static noreturn void failHostLookup( const char *ptrHost, const char *ptrPort,
                                     int lookupResult );
static noreturn void failSocketConnect( const char *ptrHost, const char *ptrPort,
                                        int connectionErrno );
static int findErrorMessageKind( int errorCode,
                                 const ErrorMessageTemplate *ptrTemplates,
                                 size_t templateCount, int defaultMessageKind );
static void formatHostLookupMessage( char *ptrBuffer, size_t bufferSize,
                                     int messageKind, const char *ptrHost,
                                     const char *ptrPort );
static void formatSocketConnectMessage( char *ptrBuffer, size_t bufferSize,
                                        int messageKind, const char *ptrHost,
                                        const char *ptrPort );

/// @brief Apply TCP keepalive settings to the active socket.
///
/// @param socketFileDescriptor Connected socket file descriptor.
/// @param isEnabled Non-zero to enable keepalive probing, zero to disable it.
///
/// @return This function does not return a value.
static void configureTcpKeepalive( int socketFileDescriptor, bool isEnabled )
{
   int enabledValue;

   if ( socketFileDescriptor < 0 )
   {
      return;
   }

   enabledValue = isEnabled ? 1 : 0;
   if ( setsockopt( socketFileDescriptor, SOL_SOCKET, SO_KEEPALIVE, &enabledValue, sizeof( enabledValue ) ) < 0 )
   {
      return;
   }
   if ( !isEnabled )
   {
      return;
   }

   // Keepalive defaults are conservative to avoid noticeable server load while
   // still preventing common ISP/NAT idle disconnects on long-lived telnet sessions.
#if defined( TCP_KEEPALIVE )
   {
      int keepaliveIdleSeconds = 120;
      (void)setsockopt( socketFileDescriptor,
                        IPPROTO_TCP,
                        TCP_KEEPALIVE,
                        &keepaliveIdleSeconds,
                        sizeof( keepaliveIdleSeconds ) );
   }
#elif defined( TCP_KEEPIDLE )
   {
      int keepaliveIdleSeconds = 120;
      (void)setsockopt( socketFileDescriptor,
                        IPPROTO_TCP,
                        TCP_KEEPIDLE,
                        &keepaliveIdleSeconds,
                        sizeof( keepaliveIdleSeconds ) );
   }
#endif
#ifdef TCP_KEEPINTVL
   {
      int keepaliveIntervalSeconds = 30;
      (void)setsockopt( socketFileDescriptor,
                        IPPROTO_TCP,
                        TCP_KEEPINTVL,
                        &keepaliveIntervalSeconds,
                        sizeof( keepaliveIntervalSeconds ) );
   }
#endif
#ifdef TCP_KEEPCNT
   {
      int keepaliveProbeCount = 3;
      (void)setsockopt( socketFileDescriptor,
                        IPPROTO_TCP,
                        TCP_KEEPCNT,
                        &keepaliveProbeCount,
                        sizeof( keepaliveProbeCount ) );
   }
#endif
}

/// @brief Resolve the target host and open the main BBS network connection.
///
/// The function reports progress for host lookup and socket connect before
/// handing the connected socket back to the rest of the client.
///
/// @return This function does not return a value.
void connectBbs( void )
{
   register int connectResult;
   struct addrinfo addressHints;
   struct addrinfo *ptrAddressInfo;
   struct addrinfo *ptrAddressList;
   int savedErrno;
   char aryPortString[8];
   const char *ptrLookupHost;

   clearKeychainSessionState();
   if ( !*aryBbsHost )
   {
      snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", BBS_HOSTNAME );
   }
   if ( !bbsPort )
   {
      bbsPort = BBS_PORT_NUMBER;
   }
   if ( !*aryCommandLineHost )
   {
      snprintf( aryCommandLineHost, sizeof( aryCommandLineHost ), "%s", aryBbsHost );
   }
   if ( !cmdLinePort )
   {
      cmdLinePort = bbsPort;
   }

   snprintf( aryPortString, sizeof( aryPortString ), "%u", (unsigned int)cmdLinePort );
   memset( &addressHints, 0, sizeof( addressHints ) );
   addressHints.ai_family = AF_INET;
   addressHints.ai_socktype = SOCK_STREAM;

   ptrLookupHost = aryCommandLineHost;
   stdPrintf( "Connection to: %s:%s\n", ptrLookupHost, aryPortString );
   stdPrintf( "Looking up host... " );
   fflush( stdout );
   connectResult = getaddrinfo( ptrLookupHost, aryPortString, &addressHints, &ptrAddressList );
   if ( connectResult != 0 )
   {
      ptrLookupHost = BBS_IP_ADDRESS;
      connectResult = getaddrinfo( ptrLookupHost, aryPortString, &addressHints, &ptrAddressList );
   }
   if ( connectResult != 0 )
   {
      stdPrintf( "failed.\n" );
      failHostLookup( aryCommandLineHost, aryPortString, connectResult );
   }
   stdPrintf( "done.\n" );

   stdPrintf( "Opening connection... " );
   fflush( stdout );
   net = -1;
   savedErrno = 0;
   for ( ptrAddressInfo = ptrAddressList; ptrAddressInfo != NULL; ptrAddressInfo = ptrAddressInfo->ai_next )
   {
      net = socket( ptrAddressInfo->ai_family,
                    ptrAddressInfo->ai_socktype,
                    ptrAddressInfo->ai_protocol );
      if ( net < 0 )
      {
         savedErrno = errno;
         continue;
      }

      // Client configuration controls keepalive probes.
      configureTcpKeepalive( net, flagsConfiguration.shouldUseTcpKeepalive );

      connectResult = connect( net, ptrAddressInfo->ai_addr, ptrAddressInfo->ai_addrlen );
      if ( connectResult == 0 )
      {
         break;
      }

      savedErrno = errno;
      close( net );
      net = -1;
   }
   freeaddrinfo( ptrAddressList );

   if ( net < 0 )
   {
      stdPrintf( "failed.\n" );
      failSocketConnect( ptrLookupHost, aryPortString, savedErrno );
   }
   stdPrintf( "done.\n" );
   stdPrintf( "[Connection Established]\n" );
   titleBar();
   fflush( stdout );

   // Let the stdio libraries handle output buffering. Input buffering still
   // relies on the lower-level path for portability.
   if ( !( netOutputFile = fdopen( net, "w" ) ) )
   {
      fatalPerror( "fdopen w", "Local error" );
   }
}

/// @brief Abort after a host lookup failure with a categorized message.
///
/// @param ptrHost Host name that was being resolved.
/// @param ptrPort Service or port string being resolved.
/// @param lookupResult `getaddrinfo()` error code.
///
/// @return This function does not return to the caller.
static noreturn void failHostLookup( const char *ptrHost, const char *ptrPort,
                                     int lookupResult )
{
   const char *ptrReason;
   char aryMessage[256];
   int messageKind;

   ptrReason = gai_strerror( lookupResult );
   messageKind = findErrorMessageKind( lookupResult, aryHostLookupErrors,
                                       sizeof( aryHostLookupErrors ) / sizeof( aryHostLookupErrors[0] ),
                                       HOST_LOOKUP_GENERIC_FAILURE );
   formatHostLookupMessage( aryMessage, sizeof( aryMessage ), messageKind, ptrHost, ptrPort );

   fatalExit( ptrReason, aryMessage );
}

/// @brief Abort after a socket connect failure with a categorized message.
///
/// @param ptrHost Host name that was being contacted.
/// @param ptrPort Port string that was being contacted.
/// @param connectionErrno Saved `errno` from the failed connect attempt.
///
/// @return This function does not return to the caller.
static noreturn void failSocketConnect( const char *ptrHost, const char *ptrPort,
                                        int connectionErrno )
{
   char aryMessage[256];
   int messageKind;

   messageKind = findErrorMessageKind( connectionErrno, arySocketConnectErrors,
                                       sizeof( arySocketConnectErrors ) / sizeof( arySocketConnectErrors[0] ),
                                       SOCKET_CONNECT_GENERIC_FAILURE );
   formatSocketConnectMessage( aryMessage, sizeof( aryMessage ), messageKind, ptrHost, ptrPort );

   errno = connectionErrno;
   fatalPerror( "connect", aryMessage );
}

/// @brief Map a platform error code to one of the client message categories.
///
/// @param errorCode Error value to classify.
/// @param ptrTemplates Mapping table to search.
/// @param templateCount Number of entries in `ptrTemplates`.
/// @param defaultMessageKind Fallback category when no mapping matches.
///
/// @return The chosen message kind.
static int findErrorMessageKind( int errorCode,
                                 const ErrorMessageTemplate *ptrTemplates,
                                 size_t templateCount, int defaultMessageKind )
{
   size_t itemIndex;

   for ( itemIndex = 0; itemIndex < templateCount; itemIndex++ )
   {
      if ( ptrTemplates[itemIndex].errorCode == errorCode )
      {
         return ptrTemplates[itemIndex].messageKind;
      }
   }
   return defaultMessageKind;
}

/// @brief Format a user-facing host lookup error message.
///
/// @param ptrBuffer Destination buffer.
/// @param bufferSize Size of `ptrBuffer`.
/// @param messageKind Categorized host lookup failure.
/// @param ptrHost Host name that failed.
/// @param ptrPort Port string that failed.
///
/// @return This function does not return a value.
static void formatHostLookupMessage( char *ptrBuffer, size_t bufferSize,
                                     int messageKind, const char *ptrHost,
                                     const char *ptrPort )
{
   switch ( messageKind )
   {
      case HOST_LOOKUP_COULD_NOT_RESOLVE:
         snprintf( ptrBuffer, bufferSize, "Could not resolve %s:%s.", ptrHost, ptrPort );
         break;
      case HOST_LOOKUP_TEMPORARY_DNS_FAILURE:
         snprintf( ptrBuffer, bufferSize,
                   "Temporary DNS failure while looking up %s:%s.", ptrHost, ptrPort );
         break;
      case HOST_LOOKUP_ADDRESS_FAMILY_UNSUPPORTED:
         snprintf( ptrBuffer, bufferSize,
                   "Address family for %s:%s is not supported.", ptrHost, ptrPort );
         break;
      case HOST_LOOKUP_SERVICE_LOOKUP_FAILED:
         snprintf( ptrBuffer, bufferSize,
                   "Service lookup failed for %s:%s.", ptrHost, ptrPort );
         break;
      default:
         snprintf( ptrBuffer, bufferSize,
                   "Host lookup failed for %s:%s.", ptrHost, ptrPort );
         break;
   }
}

/// @brief Format a user-facing socket connect error message.
///
/// @param ptrBuffer Destination buffer.
/// @param bufferSize Size of `ptrBuffer`.
/// @param messageKind Categorized connect failure.
/// @param ptrHost Host name that failed.
/// @param ptrPort Port string that failed.
///
/// @return This function does not return a value.
static void formatSocketConnectMessage( char *ptrBuffer, size_t bufferSize,
                                        int messageKind, const char *ptrHost,
                                        const char *ptrPort )
{
   switch ( messageKind )
   {
      case SOCKET_CONNECT_REFUSED:
         snprintf( ptrBuffer, bufferSize,
                   "Connection to %s:%s was refused by the server.", ptrHost, ptrPort );
         break;
      case SOCKET_CONNECT_TIMED_OUT:
         snprintf( ptrBuffer, bufferSize,
                   "Connection to %s:%s timed out.", ptrHost, ptrPort );
         break;
      case SOCKET_CONNECT_HOST_UNREACHABLE:
         snprintf( ptrBuffer, bufferSize,
                   "Host %s:%s is unreachable.", ptrHost, ptrPort );
         break;
      case SOCKET_CONNECT_HOST_DOWN:
         snprintf( ptrBuffer, bufferSize,
                   "Host %s:%s appears to be down.", ptrHost, ptrPort );
         break;
      case SOCKET_CONNECT_NETWORK_UNREACHABLE:
         snprintf( ptrBuffer, bufferSize,
                   "The network path to %s:%s is unreachable.", ptrHost, ptrPort );
         break;
      case SOCKET_CONNECT_NETWORK_DOWN:
         snprintf( ptrBuffer, bufferSize,
                   "The local network is down while connecting to %s:%s.", ptrHost, ptrPort );
         break;
      case SOCKET_CONNECT_ADDRESS_NOT_AVAILABLE:
         snprintf( ptrBuffer, bufferSize,
                   "No usable local address is available for %s:%s.", ptrHost, ptrPort );
         break;
      case SOCKET_CONNECT_ADDRESS_FAMILY_UNSUPPORTED:
         snprintf( ptrBuffer, bufferSize,
                   "The resolved address family for %s:%s is not supported.", ptrHost, ptrPort );
         break;
      case SOCKET_CONNECT_ABORTED:
         snprintf( ptrBuffer, bufferSize,
                   "Connection to %s:%s was aborted during setup.", ptrHost, ptrPort );
         break;
      default:
         snprintf( ptrBuffer, bufferSize,
                   "Could not connect to %s:%s.", ptrHost, ptrPort );
         break;
   }
}

/// @brief Wait for local or network activity.
///
/// While a child process is active, local terminal input is ignored.
///
/// @return `1` for local input, `2` for network input, `3` for both, or `-1`
/// if the `select()` call fails.
int waitNextEvent( void )
{
   fd_set fdr;
   int result;

   while ( true )
   {
      FD_ZERO( &fdr );
      if ( !childPid && !flagsConfiguration.shouldCheckExpress )
      {
         FD_SET( 0, &fdr );
      }
      if ( !shouldIgnoreNetwork )
      {
         FD_SET( net, &fdr );
      }

      if ( select( shouldIgnoreNetwork ? 1 : net + 1, &fdr, 0, 0, 0 ) < 0 )
      {
         if ( errno == EINTR )
         {
            continue;
         }
         else
         {
            stdPrintf( "\r\n" );
            fatalPerror( "select", "Local error" );
         }
      }

      if ( ( result = ( ( FD_ISSET( net, &fdr ) != 0 ) << 1 | ( FD_ISSET( 0, &fdr ) != 0 ) ) ) )
      {
         return ( result );
      }
   }
}
