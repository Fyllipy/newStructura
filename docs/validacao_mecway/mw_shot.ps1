# Abre um .liml resolvido no Mecway 28, mostra o deslocamento (magnitude) com escala de cores e captura a janela.
# uso: mw_shot.ps1 <liml> <png> [iso] [deformada]
param([string]$Liml, [string]$Png, [int]$Rows = 4, [string]$Iso = '1', [string]$Def = '1', [int]$Wait = 12)
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W { [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h); [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int c); [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y); [DllImport("user32.dll")] public static extern void mouse_event(int f,int dx,int dy,int d,int e);
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; } }
"@
function Click($x, $y) { [W]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 150; [W]::mouse_event(2,0,0,0,0); [W]::mouse_event(4,0,0,0,0); Start-Sleep -Milliseconds 900 }
$p = Start-Process 'C:\Program Files\Mecway\mecway.exe' -ArgumentList "`"$Liml`"" -PassThru
Start-Sleep -Seconds $Wait; $p.Refresh()
[W]::ShowWindow($p.MainWindowHandle, 3) | Out-Null; [W]::SetForegroundWindow($p.MainWindowHandle) | Out-Null; Start-Sleep -Seconds 2
$r = New-Object W+RECT; [W]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
# SEGURANCA: so clica e captura se a janela do Mecway estiver MESMO em primeiro plano; senao aborta (os cliques e a captura de tela iriam para outra janela do usuario)
if ($p.MainWindowHandle -eq [IntPtr]::Zero -or [W]::GetForegroundWindow() -ne $p.MainWindowHandle) {
    Stop-Process -Id $p.Id -Force
    Write-Error 'A janela do Mecway nao ficou em primeiro plano: nada foi clicado nem capturado.'
    exit 3
}
$dy = 375 - 18 * (4 - $Rows)              # a linha de "displacement" depende de quantos itens ha em Loads & Constraints
Click ($r.L + 44) ($r.T + $dy)          # expande "displacement"
Click ($r.L + 124) ($r.T + $dy + 18)    # displacement > magnitude
if ($Iso -eq '1') { Click ($r.L + 163) ($r.T + 68) }     # vista isometrica
if ($Def -eq '1') { Click ($r.L + 652) ($r.T + 93) }     # forma deformada
[W]::SetCursorPos($r.L + 120, $r.T + 900) | Out-Null   # tira o cursor de cima da barra de ferramentas (some a dica "Deformed view")
Start-Sleep -Seconds 4
$w = $r.R - $r.L; $h = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
$bmp.Save($Png); $g.Dispose(); $bmp.Dispose()
Stop-Process -Id $p.Id -Force
