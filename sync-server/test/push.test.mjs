import test from 'node:test';
import assert from 'node:assert/strict';
import { setTimeout as delay } from 'node:timers/promises';
import { createPushQueue } from '../src/push.mjs';

test('slow sender retains only the latest pending hint', async () => {
  let release;
  const blocked = new Promise(resolve => { release = resolve; });
  const sent = [];
  const queue = createPushQueue(async (...args) => { sent.push(args); await blocked; }, { interval: 10 });
  try {
    queue.enqueue('device', 'token', 1);
    await delay(40);
    for (let revision = 2; revision <= 100; revision++) queue.enqueue('device', 'token', revision);
    await delay(40);
    assert.equal(sent.length, 1);
    release();
    await delay(40);
    assert.deepEqual(sent.map(args => args[2]), [1, 100]);
  } finally { release(); queue.close(); }
});

test('push hints coalesce and revocation cancels pending delivery', async () => {
  const sent = [];
  const queue = createPushQueue(async (...args) => sent.push(args), { interval: 20 });
  try {
    queue.enqueue('device', 'token', 1);
    queue.enqueue('device', 'token', 2);
    queue.enqueue('revoked', 'old', 3);
    queue.cancel('revoked');
    await delay(60);
    assert.deepEqual(sent, [['device', 'token', 2]]);
    queue.enqueue('closed', 'token', 4);
    queue.close();
    await delay(40);
    assert.equal(sent.length, 1);
  } finally { queue.close(); }
});
