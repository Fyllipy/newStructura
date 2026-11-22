# Painel de administração — manual

Endereço: `https://<seu-dominio>/admin/` (produção) ou `http://127.0.0.1:8000/admin/` (servidor local).
Código: `server/admin-ui/` (React 19 + TypeScript + Vite). É servido pelo próprio servidor FastAPI a partir de
`server/admin-ui/dist/`.

## 1. Entrar
E-mail + senha + código 2FA (TOTP) de **administrador**. O CPF **não** é pedido. A sessão dura 30 minutos
(`ADMIN_TOKEN_TTL_SECONDS`), fica só na aba do navegador (`sessionStorage`) e é invalidada se a senha do admin for trocada.
Cinco tentativas erradas bloqueiam a conta por 15 minutos (a mesma conta serve ao app e ao painel).
Se ficar bloqueado: aguarde, ou no servidor `python -m app.unlock_user <email>`.

O primeiro administrador é criado **no servidor** (não pelo painel):
```bash
python -m app.create_admin --email admin@exemplo.com --cpf 00000000000
```
Pede a senha (oculta, mín. 7 caracteres) e mostra **uma vez** o segredo TOTP para o autenticador.

## 2. Abas
| Aba | Para quê |
|---|---|
| **Visão Geral** | Métricas de licenças, dispositivos, abusos abertos e últimos acessos |
| **Licenças** | Listar/filtrar, emitir licença para uma conta **existente**, suspender/reativar, estender validade |
| **Dispositivos** | Máquinas vinculadas; **liberar** o vínculo quando o usuário troca de computador |
| **Fila de Abuso** | Sinalizações automáticas (acesso concorrente, VM, muitos fingerprints...); revisar/ignorar. 5 sinalizações em 7 dias suspendem a licença |
| **Allowlist de VM** | Autorizar máquinas virtuais para clientes corporativos |
| **Usuários** | Contas, CPF, 2FA, criação, redefinição de senha, reset de 2FA, destravar |
| **Telemetria** | Acessos com IP/máquina/versão e sinais de fraude |

## 3. Criar um novo usuário com licença (aba Usuários → "+ Novo usuário")
Campos: **e-mail**, **CPF**, **tipo de licença** (mensal, trimestral, semestral, anual, vitalícia, demo — preenche a
validade padrão, editável) e, opcionalmente, uma **senha inicial**.

O que acontece:
1. **Senha:** se você não digitar uma, o servidor gera uma senha temporária forte (12 caracteres). Ela é exibida
   **uma única vez** numa janela com botão *Copiar senha*. Envie ao usuário por canal seguro. A conta fica marcada
   "senha temporária" e o usuário é **obrigado a trocá-la no primeiro acesso**.
2. **CPF:** validado (dígitos verificadores), guardado **cifrado** na conta e exibido na lista mascarado
   (`***.***.***-09`). *Revelar* mostra o número completo e grava a ação na auditoria.
3. **2FA:** você não gera nada. No primeiro login o próprio usuário vê o assistente com QR Code e cadastra o
   autenticador. Na lista, o 2FA aparece como "Pendente (1º acesso)" até lá.
4. **Licença:** criada junto, com o plano e a validade escolhidos. Para dar **mais uma licença** a quem já tem conta,
   use a aba *Licenças* → emitir (informa o e-mail existente).

O que dizer ao usuário: "Instale o Structura, entre com seu e-mail e a senha temporária, escaneie o QR Code no
Google/Microsoft Authenticator, digite o código e defina sua senha definitiva."

Erros do formulário: `EMAIL_ALREADY_EXISTS` (e-mail duplicado), CPF/e-mail inválidos, senha curta.

## 4. Operações do dia a dia
| Situação | Ação |
|---|---|
| Esqueceu a senha | Usuários → **Redefinir senha** (gera nova temporária; ele a troca no próximo login) |
| Perdeu o celular/autenticador | Usuários → **Resetar 2FA** (o QR Code reaparece no próximo login). Para **administradores** só pelo servidor (`create_admin` recria o 2FA) |
| Conta bloqueada por tentativas | Usuários → **Destravar** |
| Trocou de computador | Dispositivos → **liberar** o vínculo antigo (ou esperar 7 dias sem uso) |
| Cliente não pagou / fraude | Licenças → suspender; revisar Telemetria |
| Cliente corporativo em VM | Allowlist de VM → adicionar o fingerprint |
| Renovar | Licenças → estender (`add_days`) |

## 5. Telemetria e fraude
Filtros: e-mail, IP exato, licença, evento (`login`, `login_failed`, `heartbeat`) e período (24 h a 1 ano). Cada linha mostra
data, evento, e-mail, IP, licença, computador (passe o mouse: CPU, idioma, fuso, fingerprint), Windows, versão do app, VM e
detalhe (`bad_password`, `unknown_user`, `locked`...). O quadro vermelho no topo lista **sinais** nas últimas 24 h
(contas demais num IP, IPs demais numa licença, várias máquinas, rajada de falhas) com botão *Filtrar*.
Investigue antes de agir: IP compartilhado (empresa/universidade) gera falso positivo.
Dados são apagados após 180 dias. Informe seus usuários sobre a coleta (o app já exibe o aviso na tela de login).

## 6. Segurança do painel (resumo)
Token assinado de 30 min vinculado à senha; sem cookies (sem CSRF); CSP restritiva, `X-Frame-Options: DENY`,
`no-store`; servidor de arquivos estáticos protegido contra *path traversal*; todas as rotas `/admin/*` exigem token de
administrador; ações sensíveis (revelar CPF, resetar senha/2FA, criar usuário) vão para a trilha de auditoria imutável.

## 7. Desenvolvimento do painel
```powershell
cd server\admin-ui
npm ci
npm run dev        # http://localhost:5173/admin/  (proxy para o servidor em :8000)
npm run build      # gera dist/ — OBRIGATÓRIO antes de empacotar o servidor (make_package.ps1)
npm run test:e2e   # Playwright; exige E2E_ADMIN_* e um servidor de TESTE (suspende/reativa licenças)
```
Estrutura: `src/api.ts` (chamadas + sessão), `src/types.ts`, `src/components/*` (uma tela por arquivo; `NewUserModal.tsx`
é o formulário de criação). Ao adicionar uma tela: criar o componente, importar em `App.tsx` e adicionar a aba em
`components/Header.tsx`. A API do servidor correspondente fica em `server/app/main.py` (seção `/admin/...`) com teste em `server/tests/`.
