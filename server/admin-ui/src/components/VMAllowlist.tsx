import React, { useState, useEffect } from 'react';
import { VMAllowlistEntry } from '../types';
import { api } from '../api';
import { VMAllowlistModal } from './VMAllowlistModal';

export const VMAllowlist: React.FC = () => {
  const [entries, setEntries] = useState<VMAllowlistEntry[]>([]);
  const [loading, setLoading] = useState<boolean>(true);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);

  // Filter
  const [licenseIdFilter, setLicenseIdFilter] = useState<string>('');

  // Modal
  const [showAddModal, setShowAddModal] = useState<boolean>(false);

  const fetchEntries = async () => {
    try {
      setLoading(true);
      setError(null);
      const licNum = licenseIdFilter ? parseInt(licenseIdFilter, 10) : undefined;
      const data = await api.getVMAllowlist({
        license_id: isNaN(licNum as any) ? undefined : licNum,
      });
      setEntries(data);
    } catch (err: any) {
      setError(err.message || 'Erro ao carregar lista de VMs autorizadas.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchEntries();
  }, []);

  const handleFilterSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    fetchEntries();
  };

  const handleDeactivate = async (entry: VMAllowlistEntry) => {
    if (!window.confirm(`Deseja desativar a permissão de VM #${entry.id} para a licença #${entry.license_id}?`)) {
      return;
    }
    try {
      setError(null);
      setSuccess(null);
      await api.removeVMAllowlist(entry.id);
      setSuccess(`VM #${entry.id} desativada com sucesso!`);
      fetchEntries();
    } catch (err: any) {
      setError(err.message || 'Falha ao desativar VM da allowlist.');
    }
  };

  const formatDate = (isoString?: string | null) => {
    if (!isoString) return '—';
    try {
      return new Date(isoString).toLocaleString('pt-BR');
    } catch {
      return isoString;
    }
  };

  return (
    <div className="section-container">
      <div className="section-header">
        <div>
          <h2>Allowlist de Máquinas Virtuais</h2>
          <p className="text-muted">
            Controle exceções corporativas para execução autorizada em ambientes virtualizados e hypervisors.
          </p>
        </div>
        <div className="section-header-actions">
          <button
            type="button"
            className="btn btn-primary"
            onClick={() => setShowAddModal(true)}
            id="btn-add-vm-allowlist"
          >
            + Nova VM Autorizada
          </button>
        </div>
      </div>

      {success && <div className="alert alert-success mb-4">{success}</div>}
      {error && <div className="alert alert-error mb-4">{error}</div>}

      {/* Filters Toolbar */}
      <div className="filters-card">
        <form onSubmit={handleFilterSubmit} className="filters-form">
          <div className="filter-item">
            <input
              type="number"
              className="form-control"
              placeholder="Filtrar por ID da Licença..."
              value={licenseIdFilter}
              onChange={(e) => setLicenseIdFilter(e.target.value)}
              id="input-filter-vm-license"
            />
          </div>

          <div className="filter-actions">
            <button type="submit" className="btn btn-secondary" id="btn-filter-vm">
              Filtrar
            </button>
            <button
              type="button"
              className="btn btn-light"
              onClick={() => {
                setLicenseIdFilter('');
                fetchEntries();
              }}
            >
              Limpar
            </button>
          </div>
        </form>
      </div>

      {/* VM Allowlist Table */}
      <div className="data-card">
        {loading ? (
          <div className="loading-state">Carregando allowlist de VMs...</div>
        ) : entries.length === 0 ? (
          <div className="empty-state">
            <p>Nenhuma máquina virtual cadastrada na allowlist.</p>
          </div>
        ) : (
          <div className="table-responsive">
            <table className="data-table" id="table-vm-allowlist">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>Licença</th>
                  <th>Fingerprint VM</th>
                  <th>Status</th>
                  <th>Notas / Justificativa</th>
                  <th>Criado em</th>
                  <th>Ações</th>
                </tr>
              </thead>
              <tbody>
                {entries.map((entry) => (
                  <tr key={entry.id} id={`vm-row-${entry.id}`}>
                    <td className="font-bold">#{entry.id}</td>
                    <td>
                      <span className="font-bold">Licença #{entry.license_id}</span>
                    </td>
                    <td>
                      <code className="text-xs font-mono" title={entry.fingerprint_hash}>
                        {entry.fingerprint_hash.substring(0, 16)}...
                      </code>
                    </td>
                    <td>
                      {entry.active ? (
                        <span className="status-badge status-ativa">Autorizada</span>
                      ) : (
                        <span className="status-badge status-revogada">Desativada</span>
                      )}
                    </td>
                    <td>
                      <span className="text-xs text-muted block max-w-xs truncate" title={entry.notes}>
                        {entry.notes || '—'}
                      </span>
                    </td>
                    <td>
                      <span className="text-xs text-muted">{formatDate(entry.created_at)}</span>
                    </td>
                    <td>
                      {entry.active ? (
                        <button
                          type="button"
                          className="btn btn-danger btn-xs"
                          onClick={() => handleDeactivate(entry)}
                          id={`btn-deactivate-vm-${entry.id}`}
                        >
                          Desativar
                        </button>
                      ) : (
                        <span className="text-muted text-xs">Inativo</span>
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Modal Nova VM */}
      {showAddModal && (
        <VMAllowlistModal
          onClose={() => setShowAddModal(false)}
          onSuccess={() => {
            setShowAddModal(false);
            fetchEntries();
          }}
        />
      )}
    </div>
  );
};
