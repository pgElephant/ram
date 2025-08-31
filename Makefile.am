SUBDIRS = ramctrl ramd

if HAVE_PG_CONFIG
SUBDIRS += pg_ram
endif

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
    README.md \
    configure.ac \
    Makefile.am

.PHONY: clean-all
clean-all:
	$(MAKE) clean
	rm -rf build-aux m4
	rm -f aclocal.m4 config.h.in configure Makefile.in
	rm -f */Makefile.in
