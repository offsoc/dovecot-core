pkglibexecdir = $(libexecdir)/dovecot

pkglibexec_PROGRAMS = dict dict-expire

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib \
	-I$(top_srcdir)/src/lib-master \
	-I$(top_srcdir)/src/lib-settings \
	-I$(top_srcdir)/src/lib-dict \
	-I$(top_srcdir)/src/lib-dict-extra \
	-I$(top_srcdir)/src/lib-sql \
	-I$(top_srcdir)/src/lib-var-expand \
	-DDICT_MODULE_DIR=\""$(moduledir)/dict"\" \
	-DPKG_RUNDIR=\""$(rundir)"\" \
	$(BINARY_CFLAGS)

dict_LDFLAGS = -export-dynamic \
	$(BINARY_LDFLAGS)

libs = \
	../lib-dict-backend/libdict_backend.la \
	$(LIBDOVECOT_SQL)

dict_LDADD = \
	$(libs) \
	$(LIBDOVECOT) \
	$(DICT_LIBS) \
	$(SQL_LIBS) \
	-lm

dict_DEPENDENCIES = $(libs) $(LIBDOVECOT_DEPS)

dict_SOURCES = \
	dict-connection.c \
	dict-commands.c \
	dict-settings.c \
	dict-init-cache.c \
	main.c

dict_expire_LDADD = \
	$(libs) \
	$(LIBDOVECOT) \
	$(DICT_LIBS) \
	$(SQL_LIBS)
dict_expire_DEPENDENCIES = $(libs) $(LIBDOVECOT_DEPS)

dict_expire_SOURCES = \
	dict-settings.c \
	dict-expire.c

noinst_HEADERS = \
	dict-connection.h \
	dict-commands.h \
	dict-settings.h \
	dict-init-cache.h \
	main.h
