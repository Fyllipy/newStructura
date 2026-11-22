# Structura - Painel Administrativo Web

Interface web gráfica administrativa para gerenciamento de licenças, dispositivos vinculados, fila de telemetria/abuso e allowlist de máquinas virtuais.

## 🛠️ Tecnologias Utilizadas

- **Frontend**: React 19 + TypeScript + Vite 6
- **Testes E2E**: Playwright (utilizando o canal `msedge` nativo no Windows)
- **Autenticação**: Zero segredos ou bypass no front-end. Toda requisição utiliza bearer token JWT administrativo (`admin_token`) gerado após autenticação com 4 fatores:
  1. E-mail cadastrado
  2. Senha (Argon2id)
  3. CPF validado
  4. Código TOTP 2FA (RFC 6238)

---

## 🚀 Como Executar em Desenvolvimento

### 1. Iniciar o servidor Backend (FastAPI)
```bash
cd server
.venv\Scripts\python.exe -m uvicorn app.main:app --reload --port 8000
```

### 2. Iniciar o servidor de desenvolvimento do Front-end (Vite)
```bash
cd server/admin-ui
npm run dev
```
O Vite iniciará o servidor com Hot Module Replacement em `http://localhost:5173/admin/` e encaminhará todas as requisições `/admin/...` via proxy para o FastAPI na porta 8000.

---

## 📦 Como Compilar para Produção

O FastAPI está configurado para servir diretamente a SPA estática embutida no endpoint `/admin`:

```bash
cd server/admin-ui
npm run build
```
Os arquivos gerados são gravados em `server/admin-ui/dist/`. Ao acessar `http://localhost:8000/admin`, o FastAPI entrega a aplicação automaticamente.

---

## 🧪 Como Executar os Testes

### Testes End-to-End (E2E) com Playwright
Testa o fluxo completo: login com 2FA dinâmico, visualização do dashboard/métricas, listagem de licenças e suspensão/reativação de licença:
```bash
cd server/admin-ui
npm run test:e2e
```

### Testes Backend (Pytest)
```bash
cd server
.venv\Scripts\python.exe -m pytest tests/test_m7_admin.py
```
