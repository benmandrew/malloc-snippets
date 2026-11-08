# The programs only behave as the article describes on Linux with glibc.
# Debian 12 ships glibc 2.36.
#
# The base image is pinned by digest, and apt reads from snapshot.debian.org
# at a fixed date, so that a rebuild installs the same gcc, glibc and strace
# as the build that produced output/. output/environment.txt records them.
FROM debian:12.8@sha256:b877a1a3fdf02469440f1768cf69c9771338a875b7add5e80c45b756c92ac20a

ARG SNAPSHOT=20261105T000000Z

RUN rm -f /etc/apt/sources.list.d/debian.sources \
    && printf '%s\n' \
        "deb http://snapshot.debian.org/archive/debian/${SNAPSHOT} bookworm main" \
        "deb http://snapshot.debian.org/archive/debian/${SNAPSHOT} bookworm-updates main" \
        "deb http://snapshot.debian.org/archive/debian-security/${SNAPSHOT} bookworm-security main" \
        > /etc/apt/sources.list \
    && apt-get -o Acquire::Check-Valid-Until=false update \
    && apt-get install -y --no-install-recommends gcc libc6-dev make strace \
    && rm -rf /var/lib/apt/lists/*
