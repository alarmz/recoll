$ErrorActionPreference = 'Stop'

$packageName = 'recoll.install'
$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$url64 = 'https://github.com/alarmz/recoll/releases/download/Next/recoll-Next-win64-setup.exe'
$checksum64 = '434dd87e89d54cda915a3a9200c6acfd6e4e27d4ff1ecdff1fd2422d51a2a59a'

$packageArgs = @{
  packageName    = $packageName
  fileType       = 'exe'
  url64bit       = $url64
  softwareName   = 'Recoll*'
  checksum64     = $checksum64
  checksumType64 = 'sha256'
  silentArgs     = '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART'
  validExitCodes = @(0)
}

Install-ChocolateyPackage @packageArgs
