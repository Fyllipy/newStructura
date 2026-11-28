# Site, loja, área do cliente e cobrança automática

O site fica em outro repositório (`SiteStructura`, HTML/CSS/JS estático, sem build). O back-end é o servidor de licenciamento (`server/`), que ganhou
a loja, a área do cliente, a renovação automática e a aba **Planos e preços** do painel admin.

## Como funciona
| Parte | Onde |
|---|---|
| Planos/preços (admin) | tabela `plans`, aba **Planos e preços** em `/admin`; mensal (30 d) e trimestral (90 d) nascem **inativos e sem preço** |
| Loja | `GET /shop/plans`, `POST /shop/checkout`, `POST /shop/complete` (`server/app/portal.py`) |
| Área do cliente | `POST /portal/auth/login` (e-mail + senha + 2FA, **não vincula dispositivo**), `GET /portal/me`, `/portal/card/*`, `/portal/subscription/cancel`, `/portal/2fa/*` |
| Pagamento | Stripe: Payment Element (iframe do Stripe). O número do cartão **nunca** chega ao servidor; guardamos só ids, bandeira, final e validade |
| Renovação | `python -m app.renew_cli` de hora em hora (systemd timer). Só cobra licença **já vencida**, com renovação ligada e cartão |
| Webhook | `POST /webhook/payment` (assinatura verificada): conclui compras cujo navegador fechou antes de voltar |

Regras: compra de cliente novo só cria a conta **depois** do pagamento confirmado no Stripe; cancelar apaga o cartão (no Stripe e local) e desliga a renovação,
e a licença vale até o fim do período pago; cobrança recusada é repetida a cada 24 h, no máximo 3 vezes, e trocar o cartão zera as tentativas; a cobrança usa
chave de idempotência (licença + vencimento + tentativa + cartão), então uma rodada repetida nunca cobra duas vezes o mesmo vencimento.

## Publicar (resumo)
1. **Stripe**: crie a conta, use chaves restritas (`rk_`) ou secreta; em *Settings > Payment methods* deixe só **Cartões**. Crie o endpoint de webhook
   `https://<seu-dominio>/webhook/payment` com o evento `payment_intent.succeeded` e copie o `whsec_…`.
2. **`.env` do servidor** (`/opt/structura/server/.env`): `STRIPE_SECRET_KEY`, `STRIPE_PUBLISHABLE_KEY`, `STRIPE_WEBHOOK_SECRET`. Reinicie o serviço.
3. `pip install -r requirements.txt` no venv do servidor (nova dependência: `stripe`). As tabelas/colunas novas são criadas sozinhas na subida.
4. **Admin > Planos e preços**: defina o valor de mensal e trimestral e ative (mínimo R$ 0,50).
5. **Renovação**: copie `server/deploy/oracle/structura-renew.{service,timer}` para `/etc/systemd/system/` e rode
   `sudo systemctl daemon-reload && sudo systemctl enable --now structura-renew.timer`. Teste: `sudo -u structura /opt/structura/venv/bin/python -m app.renew_cli`
   (imprime `{"charged":…,"failed":…,"skipped":…,"unknown":…}`).
6. **Site**: copie o conteúdo de `SiteStructura` para `/opt/structura/site` (exceto `.git`) e use o novo `Caddyfile` (`server/deploy/oracle/Caddyfile`): API nos
   caminhos `/auth /license /compute /admin /portal /shop /webhook /health /pubkey`, site estático no resto, com CSP liberando o Stripe. `sudo systemctl reload caddy`.
7. Teste primeiro com chaves `sk_test_`/`pk_test_` e o cartão `4242 4242 4242 4242`; só depois troque para as chaves reais.

## Desenvolvimento local
`SITE_DIR=<pasta do site>` no ambiente faz o próprio `uvicorn` servir o site em `/` (mesma origem, sem CORS). Testes: `pytest tests/test_m15_billing.py` (provedor de pagamento falso, sem rede).

## Pendências / decisões suas
- Revisar os textos de **Termos** e **Privacidade** (são minutas) e preencher razão social/CNPJ/contato.
- Não há e-mail transacional: o cliente não recebe aviso de cobrança recusada ou de renovação (a área do cliente mostra o alerta). Vale integrar um serviço de e-mail.
- Nota fiscal não é emitida automaticamente.
- Cartões que exigem autenticação (3DS) numa cobrança sem o cliente presente são recusados; o cliente precisa trocar o cartão na área do cliente.
- `DEMO_URL` em `js/config.js` esconde/mostra o botão de baixar a Demo.
- Não testado com o Stripe real (sem chaves nesta máquina): a integração foi testada com provedor falso e o fluxo do navegador só até a tela de pagamento.
