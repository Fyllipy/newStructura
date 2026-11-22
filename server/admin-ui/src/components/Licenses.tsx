import React, { useState, useEffect } from 'react';
import { License } from '../types';
import { api } from '../api';
import { LicenseModal } from './LicenseModal';
import { LicenseDetailModal } from './LicenseDetailModal';

export const Licenses: React.FC = () => {
  const [licenses, setLicenses] = useState<License[]>([]);
  const [loading, setLoading] = useState<boolean>(true);
  const [error, setError] = useState<string | null>(null);

  // Filters
  const [search, setSearch] = useState<string>('');
  const [statusFilter, setStatusFilter] = useState<string>('');
  const [planFilter, setPlanFilter] = useState<string>('');

  // Modals
  const [showCreateModal, setShowCreateModal] = useState<boolean>(false);
  const [selectedLicenseId, setSelectedLicenseId] = useState<number | null>(null);

  const fetchLicenses = async () => {
    try {
      setLoading(true);
      setError(null);
      const data = await api.getLicenses({
        search: search.trim() || undefined,
        status: statusFilter || undefined,
        plan: planFilter || undefined,
      });
      setLicenses(data);
    } catch (err: any) {
      setError(err.message || 'Erro ao carregar lista de licenças.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchLicenses();
  }, [statusFilter, planFilter]);

  const handleSearchSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    fetchLicenses();
  };

  const handleQuickSuspend = async (license: License) => {
    if (!window.confirm(`Deseja suspender a licença #${license.id}?`)) return;
    try {
      await api.updateLicense(license.id, {
        status: 'suspensa',
        reason: 'Suspensão rápida via painel',
      });
      fetchLicenses();
    } catch (err: any) {
      alert(`Erro: ${err.message}`);
    }
  };

  const handleQuickReactivate = async (license: License) => {
    try {
      await api.updateLicense(license.id, {
        status: 'ativa',
        reason: 'Reativação rápida via painel',
      });
      fetchLicenses();
    } catch (err: any) {
      alert(`Erro: ${err.message}`);
    }
  };

  const formatDate = (isoString?: string | null) => {
    if (!isoString) return '—';
    try {
      const d = new Date(isoString);
      return d.toLocaleDateString('pt-BR');
    } catch {
      return isoString;
    }
  };

  return (
    <div className="section-container">
      <div className="section-header">
        <div>
          <h2>Licenças de Uso</h2>
          <p className="text-muted">Gerencie permissões, validades e suspensão de licenças de clientes.</p>
        </div>
        <div className="section-header-actions">
          <button
            type="button"
            className="btn btn-primary"
            onClick={() => setShowCreateModal(true)}
            id="btn-new-license"
          >
            + Nova Licença
          </button>
        </div>
      </div>

      {error && <div className="alert alert-error mb-4">{error}</div>}

      {/* Toolbar / Filters */}
      <div className="filters-card">
        <form onSubmit={handleSearchSubmit} className="filters-form">
          <div className="filter-item filter-search">
            <input
              type="text"
              className="form-control"
              placeholder="Buscar por e-mail ou ID..."
              value={search}
              onChange={(e) => setSearch(e.target.value)}
              id="input-search-licenses"
            />
          </div>

          <div className="filter-item">
            <select
              className="form-control"
              value={statusFilter}
              onChange={(e) => setStatusFilter(e.target.value)}
              id="select-status-filter"
            >
              <option value="">Todos os status</option>
              <option value="ativa">Ativa</option>
              <option value="suspensa">Suspensa</option>
              <option value="revogada">Revogada</option>
              <option value="expirada">Expirada</option>
            </select>
          </div>

          <div className="filter-item">
            <select
              className="form-control"
              value={planFilter}
              onChange={(e) => setPlanFilter(e.target.value)}
              id="select-plan-filter"
            >
              <option value="">Todos os planos</option>
              <option value="trial">Trial</option>
              <option value="mensal">Mensal</option>
              <option value="anual">Anual</option>
              <option value="perpetuo">Perpétuo</option>
              <option value="custom">Custom</option>
            </select>
          </div>

          <div className="filter-actions">
            <button type="submit" className="btn btn-secondary" id="btn-search">
              Filtrar
            </button>
            <button
              type="button"
              className="btn btn-light"
              onClick={() => {
                setSearch('');
                setStatusFilter('');
                setPlanFilter('');
              }}
            >
              Limpar
            </button>
          </div>
        </form>
      </div>

      {/* Licenses Table */}
      <div className="data-card">
        {loading ? (
          <div className="loading-state">Carregando licenças...</div>
        ) : licenses.length === 0 ? (
          <div className="empty-state">
            <p>Nenhuma licença encontrada com os filtros selecionados.</p>
          </div>
        ) : (
          <div className="table-responsive">
            <table className="data-table" id="table-licenses">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>Usuário</th>
                  <th>Plano</th>
                  <th>Status</th>
                  <th>Validade</th>
                  <th>Dispositivos</th>
                  <th>Ações</th>
                </tr>
              </thead>
              <tbody>
                {licenses.map((lic) => (
                  <tr key={lic.id} id={`license-row-${lic.id}`}>
                    <td className="font-bold">#{lic.id}</td>
                    <td>
                      <span className="font-mono text-sm">
                        {lic.user_email || `ID ${lic.user_id}`}
                      </span>
                    </td>
                    <td>
                      <span className="uppercase text-xs font-bold text-muted">
                        {lic.plan}
                      </span>
                    </td>
                    <td>
                      <span className={`status-badge status-${lic.status}`}>
                        {lic.status.toUpperCase()}
                      </span>
                    </td>
                    <td>{formatDate(lic.current_period_end)}</td>
                    <td>
                      <span className="badge-count">
                        {lic.devices_count ?? 0} ativo(s)
                      </span>
                    </td>
                    <td>
                      <div className="btn-row">
                        <button
                          type="button"
                          className="btn btn-secondary btn-xs"
                          onClick={() => setSelectedLicenseId(lic.id)}
                          id={`btn-manage-license-${lic.id}`}
                        >
                          Ver / Gerenciar
                        </button>
                        {lic.status === 'ativa' ? (
                          <button
                            type="button"
                            className="btn btn-warning btn-xs"
                            onClick={() => handleQuickSuspend(lic)}
                            id={`btn-suspend-license-${lic.id}`}
                          >
                            Suspender
                          </button>
                        ) : (
                          <button
                            type="button"
                            className="btn btn-success btn-xs"
                            onClick={() => handleQuickReactivate(lic)}
                            id={`btn-reactivate-license-${lic.id}`}
                          >
                            Reativar
                          </button>
                        )}
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Modal Criar Licença */}
      {showCreateModal && (
        <LicenseModal
          isOpen={showCreateModal}
          onClose={() => setShowCreateModal(false)}
          onSuccess={() => {
            setShowCreateModal(false);
            fetchLicenses();
          }}
        />
      )}

      {/* Modal Detalhes / Gerenciar Licença */}
      {selectedLicenseId !== null && (
        <LicenseDetailModal
          licenseId={selectedLicenseId}
          onClose={() => setSelectedLicenseId(null)}
          onUpdated={() => fetchLicenses()}
        />
      )}
    </div>
  );
};
