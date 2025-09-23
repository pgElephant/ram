AM_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wvla -Wno-strict-prototypes -Wno-missing-field-initializers -DNDEBUG -pthread
AM_CPPFLAGS = -I$(srcdir)/include -I/usr/local/pgsql/include -I/opt/homebrew/opt/openssl@3/include -I/opt/homebrew/include

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
               src/ramd_maintenance.c \
               src/ramd_metrics.c \
               src/ramd_basebackup.c \
               src/ramd_conn.c \
               src/ramd_query.c \
               src/ramd_pgraft.c \
               src/ramd_prometheus.c \
               src/ramd_postgresql_auth.c \
               src/ramd_security.c

# Link with pthread, PostgreSQL, jansson, and OpenSSL
ramd_LDADD = -lpthread -L/usr/local/pgsql/lib -L/opt/homebrew/lib -lpq -ljansson -lssl -lcrypto

# Clean target
clean:
	rm -f $(bin_PROGRAMS)
	rm -f src/*.o src/*.bc
	rm -f *.o *.bc
