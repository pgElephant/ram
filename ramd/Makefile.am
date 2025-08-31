bin_PROGRAMS = ramd

ramd_SOURCES = \
    src/ramd_main.c \
    src/ramd_cluster.c \
    src/ramd_config.c \
    src/ramd_config_reload.c \
    src/ramd_failover.c \
    src/ramd_http_api.c \
    src/ramd_logging.c \
    src/ramd_maintenance.c \
    src/ramd_minimal.c \
    src/ramd_monitor.c \
    src/ramd_postgresql.c \
    src/ramd_sync_replication.c

ramd_CPPFLAGS = -I$(srcdir)/include -I/opt/homebrew/include/postgresql@14

ramd_CFLAGS = -Wall -Wextra -std=c99

# Add any additional libraries needed
ramd_LDADD = -L/opt/homebrew/lib/postgresql@14 -lpq -lpthread

# Include headers
include_HEADERS = \
    include/ramd.h \
    include/ramd_cluster.h \
    include/ramd_config.h \
    include/ramd_config_reload.h \
    include/ramd_daemon.h \
    include/ramd_failover.h \
    include/ramd_formation_config.h \
    include/ramd_http_api.h \
    include/ramd_logging.h \
    include/ramd_maintenance.h \
    include/ramd_monitor.h \
    include/ramd_postgresql.h \
    include/ramd_sync_replication.h

EXTRA_DIST = \
    include/*.h \
    src/*.c
