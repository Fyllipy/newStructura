import React from 'react';
import { AdminSession } from '../types';

interface HeaderProps {
  session: AdminSession;
  activeTab: string;
  onTabChange: (tab: string) => void;
  onLogout: () => void;
  openAbuseCount?: number;
}

export const Header: React.FC<HeaderProps> = ({
  session,
  activeTab,
  onTabChange,
  onLogout,
  openAbuseCount = 0,
}) => {
  return (
    <header className="app-header">
      <div className="header-inner">
        <a href="#dashboard" onClick={(e) => { e.preventDefault(); onTabChange('dashboard'); }} className="logo-area">
          <svg style={{ width: 28, height: 28 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5" />
          </svg>
          <span>Structura</span>
          <span className="logo-badge">Admin</span>
        </a>

        <div className="user-area">
          <span className="user-email" title={`User ID: ${session.user_id}`}>
            {session.email}
          </span>
          <button className="btn-logout" onClick={onLogout} title="Encerrar sessão de administrador">
            Sair
          </button>
        </div>
      </div>

      <nav className="nav-tabs" aria-label="Navegação do painel">
        <button
          className={`nav-tab ${activeTab === 'dashboard' ? 'active' : ''}`}
          onClick={() => onTabChange('dashboard')}
        >
          Visão Geral
        </button>

        <button
          className={`nav-tab ${activeTab === 'licenses' ? 'active' : ''}`}
          onClick={() => onTabChange('licenses')}
        >
          Licenças
        </button>

        <button
          className={`nav-tab ${activeTab === 'devices' ? 'active' : ''}`}
          onClick={() => onTabChange('devices')}
        >
          Dispositivos
        </button>

        <button
          className={`nav-tab ${activeTab === 'abuse' ? 'active' : ''}`}
          onClick={() => onTabChange('abuse')}
        >
          Fila de Abuso
          {openAbuseCount > 0 && (
            <span className="tab-badge highlight">{openAbuseCount}</span>
          )}
        </button>

        <button
          className={`nav-tab ${activeTab === 'vm_allowlist' ? 'active' : ''}`}
          onClick={() => onTabChange('vm_allowlist')}
        >
          Allowlist de VM
        </button>

        <button
          className={`nav-tab ${activeTab === 'users' ? 'active' : ''}`}
          onClick={() => onTabChange('users')}
        >
          Usuários
        </button>

        <button
          className={`nav-tab ${activeTab === 'telemetry' ? 'active' : ''}`}
          onClick={() => onTabChange('telemetry')}
        >
          Telemetria
        </button>
      </nav>
    </header>
  );
};
