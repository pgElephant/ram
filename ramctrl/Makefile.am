PG_CPPFLAGS = $(shell $(PG_CONFIG) --cppflags)
PG_INCLUDEDIR = $(shell $(PG_CONFIG) --includedir)
PG_LIBDIR = $(shell $(PG_CONFIG) --libdir)

AM_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wvla -Wno-strict-prototypes -Wno-missing-field-initializers -DNDEBUG
AM_CPPFLAGS = -I$(srcdir)/include -I$(PG_INCLUDEDIR) $(PG_CPPFLAGS)
AM_LDFLAGS = -L$(PG_LIBDIR) -lpq

bin_PROGRAMS = ramctrl
ramctrl_SOURCES = \
  src/ramctrl_main.c \
  src/ramctrl_database.c \
  src/ramctrl_daemon.c \
  src/ramctrl_table.c \
  src/ramctrl_replication.c
