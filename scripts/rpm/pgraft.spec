# PostgreSQL RAM High Availability System - PGRaft Extension RPM Spec
# Copyright (c) 2024-2025, pgElephant, Inc.

Name:           pgraft
Version:        1.0.0
Release:        1%{?dist}
Summary:        PostgreSQL Raft Consensus Extension for High Availability

License:        PostgreSQL
URL:            https://github.com/pgelephant/ram
Source0:        %{name}-%{version}.tar.gz
BuildArch:      x86_64

BuildRequires:  postgresql17-devel
BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  golang >= 1.19
BuildRequires:  git

Requires:       postgresql17-server >= 17.0
Requires:       postgresql17 >= 17.0

%description
PGRaft is a PostgreSQL extension that provides distributed consensus
capabilities using the Raft algorithm. It enables automatic failover,
leader election, and cluster management for PostgreSQL high availability
deployments.

Key features:
- Raft consensus algorithm implementation
- Automatic leader election
- Node addition and removal
- Health monitoring and metrics
- Integration with PostgreSQL background workers
- Go-based Raft implementation with C wrapper

%prep
%setup -q

%build
# Build Go library first
cd pgraft/src
go mod tidy
go build -buildmode=c-shared -o pgraft_go.dylib pgraft_go.go

# Build PostgreSQL extension
cd ..
export PG_CONFIG=/usr/pgsql-17/bin/pg_config
make clean
make -j$(nproc) \
    PG_CONFIG="$PG_CONFIG" \
    CFLAGS="-Wall -Wextra -Werror -Wno-unused-parameter" \
    CPPFLAGS="-I$(pg_config --includedir)" \
    LDFLAGS="-L$(pg_config --libdir)"

%install
rm -rf $RPM_BUILD_ROOT

# Install PostgreSQL extension
cd pgraft
export PG_CONFIG=/usr/pgsql-17/bin/pg_config
make install DESTDIR=$RPM_BUILD_ROOT \
    PG_CONFIG="$PG_CONFIG"

# Install Go library
install -d $RPM_BUILD_ROOT%{_libdir}/postgresql/
install -m 755 src/pgraft_go.dylib $RPM_BUILD_ROOT%{_libdir}/postgresql/

# Install configuration files
install -d $RPM_BUILD_ROOT%{_sysconfdir}/postgresql/
install -m 644 conf/pgraft.conf $RPM_BUILD_ROOT%{_sysconfdir}/postgresql/

# Install documentation
install -d $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}
install -m 644 README.md $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}/
install -m 644 doc/getting-started/pgraft.md $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}/

%files
%defattr(-,root,root,-)
%{_libdir}/postgresql/pgraft.so
%{_libdir}/postgresql/pgraft_go.dylib
%{_datadir}/postgresql/extension/pgraft*
%{_sysconfdir}/postgresql/pgraft.conf
%{_docdir}/%{name}-%{version}/

%post
# Create extension in template1 if PostgreSQL is running
if systemctl is-active --quiet postgresql-17; then
    sudo -u postgres psql -d template1 -c "CREATE EXTENSION IF NOT EXISTS pgraft;" 2>/dev/null || true
fi

%preun
# Drop extension before removal
if systemctl is-active --quiet postgresql-17; then
    sudo -u postgres psql -d template1 -c "DROP EXTENSION IF EXISTS pgraft;" 2>/dev/null || true
fi

%postun
# Clean up configuration
rm -f %{_sysconfdir}/postgresql/pgraft.conf

%changelog
* $(date +'%a %b %d %Y') pgElephant <support@pgelephant.com> - 1.0.0-1
- Initial release of PGRaft extension
- Raft consensus algorithm implementation
- PostgreSQL 17 support
- Go-based Raft implementation
