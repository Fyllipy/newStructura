import React, { useState } from 'react';
import { AbuseFlag } from '../types';
import { api } from '../api';

interface AbuseReviewModalProps {
  flag: AbuseFlag;
  onClose: () => void;
  onSuccess: () => void;
}

export const AbuseReviewModal: React.FC<AbuseReviewModalProps> = ({ flag, onClose, onSuccess }) => {
  const [status, setStatus] = useState<string>(flag.status);
  const [notes, setNotes] = useState<string>('');
  const [loading, setLoading] = useState<boolean>(false);
  const [error, setError] = useState<string | null>(null);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    try {
      setLoading(true);
      setError(null);
      await api.reviewAbuseFlag(flag.id, {
        status,
        notes: notes.trim() || undefined,
      });
      onSuccess();
    } catch (err: any) {
      setError(err.message || 'Falha ao revisar ocorrência de abuso.');
    } finally {
      setLoading(false);
    }
  };

  const formatDate = (isoString: string) => {
    try {
      return new Date(isoString).toLocaleString('pt-BR');
    } catch {
      return isoString;
    }
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal-card" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <h3 className="modal-title">Revisar Flag de Abuso #{flag.id}</h3>
          <button className="btn-close" onClick={onClose} aria-label="Fechar">
            &times;
          </button>
        </div>

        <form onSubmit={handleSubmit}>
          <div className="modal-body">
            {error && <div className="alert alert-error mb-4">{error}</div>}

            <div className="detail-grid mb-4">
              <div className="detail-item">
                <span className="detail-label">Licença Alvo</span>
                <span className="detail-value font-bold">Licença #{flag.license_id}</span>
              </div>
              <div className="detail-item">
                <span className="detail-label">Tipo de Ocorrência</span>
                <span className="detail-value font-mono">{flag.kind}</span>
              </div>
              <div className="detail-item">
                <span className="detail-label">Score de Risco</span>
                <span className="detail-value font-bold text-danger">{flag.score}</span>
              </div>
              <div className="detail-item">
                <span className="detail-label">Registrado em</span>
                <span className="detail-value">{formatDate(flag.ts)}</span>
              </div>
            </div>

            <div className="form-group mb-3">
              <label className="form-label">Detalhes Técnicos da Ocorrência</label>
              <div className="detail-box font-mono text-xs p-3 bg-gray-50 border rounded">
                {flag.details || 'Sem detalhes técnicos adicionais.'}
              </div>
            </div>

            <div className="form-group mb-3">
              <label htmlFor="review-status" className="form-label">
                Novo Status
              </label>
              <select
                id="review-status"
                className="form-control"
                value={status}
                onChange={(e) => setStatus(e.target.value)}
                disabled={loading}
              >
                <option value="aberto">Aberto (Pendente)</option>
                <option value="revisado">Revisado (Em Análise)</option>
                <option value="resolvido">Resolvido (Tratado / Bloqueado)</option>
                <option value="ignorado">Ignorado (Falso Positivo)</option>
              </select>
            </div>

            <div className="form-group mb-3">
              <label htmlFor="review-notes" className="form-label">
                Nota da Revisão do Administrador
              </label>
              <textarea
                id="review-notes"
                className="form-control"
                rows={3}
                placeholder="Ex: Contatado usuário por telefone; máquina virtual permitida para testes pontuais."
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
            <button type="submit" className="btn btn-primary" disabled={loading} id="btn-save-abuse-review">
              {loading ? 'Salvando...' : 'Salvar Revisão'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};
