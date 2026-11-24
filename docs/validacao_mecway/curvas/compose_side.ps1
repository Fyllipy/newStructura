# Junta duas imagens lado a lado (Structura | Mecway) numa so, para comparacao.
param([string]$Left, [string]$Right, [string]$Out)
Add-Type -AssemblyName System.Drawing
$a = [System.Drawing.Image]::FromFile($Left); $b = [System.Drawing.Image]::FromFile($Right)
$bmp = New-Object System.Drawing.Bitmap ($a.Width + $b.Width + 20), ([Math]::Max($a.Height, $b.Height))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::White)
$g.DrawImage($a, 0, 0, $a.Width, $a.Height)
$g.DrawImage($b, $a.Width + 20, 0, $b.Width, $b.Height)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose(); $a.Dispose(); $b.Dispose()
