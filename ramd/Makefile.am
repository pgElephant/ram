AM_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wvla -Wno-strict-prototypes -Wno-missing-field-initializers -DNDEBUG -pthread
AM_CPPFLAGS = -I$(srcdir)/include -I$(top_srcdir)/../rale/librale/include -I/usr/local/pgsql/include -I/opt/homebrew/opt/openssl@3/include -I/opt/homebrew/include

bin_PROGRAMS = ramd
ramd_SOURCES = src/ramd_main.c \
               src/ramd_config.c \
               src/ramd_cluster.c \
               src/ramd_monitor.c \
               src/ramd_failover.c \
               src/ramd_postgresql.c \
               src/ramd_logging.c \
               src/ramd_http_api.c \
               src/ramd_sync_replication.c \
               src/ramd_config_reload.c \
               src/ramd_maintenance.c

# Link with librale, pthread and PostgreSQL
ramd_LDADD = $(top_srcdir)/../rale/lib/librale.a -lpthread -L/usr/local/pgsql/lib -L/opt/homebrew/lib -lpq
