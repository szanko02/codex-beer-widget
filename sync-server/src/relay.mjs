import { createServer as httpServer } from 'node:http';
import { createServer as httpsServer } from 'node:https';
import { createHash, randomBytes, timingSafeEqual } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { DatabaseSync } from 'node:sqlite';
import { WebSocketServer, WebSocket } from 'ws';
import Ajv from 'ajv/dist/2020.js';
import { createPushQueue } from './push.mjs';

const schema = JSON.parse(readFileSync(new URL('../../protocol/quota-state-v1.json', import.meta.url)));
const validate = new Ajv({ strict: false }).compile(schema);
const digest = text => createHash('sha256').update(text).digest('hex');
const token = () => randomBytes(32).toString('base64url');
const validId = text => typeof text === 'string' && /^[A-Za-z0-9_-]{16,128}$/.test(text);
const validSecret = text => typeof text === 'string' && /^[A-Za-z0-9_-]{43,128}$/.test(text);
const equal = (a, b) => typeof a === 'string' && typeof b === 'string' && a.length === b.length &&
  timingSafeEqual(Buffer.from(a), Buffer.from(b));
const canonical = value => JSON.stringify(value, function (key, v) {
  return v && typeof v === 'object' && !Array.isArray(v)
    ? Object.fromEntries(Object.keys(v).sort().map(k => [k, v[k]])) : v;
});
const fail = status => { throw Object.assign(new Error('Request rejected'), { status }); };
async function body(req) {
  if (!req.headers['content-type']?.startsWith('application/json')) fail(415);
  const chunks = [];
  let length = 0;
  for await (const chunk of req) {
    length += chunk.length;
    if (length > 65536) fail(413);
    chunks.push(chunk);
  }
  try { return JSON.parse(Buffer.concat(chunks).toString('utf8')); } catch { fail(400); }
}

export function createRelay({ database = ':memory:', adminSecret, tls, now = () => Date.now(), sendPush, pushInterval } = {}) {
  if (!validSecret(adminSecret)) throw new Error('A random 256-bit admin secret is required');
  const adminHash = digest(adminSecret);
  const db = new DatabaseSync(database);
  db.exec(`PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA secure_delete=ON;
    CREATE TABLE IF NOT EXISTS devices (
      id TEXT PRIMARY KEY, publisher TEXT NOT NULL, reader TEXT, invite TEXT, expires INTEGER,
      revision INTEGER NOT NULL DEFAULT 0, snapshot TEXT, received INTEGER);
  `);
  if (!db.prepare('PRAGMA table_info(devices)').all().some(column => column.name === 'push')) db.exec('ALTER TABLE devices ADD COLUMN push TEXT');
  const push = sendPush ? createPushQueue(sendPush, { interval: pushInterval }) : null;
  const get = id => db.prepare('SELECT * FROM devices WHERE id=?').get(id);
  const ws = new WebSocketServer({ noServer: true, maxPayload: 1024, perMessageDeflate: false });
  const rate = new Map();
  function rateLimit(req) {
    const key = req.socket.remoteAddress;
    const time = now();
    for (const [ip, slot] of rate) if (slot.until <= time) rate.delete(ip);
    if (!rate.has(key)) {
      if (rate.size >= 4096) fail(429);
      rate.set(key, { until: time + 60000, count: 0 });
    }
    if (++rate.get(key).count > 120) fail(429);
  }
  function authenticate(req, device, scope) {
    const header = req.headers.authorization;
    if (!header?.startsWith('Bearer ') || header.length > 150) fail(401);
    const hash = digest(header.slice(7));
    if (scope === 'admin') { if (!equal(hash, adminHash)) fail(401); return; }
    const row = get(device);
    if (!row || !equal(hash, row[scope])) fail(401);
    return row;
  }
  function disconnect(device) {
    push?.cancel(device);
    for (const client of ws.clients) if (client.device === device) client.terminate();
  }
  function broadcast(device, snapshot) {
    for (const client of ws.clients) {
      if (client.device !== device || client.readyState !== WebSocket.OPEN) continue;
      if (client.bufferedAmount > 65536) { client.terminate(); continue; }
      client.send(snapshot);
    }
  }
  const handler = async (req, res) => {
    res.setHeader('Cache-Control', 'no-store');
    res.setHeader('Content-Type', 'application/json');
    res.setHeader('X-Content-Type-Options', 'nosniff');
    try {
      rateLimit(req);
      // No cookie authentication or cross-origin browser access.
      if (req.headers.origin) fail(403);
      const url = new URL(req.url, 'https://relay.invalid');
      if (url.search) fail(400);
      const route = url.pathname;
      let result;
      if (req.method === 'POST' && route === '/v1/devices') {
        authenticate(req, null, 'admin');
        const data = await body(req);
        if (!validId(data.deviceId) || !validSecret(data.publisherSecret)) fail(400);
        if (db.prepare('SELECT count(*) AS count FROM devices').get().count >= 1000) fail(409);
        if (get(data.deviceId)) fail(409);
        db.prepare('INSERT INTO devices(id,publisher) VALUES(?,?)').run(data.deviceId, digest(data.publisherSecret));
        result = { deviceId: data.deviceId };
      } else if (req.method === 'POST' && route === '/v1/pair') {
        const data = await body(req);
        if (!validId(data.deviceId)) fail(400);
        authenticate(req, data.deviceId, 'publisher');
        const secret = token();
        const expiresAt = Math.floor(now() / 1000) + 300;
        db.prepare('UPDATE devices SET invite=?,expires=? WHERE id=?').run(digest(secret), expiresAt, data.deviceId);
        result = { version: 1, deviceId: data.deviceId, pairingSecret: secret, expiresAt };
      } else if (req.method === 'POST' && route === '/v1/pair/redeem') {
        const data = await body(req);
        if (!validId(data.deviceId) || !validSecret(data.pairingSecret)) fail(400);
        const row = get(data.deviceId);
        if (!row || row.expires <= Math.floor(now() / 1000) || !equal(row.invite, digest(data.pairingSecret))) fail(401);
        const secret = token();
        db.prepare('UPDATE devices SET reader=?,invite=NULL,expires=NULL,push=NULL WHERE id=?').run(digest(secret), data.deviceId);
        disconnect(data.deviceId); // Single subscriber in v1; replacement revokes the previous phone.
        result = { deviceId: data.deviceId, readerSecret: secret };
      } else if (req.method === 'POST' && route === '/v1/unpair') {
        const data = await body(req);
        if (!validId(data.deviceId)) fail(400);
        if (data.scope === 'subscriber') {
          authenticate(req, data.deviceId, 'reader');
          db.prepare('UPDATE devices SET reader=NULL,invite=NULL,expires=NULL,push=NULL WHERE id=?').run(data.deviceId);
        } else {
          authenticate(req, data.deviceId, 'publisher');
          db.prepare('DELETE FROM devices WHERE id=?').run(data.deviceId);
        }
        disconnect(data.deviceId);
        result = { unpaired: true };
      } else {
        const pushMatch = /^\/v1\/devices\/([A-Za-z0-9_-]{16,128})\/push$/.exec(route);
        if (pushMatch && req.method === 'POST') {
          authenticate(req, pushMatch[1], 'reader');
          const data = await body(req);
          authenticate(req, pushMatch[1], 'reader');
          if (typeof data.token !== 'string' || !/^[A-Za-z0-9_:\-]{20,4096}$/.test(data.token)) fail(400);
          push?.cancel(pushMatch[1]);
          db.prepare('UPDATE devices SET push=? WHERE id=?').run(data.token, pushMatch[1]);
          res.end(JSON.stringify({ registered: true }));
          return;
        }
        const match = /^\/v1\/devices\/([A-Za-z0-9_-]{16,128})\/state$/.exec(route);
        if (!match || !['GET', 'POST'].includes(req.method)) fail(404);
        const id = match[1];
        const row = authenticate(req, id, req.method === 'POST' ? 'publisher' : 'reader');
        if (req.method === 'GET') {
          if (!row.snapshot) fail(404);
          result = JSON.parse(row.snapshot);
        } else {
          const snapshot = await body(req);
          if (!validate(snapshot)) fail(400);
          const serialized = canonical(snapshot);
          // Re-read after awaiting body: another request may have advanced the revision.
          const current = authenticate(req, id, 'publisher');
          if (snapshot.revision < current.revision ||
              (snapshot.revision === current.revision && serialized !== current.snapshot)) fail(409);
          if (snapshot.revision > current.revision) {
            db.prepare('UPDATE devices SET revision=?,snapshot=?,received=? WHERE id=?')
              .run(snapshot.revision, serialized, Math.floor(now() / 1000), id);
            broadcast(id, serialized);
            if (current.push) push?.enqueue(id, current.push, snapshot.revision);
          }
          result = { revision: snapshot.revision };
        }
      }
      res.end(JSON.stringify(result));
    } catch (error) {
      res.statusCode = error.status ?? 500;
      res.end(JSON.stringify({ error: res.statusCode }));
    }
  };
  const server = tls ? httpsServer(tls, handler) : httpServer(handler);
  server.requestTimeout = 10000;
  server.headersTimeout = 10000;
  server.maxHeadersCount = 32;
  server.on('upgrade', (req, socket, head) => {
    try {
      rateLimit(req);
      if (req.headers.origin || ws.clients.size >= 128) fail(403);
      const match = /^\/v1\/devices\/([A-Za-z0-9_-]{16,128})\/stream$/.exec(req.url);
      if (!match) fail(404);
      const row = authenticate(req, match[1], 'reader');
      ws.handleUpgrade(req, socket, head, client => {
        client.device = match[1];
        client.alive = true;
        client.on('error', () => {});
        client.on('pong', () => { client.alive = true; });
        client.on('message', () => client.close(1008, 'Read-only stream'));
        if (row.snapshot) client.send(row.snapshot);
      });
    } catch (error) {
      socket.end(`HTTP/1.1 ${error.status ?? 500} Rejected\r\nConnection: close\r\n\r\n`);
    }
  });
  const timer = setInterval(() => {
    for (const client of ws.clients) {
      if (!client.alive) { client.terminate(); continue; }
      client.alive = false;
      client.ping();
    }
  }, 30000);
  timer.unref();
  return {
    server,
    async close() {
      clearInterval(timer);
      push?.close();
      for (const client of ws.clients) client.terminate();
      ws.close();
      await new Promise(resolve => { server.close(resolve); server.closeAllConnections(); });
      db.close();
    },
  };
}
