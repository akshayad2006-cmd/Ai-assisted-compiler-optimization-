
import { useEffect, useState } from 'react'
import './App.css'

const API = 'http://127.0.0.1:8000'

const stages = [
  { id: 'tokens', label: 'Lexer' },
  { id: 'parse', label: 'Parser' },
  { id: 'ast', label: 'AST' },
  { id: 'check', label: 'Semantic Analysis' },
  { id: 'symbols', label: 'Symbol Table' },
  { id: 'tac', label: 'Three-Address Code' },
  { id: 'optimize', label: 'Rule-Based Optimizer' },
  { id: 'opt_report', label: 'Optimization Report' },
  { id: 'analyze', label: 'AI Analysis' },
  { id: 'performance', label: 'Performance' },
  { id: 'llvm', label: 'LLVM IR' },
]

function Metric({ label, value, note }) {
  return (
    <div className="metric-card">
      <span>{label}</span>
      <strong>{value}</strong>
      {note && <small>{note}</small>}
    </div>
  )
}

function CodeOutput({ title, children }) {
  return (
    <section className="report-section">
      <h4>{title}</h4>
      <pre className="report-code">{children}</pre>
    </section>
  )
}

export default function App() {
  const [source, setSource] = useState('')
  const [examples, setExamples] = useState([])
  const [selected, setSelected] = useState('demo2.src')
  const [stage, setStage] = useState('optimize')
  const [result, setResult] = useState('')
  const [loading, setLoading] = useState(false)
  const [health, setHealth] = useState('Checking...')
  const [error, setError] = useState('')
  const [aiResult, setAiResult] = useState(null)

  useEffect(() => {
    fetch(`${API}/api/health`)
      .then((response) => {
        if (!response.ok) throw new Error('Backend unavailable')
        return response.json()
      })
      .then((data) => setHealth(data.status === 'ok' ? 'Connected' : 'Unavailable'))
      .catch(() => setHealth('Offline'))

    fetch(`${API}/api/examples`)
      .then((response) => {
        if (!response.ok) throw new Error('Could not load examples')
        return response.json()
      })
      .then((data) => {
        setExamples(data)
        const example = data.find((item) => item.name === 'demo2.src') || data[0]
        if (example) {
          setSelected(example.name)
          setSource(example.source)
        }
      })
      .catch((err) => setError(err.message))
  }, [])

  async function runStage(stageId = stage) {
    setLoading(true)
    setError('')
    setAiResult(null)

    try {
      const response = await fetch(`${API}/api/run/${stageId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ source }),
      })
      const data = await response.json()
      if (!response.ok) throw new Error(data.detail || 'Compiler request failed')

      setResult(typeof data.stdout === 'string' ? data.stdout : JSON.stringify(data, null, 2))
      if (data.stderr) setError(data.stderr)
    } catch (err) {
      setResult('')
      setError(err.message || 'Could not reach the backend')
    } finally {
      setLoading(false)
    }
  }

  async function runAI(provider = 'fake:greedy') {
    setLoading(true)
    setError('')
    setResult('')
    setAiResult(null)

    try {
      const response = await fetch(`${API}/api/ai`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ source, provider }),
      })
      const data = await response.json()
      if (!response.ok) throw new Error(data.detail || 'AI pipeline request failed')

      // The API may return a parsed object or a JSON string in stdout.
      let parsed = data.data
      if (!parsed && typeof data.stdout === 'string') {
        try {
          parsed = JSON.parse(data.stdout)
        } catch {
          parsed = null
        }
      }
      setAiResult(parsed || data)
    } catch (err) {
      setError(err.message || 'Could not run the AI pipeline')
    } finally {
      setLoading(false)
    }
  }

  function chooseExample(name) {
    const example = examples.find((item) => item.name === name)
    if (!example) return
    setSelected(name)
    setSource(example.source)
    setResult('')
    setError('')
    setAiResult(null)
  }

  const summary = aiResult?.analysis_summary
  const recommender = aiResult?.recommender
  const validator = aiResult?.validator
  const runtimeCheck = validator?.runtime_check
  const decisions = validator?.decisions || []
  const recommendations = recommender?.recommendations || []
  const accepted = validator?.summary?.accepted ?? decisions.filter((item) => item.status === 'accepted').length
  const rejected = validator?.summary?.rejected ?? decisions.filter((item) => item.status === 'rejected').length
  const selectedStage = stages.find((item) => item.id === stage)

  return (
    <main className="app-shell">
      <header className="topbar">
        <div className="brand">
          <div className="brand-mark">λ</div>
          <div>
            <h1>Opti<span>Core</span></h1>
            <p>AI-ASSISTED COMPILER OPTIMIZATION</p>
          </div>
        </div>
        <div className="connection">
          <span className={`status-dot ${health === 'Connected' ? 'online' : ''}`} />
          Backend: <strong>{health}</strong>
        </div>
      </header>

      <section className="hero">
        <div>
          <div className="eyebrow">COMPILER DESIGN LAB · PROJECT WORKSPACE</div>
          <h2>Understand your code.<br /><span>Optimize its execution.</span></h2>
          <p>Explore compiler stages, inspect optimization decisions, and validate AI-assisted recommendations.</p>
        </div>
        <div className="hero-chip"><span>●</span> C++ · Python · LLVM</div>
      </section>

      <section className="workspace">
        <aside className="sidebar panel">
          <div className="section-label">WORKSPACE</div>
          <label className="field-label" htmlFor="example">Example program</label>
          <select id="example" value={selected} onChange={(event) => chooseExample(event.target.value)}>
            {examples.map((item) => (
              <option key={item.name} value={item.name}>{item.name}</option>
            ))}
          </select>

          <div className="section-label pipeline-label">COMPILER PIPELINE</div>
          <div className="stage-list">
            {stages.map((item, index) => (
              <button
                key={item.id}
                className={`stage-button ${stage === item.id ? 'active' : ''}`}
                onClick={() => setStage(item.id)}
              >
                <span className="stage-number">{String(index + 1).padStart(2, '0')}</span>
                <span>{item.label}</span>
                {stage === item.id && <span className="stage-arrow">›</span>}
              </button>
            ))}
          </div>
          <div className="sidebar-note">
            <span>i</span>
            Compiler results come from your local backend.
          </div>
        </aside>

        <section className="editor-panel panel">
          <div className="panel-heading">
            <div>
              <div className="section-label">SOURCE EDITOR</div>
              <h3>Program input <span className="file-tag">.src</span></h3>
            </div>
            <span className="language-tag">C-like language</span>
          </div>

          <textarea
            className="code-editor"
            spellCheck="false"
            value={source}
            onChange={(event) => setSource(event.target.value)}
            aria-label="Source code editor"
          />

          <div className="editor-footer">
            <span>{source.split('\n').length} lines · {source.length} characters</span>
            <button className="secondary-button" onClick={() => chooseExample(selected)}>Reset example</button>
          </div>

          <div className="run-actions">
            <button className="primary-button" disabled={loading || !source.trim()} onClick={() => runStage()}>
              {loading ? 'Running...' : `Run ${selectedStage?.label || 'Stage'} →`}
            </button>
            <button className="secondary-button" disabled={loading || !source.trim()} onClick={() => runAI('fake:greedy')}>
              Run recommendations
            </button>
            <button className="secondary-button" disabled={loading || !source.trim()} onClick={() => runAI('fake:none')}>
              Baseline
            </button>
          </div>
          <div className="stage-hint">
            Selected stage: <strong>{selectedStage?.label}</strong>
          </div>
        </section>

        <section className="results-panel panel">
          <div className="panel-heading">
            <div>
              <div className="section-label">EXECUTION OUTPUT</div>
              <h3>{aiResult ? 'AI recommendation report' : 'Compiler results'}</h3>
            </div>
            <span className="output-tag">{loading ? 'RUNNING' : 'READY'}</span>
          </div>

          {error && <div className="error-box">{error}</div>}

          {aiResult ? (
            <div className="ai-report">
              <div className="report-notice">
                <strong>Test recommender</strong>
                <span>{recommender?.provider || 'Provider not specified'}</span>
                <p>This run uses a simulated provider; it is not a live external LLM response.</p>
              </div>

              {summary && (
                <>
                  <div className="report-grid">
                    <Metric label="Original TAC" value={summary.instruction_count ?? '—'} note="instructions" />
                    <Metric label="Optimized TAC" value={summary.optimized_instruction_count ?? '—'} note="instructions" />
                    <Metric label="Original cost" value={summary.static_cost ?? '—'} note="estimated units" />
                    <Metric label="Optimized cost" value={summary.optimized_static_cost ?? '—'} note="estimated units" />
                  </div>
                  <section className="report-section">
                    <h4>Analysis summary</h4>
                    <p>Instruction count: {summary.instruction_count ?? '—'} → {summary.optimized_instruction_count ?? '—'}</p>
                    <p>Estimated static cost: {summary.static_cost ?? '—'} → {summary.optimized_static_cost ?? '—'}</p>
                  </section>
                </>
              )}

              <section className="report-section">
                <div className="report-title-row">
                  <h4>Recommendations</h4>
                  <span className="count-pill">{recommendations.length} proposed</span>
                </div>
                {recommendations.length ? recommendations.map((item) => {
                  const decision = decisions.find((entry) => entry.candidate_id === item.candidate_id)
                  return (
                    <div className="recommendation-card" key={item.candidate_id}>
                      <div className="recommendation-top">
                        <strong>{item.candidate_id}</strong>
                        <span className={`decision-pill ${decision?.status === 'accepted' ? 'accepted' : decision?.status === 'rejected' ? 'rejected' : ''}`}>
                          {decision?.status || 'proposed'}
                        </span>
                      </div>
                      <p>{item.reason || 'No reason supplied.'}</p>
                      {decision && <small>{decision.rule?.replaceAll('_', ' ')} · instruction {decision.instruction}</small>}
                      {decision?.reason && <p className="decision-reason">{decision.reason}</p>}
                    </div>
                  )
                }) : <p className="muted-text">No recommendations were returned for this run.</p>}
              </section>

              <div className="report-grid">
                <Metric label="Accepted" value={accepted} note="validated recommendations" />
                <Metric label="Rejected" value={rejected} note="recommendations rejected" />
                <Metric label="Runtime tests" value={runtimeCheck?.tests_run ?? '—'} note="sampled tests" />
              </div>

              <section className="report-section">
                <h4>Runtime equivalence check</h4>
                <span className={`decision-pill ${runtimeCheck?.equivalent ? 'accepted' : 'rejected'}`}>
                  {runtimeCheck ? (runtimeCheck.equivalent ? 'Sampled tests equivalent' : 'Mismatch detected') : 'No runtime result'}
                </span>
                {runtimeCheck?.limitation && <p className="muted-text">{runtimeCheck.limitation}</p>}
                {validator?.failure_message && <p className="decision-reason">{validator.failure_message}</p>}
              </section>

              <details className="raw-json">
                <summary>View raw pipeline JSON</summary>
                <pre className="report-code">{JSON.stringify(aiResult, null, 2)}</pre>
              </details>
            </div>
          ) : (
            <pre className="result-output">{result || 'Choose a compiler stage and click Run to inspect the actual output.'}</pre>
          )}
        </section>
      </section>

      <footer className="footer">
        <span>OPTICORE · AI-ASSISTED COMPILER OPTIMIZATION</span>
        <span>Local development workspace</span>
      </footer>
    </main>
  )
}
