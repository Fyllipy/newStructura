# Gera o icone (PNGs em varios tamanhos) e a tela de abertura do Structura 3D.
# Uso: powershell -ExecutionPolicy Bypass -File tools\gen_branding.ps1
# Saidas: icons/app/app-<tamanho>.png e images/splash.png (o .ico e montado por tools/make_ico.py).
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$iconDir = Join-Path $root "icons\app"
$imageDir = Join-Path $root "images"
New-Item -ItemType Directory -Force $iconDir | Out-Null
New-Item -ItemType Directory -Force $imageDir | Out-Null

function New-Gfx($bmp) {
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.TextRenderingHint = 'AntiAliasGridFit'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.PixelOffsetMode = 'HighQuality'
    return $g
}

function Get-RoundedPath($x, $y, $w, $h, $r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = 2 * $r
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

# Portico triangulado (a marca): coordenadas em fracoes do quadrado do icone
function Draw-Truss($g, $size, $penWidth, $color, $nodeColor) {
    $s = $size
    $pts = @{
        a = [System.Drawing.PointF]::new($s * 0.20, $s * 0.74)
        b = [System.Drawing.PointF]::new($s * 0.50, $s * 0.74)
        c = [System.Drawing.PointF]::new($s * 0.80, $s * 0.74)
        d = [System.Drawing.PointF]::new($s * 0.35, $s * 0.42)
        e = [System.Drawing.PointF]::new($s * 0.65, $s * 0.42)
        f = [System.Drawing.PointF]::new($s * 0.50, $s * 0.24)
    }
    $pen = New-Object System.Drawing.Pen($color, $penWidth)
    $pen.StartCap = 'Round'; $pen.EndCap = 'Round'; $pen.LineJoin = 'Round'
    $links = @(@('a','b'), @('b','c'), @('a','d'), @('d','f'), @('f','e'), @('e','c'), @('d','b'), @('b','e'), @('d','e'))
    foreach ($l in $links) { $g.DrawLine($pen, $pts[$l[0]], $pts[$l[1]]) }
    $r = $penWidth * 1.2
    $brush = New-Object System.Drawing.SolidBrush($nodeColor)
    foreach ($k in $pts.Keys) {
        $g.FillEllipse($brush, $pts[$k].X - $r, $pts[$k].Y - $r, 2 * $r, 2 * $r)
    }
    $pen.Dispose(); $brush.Dispose()
}

function New-Icon($size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = New-Gfx $bmp
    $g.Clear([System.Drawing.Color]::Transparent)
    $m = $size * 0.04
    $path = Get-RoundedPath $m $m ($size - 2 * $m) ($size - 2 * $m) ($size * 0.22)
    $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        [System.Drawing.PointF]::new(0, 0), [System.Drawing.PointF]::new($size, $size),
        [System.Drawing.ColorTranslator]::FromHtml('#35b6e8'), [System.Drawing.ColorTranslator]::FromHtml('#0a5a86'))
    $g.FillPath($grad, $path)
    # brilho suave no topo
    $gloss = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        [System.Drawing.PointF]::new(0, 0), [System.Drawing.PointF]::new(0, $size * 0.55),
        [System.Drawing.Color]::FromArgb(70, 255, 255, 255), [System.Drawing.Color]::FromArgb(0, 255, 255, 255))
    $g.SetClip($path)
    $g.FillRectangle($gloss, 0, 0, $size, $size * 0.55)
    $g.ResetClip()
    Draw-Truss $g $size ([Math]::Max(1.2, $size * 0.045)) ([System.Drawing.Color]::White) ([System.Drawing.ColorTranslator]::FromHtml('#ffb400'))
    $g.Dispose()
    return $bmp
}

foreach ($size in 16, 24, 32, 48, 64, 128, 256) {
    $bmp = New-Icon $size
    $bmp.Save((Join-Path $iconDir "app-$size.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

# ---- tela de abertura -------------------------------------------------------
$w = 720; $h = 420
$bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = New-Gfx $bmp
$bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    [System.Drawing.PointF]::new(0, 0), [System.Drawing.PointF]::new($w, $h),
    [System.Drawing.ColorTranslator]::FromHtml('#0b2a44'), [System.Drawing.ColorTranslator]::FromHtml('#0d7fb9'))
$g.FillRectangle($bg, 0, 0, $w, $h)

# trelica decorativa ao fundo (linhas finas)
$deco = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(34, 255, 255, 255), 1.6)
$nodeBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(70, 255, 255, 255))
$bays = 9
$bw = 90.0
for ($i = 0; $i -le $bays; $i++) {
    $x = -20 + $i * $bw
    $g.DrawLine($deco, [float]$x, 40.0, [float]$x, 300.0)
}
for ($i = 0; $i -lt $bays; $i++) {
    $x = -20 + $i * $bw
    if ($i % 2 -eq 0) { $g.DrawLine($deco, [float]$x, 40.0, [float]($x + $bw), 300.0) } else { $g.DrawLine($deco, [float]$x, 300.0, [float]($x + $bw), 40.0) }
}
$g.DrawLine($deco, 0.0, 40.0, [float]$w, 40.0)
$g.DrawLine($deco, 0.0, 300.0, [float]$w, 300.0)
for ($i = 0; $i -le $bays; $i++) {
    $x = -20 + $i * $bw
    $g.FillEllipse($nodeBrush, [float]($x - 4), 36.0, 8.0, 8.0)
    $g.FillEllipse($nodeBrush, [float]($x - 4), 296.0, 8.0, 8.0)
}
# faixa inferior translucida
$band = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(70, 4, 20, 36))
$g.FillRectangle($band, 0, 300, $w, 120)

# marca (icone) e titulo
$logo = New-Icon 120
$g.DrawImage($logo, 46, 74, 120, 120)
$logo.Dispose()
$fontTitle = New-Object System.Drawing.Font('Segoe UI Semibold', 46, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$fontSub = New-Object System.Drawing.Font('Segoe UI', 17, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$fontSmall = New-Object System.Drawing.Font('Segoe UI', 13, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$white = [System.Drawing.Brushes]::White
$soft = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(215, 235, 247))
$g.DrawString('Structura 3D', $fontTitle, $white, 184, 84)
$g.DrawString('Modelagem, análise e dimensionamento de estruturas metálicas', $fontSub, $soft, 188, 148)
$accent = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml('#ffb400'))
$g.FillRectangle($accent, 188, 182, 64, 4)
$g.DrawString('NBR 8800 · NBR 14762 · NBR 6118 · CIDECT', $fontSmall, $soft, 188, 196)
$g.DrawString('© Structura 3D', $fontSmall, $soft, 46, 380)

$bmp.Save((Join-Path $imageDir "splash.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host "Arquivos gerados em $iconDir e $imageDir"
