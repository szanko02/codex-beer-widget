# Public relay deployment

Operator setup for a Linux server with a domain pointing to it, Node.js 24 at
`/usr/bin/node`, OpenSSL, Certbot and systemd. Open TCP 80 for certificate renewal
and TCP 443 for HTTPS/WSS. Android users only scan the pairing QR; they do not
install certificates. No Firebase project is required for foreground WSS.

Install the repository at `/opt/codex-quota`, then run as the server administrator:

```sh
useradd --system --home /var/lib/codex-quota --shell /usr/sbin/nologin codex-relay
cd /opt/codex-quota/sync-server
npm ci --omit=dev
install -d -o root -g codex-relay -m 0750 /etc/codex-quota /etc/codex-quota/tls
umask 077
openssl rand -base64 32 > /etc/codex-quota/admin-secret.txt
chown root:codex-relay /etc/codex-quota/admin-secret.txt
chmod 0640 /etc/codex-quota/admin-secret.txt
certbot certonly --standalone -d quota.example.com
printf '%s\n' /etc/letsencrypt/live/quota.example.com > /etc/codex-quota/certificate-lineage
install -m 0644 deploy/codex-quota.service /etc/systemd/system/
install -m 0755 deploy/renew-certificate.sh /etc/letsencrypt/renewal-hooks/deploy/codex-quota
systemctl daemon-reload
RENEWED_LINEAGE=/etc/letsencrypt/live/quota.example.com /etc/letsencrypt/renewal-hooks/deploy/codex-quota
systemctl enable codex-quota
certbot renew --dry-run
```

Replace `quota.example.com` with the real domain before executing. The renewal
hook copies only this domain's key and certificate, then restarts the relay.
Keep repository files root-owned; the service can write only its state directory.
Back up `/var/lib/codex-quota` and the administrator secret securely. Do not rerun
secret generation against an existing deployment.

## Connect a Windows publisher and phone

Securely copy the administrator secret to a private temporary file on the PC.
From `sync-server/`, run:

```powershell
node tools/provision-public.mjs https://quota.example.com C:/private/admin-secret.txt ../.local/public-relay
../tools/configure-sync.ps1 -ConfigurationFile ../.local/public-relay/publisher.json
Invoke-Item ../.local/public-relay/pairing.png
```

Close the Windows widget before configuring sync; restart it afterwards in normal
mode, with Codex authenticated. Scan the QR using Android's **Connect computer**
button within five minutes. Delete the temporary administrator-secret copy once
provisioning is complete. Keep publisher credentials private. An existing Windows
sync configuration must be revoked before replacement; never reset its revision.

To regenerate an expired invitation without creating another publisher:

```powershell
node tools/pair.mjs --directory ../.local/public-relay --public
```

Verify fresh real limits on the PC and phone. Then test the phone on mobile data,
restart both apps, and stop the relay to verify stale values persist. These checks
require a deployed server; local USB tests do not prove Internet delivery.

Without configured Firebase, background refresh uses Android's best-effort
15-minute worker. Instant background delivery and FCM are not promised.
