dnl Process this file with autoconf to produce a configure script.
AC_INIT([IoErrorBbsClient],[2.3.10-Stilgar])
AC_CONFIG_SRCDIR([main.c])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CANONICAL_HOST

AC_ARG_ENABLE(keychain,
   [  --enable-keychain          enable macOS Keychain support],
   [],
   [enable_keychain=no])
AC_ARG_ENABLE(native-optimizations,
   [  --disable-native-optimizations  disable host-specific tuning flags],
   [],
   [enable_native_optimizations=yes])
AC_ARG_ENABLE(release-build,
   [  --enable-release-build         build an optimized release binary],
   [],
   [enable_release_build=no])
AC_ARG_ENABLE(universal-binary,
   [  --enable-universal-binary       build a universal macOS binary for Apple Silicon and Intel],
   [],
   [enable_universal_binary=no])

dnl Checks for programs.
if test "x$CC" = "x"; then
   case "$host_os" in
   darwin*)
      AC_CHECK_PROGS([DEFAULT_CLANG], [clang], [no])
      if test "x$DEFAULT_CLANG" != "xno"; then
         CC="$DEFAULT_CLANG"
      fi
      ;;
   esac
fi

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_CHECK_TOOL([DSYMUTIL], [dsymutil], [no])
AC_CHECK_TOOL([STRIP], [strip], [no])

platformDisplayName="Unix"
platformSupportMode="least-effort"
case "$host_os" in
darwin*)
   platformDisplayName="macOS"
   platformSupportMode="supported"
   ;;
linux*)
   platformDisplayName="Linux"
   ;;
*)
   AC_MSG_WARN([host OS $host_os is not a primary target; build will proceed with least-effort Unix support])
   ;;
esac

compilerFamily="Unknown"
isClangCompiler=no
isAppleClangCompiler=no
AC_MSG_CHECKING([whether $CC is Clang])
AC_COMPILE_IFELSE(
   [AC_LANG_PROGRAM([[
#ifndef __clang__
#error not clang
#endif
   ]], [[]])],
   [
      AC_MSG_RESULT([yes])
      isClangCompiler=yes
      compilerFamily="Clang"
      AC_MSG_CHECKING([whether $CC is Apple Clang])
      AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM([[
#ifndef __apple_build_version__
#error not apple clang
#endif
         ]], [[]])],
         [
            AC_MSG_RESULT([yes])
            isAppleClangCompiler=yes
            compilerFamily="Apple Clang"
         ],
         [AC_MSG_RESULT([no])]
      )
   ],
   [
      AC_MSG_RESULT([no])
      compilerFamily="GCC-compatible"
   ]
)

OPTIMIZATION_CFLAGS=""
LTO_CFLAGS=""
LTO_LDFLAGS=""
STACK_PROTECTOR_CFLAGS=""
WARNING_CFLAGS=""
SANITIZER_CFLAGS=""
SANITIZER_LDFLAGS=""
UNIVERSAL_CFLAGS=""
UNIVERSAL_LDFLAGS=""
KEYCHAIN_LDFLAGS=""

if test "x$ac_env_CFLAGS_set" != "xset"; then
   if test "x$enable_release_build" = "xyes"; then
      CFLAGS="-O2 -g -DNDEBUG -UDEBUG"
   else
      CFLAGS="-g -Og -DDEBUG -UNDEBUG"
   fi
fi

for warningFlag in "-Wall" "-Wextra" "-Wformat=2" "-Wformat-security" "-Werror=format-security";
do
   saved_CFLAGS="$CFLAGS"
   CFLAGS="$CFLAGS $warningFlag"
   AC_MSG_CHECKING([whether $CC accepts $warningFlag])
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
      [
         AC_MSG_RESULT([yes])
         WARNING_CFLAGS="$WARNING_CFLAGS $warningFlag"
      ],
      [AC_MSG_RESULT([no])]
   )
   CFLAGS="$saved_CFLAGS"
done

if test "x$enable_release_build" = "xyes" && test "x$enable_universal_binary" != "xyes"; then
   ltoFlags="-flto"
   saved_CFLAGS="$CFLAGS"
   saved_LDFLAGS="$LDFLAGS"
   CFLAGS="$CFLAGS $ltoFlags"
   LDFLAGS="$LDFLAGS $ltoFlags"
   AC_MSG_CHECKING([whether $CC supports release LTO flags])
   AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
      [
         AC_MSG_RESULT([yes])
         LTO_CFLAGS="$ltoFlags"
         LTO_LDFLAGS="$ltoFlags"
      ],
      [AC_MSG_RESULT([no])]
   )
   CFLAGS="$saved_CFLAGS"
   LDFLAGS="$saved_LDFLAGS"
fi

for stackProtectorFlag in "-fstack-protector-strong" "-fstack-protector";
do
   saved_CFLAGS="$CFLAGS"
   CFLAGS="$CFLAGS $stackProtectorFlag"
   AC_MSG_CHECKING([whether $CC accepts $stackProtectorFlag])
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
      [
         AC_MSG_RESULT([yes])
         STACK_PROTECTOR_CFLAGS="$stackProtectorFlag"
         CFLAGS="$saved_CFLAGS"
         break
      ],
      [AC_MSG_RESULT([no])]
   )
   CFLAGS="$saved_CFLAGS"
done

if test "x$enable_release_build" != "xyes" && test "x$enable_universal_binary" != "xyes"; then
   sanitizerCflags="-fsanitize=address,undefined -fno-omit-frame-pointer"
   sanitizerLdflags="-fsanitize=address,undefined"
   saved_CFLAGS="$CFLAGS"
   saved_LDFLAGS="$LDFLAGS"
   CFLAGS="$CFLAGS $sanitizerCflags"
   LDFLAGS="$LDFLAGS $sanitizerLdflags"
   AC_MSG_CHECKING([whether $CC supports development sanitizer flags])
   AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
      [
         AC_MSG_RESULT([yes])
         SANITIZER_CFLAGS="$sanitizerCflags"
         SANITIZER_LDFLAGS="$sanitizerLdflags"
      ],
      [
         AC_MSG_RESULT([no])
         AC_MSG_WARN([development sanitizer flags are unavailable with this compiler; continuing without them])
      ]
   )
   CFLAGS="$saved_CFLAGS"
   LDFLAGS="$saved_LDFLAGS"
fi

case "$host_os" in
darwin*)
   if test "x$enable_universal_binary" = "xyes"; then
      universalFlags="-arch arm64 -arch x86_64"
      saved_CFLAGS="$CFLAGS"
      saved_LDFLAGS="$LDFLAGS"
      CFLAGS="$CFLAGS $universalFlags"
      LDFLAGS="$LDFLAGS $universalFlags"
      AC_MSG_CHECKING([whether $CC supports macOS universal binary flags])
      AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
         [
            AC_MSG_RESULT([yes])
            UNIVERSAL_CFLAGS="$universalFlags"
            UNIVERSAL_LDFLAGS="$universalFlags"
         ],
         [
            AC_MSG_RESULT([no])
            AC_MSG_ERROR([--enable-universal-binary requires compiler support for -arch arm64 -arch x86_64])
         ]
      )
      CFLAGS="$saved_CFLAGS"
      LDFLAGS="$saved_LDFLAGS"
   fi
   ;;
esac

keychainMode="disabled"
if test "x$enable_keychain" = "xyes"; then
   case "$host_os" in
   darwin*)
      saved_LDFLAGS="$LDFLAGS"
      LDFLAGS="$LDFLAGS -framework Security -framework CoreFoundation"
      AC_MSG_CHECKING([whether $CC links with Security.framework and CoreFoundation.framework])
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <Security/Security.h>]], [[CFMutableDictionaryRef queryRef = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks); if ( queryRef != NULL ) { CFDictionarySetValue(queryRef, kSecClass, kSecClassGenericPassword); CFRelease(queryRef); }]])],
         [
            AC_MSG_RESULT([yes])
            KEYCHAIN_LDFLAGS="-framework Security -framework CoreFoundation"
            keychainMode="enabled"
            AC_DEFINE([ENABLE_KEYCHAIN], [1],
               [Define if macOS Keychain support is enabled])
         ],
         [
            AC_MSG_RESULT([no])
            AC_MSG_ERROR([--enable-keychain requires Security.framework support])
         ]
      )
      LDFLAGS="$saved_LDFLAGS"
      ;;
   *)
      AC_MSG_WARN([--enable-keychain is only supported on macOS; disabling keychain support for $host_os])
      enable_keychain=no
      ;;
   esac
fi

if test "x$enable_native_optimizations" = "xyes" && test "x$ac_env_CFLAGS_set" != "xset"; then
   case "$host_cpu" in
   arm64|aarch64)
      if test "x$host_os" = "xdarwin"; then
         optimizationFlagCandidates="-mcpu=native -mcpu=apple-m1"
      else
         optimizationFlagCandidates="-mcpu=native"
      fi
      for optimizationFlag in $optimizationFlagCandidates; do
         saved_CFLAGS="$CFLAGS"
         CFLAGS="$CFLAGS $optimizationFlag"
         AC_MSG_CHECKING([whether $CC accepts $optimizationFlag])
         AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
            [
               AC_MSG_RESULT([yes])
               OPTIMIZATION_CFLAGS="$OPTIMIZATION_CFLAGS $optimizationFlag"
               CFLAGS="$saved_CFLAGS"
               break
            ],
            [AC_MSG_RESULT([no])]
         )
         CFLAGS="$saved_CFLAGS"
      done
      ;;
   x86_64|amd64)
      for optimizationFlag in "-march=native" "-mtune=native"; do
         saved_CFLAGS="$CFLAGS"
         CFLAGS="$CFLAGS $optimizationFlag"
         AC_MSG_CHECKING([whether $CC accepts $optimizationFlag])
         AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
            [
               AC_MSG_RESULT([yes])
               OPTIMIZATION_CFLAGS="$OPTIMIZATION_CFLAGS $optimizationFlag"
            ],
            [AC_MSG_RESULT([no])]
         )
         CFLAGS="$saved_CFLAGS"
      done
      ;;
   esac
fi

dnl Checks for libraries.
dnl Replace `main' with a function in -linet:
AC_CHECK_LIB(inet, main)
dnl Replace `main' with a function in -lnsl:
AC_CHECK_LIB(nsl, main)
dnl Replace `main' with a function in -lresolv:
AC_CHECK_LIB(resolv, main)
dnl Replace `main' with a function in -lseq:
AC_CHECK_LIB(seq, main)
dnl Replace `main' with a function in -lsocket:
AC_CHECK_LIB(socket, main)
dnl Replace `main' with a function in -lutil:
AC_CHECK_LIB(util, main)

PLATFORM_OBJS="unix.o"

dnl Checks for header files.
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(sgtty.h stdint.h sys/ioctl.h sys/time.h termio.h time.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

dnl Checks for library functions.
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(select socket)

AC_PATH_PROG([PKG_CONFIG], [pkg-config], [no])
CMOCKA_CFLAGS=""
CMOCKA_LIBS=""
have_cmocka=no
if test "x$PKG_CONFIG" != "xno"; then
   if $PKG_CONFIG --exists cmocka; then
      have_cmocka=yes
      CMOCKA_CFLAGS=`$PKG_CONFIG --cflags cmocka`
      CMOCKA_LIBS=`$PKG_CONFIG --libs cmocka`
   fi
fi
AM_CONDITIONAL([HAVE_CMOCKA], [test "x$have_cmocka" = "xyes"])
if test "x$have_cmocka" = "xyes"; then
   AC_DEFINE([HAVE_CMOCKA], [1], [Define if cmocka is available for unit tests])
fi
AC_SUBST([CMOCKA_CFLAGS])
AC_SUBST([CMOCKA_LIBS])
AC_SUBST([DSYMUTIL])
AC_SUBST([LTO_CFLAGS])
AC_SUBST([LTO_LDFLAGS])
AC_SUBST([OPTIMIZATION_CFLAGS])
AC_SUBST([RELEASE_BUILD_ENABLED], [$enable_release_build])
AC_SUBST([STACK_PROTECTOR_CFLAGS])
AC_SUBST([WARNING_CFLAGS])
AC_SUBST([SANITIZER_CFLAGS])
AC_SUBST([SANITIZER_LDFLAGS])
AC_SUBST([STRIP])
AC_SUBST([UNIVERSAL_CFLAGS])
AC_SUBST([UNIVERSAL_LDFLAGS])
AC_SUBST([HOST_TRIPLE], [$host])
AC_SUBST([KEYCHAIN_LDFLAGS])

if test "x$enable_release_build" = "xyes"; then
   buildMode="release"
   optimizationMode="release"
else
   buildMode="dev"
   optimizationMode="development"
fi

if test "x$LTO_CFLAGS" != "x"; then
   optimizationMode="$optimizationMode + lto"
fi

if test "x$SANITIZER_CFLAGS" != "x"; then
   sanitizerMode="enabled"
else
   sanitizerMode="disabled"
fi

if test "x$OPTIMIZATION_CFLAGS" != "x"; then
   nativeOptimizationMode="enabled"
else
   nativeOptimizationMode="disabled"
fi

if test "x$enable_universal_binary" = "xyes"; then
   universalMode="enabled"
else
   universalMode="disabled"
fi

if test "x$STACK_PROTECTOR_CFLAGS" != "x"; then
   stackProtectorMode="$STACK_PROTECTOR_CFLAGS"
else
   stackProtectorMode="disabled"
fi

if test "x$have_cmocka" = "xyes"; then
   cmockaMode="found"
else
   cmockaMode="not found"
fi

compilerSummary="$CC ($compilerFamily)"

AC_DEFINE_UNQUOTED([HOSTTYPE], ["$host"],
   [Define to the canonical host])
AC_DEFINE_UNQUOTED([BUILD_VERSION], ["$PACKAGE_VERSION"],
   [Define to the configured release version string])
AC_DEFINE_UNQUOTED([BUILD_COMPILER], ["$compilerSummary"],
   [Define to the selected compiler summary])
AC_DEFINE_UNQUOTED([BUILD_MODE], ["$buildMode"],
   [Define to the selected build mode])
AC_DEFINE_UNQUOTED([BUILD_OPTIMIZATION_MODE], ["$optimizationMode"],
   [Define to the selected optimization mode])
AC_DEFINE_UNQUOTED([BUILD_PLATFORM], ["$platformDisplayName"],
   [Define to the selected platform display name])
AC_DEFINE_UNQUOTED([BUILD_PLATFORM_SUPPORT_MODE], ["$platformSupportMode"],
   [Define to the platform support mode])
AC_DEFINE_UNQUOTED([BUILD_UNIVERSAL_MODE], ["$universalMode"],
   [Define to the universal binary mode])
AC_DEFINE_UNQUOTED([BUILD_KEYCHAIN_MODE], ["$keychainMode"],
   [Define to the keychain support mode])
AC_DEFINE_UNQUOTED([BUILD_CMOCKA_MODE], ["$cmockaMode"],
   [Define to the cmocka availability mode])
AC_DEFINE_UNQUOTED([BUILD_SANITIZER_MODE], ["$sanitizerMode"],
   [Define to the sanitizer mode])
AC_DEFINE_UNQUOTED([BUILD_NATIVE_OPTIMIZATION_MODE], ["$nativeOptimizationMode"],
   [Define to the native optimization mode])
AC_DEFINE_UNQUOTED([BUILD_STACK_PROTECTOR_MODE], ["$stackProtectorMode"],
   [Define to the stack protector mode])

AC_SUBST(PLATFORM_OBJS)
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile bbs.1])
AC_OUTPUT

cat <<CFG

Configuration summary:
Compiler : $compilerSummary
              Host : $host
                        Build mode : $buildMode
                                        Optimization mode : $optimizationMode
                                                               Platform : $platformDisplayName
                                                                             Platform support : $platformSupportMode
                                                                                                   Universal binary : $universalMode
                                                                                                                         Keychain support : $keychainMode
                                                                                                                                               CMocka : $cmockaMode
                                                                                                                                                           Sanitizers : $sanitizerMode
                                                                                                                                                                           Native optimizations : $nativeOptimizationMode
                                                                                                                                                                                                     Stack protector : $stackProtectorMode

                                                                                                                                                                                                                       CFG
