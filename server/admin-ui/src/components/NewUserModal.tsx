import React, { useState } from 'react';
import { api } from '../api';
import { CreatedUser } from '../types';

interface Props {
  onClose: () => void;
  onCreated: () => void;
}

const PLAN_DAYS: Record<string, number> = {
  mensal: 30, trimestral: 90, semestral: 180, anual: 365, vitalicia: 3650, demo: 14,
};

export const NewUserModal: React.FC<Props> = ({ onClose, onCreated }) => {
  const [email, setEmail] = useState('');
  const [cpf, setCpf] = useState('');
  const [plan, setPlan] = useState('mensal');
  const [days, setDays] = useState(30);
  const [customPassword, setCustomPassword] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [created, setCreated] = useState<CreatedUser | null>(null);
  const [copied, setCopied] = useState(false);

  const submit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null);
    const digits = cpf.replace(/\D/g, '');
    if (digits.length !== 11) {
      setError('CPF deve conter 11 dígitos.');
      return;
    }
    setLoading(true);
    try {
      const res = await api.createUser({
        email: email.trim(),
        cpf: digits,
        plan,
        days,
        password: customPassword.trim() || undefined,
      });
      setCreated(res);
      onCreated();
    } catch (err: any) {
      const map: Record<string, string> = {
        EMAIL_ALREADY_EXISTS: 'Já existe uma conta com este e-mail.',
        CPF_INVALIDO: 'CPF inválido (dígitos verificadores não conferem).',
        EMAIL_INVALIDO: 'E-mail inválido.',
        SENHA_FORA_DA_POLITICA: 'Senha fora da política (mínimo de 7 caracteres).',
      };
      setError(map[err.message] || err.message || 'Falha ao criar usuário.');
    } finally {
      setLoading(false);
    }
  };

  const copy = async () => {
    if (!created) return;
    try {
      await navigator.clipboard.writeText(created.temporary_password);
      setCopied(true);
    } catch {
      /* sem permissao de area de transferencia: o admin copia manualmente */
    }
  };

  if (created) {
    return (
      <div className="modal-overlay">
        <div className="modal-content">
          <div className="modal-header">
            <h3 className="modal-title">Usuário criado</h3>
            <button className="alert-close" onClick={onClose}>&times;</button>
          </div>
          <div className="modal-body">
            <div className="alert alert-success mb-4">
              Conta <strong>{created.email}</strong> criada com a licença #{created.license_id}.
            </div>
            <div className="form-group">
              <label className="form-label">Senha temporária (exibida somente agora)</label>
              <input className="form-control font-mono" readOnly value={created.temporary_password} id="temp-password" onFocus={(e) => e.target.select()} />
              <div className="form-helper">
                Envie ao usuário por um canal seguro. No primeiro acesso ele será obrigado a trocá-la.
              </div>
            </div>
            <div className="alert alert-error">
              <strong>Como o usuário entra:</strong>
              <ol style={{ margin: '0.4rem 0 0 1.2rem' }}>
                <li>Abre o Structura e informa e-mail e a senha temporária.</li>
                <li>O assistente mostra o QR Code para configurar o autenticador (2FA).</li>
                <li>Define a senha definitiva. O CPF já está guardado na conta.</li>
              </ol>
            </div>
          </div>
          <div className="modal-footer">
            <button type="button" className="btn btn-secondary" onClick={copy}>{copied ? 'Copiada!' : 'Copiar senha'}</button>
            <button type="button" className="btn btn-primary" onClick={onClose}>Concluir</button>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="modal-overlay">
      <div className="modal-content">
        <div className="modal-header">
          <h3 className="modal-title">Novo usuário + licença</h3>
          <button className="alert-close" onClick={onClose}>&times;</button>
        </div>
        <form onSubmit={submit}>
          {error && <div className="alert alert-error" style={{ margin: '0 1.5rem' }}>{error}</div>}
          <div className="modal-body">
            <div className="form-group">
              <label className="form-label" htmlFor="nu-email">E-mail</label>
              <input id="nu-email" type="email" className="form-control" value={email} onChange={(e) => setEmail(e.target.value)} required />
            </div>
            <div className="form-group">
              <label className="form-label" htmlFor="nu-cpf">CPF</label>
              <input id="nu-cpf" className="form-control" placeholder="000.000.000-00" value={cpf} onChange={(e) => setCpf(e.target.value)} required />
              <div className="form-helper">Fica guardado (cifrado) na conta; não é usado no login.</div>
            </div>
            <div className="form-group">
              <label className="form-label" htmlFor="nu-plan">Tipo de licença</label>
              <select id="nu-plan" className="form-control" value={plan}
                onChange={(e) => { setPlan(e.target.value); setDays(PLAN_DAYS[e.target.value] ?? 30); }}>
                <option value="mensal">Mensal</option>
                <option value="trimestral">Trimestral</option>
                <option value="semestral">Semestral</option>
                <option value="anual">Anual</option>
                <option value="vitalicia">Vitalícia / Perpétua</option>
                <option value="demo">Demonstração (Trial)</option>
              </select>
            </div>
            <div className="form-group">
              <label className="form-label" htmlFor="nu-days">Validade em dias</label>
              <input id="nu-days" type="number" min={1} max={3650} className="form-control" value={days}
                onChange={(e) => setDays(parseInt(e.target.value, 10) || 1)} required />
            </div>
            <div className="form-group">
              <label className="form-label" htmlFor="nu-pass">Senha inicial (opcional)</label>
              <input id="nu-pass" type="text" className="form-control font-mono" placeholder="Deixe vazio para gerar automaticamente"
                value={customPassword} onChange={(e) => setCustomPassword(e.target.value)} autoComplete="off" />
              <div className="form-helper">O usuário precisará trocá-la no primeiro acesso. O 2FA é configurado por ele mesmo (QR Code).</div>
            </div>
          </div>
          <div className="modal-footer">
            <button type="button" className="btn btn-secondary" onClick={onClose} disabled={loading}>Cancelar</button>
            <button type="submit" className="btn btn-primary" disabled={loading}>{loading ? 'Criando...' : 'Criar usuário'}</button>
          </div>
        </form>
      </div>
    </div>
  );
};
