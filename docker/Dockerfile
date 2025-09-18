# PostgreSQL 17 with pgraft, ramd, and ramctrl
FROM postgres:17

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    git \
    pkg-config \
    libpq-dev \
    postgresql-server-dev-17 \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY pgraft/ /app/pgraft/
COPY ramd/ /app/ramd/
COPY ramctrl/ /app/ramctrl/

# Build pgraft extension
WORKDIR /app/pgraft
RUN make clean && make && make install

# Build ramd daemon
WORKDIR /app/ramd
RUN make clean && make

# Build ramctrl utility
WORKDIR /app/ramctrl
RUN make clean && make

# Copy binaries to system path
RUN cp /app/ramd/ramd /usr/local/bin/ && \
    cp /app/ramctrl/ramctrl /usr/local/bin/ && \
    chmod +x /usr/local/bin/ramd /usr/local/bin/ramctrl

# Create configuration directory
RUN mkdir -p /etc/ramd

# Copy configuration files
COPY docker/ramd.conf /etc/ramd/ramd.conf
COPY docker/postgresql.conf /etc/postgresql/postgresql.conf
COPY docker/pg_hba.conf /etc/postgresql/pg_hba.conf

# Create startup script
COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

# Expose ports
EXPOSE 5432 8080

# Set environment variables
ENV POSTGRES_DB=postgres
ENV POSTGRES_USER=postgres
ENV POSTGRES_PASSWORD=postgres
ENV PGRAPT_ENABLED=on
ENV PGRAPT_NODE_ID=1
ENV RAMD_NODE_ID=1

# Use custom entrypoint
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["postgres"]
