# pwsh -NoLogo -NonInteractive -File $(SolutionDir)\Scripts\Package.ps1 -Header $(MSBuildProjectDirectory)\include -Lib $(OutDir)$(ProjectName).lib -Output $(OutDir)$(ProjectName)_$(ConfigurationName).zip
param(
    [Parameter(Mandatory = $true)][string]$Lib,
    [Parameter(Mandatory = $true)][string]$Header,
    [Parameter(Mandatory = $true)][string]$Output
)
function Add-ZipFile {
    param(

        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$Header,
        [Parameter(Mandatory = $true)]
        [string]$Output
    )

    if (Test-Path $Output) {
        Remove-Item $Output
    }

    $tempDir = New-Item -ItemType Directory -Path (Join-Path $env:TEMP ([System.Guid]::NewGuid().ToString()))

    Copy-Item -Path "$Source" -Destination $tempDir.FullName 
    Copy-Item -Path "$Header" -Destination $tempDir.FullName -Recurse

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::
    [System.IO.Compression.ZipFile]::CreateFromDirectory($tempDir.FullName, $Output)
    
    Write-Output "Zip file created: $Output"
    Remove-Item $tempDir.FullName -Recurse -Force
}

Write-Output "💩 Lib: $Lib"
Write-Output "💩 Header: $Header"
Write-Output "💩 Output: $Output"


Add-ZipFile -Source $Lib -Header $Header -Output $Output

Write-Output "Package created: $Output"