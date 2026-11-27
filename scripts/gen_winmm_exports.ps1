param(
    [Parameter(Mandatory = $true)]
    [string]$OutDir
)

$ErrorActionPreference = "Stop"

$winmmPath = Join-Path ([Environment]::GetFolderPath("System")) "winmm.dll"
if (-not (Test-Path $winmmPath)) {
    throw "System winmm.dll not found at: $winmmPath"
}

$bytes = [System.IO.File]::ReadAllBytes($winmmPath)

function Read-UInt16([byte[]]$b, [int]$off) { [BitConverter]::ToUInt16($b, $off) }
function Read-UInt32([byte[]]$b, [int]$off) { [BitConverter]::ToUInt32($b, $off) }

$peOff = Read-UInt32 $bytes 0x3C
$numSec = Read-UInt16 $bytes ($peOff + 6)
$optSize = Read-UInt16 $bytes ($peOff + 20)
$optOff = $peOff + 24
$magic = Read-UInt16 $bytes $optOff
if ($magic -ne 0x20B) {
    throw "Expected 64-bit System32\winmm.dll (PE32+), got magic 0x$($magic.ToString('X'))"
}

$expRva = Read-UInt32 $bytes ($optOff + 112)
if ($expRva -eq 0) {
    throw "No export directory found in $winmmPath"
}

$secOff = $optOff + $optSize
$sections = @()
for ($i = 0; $i -lt $numSec; $i++) {
    $s = $secOff + $i * 40
    $vsize = Read-UInt32 $bytes ($s + 8)
    $vaddr = Read-UInt32 $bytes ($s + 12)
    $rsize = Read-UInt32 $bytes ($s + 16)
    $rptr  = Read-UInt32 $bytes ($s + 20)
    $sections += [PSCustomObject]@{ VA = $vaddr; Size = [Math]::Max($vsize, $rsize); Ptr = $rptr }
}

function Rva-ToOff([uint32]$rva) {
    foreach ($sec in $sections) {
        if ($rva -ge $sec.VA -and $rva -lt ($sec.VA + $sec.Size)) {
            return [int]($sec.Ptr + ($rva - $sec.VA))
        }
    }
    return -1
}

function Read-CStr([int]$off) {
    if ($off -lt 0) { return "" }
    $end = $off
    while ($end -lt $bytes.Length -and $bytes[$end] -ne 0) { $end++ }
    return [System.Text.Encoding]::ASCII.GetString($bytes, $off, $end - $off)
}

$expOff = Rva-ToOff $expRva
$ordBase  = Read-UInt32 $bytes ($expOff + 16)
$numFuncs = Read-UInt32 $bytes ($expOff + 20)
$numNames = Read-UInt32 $bytes ($expOff + 24)
$funcsRva = Read-UInt32 $bytes ($expOff + 28)
$namesRva = Read-UInt32 $bytes ($expOff + 32)
$ordsRva  = Read-UInt32 $bytes ($expOff + 36)

$funcsOff = Rva-ToOff $funcsRva
$namesOff = Rva-ToOff $namesRva
$ordsOff  = Rva-ToOff $ordsRva

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$incLines = New-Object System.Collections.Generic.List[string]
$defLines = New-Object System.Collections.Generic.List[string]
$incLines.Add("// Auto-generated from $winmmPath")
$defLines.Add("LIBRARY winmm")
$defLines.Add("EXPORTS")

$namedOrds = New-Object 'System.Collections.Generic.HashSet[uint32]'
$count = 0
for ($i = 0; $i -lt $numNames; $i++) {
    $nameRva = Read-UInt32 $bytes ($namesOff + $i * 4)
    $name = Read-CStr (Rva-ToOff $nameRva)
    $ordIdx = Read-UInt16 $bytes ($ordsOff + $i * 2)
    $ordinal = [uint32]($ordBase + $ordIdx)

    if ($name -match '^[A-Za-z_][A-Za-z0-9_]*$') {
        $incLines.Add("WINMM_PROXY_STUB($name)")
        $defLines.Add("    $name = Proxy_$name @$ordinal")
        [void]$namedOrds.Add($ordinal)
        $count++
    }
}

# Also forward any unnamed ordinal-only exports
for ($i = 0; $i -lt $numFuncs; $i++) {
    $fnRva = Read-UInt32 $bytes ($funcsOff + $i * 4)
    if ($fnRva -eq 0) { continue }
    $ordinal = [uint32]($ordBase + $i)
    if (-not $namedOrds.Contains($ordinal)) {
        $incLines.Add("WINMM_PROXY_ORD_STUB($ordinal)")
        $defLines.Add("    Ord_$ordinal = Proxy_Ord_$ordinal @$ordinal NONAME")
        $count++
    }
}

$incPath = Join-Path $OutDir "winmm_exports.inc"
$defPath = Join-Path $OutDir "winmm_proxy.def"
[System.IO.File]::WriteAllLines($incPath, $incLines, [System.Text.Encoding]::ASCII)
[System.IO.File]::WriteAllLines($defPath, $defLines, [System.Text.Encoding]::ASCII)

Write-Host "[BUILD] Generated $count WinMM exports from $winmmPath"
