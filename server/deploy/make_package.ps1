# Empacota o servidor para enviar a VM (sem segredos, sem banco, sem venv, sem node_modules).
# Uso (PowerShell, na raiz do repo):  powershell -File server\deploy\make_package.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)   # raiz do repo
$server = Join-Path $root 'server'
$out = Join-Path $server 'deploy\bundle'
New-Item -ItemType Directory -Force -Path $out | Out-Null

if (-not (Test-Path (Join-Path $server 'admin-ui\dist\index.html'))) {
    throw 'Painel nao compilado. Rode: cd server\admin-ui ; npm ci ; npm run build'
}

# area de montagem dentro do proprio bundle/ (evita o caminho curto 8.3 do TEMP)
$stage = Join-Path $out 'stage'
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    # codigo + painel compilado + deploy; exclui segredos, venv, bancos, node_modules, testes e fontes do painel
    $xd = @('.venv', '__pycache__', '.pytest_cache', 'node_modules', 'test-results', 'secrets', 'bundle', 'tests', 'src', 'playwright-report')
    $xf = @('.env', '*.db', '*.key', '*.pyc', 'release.key')
    & robocopy $server $stage /E /XD $xd /XF $xf /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy falhou ($LASTEXITCODE)" }
    $tar = Join-Path $out 'structura-server.tar.gz'
    if (Test-Path -LiteralPath $tar) { Remove-Item -LiteralPath $tar -Force }
    # bsdtar do Windows (o tar do Git Bash interpreta "C:" como host remoto)
    & "$env:SystemRoot\System32\tar.exe" -czf $tar -C $stage .
    if ($LASTEXITCODE -ne 0) { throw 'tar falhou' }
    Write-Host "OK: $tar"
} finally {
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue }
}
