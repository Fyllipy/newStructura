#!/usr/bin/env bash
# Publica no servidor de producao os manifestos de UM release com varias edicoes (Comercial e Demo), sem tocar nas versoes anteriores.
# Cada edicao tem manifesto proprio (versao N para a Comercial, N+1 para a Demo): o servidor compara a raiz de arquivos e o hash do exe.
#
# Pre-requisito: ~/upload/comercial/ e ~/upload/demo/, cada uma com manifest.txt, manifest.sig e dll_key.txt
#   (da maquina de build: server/deploy/bundle/ e server/deploy/bundle/demo/).
# Uso na VM:   sudo bash /opt/structura/server/deploy/oracle/publish_releases.sh
# Nao usa --overwrite: se a versao ja existir com outro conteudo o publish FALHA (versao publicada e imutavel), o que protege as instalacoes antigas.
set -euo pipefail
[ "$(id -u)" -eq 0 ] || { echo "execute com sudo"; exit 1; }
UPLOAD="${UPLOAD_DIR:-/home/${SUDO_USER:-ubuntu}/upload}"
for ed in comercial demo; do
  for f in manifest.txt manifest.sig dll_key.txt; do
    [ -f "$UPLOAD/$ed/$f" ] || { echo "falta $UPLOAD/$ed/$f"; exit 1; }
  done
done

STAGE=$(mktemp -d /opt/structura/release-XXXXXX)
trap 'rm -rf "$STAGE"' EXIT
chown structura:structura "$STAGE"; chmod 700 "$STAGE"

cd /opt/structura/server
echo "== versoes ANTES =="
sudo -u structura /opt/structura/venv/bin/python -m app.manifest_cli list
for ed in comercial demo; do
  mkdir -p "$STAGE/$ed"
  cp "$UPLOAD/$ed/manifest.txt" "$UPLOAD/$ed/manifest.sig" "$UPLOAD/$ed/dll_key.txt" "$STAGE/$ed/"
  chown -R structura:structura "$STAGE/$ed"; chmod 700 "$STAGE/$ed"
  KEY=$(tr -d '[:space:]' < "$STAGE/$ed/dll_key.txt")
  echo "== publicando $ed =="
  sudo -u structura /opt/structura/venv/bin/python -m app.manifest_cli publish \
    --text "$STAGE/$ed/manifest.txt" --sig "$STAGE/$ed/manifest.sig" --dll-key "$KEY" --status current
done
echo "== versoes DEPOIS =="
sudo -u structura /opt/structura/venv/bin/python -m app.manifest_cli list
rm -f "$UPLOAD/comercial/dll_key.txt" "$UPLOAD/demo/dll_key.txt"
echo "Manifestos publicados. Chaves temporarias removidas."
