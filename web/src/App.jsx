import { useCallback, useEffect, useRef, useState } from 'react';
import './App.css';

const AC_LIMIT = 8;
const DEBOUNCE_MS = 180;

function useDebouncedCallback(fn, delay) {
  const t = useRef(null);
  return useCallback(
    (...args) => {
      if (t.current) {
        clearTimeout(t.current);
      }
      t.current = setTimeout(() => {
        t.current = null;
        fn(...args);
      }, delay);
    },
    [fn, delay],
  );
}

async function fetchJson(url) {
  const res = await fetch(url);
  if (!res.ok) {
    const text = await res.text();
    throw new Error(text || res.statusText);
  }
  return res.json();
}

async function fetchJsonPost(url, body) {
  const res = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) {
    throw new Error(data.error || JSON.stringify(data) || res.statusText);
  }
  return data;
}

export default function App() {
  const [query, setQuery] = useState('');
  const [suggestions, setSuggestions] = useState([]);
  const [open, setOpen] = useState(false);
  const [activeIdx, setActiveIdx] = useState(-1);
  const [searchResult, setSearchResult] = useState(null);
  const [lastSearched, setLastSearched] = useState('');
  const [loadingAc, setLoadingAc] = useState(false);
  const [loadingSearch, setLoadingSearch] = useState(false);
  const [error, setError] = useState(null);
  const [newWord, setNewWord] = useState('');
  const [loadingInsert, setLoadingInsert] = useState(false);
  const [insertMsg, setInsertMsg] = useState(null);
  const [deleteWord, setDeleteWord] = useState('');
  const [loadingDelete, setLoadingDelete] = useState(false);
  const [deleteMsg, setDeleteMsg] = useState(null);
  const wrapRef = useRef(null);

  const runAutocomplete = useCallback(async (q) => {
    const trimmed = q.trim();
    if (!trimmed) {
      setSuggestions([]);
      setOpen(false);
      return;
    }
    setLoadingAc(true);
    setError(null);
    try {
      const params = new URLSearchParams({ q: trimmed, limit: String(AC_LIMIT) });
      const data = await fetchJson(`https://search-autocomplete-2.onrender.com/api/autocomplete?${params}`);
      setSuggestions(data.suggestions || []);
      setOpen(true);
      setActiveIdx(-1);
    } catch (e) {
      setError(e.message || String(e));
      setSuggestions([]);
    } finally {
      setLoadingAc(false);
    }
  }, []);

  const debouncedAc = useDebouncedCallback(runAutocomplete, DEBOUNCE_MS);

  useEffect(() => {
    debouncedAc(query);
  }, [query, debouncedAc]);

  const runSearch = async (q) => {
    const trimmed = q.trim();
    if (!trimmed) {
      setSearchResult({ empty: true });
      return;
    }
    setLoadingSearch(true);
    setError(null);
    setOpen(false);
    try {
      const params = new URLSearchParams({ q: trimmed });
      const data = await fetchJson(`https://search-autocomplete-2.onrender.com/api/search?${params}`);
      setLastSearched(trimmed);
      setSearchResult(data);
    } catch (e) {
      setError(e.message || String(e));
      setSearchResult(null);
    } finally {
      setLoadingSearch(false);
    }
  };

  useEffect(() => {
    const onDoc = (e) => {
      if (wrapRef.current && !wrapRef.current.contains(e.target)) {
        setOpen(false);
      }
    };
    document.addEventListener('mousedown', onDoc);
    return () => document.removeEventListener('mousedown', onDoc);
  }, []);

  const pickSuggestion = (word) => {
    setQuery(word);
    setOpen(false);
    runSearch(word);
  };

  const runInsert = async () => {
    const trimmed = newWord.trim();
    if (!trimmed) {
      setInsertMsg({ ok: false, text: 'Enter a word or phrase to add.' });
      return;
    }
    setLoadingInsert(true);
    setInsertMsg(null);
    setError(null);
    try {
      const data = await fetchJsonPost('https://search-autocomplete-2.onrender.com/api/insert', { text: trimmed });
      setInsertMsg({
        ok: true,
        text: `Added to trie and file. Total frequency for this phrase: ${data.frequency}.`,
      });
      setNewWord('');
      debouncedAc(query);
    } catch (e) {
      setInsertMsg({ ok: false, text: e.message || String(e) });
    } finally {
      setLoadingInsert(false);
    }
  };

  const runDelete = async () => {
    const trimmed = deleteWord.trim();
    if (!trimmed) {
      setDeleteMsg({ ok: false, text: 'Enter a word or phrase to delete.' });
      return;
    }
    setLoadingDelete(true);
    setDeleteMsg(null);
    setError(null);
    try {
      const data = await fetchJsonPost('https://search-autocomplete-2.onrender.com/api/delete', { text: trimmed });
      if (data.deleted) {
        setDeleteMsg({
          ok: true,
          text: `Deleted one occurrence. Remaining frequency: ${data.frequency}.`,
        });
      } else {
        setDeleteMsg({ ok: false, text: 'Phrase not found in vocabulary.' });
      }
      setDeleteWord('');
      debouncedAc(query);
      if (lastSearched && trimmed.toLowerCase() === lastSearched.toLowerCase()) {
        runSearch(lastSearched);
      }
    } catch (e) {
      setDeleteMsg({ ok: false, text: e.message || String(e) });
    } finally {
      setLoadingDelete(false);
    }
  };

  const onKeyDown = (e) => {
    if (!open || suggestions.length === 0) {
      if (e.key === 'Enter') {
        e.preventDefault();
        runSearch(query);
      }
      return;
    }
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      setActiveIdx((i) => (i + 1) % suggestions.length);
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      setActiveIdx((i) => (i <= 0 ? suggestions.length - 1 : i - 1));
    } else if (e.key === 'Enter') {
      e.preventDefault();
      if (activeIdx >= 0 && activeIdx < suggestions.length) {
        pickSuggestion(suggestions[activeIdx].word);
      } else {
        runSearch(query);
      }
    } else if (e.key === 'Escape') {
      setOpen(false);
    }
  };

  return (
    <div className="app">
      <h1>Trie vocabulary</h1>
      <p className="sub">
        Search, autofill, insert, and delete go through Node to <code>trie_server</code> (C trie + file).
      </p>

      <h2 className="section-title">Add to vocabulary</h2>
      <p className="sub" style={{ marginTop: '-0.25rem', marginBottom: '0.75rem' }}>
        Inserts into the in-memory trie and appends a line to <code>autofill_words.txt</code> (same as the C CLI).
      </p>
      <div className="search-row">
        <input
          type="text"
          autoComplete="off"
          placeholder="New phrase to learn…"
          value={newWord}
          onChange={(e) => setNewWord(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === 'Enter') {
              e.preventDefault();
              runInsert();
            }
          }}
        />
        <button type="button" className="secondary" onClick={runInsert} disabled={loadingInsert}>
          {loadingInsert ? '…' : 'Insert'}
        </button>
      </div>
      {insertMsg && (
        <p className={`insert-msg ${insertMsg.ok ? '' : 'err'}`}>{insertMsg.text}</p>
      )}

      <h2 className="section-title">Delete from vocabulary</h2>
      <p className="sub" style={{ marginTop: '-0.25rem', marginBottom: '0.75rem' }}>
        Deletes one occurrence from the in-memory trie and removes one matching line from{' '}
        <code>autofill_words.txt</code>.
      </p>
      <div className="search-row">
        <input
          type="text"
          autoComplete="off"
          placeholder="Phrase to delete…"
          value={deleteWord}
          onChange={(e) => setDeleteWord(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === 'Enter') {
              e.preventDefault();
              runDelete();
            }
          }}
        />
        <button type="button" className="danger" onClick={runDelete} disabled={loadingDelete}>
          {loadingDelete ? '…' : 'Delete'}
        </button>
      </div>
      {deleteMsg && (
        <p className={`insert-msg ${deleteMsg.ok ? '' : 'err'}`}>{deleteMsg.text}</p>
      )}

      <h2 className="section-title">Search &amp; autofill</h2>
      <div className="search-shell" ref={wrapRef}>
        <div className="search-row">
          <input
            type="search"
            autoComplete="off"
            placeholder="Try hello, vijay, hello world…"
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            onFocus={() => suggestions.length > 0 && setOpen(true)}
            onKeyDown={onKeyDown}
            aria-autocomplete="list"
            aria-expanded={open}
            aria-controls="ac-list"
          />
          <button type="button" onClick={() => runSearch(query)} disabled={loadingSearch}>
            {loadingSearch ? '…' : 'Search'}
          </button>
        </div>

        {open && suggestions.length > 0 && (
          <ul id="ac-list" className="suggestions" role="listbox">
            {suggestions.map((s, i) => (
              <li
                key={`${s.word}-${i}`}
                role="option"
                aria-selected={i === activeIdx}
                className={i === activeIdx ? 'active' : ''}
                onMouseEnter={() => setActiveIdx(i)}
                onMouseDown={(e) => e.preventDefault()}
                onClick={() => pickSuggestion(s.word)}
              >
                <span className="w">{s.word}</span>
                <span className="f">{s.frequency}×</span>
              </li>
            ))}
          </ul>
        )}
      </div>

      {loadingAc && query.trim() && <p className="hint">Loading suggestions…</p>}
      {error && <p className="error">{error}</p>}

      <div className="panel">
        <h2>Lookup result</h2>
        {searchResult?.empty && <p>Type something and press Search.</p>}
        {!searchResult && !error && <p className="miss">Run a search to see frequency from the trie.</p>}
        {searchResult && !searchResult.empty && searchResult.found && (
          <p className="hit">
            Found <strong>{lastSearched}</strong> — frequency <strong>{searchResult.frequency}</strong>
          </p>
        )}
        {searchResult && !searchResult.empty && searchResult.found === false && !searchResult.error && (
          <p className="miss">
            <strong>{lastSearched}</strong> is not in the vocabulary (no trie leaf for this phrase).
          </p>
        )}
        {searchResult?.error && <p className="miss">{searchResult.error}</p>}
      </div>

      <p className="hint">
        Start the API from the project root: <code>npm install</code> in <code>backend/</code>, compile C with{' '}
        <code>npm run build:trie</code>, then <code>npm run backend</code>. In another terminal:{' '}
        <code>cd web && npm install && npm run dev</code>.
      </p>
    </div>
  );
}
