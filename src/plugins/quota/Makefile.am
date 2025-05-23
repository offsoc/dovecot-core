doveadm_moduledir = $(moduledir)/doveadm

pkglibexecdir = $(libexecdir)/dovecot
pkglibexec_PROGRAMS = quota-status

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib \
	-I$(top_srcdir)/src/lib-test \
	-I$(top_srcdir)/src/lib-master \
	-I$(top_srcdir)/src/lib-dict \
	-I$(top_srcdir)/src/lib-index \
	-I$(top_srcdir)/src/lib-mail \
	-I$(top_srcdir)/src/lib-smtp \
	-I$(top_srcdir)/src/lib-imap \
	-I$(top_srcdir)/src/lib-imap-client \
	-I$(top_srcdir)/src/lib-settings \
	-I$(top_srcdir)/src/lib-ssl-iostream \
	-I$(top_srcdir)/src/lib-storage \
	-I$(top_srcdir)/src/lib-storage/index \
	-I$(top_srcdir)/src/lib-storage/index/imapc \
	-I$(top_srcdir)/src/lib-storage/index/maildir \
	-I$(top_srcdir)/src/lib-program-client \
	-I$(top_srcdir)/src/lib-doveadm \
	-I$(top_srcdir)/src/lib-var-expand \
	-I$(top_srcdir)/src/doveadm \
	$(LIBTIRPC_CFLAGS)

NOPLUGIN_LDFLAGS =
lib10_doveadm_quota_plugin_la_LDFLAGS = -module -avoid-version
lib10_quota_plugin_la_LDFLAGS = -module -avoid-version

module_LTLIBRARIES = \
	lib10_quota_plugin.la

lib10_quota_plugin_la_SOURCES = \
	quota.c \
	quota-count.c \
	quota-fs.c \
	quota-imapc.c \
	quota-maildir.c \
        quota-plugin.c \
	quota-settings.c \
	quota-storage.c \
	quota-util.c
nodist_lib10_quota_plugin_la_SOURCES = $(RQUOTA_XDR)
lib10_quota_plugin_la_LIBADD = $(QUOTA_LIBS)

doveadm_module_LTLIBRARIES = \
	lib10_doveadm_quota_plugin.la

lib10_doveadm_quota_plugin_la_SOURCES = \
	doveadm-quota.c

quota_status_SOURCES = \
	quota-status.c \
	quota-status-settings.c

quota_status_CPPFLAGS = $(AM_CPPFLAGS) $(BINARY_CFLAGS)
quota_status_LDADD = \
	lib10_quota_plugin.la \
	$(LIBDOVECOT_STORAGE) \
	$(LIBDOVECOT) \
	$(QUOTA_LIBS) \
	$(BINARY_LDFLAGS)
quota_status_DEPENDENCIES = \
	lib10_quota_plugin.la \
	$(LIBDOVECOT_STORAGE_DEPS) \
	$(LIBDOVECOT_DEPS)

if HAVE_RQUOTA

BUILT_SOURCES = rquota_xdr.c rquota.h

RQUOTA_XDR = rquota_xdr.c

rquota_xdr.c: rquota.h rquota.x
	$(AM_V_GEN)(echo '#include "lib.h"'; \
	 echo '#undef FALSE'; \
	 echo '#undef TRUE'; \
	 echo '#ifdef HAVE_STRICT_BOOL'; \
	 echo '#  pragma GCC diagnostic ignored "-Wstrict-bool"'; \
	 echo '#endif'; \
	 echo '#include <rpc/rpc.h>'; \
	 $(RPCGEN) -c $(abs_srcdir)/rquota.x | sed \
	    -e 's/IXDR_PUT/(void)IXDR_PUT/g' \
	    -e 's,!xdr_,0 == xdr_,' \
	    -e 's,/usr/include/rpcsvc/rquota.h,rquota.h,' \
	    -e 's/int32_t \*buf/int32_t *buf ATTR_UNUSED/' \
	    -e 's/^static char rcsid.*//' \
	    -e "s@$(abs_srcdir)/@@g"; \
	 ) > $@.tmp && cat $@.tmp > $@; \
	rm -f $@.tmp

rquota.h: rquota.x
	$(AM_V_GEN)$(RPCGEN) -h $(srcdir)/rquota.x > $@.tmp && cat $@.tmp > $@; \
	rm -f $@.tmp

quota-fs.lo: rquota_xdr.lo

endif

pkginc_libdir=$(pkgincludedir)
pkginc_lib_HEADERS = \
	quota.h \
	quota-fs.h \
	quota-plugin.h \
	quota-private.h \
	quota-settings.h
noinst_HEADERS = \
	quota-status-settings.h

EXTRA_DIST = rquota.x

clean-generic:
	if [ "$(top_srcdir)" != "$(top_builddir)" ]; then \
	  rm -f $(top_builddir)/src/plugins/quota/rquota.x; \
	fi; \
	rm -f rquota_xdr.c rquota_xdr.c.tmp rquota.h rquota.h.tmp

test_programs = \
	test-quota-util
noinst_PROGRAMS = $(test_programs)

test_libs = \
	$(LIBDOVECOT_STORAGE) \
	$(LIBDOVECOT)
test_deps = $(noinst_LTLIBRARIES) \
	$(LIBDOVECOT_STORAGE_DEPS) \
	$(LIBDOVECOT_DEPS)

test_quota_util_SOURCES = test-quota-util.c
test_quota_util_LDADD = quota-util.lo $(test_libs)
test_quota_util_DEPENDENCIES = quota-util.lo $(test_deps)

check-local:
	for bin in $(test_programs); do \
	  if ! $(RUN_TEST) ./$$bin; then exit 1; fi; \
	done
