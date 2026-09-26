import { mkdirSync, readFileSync, writeFileSync, existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { randomBytes } from 'node:crypto';
import { request } from 'node:https';
import QRCode from 'qrcode';

const [originValue, adminFile, output] = process.argv.slice(2);
if (!originValue || !adminFile || !output) throw new Error('Usage: provision-public.mjs HTTPS_ORIGIN ADMIN_SECRET_FILE NEW_DIRECTORY');
const origin = new URL(originValue);
if (origin.protocol !== 'https:' || origin.username || origin.password || origin.pathname !== '/' || origin.search || origin.hash)
  throw new Error('An HTTPS origin is required');
const directory = resolve(output);
if (existsSync(directory)) throw new Error('Output directory already exists; preserve its publisher identity');
const admin = readFileSync(adminFile, 'utf8').trim();
const config = { origin: origin.origin, deviceId: randomBytes(32).toString('base64url'), publisherSecret: randomBytes(32).toString('base64url') };
const post = (path, token, data) => new Promise((resolve, reject) => {
  const req = request(new URL(path, origin), { method: 'POST', timeout: 10000,
    headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' } }, res => {
    const chunks = []; let size = 0;
    res.on('data', chunk => { size += chunk.length; if (size > 65536) res.destroy(new Error('Oversized response')); else chunks.push(chunk); });
    res.on('error', reject);
    res.on('end', () => {
      if (res.statusCode < 200 || res.statusCode >= 300) return reject(new Error(`Relay HTTP ${res.statusCode}`));
      try { resolve(JSON.parse(Buffer.concat(chunks))); } catch (error) { reject(error); }
    });
  });
  req.on('error', reject); req.on('timeout', () => req.destroy(new Error('Relay timeout')));
  req.end(JSON.stringify(data));
});
// Persist the identity before registration so a lost response cannot lose credentials.
mkdirSync(directory, { recursive: true, mode: 0o700 });
writeFileSync(resolve(directory, 'publisher.json'), JSON.stringify(config, null, 2), { mode: 0o600, flag: 'wx' });
await post('/v1/devices', admin, config);
const invite = await post('/v1/pair', config.publisherSecret, { deviceId: config.deviceId });
const qr = JSON.stringify({ ...invite, origin: config.origin });
writeFileSync(resolve(directory, 'pairing.json'), qr, { mode: 0o600 });
await QRCode.toFile(resolve(directory, 'pairing.png'), qr, { width: 600 });
console.log('Publisher configured; pairing.png is ready for scanning and expires in five minutes.');
