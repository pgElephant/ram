# pgraft Makefile
# PostgreSQL extension using etcd-io/raft for distributed consensus

MODULE_big = pgraft
OBJS = src/pgraft.o src/guc.o src/comm.o src/raft.o src/monitor.o src/worker_manager.o src/health_worker.o src/utils.o src/metrics.o

EXTENSION = pgraft
DATA = pgraft--1.0.sql
PGFILEDESC = "pgraft - PostgreSQL extension with etcd-io/raft integration"

# PostgreSQL configuration
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# Compiler flags
CFLAGS += -std=c99 -Wall -Wextra -Werror
CFLAGS += -I./include

# Extension-specific linker flags
SHLIB_LINK += -lpthread -lm

# Clean target - ensure it exists
clean: clean-extra

clean-extra:
	rm -f src/*.o

# Installation directory
DESTDIR ?= 

# Development flags
ifeq ($(DEBUG),1)
    CFLAGS += -g -O0 -DDEBUG
else
    CFLAGS += -O2 -DNDEBUG
endif

# Test target
test: all
	@echo "Running pgraft tests..."
	@echo "Tests would go here"

.PHONY: clean install test
