# PostgreSQL extension build
# This uses PostgreSQL's extension build system

EXTENSION = pg_ram
DATA = pg_ram--1.0.sql

# Source files for the extension
MODULE_big = pg_ram
OBJS = \
    src/pg_ram.o \
    src/pgram_dstore_monitor.o \
    src/pgram_guc.o \
    src/pgram_health_worker.o \
    src/pgram_librale_functions.o \
    src/pgram_librale_worker.o \
    src/pgram_librale.o \
    src/pgram_worker_manager.o \
    src/postgresql_healthcheck.o \
    src/postgresql_monitor.o \
    src/quorum_selection.o

# PostgreSQL-specific flags
PG_CPPFLAGS = -I$(srcdir)/include
PG_CFLAGS = -Wall -Wextra

# Include headers
include_HEADERS = \
    include/pgram_dstore_monitor.h \
    include/pgram_guc.h \
    include/pgram_health_types.h \
    include/pgram_health_worker.h \
    include/pgram_librale_worker.h \
    include/pgram_librale.h \
    include/pgram_workers.h \
    include/postgresql_healthcheck.h \
    include/postgresql_monitor.h \
    include/quorum_selection.h

EXTRA_DIST = \
    include/*.h \
    src/*.c \
    pg_ram--1.0.sql

# Use PostgreSQL's extension build system
PG_CONFIG = @PG_CONFIG@
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
