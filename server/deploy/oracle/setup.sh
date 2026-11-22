#!/usr/bin/env bash
# Instala o servidor de licenciamento Structura numa VM Ubuntu (Oracle Cloud Always Free, ARM ou x86).
# Executar NA VM, como root:   sudo bash setup.sh <DOMINIO>
#   ex.: sudo bash setup.sh 129-146-10-20.sslip.io
# Pre-requisitos (enviados por scp para ~/upload/):
#   structura-server.tar.gz  (make_package.ps1)    prod.env  (secrets/prod.env)
set -euo pipefail

DOMAIN="${1:-}"
[ -n "$DOMAIN" ] || { echo "uso: sudo bash setup.sh <dominio>   (ex.: 129-146-10-20.sslip.io)"; exit 1; }
[ "$(id -u)" -eq 0 ] || { echo "execute com sudo"; exit 1; }

UPLOAD="${UPLOAD_DIR:-/home/${SUDO_USER:-ubuntu}/upload}"
APP=/opt/structura
[ -f "$UPLOAD/structura-server.tar.gz" ] || { echo "falta $UPLOAD/structura-server.tar.gz"; exit 1; }
[ -f "$UPLOAD/prod.env" ] || { echo "falta $UPLOAD/prod.env"; exit 1; }

echo "== [0/7] Swap (VMs com pouca RAM, ex.: Google Cloud e2-micro = 1 GB) =="
MEM_MB=$(awk '/MemTotal/ {print int($2/1024)}' /proc/meminfo)
if [ "$MEM_MB" -lt 1800 ] && ! swapon --show | grep -q .; then
  fallocate -l 2G /swapfile && chmod 600 /swapfile && mkswap /swapfile && swapon /swapfile
  grep -q '^/swapfile' /etc/fstab || echo '/swapfile none swap sw 0 0' >> /etc/fstab
  echo "swap de 2 GB criado"
fi

echo "== [1/7] Pacotes do sistema =="
export DEBIAN_FRONTEND=noninteractive
apt-get update -y
apt-get install -y python3 python3-venv python3-pip sqlite3 ufw curl debian-keyring debian-archive-keyring apt-transport-https gnupg unattended-upgrades fail2ban

echo "== [2/7] Caddy (HTTPS automatico via Let's Encrypt) =="
if ! command -v caddy >/dev/null 2>&1; then
  curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/gpg.key' | gpg --dearmor -o /usr/share/keyrings/caddy-stable-archive-keyring.gpg
  curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/debian.deb.txt' > /etc/apt/sources.list.d/caddy-stable.list
  apt-get update -y
  apt-get install -y caddy
fi

echo "== [3/7] Usuario e diretorios =="
id structura >/dev/null 2>&1 || useradd --system --home "$APP" --shell /usr/sbin/nologin structura
mkdir -p "$APP/server" "$APP/data" /var/backups/structura
tar -xzf "$UPLOAD/structura-server.tar.gz" -C "$APP/server"
install -m 600 -o structura -g structura "$UPLOAD/prod.env" "$APP/server/.env"
chown -R structura:structura "$APP"
chmod 700 "$APP/data"

echo "== [4/7] Ambiente Python =="
sudo -u structura python3 -m venv "$APP/venv"
sudo -u structura "$APP/venv/bin/pip" install --upgrade pip
sudo -u structura "$APP/venv/bin/pip" install -r "$APP/server/requirements.txt"

echo "== [5/7] Servico systemd =="
install -m 644 "$APP/server/deploy/oracle/structura.service" /etc/systemd/system/structura.service
systemctl daemon-reload
systemctl enable structura
systemctl restart structura

echo "== [6/7] Caddy (reverse proxy + HTTPS) =="
sed "s/__DOMAIN__/$DOMAIN/g" "$APP/server/deploy/oracle/Caddyfile" > /etc/caddy/Caddyfile
systemctl enable caddy
systemctl restart caddy

echo "== [7/7] Firewall, backup e hardening =="
# Imagens Oracle trazem regras iptables restritivas: libera 80/443 antes do REJECT final.
if iptables -S INPUT 2>/dev/null | grep -q REJECT; then
  iptables -C INPUT -p tcp --dport 80  -j ACCEPT 2>/dev/null || iptables -I INPUT 1 -p tcp --dport 80  -j ACCEPT
  iptables -C INPUT -p tcp --dport 443 -j ACCEPT 2>/dev/null || iptables -I INPUT 1 -p tcp --dport 443 -j ACCEPT
  command -v netfilter-persistent >/dev/null 2>&1 && netfilter-persistent save || true
fi
ufw allow OpenSSH || true
ufw allow 80/tcp || true
ufw allow 443/tcp || true
ufw --force enable || true
install -m 755 "$APP/server/deploy/oracle/backup.sh" /usr/local/bin/structura-backup
echo "17 3 * * * root /usr/local/bin/structura-backup" > /etc/cron.d/structura-backup
systemctl enable --now fail2ban

sleep 3
echo
echo "== Verificacao =="
curl -fsS "http://127.0.0.1:8000/health" && echo
echo "Servidor local OK. Teste externo (apos abrir 80/443 na VCN):  curl https://$DOMAIN/health"
echo "Proximos passos: ver DEPLOY.md (criar admin, publicar manifesto)."
