# pgraft Makefile
# PostgreSQL extension using etcd-io/raft for distributed consensus

MODULE_big = pgraft
OBJS = src/pgraft.o src/guc.o src/comm.o src/raft.o src/monitor.o src/worker_manager.o src/health_worker.o src/utils.o src/metrics.o

EXTENSION = pgraft
DATA = pgraft--1.0.sql
PGFILEDESC = "pgraft - PostgreSQL extension with etcd-io/raft integration"

# PostgreSQL configuration - use PostgreSQL 17
PG_CONFIG = /usr/local/pgsql.17/bin/pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# Compiler flags
CFLAGS += -std=c99 -Wall -Wextra -Werror
CFLAGS += -I./include

# Extension-specific linker flags
SHLIB_LINK += -lpthread -lm -ldl

# Go Raft library
GO_RAFT_LIB = src/pgraft_go.dylib

# Build Go Raft library
$(GO_RAFT_LIB): src/pgraft_go.go src/go.mod
	cd src && go mod tidy
	cd src && go build -buildmode=c-shared -o pgraft_go.dylib pgraft_go.go

# Dependencies
$(OBJS): $(GO_RAFT_LIB)

# Clean target - ensure it exists
clean: clean-extra

clean-extra:
	rm -f src/*.o
	rm -f src/pgraft_go.dylib
	rm -f src/pgraft_go.h

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
