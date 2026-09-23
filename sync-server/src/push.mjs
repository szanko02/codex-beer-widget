// One pending hint per device; snapshots remain available only through authenticated HTTPS.
export function createPushQueue(send, { interval = 30000 } = {}) {
  const entries = new Map();
  function enqueue(id, token, revision) {
    let entry = entries.get(id);
    if (!entry) { entry = { timer: null, busy: false, version: 0 }; entries.set(id, entry); }
    entry.token = token;
    entry.revision = revision;
    entry.version++;
    schedule(id, entry);
  }
  function schedule(id, entry) {
    if (entry.timer || entry.busy) return;
    entry.timer = setTimeout(async () => {
      if (entries.get(id) !== entry) return;
      entry.timer = null;
      entry.busy = true;
      const version = entry.version;
      try { await send(id, entry.token, entry.revision); } catch { /* Polling remains the fallback. */ }
      entry.busy = false;
      if (entries.get(id) !== entry) return;
      if (entry.version !== version) schedule(id, entry);
      else entries.delete(id);
    }, interval);
    entry.timer.unref();
  }
  function cancel(id) { clearTimeout(entries.get(id)?.timer); entries.delete(id); }
  return { enqueue, cancel, close() { for (const id of entries.keys()) cancel(id); } };
}

export async function firebaseSender() {
  if (!process.env.GOOGLE_APPLICATION_CREDENTIALS) return undefined;
  const { initializeApp, applicationDefault } = await import('firebase-admin/app');
  const { getMessaging } = await import('firebase-admin/messaging');
  const app = initializeApp({ credential: applicationDefault() });
  return (deviceId, token, revision) => getMessaging(app).send({ token,
    data: { type: 'quota_changed', deviceId, revision: String(revision) },
    android: { priority: 'normal', ttl: 600000, collapseKey: deviceId },
  });
}
