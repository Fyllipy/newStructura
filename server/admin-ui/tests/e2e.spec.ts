import { test, expect } from '@playwright/test';
import crypto from 'crypto';

function computeTotp(secretB32: string): string {
  const base32chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567';
  let bits = '';
  for (let i = 0; i < secretB32.length; i++) {
    const val = base32chars.indexOf(secretB32.charAt(i).toUpperCase());
    if (val >= 0) bits += val.toString(2).padStart(5, '0');
  }
  const bytes: number[] = [];
  for (let i = 0; i + 8 <= bits.length; i += 8) {
    bytes.push(parseInt(bits.substr(i, 8), 2));
  }
  const key = Buffer.from(bytes);
  const epoch = Math.floor(Date.now() / 1000);
  const counter = Math.floor(epoch / 30);
  const buf = Buffer.alloc(8);
  buf.writeBigInt64BE(BigInt(counter));
  const hmac = crypto.createHmac('sha1', key).update(buf).digest();
  const offset = hmac[hmac.length - 1] & 0xf;
  const code = ((hmac.readUInt32BE(offset) & 0x7fffffff) % 1000000).toString().padStart(6, '0');
  return code;
}

const haveCreds = ['E2E_ADMIN_EMAIL', 'E2E_ADMIN_PASSWORD', 'E2E_ADMIN_TOTP_SECRET']
  .every((k) => !!process.env[k]);
test.skip(!haveCreds, 'defina E2E_ADMIN_* (servidor de teste) para rodar o e2e');

test.describe('Admin Panel End-to-End Flow', () => {
  test('login, view dashboard, list licenses, and suspend a license', async ({ page }) => {
    // 1. Acessar a tela inicial do admin
    await page.goto('/admin');

    // Verificar se o formulário de login está visível
    await expect(page.locator('#login-email')).toBeVisible();
    await expect(page.locator('#login-password')).toBeVisible();
    await expect(page.locator('#login-totp')).toBeVisible();

    // 2. Preencher credenciais do administrador
    // Credenciais SO por variavel de ambiente (nunca no codigo). Use um servidor/banco de TESTE:
    // este fluxo suspende/reativa licencas e consome codigos TOTP.
    const adminEmail = process.env.E2E_ADMIN_EMAIL!;
    const adminPass = process.env.E2E_ADMIN_PASSWORD!;
    const totpSecret = process.env.E2E_ADMIN_TOTP_SECRET!;
    const totpCode = computeTotp(totpSecret);

    await page.fill('#login-email', adminEmail);
    await page.fill('#login-password', adminPass);
    await page.fill('#login-totp', totpCode);

    // Submeter login
    await page.click('button[type="submit"]');

    // 3. Verificar que entrou no painel (Dashboard visível com métricas e cabeçalho)
    await expect(page.locator('.logo-area')).toBeVisible();
    await expect(page.locator('.user-email')).toHaveText(adminEmail);
    await expect(page.locator('text=Painel de Controle')).toBeVisible();
    await expect(page.locator('text=Licenças Ativas')).toBeVisible();

    // 4. Navegar para a aba de Licenças
    await page.click('button.nav-tab:has-text("Licenças")');
    await expect(page.locator('h2:has-text("Licenças de Uso")')).toBeVisible();
    await expect(page.locator('#table-licenses')).toBeVisible();

    // Verificar que há linhas de licença listadas
    const firstRow = page.locator('#table-licenses tbody tr').first();
    await expect(firstRow).toBeVisible();

    // 5. Suspender uma licença
    // Interceptar dialog de confirmação
    page.on('dialog', async (dialog) => {
      expect(dialog.message()).toContain('Deseja suspender a licença');
      await dialog.accept();
    });

    // Localizar botão suspender na primeira linha ativa
    const suspendBtn = page.locator('button[id^="btn-suspend-license-"]').first();
    await expect(suspendBtn).toBeVisible();
    const btnId = await suspendBtn.getAttribute('id');
    const licenseId = btnId?.replace('btn-suspend-license-', '');

    await suspendBtn.click();

    // 6. Verificar que a licença mudou para SUSPENSA e o botão agora é Reativar
    const reactivateBtn = page.locator(`#btn-reactivate-license-${licenseId}`);
    await expect(reactivateBtn).toBeVisible();
    const row = page.locator(`#license-row-${licenseId}`);
    await expect(row.locator('.status-badge')).toHaveText('SUSPENSA');

    // 7. Reativar para restaurar estado
    await reactivateBtn.click();
    await expect(page.locator(`#btn-suspend-license-${licenseId}`)).toBeVisible();
    await expect(row.locator('.status-badge')).toHaveText('ATIVA');
  });
});
