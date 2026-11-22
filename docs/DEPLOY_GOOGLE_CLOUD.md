# Deploy no Google Cloud (e2-micro "Always Free")

Complementa `DEPLOY_SERVIDOR.md` (que descreve o que é comum a qualquer host). Aqui estão os passos específicos do Google Cloud.
Os mesmos scripts (`server/deploy/oracle/setup.sh` etc.) funcionam; o nome da pasta `oracle` é histórico.

## Limites do plano gratuito (documentação oficial do Google, conferida em nov/2026)
- **1 VM `e2-micro` por mês**, somente nas regiões **us-west1 (Oregon), us-central1 (Iowa) ou us-east1 (Carolina do Sul)**.
  O limite é por **tempo**: o mês inteiro de uma VM ligada 24 h cabe; uma segunda VM já gera cobrança.
- **30 GB** de disco **padrão** (*Standard persistent disk*). Disco "balanceado"/SSD **não** é gratuito.
- **1 GB/mês** de saída de rede da América do Norte (excedente é cobrado). O tráfego do licenciamento é minúsculo
  (JSON); cuidado só com downloads grandes (não baixe backups enormes pela rede do Google com frequência).
- 1 GB de RAM: o `setup.sh` cria 2 GB de swap sozinho.
- **Não verificado nas páginas oficiais:** o custo do **endereço IPv4 externo** (o Google cobra por IPs externos em uso desde 2024;
  confira em *VPC network → IP addresses* e na página de preços). Configure o **alerta de orçamento** (passo 3) para não ter surpresa.
- Latência: ~120–180 ms do Brasil para us-east1. Irrelevante para licenciamento.
- Regiões e limites mudam: confirme em `cloud.google.com/free` antes de criar a VM.

## 1. Conta e projeto
1. Entre em `console.cloud.google.com` com uma conta Google. Aceite os termos e **ative o faturamento** (cartão de crédito; escolha
   tipo de conta **Individual / Pessoa física**). O cartão é exigido mesmo no nível gratuito. O crédito de teste (US$ 300/90 dias)
   é opcional: ele **não** é necessário para o Always Free, e se o teste terminar sem upgrade os recursos pagos são suspensos.
2. Crie o projeto: menu superior → *New project* → nome `structura-licencas`.

## 2. Ativar a API
*APIs & Services → Library → Compute Engine API → Enable*.

## 3. Alerta de orçamento (faça antes de criar a VM)
*Billing → Budgets & alerts → Create budget*: valor **R$ 10** (ou US$ 3), alertas em 50 %, 90 % e 100 %, com e-mail. Isso avisa se algo
passar do gratuito (IP, disco errado, tráfego).

## 4. Criar a VM
*Compute Engine → VM instances → Create instance*:
- Nome `structura-server`; região **us-east1** (a mais próxima do Brasil) ou us-central1/us-west1; qualquer zona.
- Configuração: série **E2**, tipo **e2-micro** (2 vCPU compartilhadas, 1 GB). Modelo de provisionamento **Standard** (não Spot).
- *Boot disk → Change*: **Ubuntu 24.04 LTS** (x86/64), tipo **Standard persistent disk**, tamanho **30 GB**.
- *Firewall*: marque **Allow HTTP traffic** e **Allow HTTPS traffic** (libera 80/443).
- Confira o resumo de custo mostrado: a e2-micro nas regiões elegíveis aparece coberta pelo plano gratuito.
- *Create*. Anote o **External IP** (ex.: `34.23.10.5`).

## 5. IP fixo (recomendado) e domínio
O IP efêmero **muda** se a VM for parada e religada, e o domínio `sslip.io` depende do IP. Para fixar: *VPC network → IP addresses*, linha da VM →
**Promote to static** (nome `structura-ip`). Confira o custo do IP na tela. Alternativa melhor a longo prazo: um **domínio próprio** (registro **A** apontando
para o IP): trocar de host depois vira só mudar o DNS (`DEPLOY_SERVIDOR.md` §C).
Seu domínio gratuito: IP com hífens + `.sslip.io` → `34-23-10-5.sslip.io`.

## 6. Enviar os arquivos (sem instalar nada)
Na máquina de build, gere o pacote: `powershell -File server\deploy\make_package.ps1` (painel compilado antes).
Na lista de VMs, clique **SSH** (abre um terminal no navegador). No terminal:
```bash
mkdir -p ~/upload
```
Clique na **engrenagem → Upload file** e envie, um a um, para a pasta pessoal: `structura-server.tar.gz`, `prod.env` (de
`server\deploy\secrets\`) e `setup.sh` (de `server\deploy\oracle\`). Depois:
```bash
mv ~/structura-server.tar.gz ~/prod.env ~/setup.sh ~/upload/
```
(Alternativa com a CLI `gcloud`: `gcloud compute scp --zone <zona> arquivos... structura-server:~/upload/`.)

## 7. Instalar
```bash
cd ~/upload
sudo bash setup.sh 34-23-10-5.sslip.io          # seu domínio
curl https://34-23-10-5.sslip.io/health          # {"status":"ok","db":"ok"}
shred -u ~/upload/prod.env
```
Se o `curl` de fora não responder: confira que a VM tem as marcas *http-server*/*https-server* (passo 4) e `sudo systemctl status structura caddy`.

## 8. Daqui em diante
Criar admin, gravar `server\deploy\production_url.txt`, `build_release.bat`, enviar `manifest.txt`, `manifest.sig`, `dll_key.txt` para
`~/upload/` (mesmo método de upload) e rodar `sudo bash /opt/structura/server/deploy/oracle/publish_release.sh`:
é o §B4 do `DEPLOY_SERVIDOR.md`. Backup, atualização e migração: §C e §D do mesmo documento.

## 9. Custos e armadilhas
- Mais de uma VM, disco SSD/balanceado, região fora das três, GPU, ou saída de rede acima de 1 GB/mês geram cobrança.
- Não use "Spot"/"preemptible" (a VM é derrubada).
- Se o Google encerrar o teste gratuito sem upgrade, confirme que a VM e2-micro continua ativa (o Always Free independe do crédito de teste).
- Revise *Billing → Reports* na primeira semana.
