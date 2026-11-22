import React, { useState, useEffect } from 'react';
import { AbuseFlag } from '../types';
import { api } from '../api';
import { AbuseReviewModal } from './AbuseReviewModal';
import { LicenseDetailModal } from './LicenseDetailModal';

export const AbuseQueue: React.FC = () => {
  const [flags, setFlags] = useState<AbuseFlag[]>([]);
  const [loading, setLoading] = useState<boolean>(true);
  const [error, setError] = useState<string | null>(null);

  // Filters
  const [statusFilter, setStatusFilter] = useState<string>('aberto');
  const [licenseIdFilter, setLicenseIdFilter] = useState<string>('');

  // Modals
  const [selectedFlag, setSelectedFlag] = useState<AbuseFlag | null>(null);
  const [selectedLicenseId, setSelectedLicenseId] = useState<number | null>(null);

  const fetchFlags = async () => {
    try {
      setLoading(true);
      setError(null);
      const licNum = licenseIdFilter ? parseInt(licenseIdFilter, 10) : undefined;
      const data = await api.getAbuseFlags({
        status: statusFilter === 'todos' ? undefined : statusFilter,
        license_id: isNaN(licNum as any) ? undefined : licNum,
      });
      setFlags(data);
    } catch (err: any) {
      setError(err.message || 'Erro ao carregar fila de abuso.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchFlags();
  }, [statusFilter]);

  const handleFilterSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    fetchFlags();
  };

  const formatDate = (isoString: string) => {
    try {
      return new Date(isoString).toLocaleString('pt-BR');
    } catch {
      return isoString;
    }
  };

  const getStatusBadge = (status: string) => {
    switch (status) {
      case 'aberto':
        return <span className="status-badge status-revogada">ABERTO</span>;
      case 'revisado':
        return <span className="status-badge status-suspensa">REVISADO</span>;
      case 'resolvido':
        return <span className="status-badge status-ativa">RESOLVIDO</span>;
      default:
        return <span className="status-badge">{status.toUpperCase()}</span>;
    }
  };

  return (
    <div className="section-container">
      <div className="section-header">
        <div>
          <h2>Fila de Abuso e Telemetria</h2>
          <p className="text-muted">
            Monitore acessos suspeitos, máquinas virtuais não autorizadas e violações de concorrência.
          </p>
        </div>
      </div>

      {error && <div className="alert alert-error mb-4">{error}</div>}

      {/* Filters Toolbar */}
      <div className="filters-card">
        <form onSubmit={handleFilterSubmit} className="filters-form">
          <div className="filter-item">
            <select
              className="form-control"
              value={statusFilter}
              onChange={(e) => setStatusFilter(e.target.value)}
              id="select-abuse-status-filter"
            >
              <option value="aberto">Apenas Abertos (Pendentes)</option>
              <option value="revisado">Revisados</option>
              <option value="resolvido">Resolvidos</option>
              <option value="ignorado">Ignorados</option>
              <option value="todos">Todos os status</option>
            </select>
          </div>

          <div className="filter-item">
            <input
              type="number"
              className="form-control"
              placeholder="Filtrar por ID da Licença..."
              value={licenseIdFilter}
              onChange={(e) => setLicenseIdFilter(e.target.value)}
              id="input-filter-abuse-license"
            />
          </div>

          <div className="filter-actions">
            <button type="submit" className="btn btn-secondary" id="btn-filter-abuse">
              Filtrar
            </button>
            <button
              type="button"
              className="btn btn-light"
              onClick={() => {
                setStatusFilter('aberto');
                setLicenseIdFilter('');
              }}
            >
              Resetar
            </button>
          </div>
        </form>
      </div>

      {/* Abuse Flags Table */}
      <div className="data-card">
        {loading ? (
          <div className="loading-state">Carregando fila de abuso...</div>
        ) : flags.length === 0 ? (
          <div className="empty-state">
            <p>Nenhuma ocorrência encontrada para os filtros selecionados.</p>
          </div>
        ) : (
          <div className="table-responsive">
            <table className="data-table" id="table-abuse-flags">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>Licença</th>
                  <th>Tipo (Kind)</th>
                  <th>Score</th>
                  <th>Status</th>
                  <th>Detalhes</th>
                  <th>Data</th>
                  <th>Ações</th>
                </tr>
              </thead>
              <tbody>
                {flags.map((flag) => (
                  <tr key={flag.id} id={`abuse-row-${flag.id}`}>
                    <td className="font-bold">#{flag.id}</td>
                    <td>
                      <button
                        type="button"
                        className="btn-link"
                        onClick={() => setSelectedLicenseId(flag.license_id)}
                      >
                        Licença #{flag.license_id}
                      </button>
                    </td>
                    <td>
                      <span className="font-mono text-xs font-bold text-danger">
                        {flag.kind}
                      </span>
                    </td>
                    <td>
                      <span className="font-bold">{flag.score}</span>
                    </td>
                    <td>{getStatusBadge(flag.status)}</td>
                    <td className="max-w-md">
                      <span className="text-xs text-muted block truncate" title={flag.details}>
                        {flag.details}
                      </span>
                    </td>
                    <td>
                      <span className="text-xs text-muted">{formatDate(flag.ts)}</span>
                    </td>
                    <td>
                      <button
                        type="button"
                        className="btn btn-primary btn-xs"
                        onClick={() => setSelectedFlag(flag)}
                        id={`btn-review-abuse-${flag.id}`}
                      >
                        Revisar
                      </button>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Review Modal */}
      {selectedFlag && (
        <AbuseReviewModal
          flag={selectedFlag}
          onClose={() => setSelectedFlag(null)}
          onSuccess={() => {
            setSelectedFlag(null);
            fetchFlags();
          }}
        />
      )}

      {/* License Modal */}
      {selectedLicenseId !== null && (
        <LicenseDetailModal
          licenseId={selectedLicenseId}
          onClose={() => setSelectedLicenseId(null)}
          onUpdated={() => fetchFlags()}
        />
      )}
    </div>
  );
};
