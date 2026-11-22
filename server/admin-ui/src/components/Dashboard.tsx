import React, { useEffect, useState } from 'react';
import { api } from '../api';
import { Metrics } from '../types';

interface DashboardProps {
  onNavigate: (tab: string) => void;
}

export const Dashboard: React.FC<DashboardProps> = ({ onNavigate }) => {
  const [metrics, setMetrics] = useState<Metrics | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const fetchMetrics = async () => {
    setLoading(true);
    setError(null);
    try {
      const data = await api.getMetrics();
      setMetrics(data);
    } catch (err: any) {
      setError(err.message || 'Falha ao carregar métricas.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchMetrics();
  }, []);

  if (loading) {
    return <div className="empty-state">Carregando métricas agregadas...</div>;
  }

  if (error) {
    return (
      <div className="alert alert-error">
        <div>{error}</div>
        <button className="btn btn-sm btn-secondary" onClick={fetchMetrics}>Tentar novamente</button>
      </div>
    );
  }

  if (!metrics) return null;

  return (
    <div>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '1.5rem' }}>
        <div>
          <h2 style={{ fontSize: '1.5rem', fontWeight: 700 }}>Painel de Controle</h2>
          <p style={{ color: 'var(--text-muted)', fontSize: '0.875rem' }}>
            Visão consolidada do sistema de licenciamento e segurança
          </p>
        </div>
        <button className="btn btn-secondary btn-sm" onClick={fetchMetrics}>
          Atualizar Dados
        </button>
      </div>

      {/* Grid de Métricas */}
      <div className="grid-cards">
        <div className="stat-card" style={{ borderLeft: '4px solid var(--success)' }}>
          <div className="stat-title">Licenças Ativas</div>
          <div className="stat-number">{metrics.licenses.active}</div>
          <div className="stat-sub">de {metrics.licenses.total} licenças emitidas</div>
        </div>

        <div className="stat-card" style={{ borderLeft: '4px solid var(--warning)' }}>
          <div className="stat-title">Licenças Suspensas</div>
          <div className="stat-number">{metrics.licenses.suspended}</div>
          <div className="stat-sub">{metrics.licenses.revoked} revogadas, {metrics.licenses.expired} expiradas</div>
        </div>

        <div className="stat-card" style={{ borderLeft: '4px solid var(--primary)' }}>
          <div className="stat-title">Dispositivos Vinculados</div>
          <div className="stat-number">{metrics.devices.active}</div>
          <div className="stat-sub">de {metrics.devices.total} cadastros no histórico</div>
        </div>

        <div className="stat-card" style={{ borderLeft: '4px solid var(--danger)' }}>
          <div className="stat-title">Abusos Abertos (7 dias)</div>
          <div className="stat-number">{metrics.abuse.open_7d}</div>
          <div className="stat-sub">{metrics.abuse.total_open} abertos no total</div>
        </div>
      </div>

      {/* Ações Rápidas */}
      <div style={{ display: 'flex', gap: '0.75rem', marginBottom: '2rem', flexWrap: 'wrap' }}>
        <button className="btn btn-primary" onClick={() => onNavigate('licenses')}>
          Gerenciar Licenças
        </button>
        <button className="btn btn-secondary" onClick={() => onNavigate('abuse')}>
          Revisar Fila de Abuso ({metrics.abuse.total_open})
        </button>
        <button className="btn btn-secondary" onClick={() => onNavigate('vm_allowlist')}>
          Allowlist de VM
        </button>
      </div>

      {/* Tabela de Logins Recentes */}
      <div className="card-table">
        <div className="table-header-area">
          <div className="table-title">Auditoria: Logins Recentes</div>
        </div>
        <div className="table-responsive">
          <table className="data-table">
            <thead>
              <tr>
                <th>ID</th>
                <th>Ator</th>
                <th>Ação</th>
                <th>Alvo</th>
                <th>Data e Hora (UTC)</th>
              </tr>
            </thead>
            <tbody>
              {metrics.recent_logins.length === 0 ? (
                <tr>
                  <td colSpan={5} className="empty-state">Nenhum evento recente de login registrado.</td>
                </tr>
              ) : (
                metrics.recent_logins.map((log) => (
                  <tr key={log.id}>
                    <td>#{log.id}</td>
                    <td><strong>{log.actor}</strong></td>
                    <td>
                      <span className="badge" style={{ background: log.action.includes('admin') ? '#e0e7ff' : '#ecfdf5', color: log.action.includes('admin') ? '#4338ca' : '#065f46' }}>
                        {log.action}
                      </span>
                    </td>
                    <td>{log.target}</td>
                    <td>{new Date(log.ts).toLocaleString('pt-BR')}</td>
                  </tr>
                ))
              )}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
};
