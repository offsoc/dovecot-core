pkglib_LTLIBRARIES = libdovecot-dsync.la
noinst_LTLIBRARIES = libdsync.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib \
	-I$(top_srcdir)/src/lib-test \
	-I$(top_srcdir)/src/lib-settings \
	-I$(top_srcdir)/src/lib-ssl-iostream \
	-I$(top_srcdir)/src/lib-master \
	-I$(top_srcdir)/src/lib-mail \
	-I$(top_srcdir)/src/lib-imap \
	-I$(top_srcdir)/src/lib-index \
	-I$(top_srcdir)/src/lib-storage \
	-I$(top_srcdir)/src/lib-var-expand

libdsync_la_SOURCES = \
	dsync-brain.c \
	dsync-brain-mailbox.c \
	dsync-brain-mailbox-tree.c \
	dsync-brain-mailbox-tree-sync.c \
	dsync-brain-mails.c \
	dsync-deserializer.c \
	dsync-mail.c \
	dsync-mailbox.c \
	dsync-mailbox-import.c \
	dsync-mailbox-export.c \
	dsync-mailbox-state.c \
	dsync-mailbox-tree.c \
	dsync-mailbox-tree-fill.c \
	dsync-mailbox-tree-sync.c \
	dsync-serializer.c \
	dsync-ibc.c \
	dsync-ibc-stream.c \
	dsync-ibc-pipe.c \
	dsync-transaction-log-scan.c

libdovecot_dsync_la_SOURCES =
libdovecot_dsync_la_LIBADD = libdsync.la $(LIBDOVECOT_STORAGE) $(LIBDOVECOT)
libdovecot_dsync_la_DEPENDENCIES = libdsync.la $(LIBDOVECOT_STORAGE_DEPS) $(LIBDOVECOT_DEPS)
libdovecot_dsync_la_LDFLAGS = -export-dynamic

pkginc_libdir = $(pkgincludedir)
pkginc_lib_HEADERS = \
	dsync-brain.h \
	dsync-ibc.h

noinst_HEADERS = \
	dsync-brain-private.h \
	dsync-mail.h \
	dsync-mailbox.h \
	dsync-mailbox-import.h \
	dsync-mailbox-export.h \
	dsync-mailbox-state.h \
	dsync-mailbox-tree.h \
	dsync-mailbox-tree-private.h \
	dsync-serializer.h \
	dsync-deserializer.h \
	dsync-ibc-private.h \
	dsync-transaction-log-scan.h

test_programs = \
	test-dsync-mailbox-tree-sync

noinst_PROGRAMS = $(test_programs)

test_libs = \
	../../lib-test/libtest.la \
	../../lib/liblib.la

test_dsync_mailbox_tree_sync_SOURCES = test-dsync-mailbox-tree-sync.c
test_dsync_mailbox_tree_sync_LDADD = dsync-mailbox-tree-sync.lo dsync-mailbox-tree.lo $(test_libs)
test_dsync_mailbox_tree_sync_DEPENDENCIES = $(pkglib_LTLIBRARIES) $(test_libs)

check-local:
	for bin in $(test_programs); do \
	  if ! $(RUN_TEST) ./$$bin; then exit 1; fi; \
	done
