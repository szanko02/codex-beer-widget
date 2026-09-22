import { readFileSync, mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { createRelay } from './relay.mjs';
import { firebaseSender } from './push.mjs';

const required = name => { if (!process.env[name]) throw new Error(`${name} is required`); return process.env[name]; };
const database = resolve(process.env.SYNC_DATABASE ?? '../.local/relay/state.sqlite');
mkdirSync(dirname(database), { recursive: true, mode: 0o700 });
const relay = createRelay({
  sendPush: await firebaseSender(),
  database,
  adminSecret: readFileSync(required('SYNC_ADMIN_SECRET_FILE'), 'utf8').trim(),
  tls: { key: readFileSync(required('SYNC_TLS_KEY')), cert: readFileSync(required('SYNC_TLS_CERT')), minVersion: 'TLSv1.2' },
});
relay.server.listen(Number(process.env.PORT ?? 8443), process.env.SYNC_HOST ?? '127.0.0.1', () => {
  console.log('Quota relay ready (HTTPS).');
});
for (const signal of ['SIGINT', 'SIGTERM']) process.on(signal, () => relay.close().then(() => process.exit(0)));
