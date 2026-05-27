#!/bin/sh
set -e
mkdir -p /tmp/pvpgn-conf /tmp/pvpgn-state
cp /src/build/v3-bnetd-on/conf/*.toml /tmp/pvpgn-conf/
cp /src/build/v3-bnetd-on/conf/*.conf /tmp/pvpgn-conf/ 2>/dev/null || true
cp /src/build/v3-bnetd-on/conf/*.json /tmp/pvpgn-conf/ 2>/dev/null || true
sed -i 's#${SYSCONFDIR}#/tmp/pvpgn-conf#g; s#${LOCALSTATEDIR}#/tmp/pvpgn-state#g' /tmp/pvpgn-conf/bnetd.toml
echo '--- after sed ---'
grep -E 'logfile|LOCALSTATEDIR|SYSCONFDIR' /tmp/pvpgn-conf/bnetd.toml | head -10
