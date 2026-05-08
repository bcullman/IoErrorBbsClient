AM_CPPFLAGS = -I$(srcdir)
AM_CFLAGS = $(OPTIMIZATION_CFLAGS) $(LTO_CFLAGS) $(STACK_PROTECTOR_CFLAGS) $(WARNING_CFLAGS) $(SANITIZER_CFLAGS)
AM_LDFLAGS = $(LTO_LDFLAGS) $(SANITIZER_LDFLAGS)
bbs_CFLAGS = $(AM_CFLAGS) $(UNIVERSAL_CFLAGS)
bbs_LDFLAGS = $(AM_LDFLAGS) $(UNIVERSAL_LDFLAGS) $(KEYCHAIN_LDFLAGS)

bin_PROGRAMS = bbs
man_MANS = bbs.1
releasepackagedir = release
releaseartifact = $(PACKAGE)-$(VERSION)-$(HOST_TRIPLE)
releasebinary = $(releasepackagedir)/$(releaseartifact)
releasedsym = $(releasebinary).dSYM
scanbuildreportdir = scan-build-report
noinst_HEADERS = \
	browser.h \
	defs.h \
	ext.h \
	macos_keychain.h \
	proto.h \
	sysio.h \
	telnet.h \
	unix.h \
	users.h

bbs_SOURCES = \
	browser.c \
	browser_urls.c \
	bbsrc.c \
	bbsrc_parse.c \
	color.c \
	color_config.c \
	color_themes.c \
	config.c \
	config_express.c \
	config_keys.c \
	config_options.c \
	config_write.c \
	config_users.c \
	edit.c \
	edit_check.c \
	edit_prompt.c \
	filter_data.c \
	filter_express.c \
	filter_post.c \
	filter_who.c \
	getline.c \
	getline_name.c \
	global.c \
	info.c \
	inkey.c \
	macos_keychain.c \
	main.c \
	queue.c \
	slist.c \
	sysio.c \
	telnet.c \
	utility_args.c \
	utility_color.c \
	utility_compare.c \
	utility_error.c \
	utility_input.c \
	utility_line.c \
	utility_send.c \
	utility_string.c \
	unix.c \
	unix_net.c \
	unix_term.c

if HAVE_CMOCKA
check_PROGRAMS = test_queue test_slist test_utility test_bbsrc test_getline test_inkey test_filter test_color test_config test_macos_keychain test_sysio test_edit test_telnet
TESTS = $(check_PROGRAMS)
test_queue_SOURCES = test/test_queue.c test/test_helpers.c queue.c
test_queue_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_queue_LDADD = $(CMOCKA_LIBS)
test_slist_SOURCES = test/test_slist.c test/test_helpers.c slist.c
test_slist_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_slist_LDADD = $(CMOCKA_LIBS)
test_utility_SOURCES = test/test_utility.c test/test_helpers.c utility_error.c utility_input.c utility_send.c utility_string.c global.c
test_utility_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_utility_LDADD = $(CMOCKA_LIBS)
test_bbsrc_SOURCES = test/test_bbsrc.c test/test_helpers.c bbsrc.c bbsrc_parse.c queue.c slist.c global.c
test_bbsrc_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_bbsrc_LDADD = $(CMOCKA_LIBS)
test_getline_SOURCES = test/test_getline.c test/test_helpers.c getline.c getline_name.c slist.c global.c
test_getline_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_getline_LDADD = $(CMOCKA_LIBS)
test_inkey_SOURCES = test/test_inkey.c test/test_helpers.c inkey.c global.c
test_inkey_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_inkey_LDADD = $(CMOCKA_LIBS)
test_filter_SOURCES = test/test_filter.c test/test_helpers.c browser.c browser_urls.c filter_data.c filter_express.c filter_post.c filter_who.c queue.c slist.c global.c
test_filter_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_filter_LDADD = $(CMOCKA_LIBS)
test_color_SOURCES = test/test_color.c test/test_helpers.c color.c color_config.c color_themes.c slist.c global.c
test_color_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_color_LDADD = $(CMOCKA_LIBS)
test_config_SOURCES = test/test_config.c test/test_helpers.c config.c config_express.c config_keys.c config_options.c config_write.c config_users.c slist.c global.c
test_config_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_config_LDADD = $(CMOCKA_LIBS)
test_macos_keychain_SOURCES = test/test_macos_keychain.c test/test_helpers.c macos_keychain.c global.c
test_macos_keychain_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_macos_keychain_LDADD = $(CMOCKA_LIBS) $(KEYCHAIN_LDFLAGS)
test_sysio_SOURCES = test/test_sysio.c test/test_helpers.c sysio.c global.c
test_sysio_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_sysio_LDADD = $(CMOCKA_LIBS)
test_edit_SOURCES = test/test_edit.c test/test_helpers.c edit.c edit_check.c edit_prompt.c global.c
test_edit_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_edit_LDADD = $(CMOCKA_LIBS)
test_telnet_SOURCES = test/test_telnet.c test/test_helpers.c telnet.c queue.c global.c
test_telnet_CFLAGS = $(AM_CFLAGS) $(CMOCKA_CFLAGS)
test_telnet_LDADD = $(CMOCKA_LIBS)
EXTRA_DIST = bbs.1.in test/test_helpers.h
else
TESTS =
endif

install-exec-hook:
	@echo 'Now hot Asian sluts will come out of your computer whenever you type "bbs".'

.PHONY: analyze cppcheck release-package scan-build

release-package: bbs
	@if test "x$(RELEASE_BUILD_ENABLED)" != "xyes"; then \
		echo "release-package requires ./configure --enable-release-build"; \
		exit 1; \
	fi
	@mkdir -p $(releasepackagedir)
	@rm -f $(releasebinary)
	@rm -rf $(releasedsym)
	cp bbs $(releasebinary)
	@if test "x$(DSYMUTIL)" != "xno"; then \
		$(DSYMUTIL) $(releasebinary) -o $(releasedsym); \
	fi
	@if test "x$(STRIP)" != "xno"; then \
		$(STRIP) -x $(releasebinary); \
	fi
	@echo "Release binary: $(releasebinary)"
	@if test -d "$(releasedsym)"; then \
		echo "Debug symbols: $(releasedsym)"; \
	fi

cppcheck:
	@if ! command -v cppcheck >/dev/null 2>&1; then \
		echo "cppcheck is not installed. Install it with: brew install cppcheck"; \
		exit 1; \
	fi; \
	cppcheck --enable=warning,style,performance,portability \
		--inconclusive --std=c11 -DHAVE_CONFIG_H -I. $(bbs_SOURCES)

scan-build:
	@SCAN_BUILD_BIN="$$(command -v scan-build 2>/dev/null || true)"; \
	if test -z "$$SCAN_BUILD_BIN" && test -x /opt/homebrew/opt/llvm/bin/scan-build; then \
		SCAN_BUILD_BIN=/opt/homebrew/opt/llvm/bin/scan-build; \
	fi; \
	if test -z "$$SCAN_BUILD_BIN"; then \
		echo "scan-build is not installed or not on PATH. Install it with: brew install llvm"; \
		exit 1; \
	fi; \
	rm -rf $(scanbuildreportdir); \
	mkdir -p $(scanbuildreportdir); \
	echo "Using scan-build at $$SCAN_BUILD_BIN"; \
	"$$SCAN_BUILD_BIN" --status-bugs -o $(scanbuildreportdir) $(MAKE) -j4 clean all

analyze: cppcheck
	@if command -v clang-tidy >/dev/null 2>&1 && [ -f compile_commands.json ]; then \
		clang-tidy -p . $(bbs_SOURCES); \
	else \
		echo "Skipping clang-tidy (requires clang-tidy and compile_commands.json)"; \
	fi
