export interface AdminSession {
  admin_token: string;
  expires_at: string;
  user_id: number;
  email: string;
}

export interface License {
  id: number;
  user_id: number;
  user_email?: string | null;
  status: 'ativa' | 'suspensa' | 'revogada' | 'expirada';
  current_period_end: string;
  plan: string;
  created_at: string;
  devices_count: number;
}

export interface Device {
  id: number;
  license_id: number;
  fingerprint_hash: string;
  active: boolean;
  bound_at: string;
  last_heartbeat_at: string;
  released_at?: string | null;
}

export interface AbuseFlag {
  id: number;
  license_id: number;
  kind: string;
  score: number;
  details: string;
  status: 'aberto' | 'revisado' | 'resolvido' | 'ignorado';
  ts: string;
}

export interface VMAllowlistEntry {
  id: number;
  license_id: number;
  fingerprint_hash: string;
  notes: string;
  active: boolean;
  created_at?: string;
}

export interface User {
  id: number;
  email: string;
  status: string;
  is_admin: boolean;
  totp_enabled: boolean;
  created_at: string;
  licenses_count: number;
  must_change_password?: boolean;
  cpf_masked: string;
}

export interface CreatedUser {
  user_id: number;
  license_id: number;
  email: string;
  temporary_password: string;
  must_change_password: boolean;
  twofa: string;
}

export interface TelemetryEvent {
  id: number;
  ts: string;
  event: string;
  user_id: number | null;
  license_id: number | null;
  email: string;
  ip: string;
  fingerprint: string;
  app_version: string;
  os_version: string;
  hostname: string;
  cpu: string;
  locale: string;
  timezone: string;
  is_vm: boolean;
  vm_vendor: string;
  user_agent: string;
  detail: string;
}

export interface FraudSignal {
  kind: string;
  subject: string;
  count: number;
  detail: string;
}

export interface RecentLogin {
  id: number;
  actor: string;
  action: string;
  target: string;
  ts: string;
}

export interface Metrics {
  licenses: {
    total: number;
    active: number;
    suspended: number;
    revoked: number;
    expired: number;
  };
  devices: {
    total: number;
    active: number;
  };
  abuse: {
    total: number;
    total_open: number;
    open_7d: number;
  };
  recent_logins: RecentLogin[];
}

export interface Plan {
  code: string;
  name: string;
  period_days: number;
  price_cents: number;
  active: boolean;
  updated_at?: string;
}
