bin_PROGRAMS = ramctrl

ramctrl_SOURCES = \
    src/ramctrl_main.c \
    src/ramctrl_common.c \
    src/ramctrl_daemon.c \
    src/ramctrl_database.c \
    src/ramctrl_formation.c \
    src/ramctrl_help.c \
    src/ramctrl_replication.c \
    src/ramctrl_show.c \
    src/ramctrl_table.c \
    src/ramctrl_watch.c

ramctrl_CPPFLAGS = -I$(srcdir)/include -I/opt/homebrew/include/postgresql@14

ramctrl_CFLAGS = -Wall -Wextra -std=c99

# Add any additional libraries needed
ramctrl_LDADD = -L/opt/homebrew/lib/postgresql@14 -lpq

# Include headers
include_HEADERS = \
    include/ramctrl.h \
    include/ramctrl_daemon.h \
    include/ramctrl_database.h \
    include/ramctrl_replication.h \
    include/ramctrl_table.h

EXTRA_DIST = \
    include/*.h \
    src/*.c
