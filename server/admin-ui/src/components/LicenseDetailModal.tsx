import React, { useState, useEffect } from 'react';
import { License, Device } from '../types';
import { api } from '../api';

interface LicenseDetailModalProps {
  licenseId: number;
  onClose: () => void;
  onUpdated: () => void;
}

export const LicenseDetailModal: React.FC<LicenseDetailModalProps> = ({ licenseId, onClose, onUpdated }) => {
  const [license, setLicense] = useState<License | null>(null);
  const [devices, setDevices] = useState<Device[]>([]);
  const [loading, setLoading] = useState<boolean>(true);
  const [error, setError] = useState<string | null>(null);
  const [actionError, setActionError] = useState<string | null>(null);
  const [actionSuccess, setActionSuccess] = useState<string | null>(null);
  const [actionLoading, setActionLoading] = useState<boolean>(false);
  const [reason, setReason] = useState<string>('');
  const [customDays, setCustomDays] = useState<number>(30);

  const loadData = async () => {
    try {
      setLoading(true);
      setError(null);
      const [lic, devs] = await Promise.all([
        api.getLicense(licenseId),
        api.getLicenseDevices(licenseId),
      ]);
      setLicense(lic);
      setDevices(devs);
    } catch (err: any) {
      setError(err.message || 'Erro ao carregar detalhes da licença.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadData();
  }, [licenseId]);

  const handleStatusChange = async (newStatus: 'ativa' | 'suspensa' | 'revogada') => {
    try {
      setActionLoading(true);
      setActionError(null);
      setActionSuccess(null);
      await api.updateLicense(licenseId, {
        status: newStatus,
        reason: reason || `Alteração manual de status para ${newStatus}`,
      });
      setActionSuccess(`Status alterado para "${newStatus}" com sucesso!`);
      setReason('');
      await loadData();
      onUpdated();
    } catch (err: any) {
      setActionError(err.message || 'Falha ao atualizar status da licença.');
    } finally {
      setActionLoading(false);
    }
  };

  const handleAddDays = async (days: number) => {
    try {
      setActionLoading(true);
      setActionError(null);
      setActionSuccess(null);
      await api.updateLicense(licenseId, {
        add_days: days,
        reason: reason || `Extensão de validade por +${days} dias`,
      });
      setActionSuccess(`Validade estendida por +${days} dias com sucesso!`);
      setReason('');
      await loadData();
      onUpdated();
    } catch (err: any) {
      setActionError(err.message || 'Falha ao estender validade.');
    } finally {
      setActionLoading(false);
    }
  };

  const handleReleaseDevice = async (deviceId: number) => {
    if (!window.confirm('Tem certeza que deseja liberar este dispositivo? O usuário precisará vincular novamente.')) {
      return;
    }
    try {
      setActionLoading(true);
      setActionError(null);
      setActionSuccess(null);
      await api.releaseDevice(deviceId);
      setActionSuccess(`Dispositivo #${deviceId} liberado com sucesso!`);
      await loadData();
      onUpdated();
    } catch (err: any) {
      setActionError(err.message || 'Falha ao liberar dispositivo.');
    } finally {
      setActionLoading(false);
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
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal-card modal-large" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <h3 className="modal-title">Detalhes da Licença #{licenseId}</h3>
          <button className="btn-close" onClick={onClose} aria-label="Fechar">
            &times;
          </button>
        </div>

        <div className="modal-body">
          {loading ? (
            <div className="loading-state">Carregando dados da licença...</div>
          ) : error ? (
            <div className="alert alert-error">{error}</div>
          ) : license ? (
            <>
              {actionSuccess && <div className="alert alert-success">{actionSuccess}</div>}
              {actionError && <div className="alert alert-error">{actionError}</div>}

              {/* Informações Gerais */}
              <div className="detail-section">
                <h4>Informações da Licença</h4>
                <div className="detail-grid">
                  <div className="detail-item">
                    <span className="detail-label">ID da Licença</span>
                    <span className="detail-value">#{license.id}</span>
                  </div>
                  <div className="detail-item">
                    <span className="detail-label">Status</span>
                    <span className={`status-badge status-${license.status}`}>
                      {license.status.toUpperCase()}
                    </span>
                  </div>
                  <div className="detail-item">
                    <span className="detail-label">Usuário</span>
                    <span className="detail-value font-mono">
                      {license.user_email || `ID ${license.user_id}`}
                    </span>
                  </div>
                  <div className="detail-item">
                    <span className="detail-label">Plano</span>
                    <span className="detail-value uppercase">{license.plan}</span>
                  </div>
                  <div className="detail-item">
                    <span className="detail-label">Validade (Fim do Período)</span>
                    <span className="detail-value font-bold">
                      {formatDate(license.current_period_end)}
                    </span>
                  </div>
                  <div className="detail-item">
                    <span className="detail-label">Criada em</span>
                    <span className="detail-value">{formatDate(license.created_at)}</span>
                  </div>
                </div>
              </div>

              {/* Ações de Status e Validade */}
              <div className="detail-section">
                <h4>Gerenciar Status e Validade</h4>
                
                <div className="form-group mb-3">
                  <label htmlFor="audit-reason" className="form-label">
                    Motivo / Justificativa (Registrado na Auditoria)
                  </label>
                  <input
                    id="audit-reason"
                    type="text"
                    className="form-control"
                    placeholder="Ex: Solicitação via ticket de suporte #1024"
                    value={reason}
                    onChange={(e) => setReason(e.target.value)}
                    disabled={actionLoading}
                  />
                </div>

                <div className="action-buttons-group">
                  <div className="action-subgroup">
                    <span className="text-muted text-sm block mb-1">Mudar Status:</span>
                    <div className="btn-row">
                      {license.status !== 'ativa' && (
                        <button
                          type="button"
                          className="btn btn-success btn-sm"
                          onClick={() => handleStatusChange('ativa')}
                          disabled={actionLoading}
                        >
                          Reativar
                        </button>
                      )}
                      {license.status !== 'suspensa' && (
                        <button
                          type="button"
                          className="btn btn-warning btn-sm"
                          onClick={() => handleStatusChange('suspensa')}
                          disabled={actionLoading}
                        >
                          Suspender
                        </button>
                      )}
                      {license.status !== 'revogada' && (
                        <button
                          type="button"
                          className="btn btn-danger btn-sm"
                          onClick={() => handleStatusChange('revogada')}
                          disabled={actionLoading}
                        >
                          Revogar
                        </button>
                      )}
                    </div>
                  </div>

                  <div className="action-subgroup">
                    <span className="text-muted text-sm block mb-1">Estender Validade:</span>
                    <div className="btn-row">
                      <button
                        type="button"
                        className="btn btn-secondary btn-sm"
                        onClick={() => handleAddDays(30)}
                        disabled={actionLoading}
                      >
                        +30 Dias
                      </button>
                      <button
                        type="button"
                        className="btn btn-secondary btn-sm"
                        onClick={() => handleAddDays(90)}
                        disabled={actionLoading}
                      >
                        +90 Dias
                      </button>
                      <button
                        type="button"
                        className="btn btn-secondary btn-sm"
                        onClick={() => handleAddDays(365)}
                        disabled={actionLoading}
                      >
                        +1 Ano
                      </button>
                    </div>
                  </div>
                </div>

                <div className="custom-extend-row mt-2">
                  <div className="input-group">
                    <input
                      type="number"
                      min="1"
                      className="form-control form-control-sm"
                      value={customDays}
                      onChange={(e) => setCustomDays(parseInt(e.target.value) || 1)}
                      style={{ width: '100px' }}
                      disabled={actionLoading}
                    />
                    <button
                      type="button"
                      className="btn btn-secondary btn-sm"
                      onClick={() => handleAddDays(customDays)}
                      disabled={actionLoading}
                    >
                      Adicionar Dias Customizados
                    </button>
                  </div>
                </div>
              </div>

              {/* Dispositivos Vinculados */}
              <div className="detail-section">
                <h4>Dispositivos Vinculados ({devices.filter(d => d.active).length} ativo / {devices.length} total)</h4>
                {devices.length === 0 ? (
                  <p className="text-muted text-sm">Nenhum dispositivo registrado para esta licença.</p>
                ) : (
                  <div className="table-responsive">
                    <table className="data-table">
                      <thead>
                        <tr>
                          <th>ID</th>
                          <th>Fingerprint Hash</th>
                          <th>Status</th>
                          <th>Vinculado em</th>
                          <th>Último Heartbeat</th>
                          <th>Ação</th>
                        </tr>
                      </thead>
                      <tbody>
                        {devices.map((dev) => (
                          <tr key={dev.id}>
                            <td>#{dev.id}</td>
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
                                  onClick={() => handleReleaseDevice(dev.id)}
                                  disabled={actionLoading}
                                >
                                  Liberar
                                </button>
                              ) : (
                                <span className="text-muted text-xs">Liberado</span>
                              )}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                )}
              </div>
            </>
          ) : null}
        </div>

        <div className="modal-footer">
          <button type="button" className="btn btn-secondary" onClick={onClose}>
            Fechar
          </button>
        </div>
      </div>
    </div>
  );
};
