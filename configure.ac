AC_INIT([ram], [1.0], [support@pgelephant.com])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects dist-xz no-dist-gzip])
AC_PROG_CC

# Detect pg_config for building the PostgreSQL extension
# Order of preference:
#  1) --with-pg-config=PATH if provided
#  2) /usr/local/pgsql/bin/pg_config (user-provided default path)
#  3) First pg_config found on PATH
AC_ARG_WITH([pg_config],
  [AS_HELP_STRING([--with-pg-config=PATH], [path to pg_config])],
  [PG_CONFIG="$withval"],
  [PG_CONFIG=""])

if test -z "$PG_CONFIG"; then
  if test -x "/usr/local/pgsql/bin/pg_config"; then
    PG_CONFIG="/usr/local/pgsql/bin/pg_config"
  else
    AC_PATH_PROG([PG_CONFIG], [pg_config], [no])
  fi
fi

if test "x$PG_CONFIG" = "xno" -o ! -x "$PG_CONFIG"; then
  AC_MSG_WARN([pg_config not found; pg_ram extension may not build. You can pass --with-pg-config=/usr/local/pgsql/bin/pg_config])
fi

AC_SUBST([PG_CONFIG])

# Conditionally include pg_ram in the build when pg_config is available
AM_CONDITIONAL([HAVE_PG_CONFIG], [test "x$PG_CONFIG" != "xno" -a -x "$PG_CONFIG"])

# pg_ram uses its own PostgreSQL extension Makefile, don't generate one

AC_CONFIG_FILES([
  Makefile
  ramctrl/Makefile
  ramd/Makefile
])
AC_OUTPUT
