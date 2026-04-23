'use strict';

const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const readline = require('readline');
const express = require('express');
const cors = require('cors');

const PORT = Number(process.env.PORT) || 3001;
const PROJECT_ROOT = path.resolve(__dirname, '..');
const TRIE_EXE = path.join(PROJECT_ROOT, 'trie_server');
const DEFAULT_VOCAB = path.join(PROJECT_ROOT, 'autofill_words.txt');

class TrieProcess {
  constructor() {
    this.child = null;
    this.rl = null;
    this.lineQueue = [];
    this.lineWaiters = [];
  }

  pushLine(line) {
    if (this.lineWaiters.length > 0) {
      const { resolve } = this.lineWaiters.shift();
      resolve(line);
    } else {
      this.lineQueue.push(line);
    }
  }

  readLine() {
    if (this.lineQueue.length > 0) {
      return Promise.resolve(this.lineQueue.shift());
    }
    return new Promise((resolve, reject) => {
      this.lineWaiters.push({ resolve, reject });
    });
  }

  rejectAllWaiters(err) {
    while (this.lineWaiters.length) {
      const { reject } = this.lineWaiters.shift();
      reject(err);
    }
  }

  async start() {
    const vocabPath = process.env.TRIE_VOCAB || DEFAULT_VOCAB;

    this.child = spawn(TRIE_EXE, [vocabPath], {
      cwd: PROJECT_ROOT,
      stdio: ['pipe', 'pipe', 'pipe'],
    });

    this.child.stderr.on('data', (buf) => {
      process.stderr.write(buf);
    });

    this.child.on('error', (err) => {
      this.rejectAllWaiters(err);
    });

    this.rl = readline.createInterface({ input: this.child.stdout });

    this.rl.on('line', (line) => this.pushLine(line));

    this.child.on('close', (code) => {
      this.child = null;
      this.rejectAllWaiters(new Error(`trie_server exited (${code})`));
    });

    const first = await this.readLine();
    if (!first || !first.startsWith('READY|')) {
      throw new Error(`Expected READY from trie_server, got: ${first}`);
    }
    const loaded = Number(first.slice('READY|'.length)) || 0;
    return { loaded, vocabPath };
  }

  writeLine(s) {
    if (!this.child || !this.child.stdin.writable) {
      throw new Error('trie_server is not running');
    }
    this.child.stdin.write(`${s}\n`);
  }

  async search(query) {
    const q = String(query).replace(/\|/g, ' ').replace(/\r?\n/g, ' ').trim();
    if (!q) {
      return { found: false, frequency: 0, error: 'empty query' };
    }
    this.writeLine(`SEARCH|${q}`);
    const line = await this.readLine();
    if (line.startsWith('HIT|')) {
      return { found: true, frequency: Number(line.slice(4)) || 0 };
    }
    if (line === 'MISS') {
      return { found: false, frequency: 0 };
    }
    if (line.startsWith('ERR|')) {
      return { found: false, frequency: 0, error: line.slice(4) };
    }
    return { found: false, frequency: 0, error: `unexpected: ${line}` };
  }

  async autocomplete(prefix, limit) {
    const pref = String(prefix).replace(/\|/g, ' ').replace(/\r?\n/g, ' ').trim();
    const k = Math.max(1, Math.min(50, Number(limit) || 8));
    const suggestions = [];

    if (!pref) {
      return { suggestions: [] };
    }

    this.writeLine(`AUTO|${k}|${pref}`);

    for (;;) {
      const line = await this.readLine();
      if (line === 'END') {
        break;
      }
      if (line.startsWith('ROW|')) {
        const parts = line.split('|');
        if (parts.length >= 3) {
          const word = parts.slice(1, -1).join('|');
          const freq = Number(parts[parts.length - 1]) || 0;
          suggestions.push({ word, frequency: freq });
        }
      } else if (line.startsWith('ERR|')) {
        return { suggestions: [], error: line.slice(4) };
      }
    }

    return { suggestions };
  }

  async insert(text) {
    const raw = String(text).replace(/\|/g, ' ').replace(/\r?\n/g, ' ').trim();
    if (!raw) {
      return { ok: false, error: 'empty text' };
    }
    this.writeLine(`INSERT|${raw}`);
    const line = await this.readLine();
    if (line.startsWith('OK|')) {
      return { ok: true, frequency: Number(line.slice(3)) || 0 };
    }
    if (line.startsWith('ERR|')) {
      return { ok: false, error: line.slice(4) };
    }
    return { ok: false, error: `unexpected: ${line}` };
  }

  async delete(text) {
    const raw = String(text).replace(/\|/g, ' ').replace(/\r?\n/g, ' ').trim();
    if (!raw) {
      return { deleted: false, frequency: 0, error: 'empty text' };
    }
    this.writeLine(`DELETE|${raw}`);
    const line = await this.readLine();
    if (line.startsWith('DELETED|')) {
      return { deleted: true, frequency: Number(line.slice(8)) || 0 };
    }
    if (line === 'MISS') {
      return { deleted: false, frequency: 0 };
    }
    if (line.startsWith('ERR|')) {
      return { deleted: false, frequency: 0, error: line.slice(4) };
    }
    return { deleted: false, frequency: 0, error: `unexpected: ${line}` };
  }

  stop() {
    if (this.child && this.child.stdin.writable) {
      try {
        this.child.stdin.write('EXIT\n');
      } catch (_) {
        /* ignore */
      }
    }
    if (this.child) {
      this.child.kill('SIGTERM');
    }
    this.child = null;
    if (this.rl) {
      this.rl.close();
    }
    this.rl = null;
  }
}

const trie = new TrieProcess();
let mutex = Promise.resolve();

function serial(task) {
  const next = mutex.then(() => task());
  mutex = next.catch(() => {});
  return next;
}

async function main() {
  if (!fs.existsSync(TRIE_EXE)) {
    console.error(`Missing ${TRIE_EXE}. Run from project root: npm run build:trie`);
    process.exit(1);
  }

  const meta = await trie.start();
  console.log(`trie_server ready (loaded ${meta.loaded} lines from ${meta.vocabPath})`);

  const app = express();
  app.use(cors({ origin: true }));
  app.use(express.json());

  app.get('/api/health', (_req, res) => {
    res.json({ ok: true, trie: !!trie.child });
  });

  app.get('/api/search', (req, res) => {
    const q = req.query.q;
    serial(() => trie.search(q))
      .then((body) => res.json(body))
      .catch((e) => res.status(500).json({ error: String(e.message || e) }));
  });

  app.get('/api/autocomplete', (req, res) => {
    const q = req.query.q;
    const limit = req.query.limit;
    serial(() => trie.autocomplete(q, limit))
      .then((body) => res.json(body))
      .catch((e) => res.status(500).json({ error: String(e.message || e) }));
  });

  app.post('/api/insert', (req, res) => {
    const text = req.body && (req.body.text ?? req.body.word);
    serial(() => trie.insert(text))
      .then((body) => {
        if (body.ok) {
          res.json(body);
        } else {
          res.status(400).json(body);
        }
      })
      .catch((e) => res.status(500).json({ ok: false, error: String(e.message || e) }));
  });

  app.post('/api/delete', (req, res) => {
    const text = req.body && (req.body.text ?? req.body.word);
    serial(() => trie.delete(text))
      .then((body) => {
        if (body.error) {
          res.status(400).json(body);
        } else {
          res.json(body);
        }
      })
      .catch((e) => res.status(500).json({ deleted: false, error: String(e.message || e) }));
  });

  const server = app.listen(PORT, () => {
    console.log(`API http://127.0.0.1:${PORT}`);
  });

  const shutdown = () => {
    server.close(() => {
      trie.stop();
      process.exit(0);
    });
  };
  process.on('SIGINT', shutdown);
  process.on('SIGTERM', shutdown);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
