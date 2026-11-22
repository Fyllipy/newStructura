import React, { useState, useEffect } from 'react';
import { User } from '../types';
import { api } from '../api';
import { NewUserModal } from './NewUserModal';

const fmt = (iso: string) => {
  try {
    return new Date(/[zZ]|[+-]\d\d:\d\d$/.test(iso) ? iso : iso + 'Z').toLocaleString('pt-BR');
  } catch {
    return iso;
  }
};

export const Users: React.FC = () => {
  const [users, setUsers] = useState<User[]>([]);
  const [search, setSearch] = useState('');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);
  // CPFs revelados nesta sessao de tela (cada revelacao e registrada na auditoria do servidor)
  const [showNew, setShowNew] = useState(false);
  const [tempPass, setTempPass] = useState<{ email: string; password: string } | null>(null);
  const [revealed, setRevealed] = useState<Record<number, string>>({});

  const load = async (q?: string) => {
    try {
      setLoading(true);
      setError(null);
      setUsers(await api.getUsers(q || undefined));
    } catch (err: any) {
      setError(err.message || 'Erro ao carregar usuários.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    load();
  }, []);

  const reveal = async (u: User) => {
    try {
      setError(null);
      const r = await api.revealCpf(u.id);
      setRevealed((prev) => ({ ...prev, [u.id]: r.cpf }));
    } catch (err: any) {
      setError(err.message || 'Não foi possível revelar o CPF.');
    }
  };

  const resetPassword = async (u: User) => {
    if (!window.confirm(`Gerar nova senha temporária para ${u.email}? A senha atual deixa de valer.`)) return;
    try {
      setError(null);
      setSuccess(null);
      const r = await api.resetPassword(u.id);
      setTempPass({ email: u.email, password: r.temporary_password });
      load(search);
    } catch (err: any) {
      setError(err.message || 'Falha ao redefinir a senha.');
    }
  };

  const reset2fa = async (u: User) => {
    if (!window.confirm(`Resetar o 2FA de ${u.email}? Ele deverá configurar o autenticador de novo no próximo acesso.`)) return;
    try {
      setError(null);
      setSuccess(null);
      await api.reset2fa(u.id);
      setSuccess(`2FA de ${u.email} resetado: o assistente (QR Code) aparecerá no próximo login.`);
      load(search);
    } catch (err: any) {
      setError(err.message === 'CANNOT_RESET_ADMIN_2FA' ? 'O 2FA de administradores só pode ser redefinido pelo servidor.' : (err.message || 'Falha ao resetar o 2FA.'));
    }
  };

  const unlock = async (u: User) => {
    try {
      setError(null);
      setSuccess(null);
      await api.unlockUser(u.id);
      setSuccess(`Conta ${u.email} destravada.`);
    } catch (err: any) {
      setError(err.message || 'Falha ao destravar a conta.');
    }
  };

  return (
    <div className="section-container">
      <div className="section-header">
        <div>
          <h2>Usuários</h2>
          <p className="text-muted">Contas cadastradas. O CPF fica guardado na conta; revelar o número completo é registrado na auditoria.</p>
        </div>
        <div className="section-header-actions">
          <button type="button" className="btn btn-primary" id="btn-new-user" onClick={() => setShowNew(true)}>
            + Novo usuário
          </button>
        </div>
      </div>

      {tempPass && (
        <div className="alert alert-success mb-4">
          Nova senha temporária de <strong>{tempPass.email}</strong>: <code className="font-mono">{tempPass.password}</code>
          {' '}(exibida só agora; o usuário deverá trocá-la no próximo login).
          <button style={{ float: 'right', background: 'none', border: 'none', cursor: 'pointer', fontWeight: 'bold' }} onClick={() => setTempPass(null)}>&times;</button>
        </div>
      )}

      {success && <div className="alert alert-success mb-4">{success}</div>}
      {error && <div className="alert alert-error mb-4">{error}</div>}

      <div className="filters-card">
        <form onSubmit={(e) => { e.preventDefault(); load(search); }} className="filters-form">
          <div className="filter-item">
            <input
              type="text"
              className="form-control"
              placeholder="Buscar por e-mail..."
              value={search}
              onChange={(e) => setSearch(e.target.value)}
              id="input-filter-users"
            />
          </div>
          <div className="filter-actions">
            <button type="submit" className="btn btn-secondary">Buscar</button>
            <button type="button" className="btn btn-light" onClick={() => { setSearch(''); load(); }}>Limpar</button>
          </div>
        </form>
      </div>

      <div className="data-card">
        {loading ? (
          <div className="loading-state">Carregando usuários...</div>
        ) : users.length === 0 ? (
          <div className="empty-state"><p>Nenhum usuário encontrado.</p></div>
        ) : (
          <div className="table-responsive">
            <table className="data-table" id="table-users">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>E-mail</th>
                  <th>CPF</th>
                  <th>Status</th>
                  <th>2FA</th>
                  <th>Licenças</th>
                  <th>Criado em</th>
                  <th>Ações</th>
                </tr>
              </thead>
              <tbody>
                {users.map((u) => (
                  <tr key={u.id}>
                    <td className="font-bold">#{u.id}</td>
                    <td>
                      {u.email}
                      {u.is_admin && <span className="status-badge status-ativa" style={{ marginLeft: 6 }}>admin</span>}
                    </td>
                    <td>
                      <code className="text-xs font-mono">{revealed[u.id] ?? (u.cpf_masked || '—')}</code>
                      {revealed[u.id] === undefined && u.cpf_masked && (
                        <button type="button" className="btn btn-light" style={{ marginLeft: 8 }} onClick={() => reveal(u)}>
                          Revelar
                        </button>
                      )}
                    </td>
                    <td><span className={`status-badge status-${u.status}`}>{u.status}</span></td>
                    <td>{u.totp_enabled ? 'Ativo' : 'Pendente (1º acesso)'}{u.must_change_password ? ' · senha temporária' : ''}</td>
                    <td>{u.licenses_count}</td>
                    <td><span className="text-xs text-muted">{fmt(u.created_at)}</span></td>
                    <td>
                      <button type="button" className="btn btn-light" onClick={() => unlock(u)} title="Remove bloqueio por tentativas falhas">
                        Destravar
                      </button>{' '}
                      <button type="button" className="btn btn-light" onClick={() => resetPassword(u)} title="Gera nova senha temporária">
                        Redefinir senha
                      </button>{' '}
                      {!u.is_admin && (
                        <button type="button" className="btn btn-light" onClick={() => reset2fa(u)} title="Usuário perdeu o autenticador">
                          Resetar 2FA
                        </button>
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {showNew && (
        <NewUserModal
          onClose={() => setShowNew(false)}
          onCreated={() => load(search)}
        />
      )}
    </div>
  );
};
