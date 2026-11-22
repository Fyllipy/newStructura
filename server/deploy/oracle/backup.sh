#!/usr/bin/env bash
# Backup diario consistente do SQLite (usa .backup, seguro com o servidor rodando). Mantem 14 dias.
set -euo pipefail
DB=/opt/structura/data/structura.db
DEST=/var/backups/structura
[ -f "$DB" ] || exit 0
STAMP=$(date +%Y%m%d-%H%M%S)
sqlite3 "$DB" ".backup '$DEST/structura-$STAMP.db'"
gzip -f "$DEST/structura-$STAMP.db"
chmod 600 "$DEST"/structura-*.db.gz
find "$DEST" -name 'structura-*.db.gz' -mtime +14 -delete
