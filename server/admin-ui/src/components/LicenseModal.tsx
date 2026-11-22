import React, { useState } from 'react';
import { api } from '../api';
import { License } from '../types';

interface LicenseModalProps {
  isOpen: boolean;
  onClose: () => void;
  onSuccess: (license: License) => void;
}

export const LicenseModal: React.FC<LicenseModalProps> = ({ isOpen, onClose, onSuccess }) => {
  const [email, setEmail] = useState('');
  const [userId, setUserId] = useState('');
  const [plan, setPlan] = useState('mensal');
  const [days, setDays] = useState(30);
  const [status, setStatus] = useState<'ativa' | 'suspensa'>('ativa');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  if (!isOpen) return null;

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null);

    if (!email && !userId) {
      setError('Informe o e-mail ou o ID do usuário.');
      return;
    }

    setLoading(true);
    try {
      const created = await api.createLicense({
        email: email.trim() || undefined,
        user_id: userId ? parseInt(userId, 10) : undefined,
        plan,
        days: Number(days),
        status,
      });
      onSuccess(created);
      onClose();
    } catch (err: any) {
      let msg = err.message || 'Falha ao criar licença.';
      if (msg === 'USER_NOT_FOUND') {
        msg = 'Usuário não encontrado. Verifique o e-mail ou ID.';
      }
      setError(msg);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="modal-overlay">
      <div className="modal-content">
        <div className="modal-header">
          <h3 className="modal-title">Emitir Nova Licença</h3>
          <button className="alert-close" onClick={onClose}>&times;</button>
        </div>

        {error && (
          <div style={{ padding: '1rem 1.5rem 0' }}>
            <div className="alert alert-error">
              <div>{error}</div>
              <button className="alert-close" onClick={() => setError(null)}>&times;</button>
            </div>
          </div>
        )}

        <form onSubmit={handleSubmit}>
          <div className="modal-body">
            <div className="form-group">
              <label className="form-label">E-mail do Usuário</label>
              <input
                type="email"
                className="form-control"
                placeholder="cliente@exemplo.com"
                value={email}
                onChange={(e) => setEmail(e.target.value)}
              />
              <div className="form-helper">O usuário já deve estar cadastrado no sistema</div>
            </div>

            <div className="form-group">
              <label className="form-label">Ou ID Numérico do Usuário</label>
              <input
                type="number"
                className="form-control"
                placeholder="Ex: 42"
                value={userId}
                onChange={(e) => setUserId(e.target.value)}
              />
            </div>

            <div className="form-group">
              <label className="form-label">Plano</label>
              <select
                className="form-control"
                value={plan}
                onChange={(e) => setPlan(e.target.value)}
              >
                <option value="mensal">Mensal</option>
                <option value="trimestral">Trimestral</option>
                <option value="semestral">Semestral</option>
                <option value="anual">Anual</option>
                <option value="vitalicia">Vitalícia / Perpétua</option>
                <option value="demo">Demonstração (Trial)</option>
              </select>
            </div>

            <div className="form-group">
              <label className="form-label">Validade em Dias</label>
              <input
                type="number"
                min={1}
                max={3650}
                className="form-control"
                value={days}
                onChange={(e) => setDays(parseInt(e.target.value, 10) || 30)}
                required
              />
              <div className="form-helper">Calcula a expiração a partir do momento da emissão</div>
            </div>

            <div className="form-group">
              <label className="form-label">Status Inicial</label>
              <select
                className="form-control"
                value={status}
                onChange={(e) => setStatus(e.target.value as any)}
              >
                <option value="ativa">Ativa</option>
                <option value="suspensa">Suspensa</option>
              </select>
            </div>
          </div>

          <div className="modal-footer">
            <button type="button" className="btn btn-secondary" onClick={onClose} disabled={loading}>
              Cancelar
            </button>
            <button type="submit" className="btn btn-primary" disabled={loading}>
              {loading ? 'Emitindo...' : 'Emitir Licença'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};
