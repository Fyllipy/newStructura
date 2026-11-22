import React, { useState, useEffect } from 'react';
import { TelemetryEvent, FraudSignal } from '../types';
import { api } from '../api';

const fmt = (iso: string) => {
  try {
    return new Date(/[zZ]|[+-]\d\d:\d\d$/.test(iso) ? iso : iso + 'Z').toLocaleString('pt-BR');
  } catch {
    return iso;
  }
};

const EVENTS = ['', 'login', 'login_failed', 'heartbeat'];

export const Telemetry: React.FC = () => {
  const [events, setEvents] = useState<TelemetryEvent[]>([]);
  const [signals, setSignals] = useState<FraudSignal[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const [email, setEmail] = useState('');
  const [ip, setIp] = useState('');
  const [licenseId, setLicenseId] = useState('');
  const [event, setEvent] = useState('');
  const [hours, setHours] = useState('168');

  const load = async () => {
    try {
      setLoading(true);
      setError(null);
      const lic = licenseId ? parseInt(licenseId, 10) : undefined;
      const [ev, sg] = await Promise.all([
        api.getTelemetry({
          email: email || undefined,
          ip: ip || undefined,
          license_id: lic && !isNaN(lic) ? lic : undefined,
          event: event || undefined,
          hours: parseInt(hours, 10) || 168,
        }),
        api.getFraudSignals(24),
      ]);
      setEvents(ev);
      setSignals(sg);
    } catch (err: any) {
      setError(err.message || 'Erro ao carregar telemetria.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    load();
  }, []);

  return (
    <div className="section-container">
      <div className="section-header">
        <div>
          <h2>Telemetria e Antifraude</h2>
          <p className="text-muted">
            Acessos registrados pelo servidor (IP observado, máquina, versão). Dados expurgados automaticamente após o período de retenção.
          </p>
        </div>
      </div>

      {error && <div className="alert alert-error mb-4">{error}</div>}

      {signals.length > 0 && (
        <div className="alert alert-error mb-4" id="fraud-signals">
          <strong>Sinais de possível fraude (últimas 24h):</strong>
          <ul style={{ margin: '0.5rem 0 0 1.2rem' }}>
            {signals.map((s, i) => (
              <li key={i}>
                <code>{s.subject}</code> — {s.detail}
                <button type="button" className="btn btn-light" style={{ marginLeft: 8 }}
                  onClick={() => {
                    if (s.kind === 'ip_many_accounts' || s.kind === 'failed_burst') { setIp(s.subject); setLicenseId(''); }
                    else if (s.subject.startsWith('license:')) { setLicenseId(s.subject.slice(8)); setIp(''); }
                  }}>
                  Filtrar
                </button>
              </li>
            ))}
          </ul>
        </div>
      )}

      <div className="filters-card">
        <form onSubmit={(e) => { e.preventDefault(); load(); }} className="filters-form">
          <div className="filter-item">
            <input className="form-control" placeholder="E-mail..." value={email} onChange={(e) => setEmail(e.target.value)} id="tel-email" />
          </div>
          <div className="filter-item">
            <input className="form-control" placeholder="IP exato..." value={ip} onChange={(e) => setIp(e.target.value)} id="tel-ip" />
          </div>
          <div className="filter-item">
            <input type="number" className="form-control" placeholder="ID da licença..." value={licenseId} onChange={(e) => setLicenseId(e.target.value)} id="tel-license" />
          </div>
          <div className="filter-item">
            <select className="form-control" value={event} onChange={(e) => setEvent(e.target.value)} id="tel-event">
              {EVENTS.map((v) => <option key={v} value={v}>{v || 'Todos os eventos'}</option>)}
            </select>
          </div>
          <div className="filter-item">
            <select className="form-control" value={hours} onChange={(e) => setHours(e.target.value)} id="tel-hours">
              <option value="24">Últimas 24h</option>
              <option value="168">Últimos 7 dias</option>
              <option value="720">Últimos 30 dias</option>
              <option value="8760">Último ano</option>
            </select>
          </div>
          <div className="filter-actions">
            <button type="submit" className="btn btn-secondary">Filtrar</button>
            <button type="button" className="btn btn-light"
              onClick={() => { setEmail(''); setIp(''); setLicenseId(''); setEvent(''); setHours('168'); setTimeout(load, 0); }}>
              Limpar
            </button>
          </div>
        </form>
      </div>

      <div className="data-card">
        {loading ? (
          <div className="loading-state">Carregando telemetria...</div>
        ) : events.length === 0 ? (
          <div className="empty-state"><p>Nenhum evento no período.</p></div>
        ) : (
          <div className="table-responsive">
            <table className="data-table" id="table-telemetry">
              <thead>
                <tr>
                  <th>Quando</th>
                  <th>Evento</th>
                  <th>E-mail</th>
                  <th>IP</th>
                  <th>Licença</th>
                  <th>Computador</th>
                  <th>Sistema</th>
                  <th>Versão</th>
                  <th>VM</th>
                  <th>Detalhe</th>
                </tr>
              </thead>
              <tbody>
                {events.map((e) => (
                  <tr key={e.id}>
                    <td><span className="text-xs text-muted">{fmt(e.ts)}</span></td>
                    <td>
                      <span className={`status-badge ${e.event === 'login_failed' ? 'status-revogada' : 'status-ativa'}`}>{e.event}</span>
                    </td>
                    <td>{e.email || '—'}</td>
                    <td><code className="text-xs font-mono">{e.ip || '—'}</code></td>
                    <td>{e.license_id ? `#${e.license_id}` : '—'}</td>
                    <td title={`CPU: ${e.cpu || '—'} | Locale: ${e.locale || '—'} | Fuso: ${e.timezone || '—'} | FP: ${e.fingerprint.slice(0, 16)}`}>
                      {e.hostname || '—'}
                    </td>
                    <td>{e.os_version || '—'}</td>
                    <td>{e.app_version || '—'}</td>
                    <td>{e.is_vm ? (e.vm_vendor || 'sim') : 'não'}</td>
                    <td><span className="text-xs text-muted">{e.detail}</span></td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  );
};
