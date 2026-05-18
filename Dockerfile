# =============================================================================
# PvPGN Multi-Stage Dockerfile
# =============================================================================
# Build argument that selects the database backend.
# Valid values: plain | mysql | pgsql | sqlite3 | odbc
# Default: mysql
# =============================================================================
ARG MODE=mysql


# =============================================================================
# Stage: build-base
# Common build environment shared by all database-backend build stages.
# =============================================================================
FROM alpine:latest AS build-base

# ---------------------------------------------------------------------------
# Install core build toolchain and shared library dependencies
# ---------------------------------------------------------------------------
RUN apk --quiet --no-cache add \
      build-base \
      clang \
      cmake \
      git \
      make \
      curl-dev \
      openssl-dev \
      zlib-dev \
    && rm -rf /var/cache/apk/*

# ---------------------------------------------------------------------------
# Copy local source tree into the image and prepare build directories
# ---------------------------------------------------------------------------
COPY . /src
RUN mkdir -p /src/build /usr/local/pvpgn
WORKDIR /src

# ---------------------------------------------------------------------------
# Default feature flags – individual build stages override these as needed
# ---------------------------------------------------------------------------
ENV WITH_LUA=false
ENV WITH_MYSQL=false
ENV WITH_PGSQL=false
ENV WITH_SQLITE3=false
ENV WITH_ODBC=false


# =============================================================================
# Stage: build-plain
# No external database backend; uses the built-in flat-file storage.
# =============================================================================
FROM build-base AS build-plain

RUN cmake -S ./ -B ./build \
      -D WITH_LUA=${WITH_LUA} \
      -D WITH_MYSQL=${WITH_MYSQL} \
      -D WITH_PGSQL=${WITH_PGSQL} \
      -D WITH_SQLITE3=${WITH_SQLITE3} \
      -D WITH_ODBC=${WITH_ODBC} \
      -D CMAKE_INSTALL_PREFIX=/usr/local/pvpgn \
    && cd build && make


# =============================================================================
# Stage: build-mysql
# Adds MariaDB/MySQL client library and enables the MySQL storage backend.
# =============================================================================
FROM build-base AS build-mysql

# Install the MariaDB development headers and client library
RUN apk --quiet --no-cache add \
      mariadb-dev \
    && rm -rf /var/cache/apk/*

ENV WITH_MYSQL=true

RUN cmake -S ./ -B ./build \
      -D WITH_LUA=${WITH_LUA} \
      -D WITH_MYSQL=${WITH_MYSQL} \
      -D WITH_PGSQL=${WITH_PGSQL} \
      -D WITH_SQLITE3=${WITH_SQLITE3} \
      -D WITH_ODBC=${WITH_ODBC} \
      -D CMAKE_INSTALL_PREFIX=/usr/local/pvpgn \
    && cd build && make


# =============================================================================
# Stage: build-pgsql
# Adds PostgreSQL client library and enables the PostgreSQL storage backend.
# =============================================================================
FROM build-base AS build-pgsql

# Install the PostgreSQL development headers and client library
RUN apk --quiet --no-cache add \
      libpq-dev \
    && rm -rf /var/cache/apk/*

ENV WITH_PGSQL=true

RUN cmake -S ./ -B ./build \
      -D WITH_LUA=${WITH_LUA} \
      -D WITH_MYSQL=${WITH_MYSQL} \
      -D WITH_PGSQL=${WITH_PGSQL} \
      -D WITH_SQLITE3=${WITH_SQLITE3} \
      -D WITH_ODBC=${WITH_ODBC} \
      -D CMAKE_INSTALL_PREFIX=/usr/local/pvpgn \
    && cd build && make


# =============================================================================
# Stage: build-sqlite3
# Adds SQLite3 library and enables the SQLite3 storage backend.
# =============================================================================
FROM build-base AS build-sqlite3

# Install the SQLite3 development headers and library
RUN apk --quiet --no-cache add \
      sqlite-dev \
    && rm -rf /var/cache/apk/*

ENV WITH_SQLITE3=true

RUN cmake -S ./ -B ./build \
      -D WITH_LUA=${WITH_LUA} \
      -D WITH_MYSQL=${WITH_MYSQL} \
      -D WITH_PGSQL=${WITH_PGSQL} \
      -D WITH_SQLITE3=${WITH_SQLITE3} \
      -D WITH_ODBC=${WITH_ODBC} \
      -D CMAKE_INSTALL_PREFIX=/usr/local/pvpgn \
    && cd build && make


# =============================================================================
# Stage: build-odbc
# Adds unixODBC library and enables the ODBC storage backend.
# =============================================================================
FROM build-base AS build-odbc

# Install the unixODBC development headers and library
RUN apk --quiet --no-cache add \
      unixodbc-dev \
    && rm -rf /var/cache/apk/*

ENV WITH_ODBC=true

RUN cmake -S ./ -B ./build \
      -D WITH_LUA=${WITH_LUA} \
      -D WITH_MYSQL=${WITH_MYSQL} \
      -D WITH_PGSQL=${WITH_PGSQL} \
      -D WITH_SQLITE3=${WITH_SQLITE3} \
      -D WITH_ODBC=${WITH_ODBC} \
      -D CMAKE_INSTALL_PREFIX=/usr/local/pvpgn \
    && cd build && make


# =============================================================================
# Stage: build  (selector)
# Resolves to the correct build-<MODE> stage and runs `make install`.
# =============================================================================
FROM build-${MODE} AS build

WORKDIR /src/build

# Install compiled binaries/configs into the prefix and fix ownership
RUN make install && chown -R 1001:1001 /usr/local/pvpgn


# =============================================================================
# Stage: runner-plain
# Minimal Alpine runtime with only the shared libraries needed for plain mode.
# All other runner-* stages extend this one.
# =============================================================================
FROM alpine:latest AS runner-plain

RUN apk --quiet --no-cache add \
      ca-certificates \
      libcurl \
      libgcc \
      libstdc++ \
      openssl \
    && rm -rf /var/cache/apk/*


# =============================================================================
# Stage: runner-mysql
# Extends runner-plain with the MariaDB connector runtime library.
# =============================================================================
FROM runner-plain AS runner-mysql

RUN apk --quiet --no-cache add \
      mariadb-connector-c \
    && rm -rf /var/cache/apk/*


# =============================================================================
# Stage: runner-pgsql
# Extends runner-plain with the PostgreSQL client runtime library.
# =============================================================================
FROM runner-plain AS runner-pgsql

RUN apk --quiet --no-cache add \
      libpq \
    && rm -rf /var/cache/apk/*


# =============================================================================
# Stage: runner-sqlite3
# Extends runner-plain with the SQLite3 runtime library.
# =============================================================================
FROM runner-plain AS runner-sqlite3

RUN apk --quiet --no-cache add \
      sqlite-libs \
    && rm -rf /var/cache/apk/*


# =============================================================================
# Stage: runner-odbc
# Extends runner-plain with the unixODBC runtime library.
# =============================================================================
FROM runner-plain AS runner-odbc

RUN apk --quiet --no-cache add \
      unixodbc \
    && rm -rf /var/cache/apk/*


# =============================================================================
# Stage: runner  (final image)
# Selects the correct runner-<MODE> base, copies the installed files from the
# build stage, and configures the container for production use.
# =============================================================================
FROM runner-${MODE} AS runner

# ---------------------------------------------------------------------------
# Copy the installed PvPGN tree from the build stage
# ---------------------------------------------------------------------------
COPY --from=build --chown=1001:1001 /usr/local/pvpgn /usr/local/pvpgn

# ---------------------------------------------------------------------------
# Symlink PvPGN binaries into standard PATH locations so they are accessible
# without modifying PATH inside the container
# ---------------------------------------------------------------------------
RUN ln -s /usr/local/pvpgn/sbin/* /usr/local/sbin/ 2>/dev/null || true \
    && ln -s /usr/local/pvpgn/bin/*  /usr/local/bin/  2>/dev/null || true

# ---------------------------------------------------------------------------
# Create a dedicated system user/group (uid/gid 1001) for running PvPGN
# ---------------------------------------------------------------------------
RUN addgroup --gid 1001 pvpgn \
    && adduser \
         --uid 1001 \
         --ingroup pvpgn \
         --home /var/pvpgn \
         --shell /sbin/nologin \
         --gecos "" \
         --system \
         --disabled-password \
         --no-create-home \
         pvpgn

# ---------------------------------------------------------------------------
# Create runtime data and configuration directories with correct ownership
# ---------------------------------------------------------------------------
RUN mkdir -p /var/pvpgn /etc/pvpgn /var/pvpgn/logs /var/pvpgn/users /var/pvpgn/clans /var/pvpgn/teams \
    && chown -R 1001:1001 /var/pvpgn /etc/pvpgn \
    && chmod -R 755 /var/pvpgn \
    && echo "storage_path = \"file:mode=plain;dir=/var/pvpgn/users;clan=/var/pvpgn/clans;team=/var/pvpgn/teams;default=/etc/pvpgn/bnetd_default_user.plain\"" > /etc/pvpgn/bnetd.conf \
    && echo "logfile = /var/pvpgn/bnetd.log" >> /etc/pvpgn/bnetd.conf \
    && echo "servername = PvPGN" >> /etc/pvpgn/bnetd.conf \
    && chown 1001:1001 /etc/pvpgn/bnetd.conf

# ---------------------------------------------------------------------------
# Seed config and data directories from the installed prefix (best-effort)
# ---------------------------------------------------------------------------
RUN if [ -d /usr/local/pvpgn/etc/pvpgn ]; then \
      cp -r /usr/local/pvpgn/etc/pvpgn/* /etc/pvpgn/ 2>/dev/null || true; \
    fi && \
    if [ -d /usr/local/pvpgn/var/pvpgn ]; then \
      cp -r /usr/local/pvpgn/var/pvpgn/* /var/pvpgn/ 2>/dev/null || true; \
    fi && \
    chown -R 1001:1001 /var/pvpgn /etc/pvpgn

# ---------------------------------------------------------------------------
# Declare persistent volumes for data and configuration
# (these are overridden by named volumes in docker-compose)
# ---------------------------------------------------------------------------
VOLUME /var/pvpgn
VOLUME /etc/pvpgn

# ---------------------------------------------------------------------------
# Expose Battle.net (6112) and additional service (4000) ports
# ---------------------------------------------------------------------------
EXPOSE 6112
EXPOSE 4000

# ---------------------------------------------------------------------------
# Drop privileges to the pvpgn system user
# ---------------------------------------------------------------------------
USER 1001:1001

# ---------------------------------------------------------------------------
# Default command: run bnetd in foreground mode with config from /etc/pvpgn
# ---------------------------------------------------------------------------
ENTRYPOINT ["/usr/local/pvpgn/sbin/bnetd"]
CMD ["-f", "-c", "/etc/pvpgn/bnetd.conf"]
