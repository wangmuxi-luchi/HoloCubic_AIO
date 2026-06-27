param([string]$LogFile, [string]$Title)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class WinAPI {
    [DllImport("kernel32.dll")] public static extern IntPtr GetConsoleWindow();
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int nIndex);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);
    public const int SM_CXSCREEN = 0;
    public const int SM_CYSCREEN = 1;
}
"@
try {
    $hwnd = [WinAPI]::GetConsoleWindow()
    $sw = [WinAPI]::GetSystemMetrics([WinAPI]::SM_CXSCREEN)
    $sh = [WinAPI]::GetSystemMetrics([WinAPI]::SM_CYSCREEN)
    [WinAPI]::MoveWindow($hwnd, 0, 0, [Math]::Floor($sw/2), [Math]::Floor($sh/2), $true)
    [Console]::Title = $Title
    Write-Host "=== $Title ==="
    Get-Content $LogFile -Tail 0
    Get-Content -Wait $LogFile
}
catch {
    Write-Host "ERROR: $_"
    Read-Host "Press Enter to close"
}