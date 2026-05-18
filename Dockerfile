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
# Default feature flags - individual build stages override these as needed
# ---------------------------------------------------------------------------
ENV WITH_LUA=false
ENV WITH_MYSQL=false
ENV WITH_PGSQL=false
ENV WITH_SQLITE3=false
ENV WITH_ODBC=false

# ---------------------------------------------------------------------------
# Common install layout used by every build-<MODE> stage:
#   /usr/local/pvpgn -> binaries (sbin, bin, lib, share)
#   /etc/pvpgn       -> configuration files
#   /var/pvpgn       -> runtime data (users, clans, teams, files, ...)
#
# Pinning SYSCONF_INSTALL_DIR / LOCALSTATE_INSTALL_DIR here is what makes
# `make install` actually populate /etc/pvpgn and /var/pvpgn inside the
# build image. Without this they default to /etc and /var, so the runtime
# image never ships the configs/templates that bnetd needs to start.
# ---------------------------------------------------------------------------
ENV PVPGN_SYSCONFDIR=/etc/pvpgn
ENV PVPGN_LOCALSTATEDIR=/var/pvpgn


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
      -D SYSCONF_INSTALL_DIR=${PVPGN_SYSCONFDIR} \
      -D LOCALSTATE_INSTALL_DIR=${PVPGN_LOCALSTATEDIR} \
    && cd build && make


# =============================================================================
# Stage: build-mysql
# Adds MariaDB/MySQL client library and enables the MySQL storage backend.
# =============================================================================
FROM build-base AS build-mysql

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
      -D SYSCONF_INSTALL_DIR=${PVPGN_SYSCONFDIR} \
      -D LOCALSTATE_INSTALL_DIR=${PVPGN_LOCALSTATEDIR} \
    && cd build && make


# =============================================================================
# Stage: build-pgsql
# =============================================================================
FROM build-base AS build-pgsql

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
      -D SYSCONF_INSTALL_DIR=${PVPGN_SYSCONFDIR} \
      -D LOCALSTATE_INSTALL_DIR=${PVPGN_LOCALSTATEDIR} \
    && cd build && make


# =============================================================================
# Stage: build-sqlite3
# =============================================================================
FROM build-base AS build-sqlite3

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
      -D SYSCONF_INSTALL_DIR=${PVPGN_SYSCONFDIR} \
      -D LOCALSTATE_INSTALL_DIR=${PVPGN_LOCALSTATEDIR} \
    && cd build && make


# =============================================================================
# Stage: build-odbc
# =============================================================================
FROM build-base AS build-odbc

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
      -D SYSCONF_INSTALL_DIR=${PVPGN_SYSCONFDIR} \
      -D LOCALSTATE_INSTALL_DIR=${PVPGN_LOCALSTATEDIR} \
    && cd build && make


# =============================================================================
# Stage: build  (selector)
# Resolves to the correct build-<MODE> stage, installs everything, and stashes
# a pristine copy of /etc/pvpgn and /var/pvpgn under /usr/local/share/pvpgn so
# the runtime entrypoint can seed mounted named volumes that come up empty.
# =============================================================================
FROM build-${MODE} AS build

WORKDIR /src/build

RUN make install \
    && mkdir -p /usr/local/share/pvpgn \
    && cp -a /etc/pvpgn /usr/local/share/pvpgn/etc \
    && cp -a /var/pvpgn /usr/local/share/pvpgn/var \
    && chown -R 1001:1001 \
         /usr/local/pvpgn \
         /usr/local/share/pvpgn


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
      su-exec \
    && rm -rf /var/cache/apk/*


# =============================================================================
# Stage: runner-mysql
# =============================================================================
FROM runner-plain AS runner-mysql

RUN apk --quiet --no-cache add \
      mariadb-connector-c \
    && rm -rf /var/cache/apk/*


# =============================================================================
# Stage: runner-pgsql
# =============================================================================
FROM runner-plain AS runner-pgsql

RUN apk --quiet --no-cache add \
      libpq \
    && rm -rf /var/cache/apk/*


# =============================================================================
# Stage: runner-sqlite3
# =============================================================================
FROM runner-plain AS runner-sqlite3

RUN apk --quiet --no-cache add \
      sqlite-libs \
    && rm -rf /var/cache/apk/*


# =============================================================================
# Stage: runner-odbc
# =============================================================================
FROM runner-plain AS runner-odbc

RUN apk --quiet --no-cache add \
      unixodbc \
    && rm -rf /var/cache/apk/*


# =============================================================================
# Stage: runner  (final image)
# =============================================================================
FROM runner-${MODE} AS runner

# ---------------------------------------------------------------------------
# Create the unprivileged pvpgn user/group (uid/gid 1001)
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
# Copy installed binaries and the pristine configs+data seed from the build
# stage.
# ---------------------------------------------------------------------------
COPY --from=build --chown=1001:1001 /usr/local/pvpgn       /usr/local/pvpgn
COPY --from=build --chown=1001:1001 /usr/local/share/pvpgn /usr/local/share/pvpgn

# ---------------------------------------------------------------------------
# Symlink PvPGN binaries into standard PATH locations
# ---------------------------------------------------------------------------
RUN ln -sf /usr/local/pvpgn/sbin/* /usr/local/sbin/ 2>/dev/null || true \
    && ln -sf /usr/local/pvpgn/bin/*  /usr/local/bin/  2>/dev/null || true

# ---------------------------------------------------------------------------
# Create runtime mount points. The entrypoint will seed them from
# /usr/local/share/pvpgn if a named volume mounts them empty.
# ---------------------------------------------------------------------------
RUN mkdir -p /etc/pvpgn /var/pvpgn \
    && chown -R 1001:1001 /etc/pvpgn /var/pvpgn

# ---------------------------------------------------------------------------
# Install the entrypoint script
# ---------------------------------------------------------------------------
COPY --chown=root:root scripts/docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh
RUN chmod 0755 /usr/local/bin/docker-entrypoint.sh

# ---------------------------------------------------------------------------
# Network ports
# ---------------------------------------------------------------------------
EXPOSE 6112
EXPOSE 4000

# ---------------------------------------------------------------------------
# Entrypoint runs as root so it can chown the named volumes and seed them,
# then drops to uid/gid 1001 via su-exec before exec'ing bnetd in the
# foreground. `-f` keeps bnetd attached so it stays PID 1; the entrypoint
# also patches bnetd.conf so logfile=/dev/stdout, which is what makes
# `docker logs` actually show server output.
# ---------------------------------------------------------------------------
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
CMD ["/usr/local/pvpgn/sbin/bnetd", "-f", "-c", "/etc/pvpgn/bnetd.conf"]
