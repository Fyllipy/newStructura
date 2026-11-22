#!/usr/bin/env bash
# Publica no servidor de producao o manifesto de integridade + DLL_RELEASE_KEY gerados pelo build.
# Pre-requisito: ~/upload/manifest.txt, manifest.sig, dll_key.txt (server/deploy/bundle/ da maquina de build).
# Uso na VM:   sudo bash /opt/structura/server/deploy/oracle/publish_release.sh
set -euo pipefail
[ "$(id -u)" -eq 0 ] || { echo "execute com sudo"; exit 1; }
UPLOAD="${UPLOAD_DIR:-/home/${SUDO_USER:-ubuntu}/upload}"
for f in manifest.txt manifest.sig dll_key.txt; do
  [ -f "$UPLOAD/$f" ] || { echo "falta $UPLOAD/$f"; exit 1; }
done

# O usuario de servico (structura) nao consegue ler a pasta pessoal do usuario SSH (modo 700):
# copia para uma area temporaria so dele e apaga ao terminar (mesmo em caso de erro).
STAGE=$(mktemp -d /opt/structura/release-XXXXXX)
trap 'rm -rf "$STAGE"' EXIT
cp "$UPLOAD/manifest.txt" "$UPLOAD/manifest.sig" "$UPLOAD/dll_key.txt" "$STAGE/"
chown -R structura:structura "$STAGE"
chmod 700 "$STAGE"
KEY=$(tr -d '[:space:]' < "$STAGE/dll_key.txt")

cd /opt/structura/server
sudo -u structura /opt/structura/venv/bin/python -m app.manifest_cli publish \
  --text "$STAGE/manifest.txt" --sig "$STAGE/manifest.sig" --dll-key "$KEY" --status current --overwrite
sudo -u structura /opt/structura/venv/bin/python -m app.manifest_cli list
rm -f "$UPLOAD/dll_key.txt"
echo "Manifesto publicado. Chave temporaria removida."
