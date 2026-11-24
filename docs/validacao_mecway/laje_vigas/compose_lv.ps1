# Monta o print lado a lado (Structura | Mecway 28) de um caso e recorta a janela do Mecway na area 3D.
# uso: compose_lv.ps1 <structura.png> <mecway_raw.png> <saida_mecway.png> <saida_comparacao.png> <titulo>
param([string]$Structura, [string]$MecwayRaw, [string]$MecwayOut, [string]$Both, [string]$Title)
Add-Type -AssemblyName System.Drawing
$mw = [System.Drawing.Bitmap]::FromFile($MecwayRaw)
# area 3D da janela do Mecway (sem a arvore de modelo e a barra de status)
$crop = New-Object System.Drawing.Rectangle 250, 130, ($mw.Width - 252), ($mw.Height - 130 - 40)
$mwc = $mw.Clone($crop, $mw.PixelFormat)
# tela cheia do Mecway com titulo
$tw = 1800
$mh = [int]($mwc.Height * $tw / $mwc.Width)
$mout = New-Object System.Drawing.Bitmap $tw, ($mh + 110)
$g = [System.Drawing.Graphics]::FromImage($mout)
$g.Clear([System.Drawing.Color]::White)
$g.InterpolationMode = 'HighQualityBicubic'
$g.TextRenderingHint = 'ClearTypeGridFit'
$f1 = New-Object System.Drawing.Font 'Segoe UI', 22, ([System.Drawing.FontStyle]::Bold)
$f2 = New-Object System.Drawing.Font 'Segoe UI', 14
$g.DrawString("Mecway 28 - $Title", $f1, [System.Drawing.Brushes]::Black, 40, 20)
$g.DrawString('Mesmo modelo (nos compartilhados), mesma carga e mesmos apoios. Escala de cores do Mecway.', $f2, [System.Drawing.Brushes]::DimGray, 40, 68)
$g.DrawImage($mwc, 0, 110, $tw, $mh)
$g.Dispose()
$mout.Save($MecwayOut)
# lado a lado
$st = [System.Drawing.Bitmap]::FromFile($Structura)
$sw = 1400
$sh = [int]($st.Height * $sw / $st.Width)
$mh2 = [int]($mout.Height * $sw / $mout.Width)
$H = [Math]::Max($sh, $mh2) + 90
$sbs = New-Object System.Drawing.Bitmap ($sw * 2), $H
$g = [System.Drawing.Graphics]::FromImage($sbs)
$g.Clear([System.Drawing.Color]::White)
$g.InterpolationMode = 'HighQualityBicubic'
$g.TextRenderingHint = 'ClearTypeGridFit'
$g.DrawString("Validacao Structura x Mecway 28 - $Title", $f1, [System.Drawing.Brushes]::Black, 30, 14)
$g.DrawImage($st, 0, 80, $sw, $sh)
$g.DrawImage($mout, $sw, 80, $sw, $mh2)
$g.Dispose()
$sbs.Save($Both)
$mw.Dispose(); $mwc.Dispose(); $mout.Dispose(); $st.Dispose(); $sbs.Dispose()
