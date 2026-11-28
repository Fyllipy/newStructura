import React, { useEffect, useState } from 'react';
import { Plan } from '../types';
import { api } from '../api';

const brl = (cents: number) => (cents / 100).toLocaleString('pt-BR', { style: 'currency', currency: 'BRL' });
const toCents = (txt: string): number => Math.round(parseFloat(txt.replace(/\./g, '').replace(',', '.')) * 100);

const ERRORS: Record<string, string> = {
  PRICE_TOO_LOW: 'O valor mínimo para um plano ativo é R$ 0,50 (limite do Stripe).',
  PRICE_INVALID: 'Valor inválido.',
  PERIOD_INVALID: 'Duração inválida (1 a 3660 dias).',
  PLAN_EXISTS: 'Já existe um plano com esse código.',
};

export const Plans: React.FC = () => {
  const [plans, setPlans] = useState<Plan[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);
  const [drafts, setDrafts] = useState<Record<string, string>>({});
  const [adding, setAdding] = useState(false);
  const [nw, setNw] = useState({ code: '', name: '', period_days: '30', price: '' });

  const load = async () => {
    try {
      setLoading(true);
      const rows = await api.getPlans();
      setPlans(rows);
      setDrafts(Object.fromEntries(rows.map((p) => [p.code, (p.price_cents / 100).toFixed(2).replace('.', ',')])));
    } catch (e: any) {
      setError(e.message || 'Erro ao carregar planos.');
    } finally {
      setLoading(false);
    }
  };
  useEffect(() => { load(); }, []);

  const explain = (e: any) => ERRORS[e?.message] || e?.message || 'Falha na operação.';

  const save = async (p: Plan, patch: Partial<Plan>) => {
    setError(null); setSuccess(null);
    try {
      await api.updatePlan(p.code, patch);
      setSuccess('Plano "' + p.name + '" atualizado.');
      await load();
    } catch (e: any) { setError(explain(e)); }
  };

  const savePrice = (p: Plan) => {
    const cents = toCents(drafts[p.code] || '');
    if (!Number.isFinite(cents) || cents < 0) { setError('Informe um valor válido, ex.: 99,90'); return; }
    save(p, { price_cents: cents });
  };

  const create = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null); setSuccess(null);
    const cents = nw.price ? toCents(nw.price) : 0;
    try {
      await api.createPlan({ code: nw.code.trim().toLowerCase(), name: nw.name.trim(),
        period_days: parseInt(nw.period_days, 10), price_cents: cents, active: cents >= 50 });
      setSuccess('Plano criado.');
      setAdding(false);
      setNw({ code: '', name: '', period_days: '30', price: '' });
      await load();
    } catch (err: any) { setError(explain(err)); }
  };

  return (
    <div className="section-container">
      <div className="section-header">
        <div>
          <h2>Planos e preços</h2>
          <p className="text-muted">
            Valores cobrados na loja do site e na renovação automática. Só planos <b>ativos</b> aparecem para compra.
            Alterar um preço vale para novas compras e para as próximas renovações.
          </p>
        </div>
        <div className="section-header-actions">
          <button type="button" className="btn btn-primary" onClick={() => setAdding(!adding)} id="btn-new-plan">
            + Novo plano
          </button>
        </div>
      </div>

      {success && <div className="alert alert-success mb-4">{success}</div>}
      {error && <div className="alert alert-error mb-4">{error}</div>}

      {adding && (
        <form className="filters-card" onSubmit={create} style={{ display: 'flex', gap: '0.75rem', flexWrap: 'wrap', alignItems: 'end' }}>
          <div className="form-group"><label className="form-label">Código</label>
            <input className="form-control" required pattern="[a-z0-9_\-]{2,32}" placeholder="semestral" value={nw.code}
              onChange={(e) => setNw({ ...nw, code: e.target.value })} /></div>
          <div className="form-group"><label className="form-label">Nome</label>
            <input className="form-control" required placeholder="Semestral" value={nw.name}
              onChange={(e) => setNw({ ...nw, name: e.target.value })} /></div>
          <div className="form-group"><label className="form-label">Duração (dias)</label>
            <input className="form-control" type="number" min={1} max={3660} required value={nw.period_days}
              onChange={(e) => setNw({ ...nw, period_days: e.target.value })} /></div>
          <div className="form-group"><label className="form-label">Preço (R$)</label>
            <input className="form-control" placeholder="499,90" value={nw.price}
              onChange={(e) => setNw({ ...nw, price: e.target.value })} /></div>
          <button className="btn btn-primary" type="submit">Criar</button>
        </form>
      )}

      <div className="data-card">
        {loading ? (
          <div className="loading-state">Carregando planos...</div>
        ) : (
          <div className="table-responsive">
            <table className="data-table" id="table-plans">
              <thead>
                <tr><th>Plano</th><th>Duração</th><th>Preço (R$)</th><th>Na loja</th><th>Atualizado</th></tr>
              </thead>
              <tbody>
                {plans.map((p) => (
                  <tr key={p.code} id={'plan-row-' + p.code}>
                    <td><span className="font-bold">{p.name}</span> <code className="text-xs">{p.code}</code></td>
                    <td>{p.period_days} dias</td>
                    <td>
                      <div style={{ display: 'flex', gap: '0.5rem', alignItems: 'center' }}>
                        <input className="form-control" style={{ width: '8rem' }} inputMode="decimal"
                          value={drafts[p.code] ?? ''} aria-label={'Preço do plano ' + p.name}
                          onChange={(e) => setDrafts({ ...drafts, [p.code]: e.target.value })} />
                        <button className="btn btn-secondary btn-sm" onClick={() => savePrice(p)}
                          disabled={toCents(drafts[p.code] || '') === p.price_cents}>Salvar</button>
                        <span className="text-xs text-muted">atual: {brl(p.price_cents)}</span>
                      </div>
                    </td>
                    <td>
                      {p.active
                        ? <button className="btn btn-danger btn-sm" onClick={() => save(p, { active: false })}>Desativar</button>
                        : <button className="btn btn-primary btn-sm" onClick={() => save(p, { active: true })}
                            title="Precisa de preço ≥ R$ 0,50">Ativar</button>}
                      {' '}<span className={'status-badge ' + (p.active ? 'status-ativa' : 'status-revogada')}>
                        {p.active ? 'Ativo' : 'Inativo'}</span>
                    </td>
                    <td><span className="text-xs text-muted">{p.updated_at ? new Date(p.updated_at).toLocaleString('pt-BR') : '—'}</span></td>
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
