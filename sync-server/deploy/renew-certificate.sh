#!/bin/sh
set -eu
# Certbot supplies the successfully renewed certificate directory.
expected=$(cat /etc/codex-quota/certificate-lineage)
[ "${RENEWED_LINEAGE:-}" = "$expected" ] || exit 0
install -o root -g codex-relay -m 0640 "$RENEWED_LINEAGE/privkey.pem" /etc/codex-quota/tls/privkey.pem
install -o root -g codex-relay -m 0640 "$RENEWED_LINEAGE/fullchain.pem" /etc/codex-quota/tls/fullchain.pem
systemctl restart codex-quota.service
