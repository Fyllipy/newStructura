import React, { useState, useEffect } from 'react';
import { AdminSession } from './types';
import { getSession, clearSession, setOnUnauthorized, api } from './api';
import { Login } from './components/Login';
import { Header } from './components/Header';
import { Dashboard } from './components/Dashboard';
import { Licenses } from './components/Licenses';
import { Devices } from './components/Devices';
import { AbuseQueue } from './components/AbuseQueue';
import { VMAllowlist } from './components/VMAllowlist';
import { Users } from './components/Users';
import { Telemetry } from './components/Telemetry';
import { Plans } from './components/Plans';

export const App: React.FC = () => {
  const [session, setSession] = useState<AdminSession | null>(() => getSession());
  const [activeTab, setActiveTab] = useState<string>('dashboard');
  const [openAbuseCount, setOpenAbuseCount] = useState<number>(0);
  const [globalError, setGlobalError] = useState<string | null>(null);

  useEffect(() => {
    setOnUnauthorized(() => {
      setSession(null);
      setGlobalError('Sessão expirada ou não autorizada. Faça login novamente com suas credenciais de administrador.');
    });
  }, []);

  useEffect(() => {
    if (session) {
      // Carregar contagem de abusos abertos para o badge do menu
      api.getMetrics()
        .then((m) => {
          setOpenAbuseCount(m.abuse.total_open);
        })
        .catch(() => {
          // Silencioso se der erro na métrica inicial
        });
    }
  }, [session, activeTab]);

  const handleLogout = () => {
    clearSession();
    setSession(null);
    setGlobalError(null);
  };

  const handleLoginSuccess = (newSession: AdminSession) => {
    setSession(newSession);
    setActiveTab('dashboard');
    setGlobalError(null);
  };

  if (!session) {
    return (
      <div className="login-screen-wrapper">
        {globalError && (
          <div style={{ maxWidth: '420px', margin: '2rem auto -1rem', padding: '0 1rem' }}>
            <div className="alert alert-error">{globalError}</div>
          </div>
        )}
        <Login onSuccess={handleLoginSuccess} />
      </div>
    );
  }

  return (
    <div className="app-container">
      <Header
        session={session}
        activeTab={activeTab}
        onTabChange={(tab) => {
          setActiveTab(tab);
          setGlobalError(null);
        }}
        onLogout={handleLogout}
        openAbuseCount={openAbuseCount}
      />

      <main className="main-content">
        {globalError && (
          <div className="alert alert-error mb-4">
            {globalError}
            <button
              style={{ float: 'right', background: 'none', border: 'none', cursor: 'pointer', fontWeight: 'bold' }}
              onClick={() => setGlobalError(null)}
            >
              &times;
            </button>
          </div>
        )}

        {activeTab === 'dashboard' && <Dashboard onNavigate={setActiveTab} />}
        {activeTab === 'licenses' && <Licenses />}
        {activeTab === 'devices' && <Devices />}
        {activeTab === 'abuse' && <AbuseQueue />}
        {activeTab === 'vm_allowlist' && <VMAllowlist />}
        {activeTab === 'users' && <Users />}
        {activeTab === 'plans' && <Plans />}
        {activeTab === 'telemetry' && <Telemetry />}
      </main>

      <footer style={{
        textAlign: 'center',
        padding: '1.5rem',
        fontSize: '0.8rem',
        color: 'var(--text-light)',
        borderTop: '1px solid var(--border-color)',
        marginTop: 'auto'
      }}>
        Structura Cloud Licensing & Security Platform &bull; Painel Administrativo com Autenticação Forte 2FA &bull; Conexão Criptografada
      </footer>
    </div>
  );
};
