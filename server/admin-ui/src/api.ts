import { AdminSession, License, Device, AbuseFlag, VMAllowlistEntry, User, Metrics, TelemetryEvent, FraudSignal, CreatedUser, Plan } from './types';

const SESSION_KEY = 'structura_admin_session';

// Sessao em sessionStorage (morre ao fechar a aba; nao persiste em disco como o localStorage) e com
// verificacao de expiracao local. O token tem TTL curto no servidor e e invalidado ao trocar a senha.
export function getSession(): AdminSession | null {
  try {
    const raw = sessionStorage.getItem(SESSION_KEY);
    if (!raw) return null;
    const s = JSON.parse(raw) as AdminSession;
    if (!s?.admin_token || (s.expires_at && Date.parse(s.expires_at) <= Date.now())) {
      sessionStorage.removeItem(SESSION_KEY);
      return null;
    }
    return s;
  } catch {
    return null;
  }
}

export function setSession(session: AdminSession): void {
  try { localStorage.removeItem(SESSION_KEY); } catch { /* sessao antiga em localStorage */ }
  sessionStorage.setItem(SESSION_KEY, JSON.stringify(session));
}

export function clearSession(): void {
  sessionStorage.removeItem(SESSION_KEY);
  try { localStorage.removeItem(SESSION_KEY); } catch { /* ignore */ }
}

let onUnauthorizedCallback: (() => void) | null = null;
export function setOnUnauthorized(cb: () => void) {
  onUnauthorizedCallback = cb;
}

async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
  const session = getSession();
  const headers: Record<string, string> = {
    'Content-Type': 'application/json',
    ...(options.headers as Record<string, string> || {}),
  };

  if (session?.admin_token) {
    headers['Authorization'] = `Bearer ${session.admin_token}`;
  }

  // Base path handling for production vs dev
  const url = path.startsWith('/admin') ? path : `/admin${path}`;

  const response = await fetch(url, {
    ...options,
    headers,
  });

  if (response.status === 401) {
    clearSession();
    if (onUnauthorizedCallback) {
      onUnauthorizedCallback();
    }
    const errData = await response.json().catch(() => ({}));
    throw new Error(errData.detail || 'Sessão expirada ou não autorizada. Faça login novamente.');
  }

  if (!response.ok) {
    const errData = await response.json().catch(() => ({}));
    const msg = errData.detail || `Erro HTTP ${response.status}: ${response.statusText}`;
    throw new Error(msg);
  }

  if (response.status === 204) {
    return null as T;
  }

  return response.json();
}

export const api = {
  async login(credentials: { email: string; password: string; totp: string }): Promise<AdminSession> {
    const res = await request<AdminSession>('/admin/auth/login', {
      method: 'POST',
      body: JSON.stringify(credentials),
    });
    setSession(res);
    return res;
  },

  async getPlans(): Promise<Plan[]> {
    return request<Plan[]>('/admin/plans');
  },

  async updatePlan(code: string, data: Partial<Pick<Plan, 'name' | 'period_days' | 'price_cents' | 'active'>>): Promise<Plan> {
    return request<Plan>(`/admin/plans/${encodeURIComponent(code)}`, { method: 'PATCH', body: JSON.stringify(data) });
  },

  async createPlan(data: { code: string; name: string; period_days: number; price_cents: number; active: boolean }): Promise<Plan> {
    return request<Plan>('/admin/plans', { method: 'POST', body: JSON.stringify(data) });
  },

  async getMetrics(): Promise<Metrics> {
    return request<Metrics>('/admin/metrics');
  },

  async getLicenses(params: { status?: string; user_id?: number; search?: string; plan?: string } = {}): Promise<License[]> {
    const query = new URLSearchParams();
    if (params.status) query.set('status', params.status);
    if (params.user_id) query.set('user_id', String(params.user_id));
    if (params.search) query.set('search', params.search);
    if (params.plan) query.set('plan', params.plan);
    const qs = query.toString();
    return request<License[]>(`/admin/licenses${qs ? `?${qs}` : ''}`);
  },

  async getLicense(id: number): Promise<License> {
    return request<License>(`/admin/licenses/${id}`);
  },

  async createLicense(data: { user_id?: number; email?: string; plan: string; days: number; status?: string }): Promise<License> {
    return request<License>('/admin/licenses', {
      method: 'POST',
      body: JSON.stringify(data),
    });
  },

  async updateLicense(id: number, data: { status?: string; add_days?: number; current_period_end?: string; reason?: string }): Promise<License> {
    return request<License>(`/admin/licenses/${id}`, {
      method: 'PATCH',
      body: JSON.stringify(data),
    });
  },

  async getLicenseDevices(licenseId: number): Promise<Device[]> {
    return request<Device[]>(`/admin/licenses/${licenseId}/devices`);
  },

  async getDevices(params: { license_id?: number; active_only?: boolean } = {}): Promise<Device[]> {
    const query = new URLSearchParams();
    if (params.license_id) query.set('license_id', String(params.license_id));
    if (params.active_only !== undefined) query.set('active_only', String(params.active_only));
    const qs = query.toString();
    return request<Device[]>(`/admin/devices${qs ? `?${qs}` : ''}`);
  },

  async releaseDevice(deviceId: number): Promise<{ ok: boolean; device_id: number; status: string }> {
    return request<{ ok: boolean; device_id: number; status: string }>(`/admin/devices/${deviceId}`, {
      method: 'DELETE',
    });
  },

  async getAbuseFlags(params: { status?: string; license_id?: number } = {}): Promise<AbuseFlag[]> {
    const query = new URLSearchParams();
    if (params.status) query.set('status', params.status);
    if (params.license_id) query.set('license_id', String(params.license_id));
    const qs = query.toString();
    return request<AbuseFlag[]>(`/admin/abuse${qs ? `?${qs}` : ''}`);
  },

  async reviewAbuseFlag(flagId: number, data: { status: string; notes?: string }): Promise<AbuseFlag> {
    return request<AbuseFlag>(`/admin/abuse/${flagId}`, {
      method: 'PATCH',
      body: JSON.stringify(data),
    });
  },

  async getVMAllowlist(params: { license_id?: number } = {}): Promise<VMAllowlistEntry[]> {
    const query = new URLSearchParams();
    if (params.license_id) query.set('license_id', String(params.license_id));
    const qs = query.toString();
    return request<VMAllowlistEntry[]>(`/admin/vm-allowlist${qs ? `?${qs}` : ''}`);
  },

  async addVMAllowlist(data: { license_id: number; fingerprint: string; notes: string }): Promise<VMAllowlistEntry> {
    return request<VMAllowlistEntry>('/admin/vm-allowlist', {
      method: 'POST',
      body: JSON.stringify(data),
    });
  },

  async removeVMAllowlist(entryId: number): Promise<{ ok: boolean; entry_id: number; status: string }> {
    return request<{ ok: boolean; entry_id: number; status: string }>(`/admin/vm-allowlist/${entryId}`, {
      method: 'DELETE',
    });
  },

  async createUser(data: { email: string; cpf: string; plan: string; days: number; password?: string }): Promise<CreatedUser> {
    return request<CreatedUser>('/admin/users', { method: 'POST', body: JSON.stringify(data) });
  },

  async resetPassword(userId: number): Promise<{ user_id: number; temporary_password: string }> {
    return request<{ user_id: number; temporary_password: string }>(`/admin/users/${userId}/reset-password`, { method: 'POST' });
  },

  async reset2fa(userId: number): Promise<{ ok: boolean; user_id: number }> {
    return request<{ ok: boolean; user_id: number }>(`/admin/users/${userId}/reset-2fa`, { method: 'POST' });
  },

  async revealCpf(userId: number): Promise<{ user_id: number; cpf: string }> {
    return request<{ user_id: number; cpf: string }>(`/admin/users/${userId}/cpf`);
  },

  async unlockUser(userId: number): Promise<{ ok: boolean; user_id: number }> {
    return request<{ ok: boolean; user_id: number }>(`/admin/users/${userId}/unlock`, { method: 'POST' });
  },

  async getTelemetry(params: { email?: string; ip?: string; license_id?: number; event?: string; hours?: number } = {}): Promise<TelemetryEvent[]> {
    const q = new URLSearchParams();
    if (params.email) q.set('email', params.email);
    if (params.ip) q.set('ip', params.ip);
    if (params.license_id) q.set('license_id', String(params.license_id));
    if (params.event) q.set('event', params.event);
    if (params.hours) q.set('hours', String(params.hours));
    const qs = q.toString();
    return request<TelemetryEvent[]>(`/admin/telemetry${qs ? `?${qs}` : ''}`);
  },

  async getFraudSignals(hours = 24): Promise<FraudSignal[]> {
    return request<FraudSignal[]>(`/admin/telemetry/signals?hours=${hours}`);
  },

  async getUsers(search?: string): Promise<User[]> {
    const query = search ? `?search=${encodeURIComponent(search)}` : '';
    return request<User[]>(`/admin/users${query}`);
  },
};
