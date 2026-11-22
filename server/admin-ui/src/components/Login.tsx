import React, { useState } from 'react';
import { api } from '../api';
import { AdminSession } from '../types';

interface LoginProps {
  onSuccess: (session: AdminSession) => void;
}

export const Login: React.FC<LoginProps> = ({ onSuccess }) => {
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [totp, setTotp] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null);

    // Validações locais
    const cleanTotp = totp.replace(/\D/g, '');
    if (cleanTotp.length !== 6) {
      setError('Código 2FA deve conter 6 dígitos.');
      return;
    }

    setLoading(true);
    try {
      const session = await api.login({
        email: email.trim(),
        password,
        totp: cleanTotp,
      });
      onSuccess(session);
    } catch (err: any) {
      let msg = err.message || 'Falha ao autenticar.';
      if (msg === 'ADMIN_ACCESS_REQUIRED') {
        msg = 'Acesso restrito: esta conta não possui permissões de administrador.';
      } else if (msg === 'CREDENCIAIS') {
        msg = 'Credenciais ou código 2FA inválidos.';
      } else if (msg === 'TOTP_REQUIRED') {
        msg = 'Código 2FA obrigatório.';
      } else if (msg === 'ACCOUNT_LOCKED') {
        msg = 'Conta temporariamente bloqueada por excesso de tentativas.';
      }
      setError(msg);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="login-wrap">
      <div className="login-card">
        <div className="login-header">
          <div style={{ display: 'inline-flex', alignItems: 'center', gap: '0.5rem', marginBottom: '0.75rem' }}>
            <span style={{ fontSize: '1.75rem', fontWeight: 800, color: '#2563eb' }}>Structura</span>
            <span className="logo-badge">Admin</span>
          </div>
          <h1 className="login-title">Painel de Gerenciamento</h1>
          <p className="login-sub">Autenticação com credenciais administrativas e 2FA</p>
        </div>

        {error && (
          <div className="alert alert-error" role="alert">
            <div>{error}</div>
            <button className="alert-close" onClick={() => setError(null)}>&times;</button>
          </div>
        )}

        <form onSubmit={handleSubmit}>
          <div className="form-group">
            <label className="form-label" htmlFor="login-email">E-mail</label>
            <input
              id="login-email"
              type="email"
              className="form-control"
              placeholder="admin@dominio.com"
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              required
              autoFocus
            />
          </div>

          <div className="form-group">
            <label className="form-label" htmlFor="login-password">Senha</label>
            <input
              id="login-password"
              type="password"
              className="form-control"
              placeholder="••••••••"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              required
            />
          </div>

          <div className="form-group">
            <label className="form-label" htmlFor="login-totp">Código 2FA (TOTP)</label>
            <input
              id="login-totp"
              type="text"
              className="form-control totp-input"
              placeholder="000000"
              maxLength={6}
              value={totp}
              onChange={(e) => setTotp(e.target.value.replace(/\D/g, ''))}
              required
            />
            <div className="form-helper" style={{ textAlign: 'center' }}>Código de 6 dígitos do app autenticador</div>
          </div>

          <button
            type="submit"
            className="btn btn-primary"
            style={{ width: '100%', marginTop: '0.75rem', padding: '0.75rem' }}
            disabled={loading}
          >
            {loading ? 'Verificando...' : 'Entrar no Painel'}
          </button>
        </form>
      </div>
    </div>
  );
};
