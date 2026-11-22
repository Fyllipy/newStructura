import React, { useState } from 'react';
import { api } from '../api';

interface VMAllowlistModalProps {
  onClose: () => void;
  onSuccess: () => void;
}

export const VMAllowlistModal: React.FC<VMAllowlistModalProps> = ({ onClose, onSuccess }) => {
  const [licenseId, setLicenseId] = useState<string>('');
  const [fingerprint, setFingerprint] = useState<string>('');
  const [notes, setNotes] = useState<string>('');
  const [loading, setLoading] = useState<boolean>(false);
  const [error, setError] = useState<string | null>(null);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    const licIdNum = parseInt(licenseId, 10);
    if (isNaN(licIdNum) || licIdNum <= 0) {
      setError('Informe um ID de licença válido.');
      return;
    }
    if (!fingerprint.trim()) {
      setError('O fingerprint de hardware da máquina virtual é obrigatório.');
      return;
    }

    try {
      setLoading(true);
      setError(null);
      await api.addVMAllowlist({
        license_id: licIdNum,
        fingerprint: fingerprint.trim(),
        notes: notes.trim() || 'Autorizado pelo administrador',
      });
      onSuccess();
    } catch (err: any) {
      setError(err.message || 'Falha ao autorizar máquina virtual.');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal-card" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <h3 className="modal-title">Autorizar Máquina Virtual (Allowlist)</h3>
          <button className="btn-close" onClick={onClose} aria-label="Fechar">
            &times;
          </button>
        </div>

        <form onSubmit={handleSubmit}>
          <div className="modal-body">
            {error && <div className="alert alert-error mb-4">{error}</div>}

            <p className="text-muted text-sm mb-4">
              Por padrão de segurança (M6), ambientes virtualizados são bloqueados. Cadastre o fingerprint da VM autorizada para permitir a execução da licença.
            </p>

            <div className="form-group mb-3">
              <label htmlFor="vm-license-id" className="form-label">
                ID da Licença *
              </label>
              <input
                id="vm-license-id"
                type="number"
                min="1"
                className="form-control"
                placeholder="Ex: 1"
                value={licenseId}
                onChange={(e) => setLicenseId(e.target.value)}
                required
                disabled={loading}
              />
            </div>

            <div className="form-group mb-3">
              <label htmlFor="vm-fingerprint" className="form-label">
                Fingerprint de Hardware (SHA-256) *
              </label>
              <input
                id="vm-fingerprint"
                type="text"
                className="form-control font-mono"
                placeholder="Cole o hash de 64 caracteres do cliente..."
                value={fingerprint}
                onChange={(e) => setFingerprint(e.target.value)}
                required
                disabled={loading}
              />
            </div>

            <div className="form-group mb-3">
              <label htmlFor="vm-notes" className="form-label">
                Justificativa / Notas
              </label>
              <textarea
                id="vm-notes"
                className="form-control"
                rows={2}
                placeholder="Ex: Servidor de CI/CD aprovado conforme contrato corp-982."
                value={notes}
                onChange={(e) => setNotes(e.target.value)}
                disabled={loading}
              />
            </div>
          </div>

          <div className="modal-footer">
            <button type="button" className="btn btn-secondary" onClick={onClose} disabled={loading}>
              Cancelar
            </button>
            <button type="submit" className="btn btn-primary" disabled={loading} id="btn-save-vm-allowlist">
              {loading ? 'Autorizando...' : 'Autorizar VM'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};
