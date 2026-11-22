import React, { useState, useEffect } from 'react';
import { Device } from '../types';
import { api } from '../api';
import { LicenseDetailModal } from './LicenseDetailModal';

export const Devices: React.FC = () => {
  const [devices, setDevices] = useState<Device[]>([]);
  const [loading, setLoading] = useState<boolean>(true);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);

  // Filters
  const [licenseIdFilter, setLicenseIdFilter] = useState<string>('');
  const [activeOnly, setActiveOnly] = useState<boolean>(true);

  // Selected License Modal
  const [selectedLicenseId, setSelectedLicenseId] = useState<number | null>(null);

  const fetchDevices = async () => {
    try {
      setLoading(true);
      setError(null);
      const licIdNum = licenseIdFilter ? parseInt(licenseIdFilter, 10) : undefined;
      const data = await api.getDevices({
        license_id: isNaN(licIdNum as any) ? undefined : licIdNum,
        active_only: activeOnly ? true : undefined,
      });
      setDevices(data);
    } catch (err: any) {
      setError(err.message || 'Erro ao carregar dispositivos.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchDevices();
  }, [activeOnly]);

  const handleFilterSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    fetchDevices();
  };

  const handleRelease = async (deviceId: number, licenseId: number) => {
    if (!window.confirm(`Tem certeza que deseja liberar o dispositivo #${deviceId} da licença #${licenseId}?`)) {
      return;
    }
    try {
      setError(null);
      setSuccess(null);
      await api.releaseDevice(deviceId);
      setSuccess(`Dispositivo #${deviceId} liberado com sucesso!`);
      fetchDevices();
    } catch (err: any) {
      setError(err.message || 'Erro ao liberar dispositivo.');
    }
  };

  const formatDate = (isoString?: string | null) => {
    if (!isoString) return '—';
    try {
      const d = new Date(isoString);
      return d.toLocaleString('pt-BR');
    } catch {
      return isoString;
    }
  };

  return (
    <div className="section-container">
      <div className="section-header">
        <div>
          <h2>Dispositivos Vinculados</h2>
          <p className="text-muted">
            Monitore máquinas vinculadas a licenças e libere estações de trabalho de clientes.
          </p>
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
              id="input-filter-device-license"
            />
          </div>

          <div className="filter-item flex items-center gap-2">
            <label className="checkbox-label">
              <input
                type="checkbox"
                checked={activeOnly}
                onChange={(e) => setActiveOnly(e.target.checked)}
                id="checkbox-active-only"
              />
              <span>Apenas ativos</span>
            </label>
          </div>

          <div className="filter-actions">
            <button type="submit" className="btn btn-secondary" id="btn-filter-devices">
              Filtrar
            </button>
            <button
              type="button"
              className="btn btn-light"
              onClick={() => {
                setLicenseIdFilter('');
                setActiveOnly(false);
              }}
            >
              Limpar
            </button>
          </div>
        </form>
      </div>

      {/* Devices Table */}
      <div className="data-card">
        {loading ? (
          <div className="loading-state">Carregando dispositivos...</div>
        ) : devices.length === 0 ? (
          <div className="empty-state">
            <p>Nenhum dispositivo encontrado para os filtros selecionados.</p>
          </div>
        ) : (
          <div className="table-responsive">
            <table className="data-table" id="table-devices">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>Licença</th>
                  <th>Fingerprint Hardware</th>
                  <th>Status</th>
                  <th>Vinculado em</th>
                  <th>Último Heartbeat</th>
                  <th>Ações</th>
                </tr>
              </thead>
              <tbody>
                {devices.map((dev) => (
                  <tr key={dev.id} id={`device-row-${dev.id}`}>
                    <td className="font-bold">#{dev.id}</td>
                    <td>
                      <button
                        type="button"
                        className="btn-link"
                        onClick={() => setSelectedLicenseId(dev.license_id)}
                      >
                        Licença #{dev.license_id}
                      </button>
                    </td>
                    <td>
                      <code className="text-xs" title={dev.fingerprint_hash}>
                        {dev.fingerprint_hash.substring(0, 16)}...
                      </code>
                    </td>
                    <td>
                      {dev.active ? (
                        <span className="status-badge status-ativa">Ativo</span>
                      ) : (
                        <span className="status-badge status-revogada">Liberado</span>
                      )}
                    </td>
                    <td>{formatDate(dev.bound_at)}</td>
                    <td>{formatDate(dev.last_heartbeat_at)}</td>
                    <td>
                      {dev.active ? (
                        <button
                          type="button"
                          className="btn btn-danger btn-xs"
                          onClick={() => handleRelease(dev.id, dev.license_id)}
                          id={`btn-release-device-${dev.id}`}
                        >
                          Liberar Dispositivo
                        </button>
                      ) : (
                        <span className="text-muted text-xs">Já liberado</span>
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Modal Licença */}
      {selectedLicenseId !== null && (
        <LicenseDetailModal
          licenseId={selectedLicenseId}
          onClose={() => setSelectedLicenseId(null)}
          onUpdated={() => fetchDevices()}
        />
      )}
    </div>
  );
};
