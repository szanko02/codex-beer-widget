import { mkdirSync, writeFileSync, existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { randomBytes } from 'node:crypto';
import { execFileSync } from 'node:child_process';

const directory = resolve('../.local/relay');
if (existsSync(directory)) throw new Error('Relay directory exists; preserve existing identity and data');
const host = process.argv[2] ?? 'localhost';
if (!/^[A-Za-z0-9.-]+$/.test(host)) throw new Error('Provide a DNS hostname or IPv4 address');
mkdirSync(directory, { recursive: true, mode: 0o700 });
const openssl = process.env.OPENSSL ?? (process.platform === 'win32' ? 'C:/Program Files/Git/usr/bin/openssl.exe' : 'openssl');
const san = /^\d+\.\d+\.\d+\.\d+$/.test(host) ? `IP:${host}` : `DNS:${host}`;
execFileSync(openssl, ['req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '30', '-subj', '/CN=Codex Local Relay',
  '-addext', `subjectAltName=DNS:localhost,IP:127.0.0.1,IP:10.0.2.2,${san}`,
  '-keyout', resolve(directory, 'tls-key.pem'), '-out', resolve(directory, 'tls-cert.pem')], { stdio: 'ignore' });
writeFileSync(resolve(directory, 'admin-secret.txt'), randomBytes(32).toString('base64url'), { mode: 0o600, flag: 'wx' });
writeFileSync(resolve(directory, 'publisher.json'), JSON.stringify({
  origin: `https://${host}:8443`, deviceId: randomBytes(32).toString('base64url'),
  publisherSecret: randomBytes(32).toString('base64url'),
}, null, 2), { mode: 0o600, flag: 'wx' });
console.log('Local certificate and private configuration created in .local/relay. No certificate trust was changed.');
