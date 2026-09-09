// Development-only probe; the shipped widget does not depend on Node.js.
import { spawn, execFileSync } from 'node:child_process';
import { createInterface } from 'node:readline';
import { mkdirSync, writeFileSync, existsSync } from 'node:fs';
import { join } from 'node:path';
import { createHash } from 'node:crypto';

const codex = process.env.CODEX_WIDGET_CODEX_PATH || join(process.env.APPDATA,
  'npm/node_modules/@openai/codex/node_modules/@openai/codex-win32-x64/vendor/x86_64-pc-windows-msvc/bin/codex.exe');
if (!existsSync(codex)) throw new Error('Set CODEX_WIDGET_CODEX_PATH to native codex.exe');
mkdirSync('.local', { recursive: true });
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function session(seconds) {
  const started = performance.now();
  const child = spawn(codex, ['app-server', '--stdio'], { windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'] });
  let sequence = 0, notifications = 0, stderrBytes = 0;
  const pending = new Map();
  const reads = [];
  child.stderr.on('data', b => { stderrBytes += b.length; });
  const lines = createInterface({ input: child.stdout });
  lines.on('line', line => {
    let message;
    try { message = JSON.parse(line); } catch { return; }
    if (message.method === 'account/rateLimits/updated') notifications++;
    if (pending.has(message.id)) {
      const { resolve, reject, timer } = pending.get(message.id);
      clearTimeout(timer); pending.delete(message.id);
      message.error ? reject(new Error(`RPC ${message.error.code}: ${message.error.message}`)) : resolve(message.result);
    }
  });
  const rpc = (method, params) => new Promise((resolve, reject) => {
    const id = ++sequence;
    const timer = setTimeout(() => { pending.delete(id); reject(new Error(`Timeout: ${method}`)); }, 25000);
    pending.set(id, { resolve, reject, timer });
    child.stdin.write(JSON.stringify({ id, method, params }) + '\n');
  });
  function metrics() {
    return JSON.parse(execFileSync('powershell.exe', ['-NoProfile', '-Command',
      `$p = Get-Process -Id ${child.pid}; [pscustomobject]@{workingSetMiB=$p.WorkingSet64/1MB;privateMiB=$p.PrivateMemorySize64/1MB;cpuSeconds=$p.TotalProcessorTime.TotalSeconds;threads=$p.Threads.Count} | ConvertTo-Json -Compress`
    ], { windowsHide: true, encoding: 'utf8' }));
  }
  try {
    await rpc('initialize', { clientInfo: { name: 'codex_beer_widget_probe', title: 'Codex Beer Widget Probe', version: '0.1.0' } });
    child.stdin.write('{"method":"initialized"}\n');
    const account = await rpc('account/read', { refreshToken: false });
    const identity = account.account ? createHash('sha256').update(JSON.stringify(account.account)).digest('hex') : null;
    const read = async () => {
      const result = await rpc('account/rateLimits/read', {});
      reads.push({ at: new Date().toISOString(), ...result });
      return result;
    };
    const first = await read();
    const firstReadMs = performance.now() - started;
    const before = metrics();
    const measureStarted = performance.now();
    for (let elapsed = 0; elapsed < seconds; elapsed += 15) {
      await delay(Math.min(15, seconds - elapsed) * 1000);
      await read();
    }
    const after = metrics();
    const elapsedSeconds = (performance.now() - measureStarted) / 1000;
    return { authenticated: !!account.account, authType: account.account?.type, identity,
      pid: child.pid, firstReadMs, elapsedSeconds, notifications, stderrBytes, before, after,
      averageCpuOneCorePercent: 100 * (after.cpuSeconds - before.cpuSeconds) / elapsedSeconds,
      windows: first.rateLimitsByLimitId ?? { legacy: first.rateLimits }, reads };
  } finally {
    for (const { timer, reject } of pending.values()) { clearTimeout(timer); reject(new Error('Probe ended')); }
    child.stdin.end();
    await delay(250);
    if (child.exitCode === null) child.kill();
    lines.close();
  }
}
const first = await session(60);
const restart = await session(0);
const report = { version: execFileSync(codex, ['--version'], { encoding: 'utf8', windowsHide: true }).trim(),
  at: new Date().toISOString(), sameAccountAfterRestart: !!first.identity && first.identity === restart.identity, first, restart };
writeFileSync('.local/source-probe.json', JSON.stringify(report, null, 2));
const { identity, reads, ...safe } = first;
console.log(JSON.stringify({ version: report.version, sameAccountAfterRestart: report.sameAccountAfterRestart, ...safe }, null, 2));
