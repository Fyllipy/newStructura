param([string]$Csv, [string]$Out)
Add-Type -AssemblyName System.Drawing
$rows = Import-Csv -LiteralPath $Csv
$names = $rows[0].PSObject.Properties.Name | Where-Object { $_ -ne 'x' }
$W = 1500; $H = 900
$bmp = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAlias'
$g.Clear([System.Drawing.Color]::White)
$left = 140; $right = 60; $top = 110; $bottom = 110
$pw = $W - $left - $right; $ph = $H - $top - $bottom
$ymin = -30000.0; $ymax = 25000.0
function PX($x) { return $left + $x / 6.0 * $pw }
function PY($v) { return $top + ($ymax - $v) / ($ymax - $ymin) * $ph }
$fontT = New-Object System.Drawing.Font 'Segoe UI', 20, ([System.Drawing.FontStyle]::Bold)
$fontS = New-Object System.Drawing.Font 'Segoe UI', 13
$fontL = New-Object System.Drawing.Font 'Segoe UI', 12
$g.DrawString('Laje armada numa direcao sobre 2 portico planos: momento fletor da viga (sagrante positivo)', $fontT, [System.Drawing.Brushes]::Black, 40, 20)
$g.DrawString('LV3: laje 6x4 m, vigas 30x30, pilares 3 m engastados, q = 5 kN/m2 (w = 10 kN/m por viga). Linhas: formulacao analitica (portico) x Structura', $fontS, [System.Drawing.Brushes]::DimGray, 40, 62)
$grid = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(225, 228, 232)), 1
$axis = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(90, 95, 105)), 2
for ($v = -30000; $v -le 25000; $v += 5000) {
    $y = PY $v
    $g.DrawLine($grid, $left, $y, $left + $pw, $y)
    $g.DrawString(('{0:N0}' -f ($v / 1000.0)), $fontL, [System.Drawing.Brushes]::DimGray, 80, $y - 10)
}
for ($x = 0; $x -le 6; $x++) {
    $px = PX $x
    $g.DrawLine($grid, $px, $top, $px, $top + $ph)
    $g.DrawString("$x", $fontL, [System.Drawing.Brushes]::DimGray, $px - 6, $top + $ph + 8)
}
$g.DrawLine($axis, $left, (PY 0), $left + $pw, (PY 0))
$g.DrawString('x (m)', $fontS, [System.Drawing.Brushes]::Black, $left + $pw / 2 - 20, $top + $ph + 40)
$g.DrawString('M (kN.m)', $fontS, [System.Drawing.Brushes]::Black, 20, $top - 30)
$colors = @{
    'analitico_uniforme' = [System.Drawing.Color]::FromArgb(40, 40, 40)
    'analitico_nodal' = [System.Drawing.Color]::FromArgb(120, 120, 120)
    'T (quinhoes)' = [System.Drawing.Color]::FromArgb(0, 114, 178)
    'A t=150 mm' = [System.Drawing.Color]::FromArgb(213, 94, 0)
    'A t=20 mm' = [System.Drawing.Color]::FromArgb(230, 159, 0)
    'A t=20 mm nu=0' = [System.Drawing.Color]::FromArgb(0, 158, 115)
}
$labels = @{
    'analitico_uniforme' = 'Analitico, carga uniforme'
    'analitico_nodal' = 'Analitico, cargas nodais equivalentes'
    'T (quinhoes)' = 'Structura, modo T (quinhoes)'
    'A t=150 mm' = 'Structura, modo A, laje 150 mm'
    'A t=20 mm' = 'Structura, modo A, laje 20 mm'
    'A t=20 mm nu=0' = 'Structura, modo A, laje 20 mm, nu = 0'
}
$ly = (PY -12000)
foreach ($n in $names) {
    $c = $colors[$n]
    $pen = New-Object System.Drawing.Pen $c, 3
    if ($n -like 'analitico*') { $pen.DashStyle = 'Dash'; $pen.Width = 2.5 }
    $pts = @()
    $fx = @();
    foreach ($r in $rows) { $pts += New-Object System.Drawing.PointF ([single](PX ([double]$r.x))), ([single](PY ([double]$r.$n))) }
    $g.DrawLines($pen, [System.Drawing.PointF[]]$pts)
    if ($n -notlike 'analitico*') { foreach ($p in $pts) { $g.FillEllipse((New-Object System.Drawing.SolidBrush $c), $p.X - 5, $p.Y - 5, 10, 10) } }
    $lx = (PX 1.3)
    $g.DrawLine($pen, $lx, $ly + 10, $lx + 40, $ly + 10)
    $g.DrawString($labels[$n], $fontL, [System.Drawing.Brushes]::Black, $lx + 50, $ly)
    $ly += 24
}
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
