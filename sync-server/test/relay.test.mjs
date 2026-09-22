import test from 'node:test';
import assert from 'node:assert/strict';
import { randomBytes } from 'node:crypto';
import { readFileSync, mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { once } from 'node:events';
import { execFileSync } from 'node:child_process';
import { request as httpsRequest } from 'node:https';
import { WebSocket } from 'ws';
import { createRelay } from '../src/relay.mjs';

const secret = () => randomBytes(32).toString('base64url');
const fixture = JSON.parse(readFileSync(new URL('../../protocol/fixtures/normalization.json', import.meta.url)))[0].expected;
async function setup(t, options = {}) {
  const admin = secret(), publisher = secret(), device = secret();
  const relay = createRelay({ adminSecret: admin, ...options });
  relay.server.listen(0, '127.0.0.1');
  await once(relay.server, 'listening');
  const origin = `http://127.0.0.1:${relay.server.address().port}`;
  const request = (path, token, data) => fetch(origin + path, {
    method: data === undefined ? 'GET' : 'POST',
    headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' },
    body: data === undefined ? undefined : JSON.stringify(data),
  });
  const close = () => relay.close();
  t.after(close);
  assert.equal((await request('/v1/devices', admin, { deviceId: device, publisherSecret: publisher })).status, 200);
  const invite = await (await request('/v1/pair', publisher, { deviceId: device })).json();
  const reader = (await (await request('/v1/pair/redeem', '', invite)).json()).readerSecret;
  return { relay, origin, request, admin, publisher, device, reader, invite, path: `/v1/devices/${device}/state` };
}

test('push registration is reader scoped and unpair cancels pending hints', async t => {
  const sent = [];
  const { request, publisher, reader, device, path } = await setup(t, {
    sendPush: async (...args) => sent.push(args), pushInterval: 100,
  });
  const endpoint = `/v1/devices/${device}/push`;
  assert.equal((await request(endpoint, publisher, { token: secret() })).status, 401);
  assert.equal((await request(endpoint, reader, { token: 'invalid' })).status, 400);
  assert.equal((await request(endpoint, reader, { token: secret() })).status, 200);
  await request(path, publisher, fixture);
  await request('/v1/unpair', reader, { deviceId: device, scope: 'subscriber' });
  await new Promise(resolve => setTimeout(resolve, 150));
  assert.equal(sent.length, 0);
  assert.equal((await request(endpoint, reader, { token: secret() })).status, 401);
});

test('scoped authentication, snapshot validation and idempotent revisions', async t => {
  const { request, publisher, reader, path, invite } = await setup(t);
  assert.equal((await request(path, reader)).status, 404);
  assert.equal((await request(path, reader, fixture)).status, 401);
  assert.equal((await request(path, publisher, { ...fixture, email: 'private' })).status, 400);
  assert.equal((await request(path, publisher, fixture)).status, 200);
  assert.equal((await request(path, publisher, fixture)).status, 200);
  assert.equal((await request(path, publisher, { ...fixture, revision: 152 })).status, 409);
  assert.equal((await request(path, publisher, { ...fixture, stale: true })).status, 409);
  assert.deepEqual(await (await request(path, reader)).json(), fixture);
  assert.equal((await request(path, publisher)).status, 401);
  assert.equal((await request('/v1/pair/redeem', '', invite)).status, 401);
});

test('WebSocket stream receives state and unpair revokes access', async t => {
  const { request, publisher, reader, path, origin, device } = await setup(t);
  const socket = new WebSocket(origin.replace('http:', 'ws:') + `/v1/devices/${device}/stream`, {
    headers: { Authorization: `Bearer ${reader}` },
  });
  t.after(() => socket.terminate());
  await once(socket, 'open');
  const message = once(socket, 'message');
  await request(path, publisher, fixture);
  assert.deepEqual(JSON.parse((await message)[0]), fixture);
  const closed = once(socket, 'close');
  assert.equal((await request('/v1/unpair', reader, { deviceId: device, scope: 'subscriber' })).status, 200);
  await closed;
  assert.equal((await request(path, reader)).status, 401);
  assert.equal((await request(path, publisher, { ...fixture, revision: 154 })).status, 200);
});

test('pairing expires and replacing a subscriber revokes its token', async t => {
  let now = Date.now();
  const { request, publisher, reader, path, device } = await setup(t, { now: () => now });
  let invite = await (await request('/v1/pair', publisher, { deviceId: device })).json();
  now += 301000;
  assert.equal((await request('/v1/pair/redeem', '', invite)).status, 401);
  invite = await (await request('/v1/pair', publisher, { deviceId: device })).json();
  assert.equal((await request('/v1/pair/redeem', '', invite)).status, 200);
  assert.equal((await request(path, reader)).status, 401);
  assert.equal((await request('/v1/unpair', publisher, { deviceId: device })).status, 200);
  assert.equal((await request(path, publisher, fixture)).status, 401);
});

test('last snapshot and revision survive relay restart', async () => {
  const directory = mkdtempSync(join(tmpdir(), 'quota-relay-'));
  const database = join(directory, 'state.sqlite');
  const adminSecret = secret(), publisherSecret = secret(), deviceId = secret();
  let relay;
  const launch = async () => {
    relay = createRelay({ database, adminSecret });
    relay.server.listen(0, '127.0.0.1');
    await once(relay.server, 'listening');
    return `http://127.0.0.1:${relay.server.address().port}`;
  };
  try {
    let origin = await launch();
    const request = (path, token, data) => fetch(origin + path, {
      method: data === undefined ? 'GET' : 'POST', headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' },
      body: data === undefined ? undefined : JSON.stringify(data),
    });
    await request('/v1/devices', adminSecret, { deviceId, publisherSecret });
    const invite = await (await request('/v1/pair', publisherSecret, { deviceId })).json();
    const { readerSecret } = await (await request('/v1/pair/redeem', '', invite)).json();
    const path = `/v1/devices/${deviceId}/state`;
    await request(path, publisherSecret, fixture);
    await relay.close(); relay = null;
    origin = await launch();
    assert.deepEqual(await (await request(path, readerSecret)).json(), fixture);
    assert.equal((await request(path, publisherSecret, { ...fixture, revision: 1 })).status, 409);
  } finally {
    if (relay) await relay.close();
    rmSync(directory, { recursive: true, force: true });
  }
});

test('reject cross-origin requests and rate-limit guesses', async t => {
  const { origin, request, path } = await setup(t);
  assert.equal((await fetch(origin + path, { headers: { Origin: 'https://attacker.invalid' } })).status, 403);
  let response;
  for (let i = 0; i < 121; i++) response = await request(path, 'invalid');
  assert.equal(response.status, 429);
});

test('HTTPS and WSS verify the local CA without disabling TLS validation', async () => {
  const directory = mkdtempSync(join(tmpdir(), 'quota-tls-'));
  const key = join(directory, 'key.pem'), cert = join(directory, 'cert.pem');
  const openssl = process.platform === 'win32' ? 'C:/Program Files/Git/usr/bin/openssl.exe' : 'openssl';
  let relay;
  try {
    execFileSync(openssl, ['req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1', '-subj', '/CN=localhost',
      '-addext', 'subjectAltName=DNS:localhost,IP:127.0.0.1', '-keyout', key, '-out', cert], { stdio: 'ignore' });
    const ca = readFileSync(cert), admin = secret(), publisher = secret(), device = secret();
    relay = createRelay({ adminSecret: admin, tls: { key: readFileSync(key), cert: ca } });
    relay.server.listen(0, '127.0.0.1');
    await once(relay.server, 'listening');
    const origin = `https://127.0.0.1:${relay.server.address().port}`;
    const post = (path, token, data) => new Promise((resolve, reject) => {
      const req = httpsRequest(origin + path, { method: 'POST', ca,
        headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' } }, res => {
        const chunks = [];
        res.on('data', c => chunks.push(c));
        res.on('end', () => { try { assert.equal(res.statusCode, 200); resolve(JSON.parse(Buffer.concat(chunks))); } catch (e) { reject(e); } });
      });
      req.on('error', reject);
      req.end(JSON.stringify(data));
    });
    await assert.rejects(fetch(origin + '/v1/devices')); // Self-signed cert is not globally trusted.
    await post('/v1/devices', admin, { deviceId: device, publisherSecret: publisher });
    const invite = await post('/v1/pair', publisher, { deviceId: device });
    const { readerSecret } = await post('/v1/pair/redeem', '', invite);
    const socket = new WebSocket(origin.replace('https:', 'wss:') + `/v1/devices/${device}/stream`, {
      ca, headers: { Authorization: `Bearer ${readerSecret}` },
    });
    await once(socket, 'open');
    const received = once(socket, 'message');
    await post(`/v1/devices/${device}/state`, publisher, fixture);
    assert.deepEqual(JSON.parse((await received)[0]), fixture);
    socket.terminate();
  } finally {
    if (relay) await relay.close();
    rmSync(directory, { recursive: true, force: true });
  }
});
