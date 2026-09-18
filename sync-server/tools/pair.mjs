import { readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { request } from 'node:https';
import QRCode from 'qrcode';

const directory = resolve('../.local/relay');
const config = JSON.parse(readFileSync(resolve(directory, 'publisher.json'), 'utf8'));
const ca = readFileSync(resolve(directory, 'tls-cert.pem'));
const post = (path, token, data) => new Promise((resolve, reject) => {
  const req = request(new URL(path, config.origin), {
    method: 'POST', ca, timeout: 5000,
    headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' },
  }, res => {
    const chunks = [];
    res.on('data', chunk => chunks.push(chunk));
    res.on('end', () => {
      if (res.statusCode < 200 || res.statusCode >= 300) { reject(new Error(`Relay status ${res.statusCode}`)); return; }
      try { resolve(JSON.parse(Buffer.concat(chunks))); } catch (error) { reject(error); }
    });
  });
  req.on('error', reject);
  req.on('timeout', () => req.destroy(new Error('Relay timeout')));
  req.end(JSON.stringify(data));
});
if (process.argv.includes('--register')) {
  await post('/v1/devices', readFileSync(resolve(directory, 'admin-secret.txt'), 'utf8').trim(), config);
}
const invite = await post('/v1/pair', config.publisherSecret, { deviceId: config.deviceId });
const qr = JSON.stringify({ ...invite, origin: config.origin });
writeFileSync(resolve(directory, 'pairing.json'), qr, { mode: 0o600 });
await QRCode.toFile(resolve(directory, 'pairing.png'), qr, { width: 600 });
console.log('Pairing QR saved to .local/relay/pairing.png; one use, expires in five minutes.');
