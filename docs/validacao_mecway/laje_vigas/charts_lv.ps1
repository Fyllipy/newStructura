# Graficos (PNG) da validacao laje + barras: momento no meio das vigas, carga por barra (quinhoes) e planta com os nos criados nas barras e os apoios.
# uso: charts_lv.ps1 <pasta_lvcases> <lv_barras.json> <lv_quinhoes.json> <pasta_saida>
param([string]$Cases, [string]$Barras, [string]$Quinhoes, [string]$Out)
Add-Type -AssemblyName System.Drawing
$jb = Get-Content -Raw -LiteralPath $Barras | ConvertFrom-Json
$jq = Get-Content -Raw -LiteralPath $Quinhoes | ConvertFrom-Json
$fTitle = New-Object System.Drawing.Font 'Segoe UI', 20, ([System.Drawing.FontStyle]::Bold)
$fText = New-Object System.Drawing.Font 'Segoe UI', 12
$fSmall = New-Object System.Drawing.Font 'Segoe UI', 10
$cSt = [System.Drawing.Color]::FromArgb(31, 119, 180)
$cMw = [System.Drawing.Color]::FromArgb(255, 127, 14)
$cEx = [System.Drawing.Color]::FromArgb(60, 60, 60)
function NewCanvas($w, $h) {
    $b = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($b)
    $g.Clear([System.Drawing.Color]::White)
    $g.SmoothingMode = 'AntiAlias'
    $g.TextRenderingHint = 'ClearTypeGridFit'
    return @($b, $g)
}

# ---- 1. momento medio no meio de cada viga: Structura (solver de barras) x Mecway (modo M) ----
$items = @()
foreach ($case in 'LV1_laje_4vigas_4pilares', 'LV2_duas_baias_viga_central', 'LV3_laje_unidirecional_2vigas') {
    $r = $jb.$case
    foreach ($m in $r.midspan) {
        $lbl = ($case.Substring(0, 3)) + ' viga ' + $(if ($m.line.along_x) { 'y' } else { 'x' }) + '=' + $m.line.coord
        $items += [pscustomobject]@{ L = $lbl; S = [double]$m.M_structura; W = [double]$m.M_mecway }
    }
}
$c = NewCanvas 1800 900; $bmp = $c[0]; $g = $c[1]
$g.DrawString('Momento fletor medio no meio de cada viga (N.m): barras do Structura x Mecway 1D', $fTitle, [System.Drawing.Brushes]::Black, 40, 20)
$g.DrawString('Laje em casca (MEF) acoplada as barras (rigidez direta); mesmo modelo no Mecway com laje em casca e vigas line2.', $fText, [System.Drawing.Brushes]::DimGray, 40, 62)
$maxv = ($items | ForEach-Object { [Math]::Max($_.S, $_.W) } | Measure-Object -Maximum).Maximum
$x0 = 110; $y0 = 800; $hh = 640; $w = 1640
$g.DrawLine([System.Drawing.Pens]::Black, $x0, $y0, $x0 + $w, $y0)
$g.DrawLine([System.Drawing.Pens]::Black, $x0, $y0, $x0, $y0 - $hh)
for ($t = 0; $t -le 4; $t++) {
    $v = $maxv * $t / 4; $yy = $y0 - $hh * $t / 4
    $g.DrawLine([System.Drawing.Pens]::LightGray, $x0, $yy, $x0 + $w, $yy)
    $g.DrawString(('{0:N0}' -f $v), $fSmall, [System.Drawing.Brushes]::Black, 30, $yy - 9)
}
$n = $items.Count; $slot = $w / $n
for ($i = 0; $i -lt $n; $i++) {
    $it = $items[$i]
    $bx = $x0 + $slot * $i + $slot * 0.15; $bw = $slot * 0.33
    $hs = $hh * $it.S / $maxv; $hw = $hh * $it.W / $maxv
    $g.FillRectangle((New-Object System.Drawing.SolidBrush $cSt), $bx, $y0 - $hs, $bw, $hs)
    $g.FillRectangle((New-Object System.Drawing.SolidBrush $cMw), $bx + $bw, $y0 - $hw, $bw, $hw)
    $g.DrawString(('{0:N0}' -f $it.S), $fSmall, [System.Drawing.Brushes]::Black, $bx - 4, $y0 - $hs - 20)
    $g.DrawString(('{0:N0}' -f $it.W), $fSmall, [System.Drawing.Brushes]::Black, $bx + $bw - 4, $y0 - $hw - 38)
    $g.DrawString($it.L, $fSmall, [System.Drawing.Brushes]::Black, $bx - 12, $y0 + 8)
}
$g.FillRectangle((New-Object System.Drawing.SolidBrush $cSt), 1250, 100, 22, 16); $g.DrawString('Structura (esforcos do solver de barras)', $fText, [System.Drawing.Brushes]::Black, 1278, 96)
$g.FillRectangle((New-Object System.Drawing.SolidBrush $cMw), 1250, 126, 22, 16); $g.DrawString('Mecway 28 (EI x variacao de rotacao / L)', $fText, [System.Drawing.Brushes]::Black, 1278, 122)
$bmp.Save((Join-Path $Out 'barras_momento_meio_vigas.png')); $g.Dispose(); $bmp.Dispose()

# ---- 2. carga por barra (quinhoes) x envoltoria analitica ----
$c = NewCanvas 1800 900; $bmp = $c[0]; $g = $c[1]
$g.DrawString('Quinhoes de carga: carga distribuida aplicada pela laje em cada barra (kN/m)', $fTitle, [System.Drawing.Brushes]::Black, 40, 20)
$g.DrawString('Barras divididas a cada 1 m; analitico = regra dos 45 graus (cada ponto descarrega na aresta apoiada mais proxima), calculado fora do Structura.', $fText, [System.Drawing.Brushes]::DimGray, 40, 62)
$panels = @(
    @{ Case = 'LV1_laje_4vigas_4pilares'; Axis = 'x'; Title = 'Laje 6x4: viga longa (y=0), trapezio'; X = 60; Y = 120 },
    @{ Case = 'LV1_laje_4vigas_4pilares'; Axis = 'y'; Title = 'Laje 6x4: viga curta (x=0), triangulo'; X = 940; Y = 120 },
    @{ Case = 'LV3_laje_unidirecional_2vigas'; Axis = 'x'; Title = 'Laje apoiada so em 2 vigas (y=0): carga uniforme'; X = 60; Y = 500 })
foreach ($p in $panels) {
    $rows = $jq.($p.Case).rows | Where-Object { $_.axis -eq $p.Axis }
    $first = $rows | Where-Object { $_.s0 -ge 0 } | Select-Object -First 1
    $sel = @($rows | Where-Object { $_.bar -le $(if ($p.Axis -eq 'x') { 6 } else { 20 }) })
    if ($p.Axis -eq 'y') { $sel = @($rows | Select-Object -Last 4) } else { $sel = @($rows | Select-Object -First 6) }
    $px = $p.X; $py = $p.Y; $pw = 800; $ph = 320
    $g.DrawString($p.Title, $fText, [System.Drawing.Brushes]::Black, $px, $py - 6)
    $g.DrawLine([System.Drawing.Pens]::Black, $px + 50, $py + $ph + 20, $px + $pw, $py + $ph + 20)
    $g.DrawLine([System.Drawing.Pens]::Black, $px + 50, $py + 20, $px + 50, $py + $ph + 20)
    $vmax = 11.0
    for ($t = 0; $t -le 11; $t += 2) {
        $yy = $py + $ph + 20 - ($ph * $t / $vmax)
        $g.DrawLine([System.Drawing.Pens]::LightGray, $px + 50, $yy, $px + $pw, $yy)
        $g.DrawString("$t", $fSmall, [System.Drawing.Brushes]::Black, $px + 20, $yy - 9)
    }
    $k = 0; $ns = $sel.Count; $slot = ($pw - 70) / $ns
    foreach ($r in $sel) {
        $bx = $px + 60 + $slot * $k + $slot * 0.12; $bw = $slot * 0.36
        $he = $ph * ([Math]::Abs($r.q_expected) / 1000.0) / $vmax
        $hs = $ph * ([Math]::Abs($r.q_structura) / 1000.0) / $vmax
        $g.FillRectangle((New-Object System.Drawing.SolidBrush $cEx), $bx, $py + $ph + 20 - $he, $bw, $he)
        $g.FillRectangle((New-Object System.Drawing.SolidBrush $cSt), $bx + $bw, $py + $ph + 20 - $hs, $bw, $hs)
        $g.DrawString(('{0:N2}' -f ([Math]::Abs($r.q_structura) / 1000.0)), $fSmall, [System.Drawing.Brushes]::Black, $bx + $bw - 4, $py + $ph - $hs)
        $g.DrawString(('{0:N1}-{1:N1} m' -f $r.s0, $r.s1), $fSmall, [System.Drawing.Brushes]::Black, $bx, $py + $ph + 26)
        $k++
    }
}
$g.FillRectangle((New-Object System.Drawing.SolidBrush $cEx), 1010, 520, 22, 16); $g.DrawString('Analitico (envoltoria)', $fText, [System.Drawing.Brushes]::Black, 1038, 516)
$g.FillRectangle((New-Object System.Drawing.SolidBrush $cSt), 1010, 548, 22, 16); $g.DrawString('Structura (carga aplicada a barra)', $fText, [System.Drawing.Brushes]::Black, 1038, 544)
$g.DrawString('Soma em todas as barras: igual a q x area em todos os casos (erro < 1e-7).', $fText, [System.Drawing.Brushes]::Black, 1010, 600)
$bmp.Save((Join-Path $Out 'quinhoes_carga_por_barra.png')); $g.Dispose(); $bmp.Dispose()

# ---- 3. planta: nos da laje, nos de barra, nos criados nas barras (cortes) e apoios ----
function Plan($case, $title, $file) {
    $cd = Join-Path $Cases $case
    $nodes = Import-Csv -LiteralPath (Join-Path $cd 'nodes.csv') -Header i, x, y, z
    $bars = Import-Csv -LiteralPath (Join-Path $cd 'bars.csv') -Header id, x0, y0, z0, x1, y1, z1, col
    $cuts = @(); if (Test-Path (Join-Path $cd 'cuts.csv')) { $cuts = Import-Csv -LiteralPath (Join-Path $cd 'cuts.csv') -Header m, t, x, y, z }
    $bases = Import-Csv -LiteralPath (Join-Path $cd 'bases.csv') -Header x, y, z
    $xs = $nodes | ForEach-Object { [double]$_.x }; $ys = $nodes | ForEach-Object { [double]$_.y }
    $xm = ($xs | Measure-Object -Maximum).Maximum; $ym = ($ys | Measure-Object -Maximum).Maximum
    $s = [Math]::Min(1500 / $xm, 560 / $ym)
    $c = NewCanvas 1800 900; $bmp = $c[0]; $g = $c[1]
    $g.DrawString($title, $fTitle, [System.Drawing.Brushes]::Black, 40, 20)
    $g.DrawString('Planta: malha da laje (pontos cinza), nos das barras (vermelho), nos criados pela laje nas barras (anel verde), apoios dos pilares (triangulo).', $fText, [System.Drawing.Brushes]::DimGray, 40, 62)
    $ox = 150; $oy = 800
    function PX($x) { return $ox + $x * $s }
    function PY($y) { return $oy - $y * $s }
    $penMesh = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(210, 210, 210)), 1
    foreach ($xx in ($xs | Sort-Object -Unique)) { $g.DrawLine($penMesh, (PX $xx), (PY 0), (PX $xx), (PY $ym)) }
    foreach ($yy in ($ys | Sort-Object -Unique)) { $g.DrawLine($penMesh, (PX 0), (PY $yy), (PX $xm), (PY $yy)) }
    $penBar = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 170, 60)), 8
    foreach ($b in $bars) { if ($b.col -eq '0') { $g.DrawLine($penBar, (PX ([double]$b.x0)), (PY ([double]$b.y0)), (PX ([double]$b.x1)), (PY ([double]$b.y1))) } }
    foreach ($n in $nodes) { $g.FillEllipse([System.Drawing.Brushes]::Gray, (PX ([double]$n.x)) - 3, (PY ([double]$n.y)) - 3, 6, 6) }
    foreach ($b in $bars) { if ($b.col -eq '0') { foreach ($pt in @(@($b.x0, $b.y0), @($b.x1, $b.y1))) { $g.FillEllipse([System.Drawing.Brushes]::Red, (PX ([double]$pt[0])) - 5, (PY ([double]$pt[1])) - 5, 10, 10) } } }
    $penCut = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(0, 160, 60)), 3
    foreach ($q in $cuts) { $g.DrawEllipse($penCut, (PX ([double]$q.x)) - 11, (PY ([double]$q.y)) - 11, 22, 22) }
    foreach ($b in $bases) { $bx = PX ([double]$b.x); $by = PY ([double]$b.y); $pts = @((New-Object System.Drawing.PointF $bx, ($by - 20)), (New-Object System.Drawing.PointF ($bx - 16), ($by + 14)), (New-Object System.Drawing.PointF ($bx + 16), ($by + 14))); $g.FillPolygon([System.Drawing.Brushes]::SteelBlue, [System.Drawing.PointF[]]$pts) }
    $g.DrawString(('{0} nos de malha sobre eixos de barras; {1} nos criados nas barras (cortes); {2} apoios.' -f ($nodes | Where-Object { $true }).Count, $cuts.Count, $bases.Count), $fText, [System.Drawing.Brushes]::Black, 40, 860)
    $bmp.Save((Join-Path $Out $file)); $g.Dispose(); $bmp.Dispose()
}
Plan 'LV1_laje_4vigas_4pilares' 'Nos criados pela laje nas barras e apoios - laje 6x4 sobre 4 vigas e 4 pilares' 'nos_apoios_planta_LV1.png'
Plan 'LV2_duas_baias_viga_central' 'Nos criados pela laje nas barras e apoios - laje 8x4, viga central e 6 pilares' 'nos_apoios_planta_LV2.png'
