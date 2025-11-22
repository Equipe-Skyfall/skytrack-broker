# PowerShell script to create mosquitto password file using the eclipse-mosquitto image
Param(
  [Parameter(Mandatory=$true)] [string]$Username,
  [Parameter(Mandatory=$false)] [string]$Password
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Join-Path $ScriptDir ".."
$ConfigDir = Join-Path $RootDir "config"
$PassFile = Join-Path $ConfigDir "passwords"

# Check if Docker is running
Write-Host "Checking if Docker is running..."
try {
  $dockerVersion = docker version --format '{{.Server.Version}}' 2>$null
  if (-not $dockerVersion) {
    throw "Docker not responding"
  }
  Write-Host "Docker is running (version: $dockerVersion)"
} catch {
  Write-Error "Docker is not running or not accessible. Please start Docker Desktop and try again."
  Write-Host ""
  Write-Host "Manual alternative:"
  Write-Host "1. Install mosquitto locally (https://mosquitto.org/download/)"
  Write-Host "2. Run: mosquitto_passwd -c `"$PassFile`" $Username"
  Write-Host "3. Or start Docker and run this script again"
  exit 1
}

# Ensure config directory exists
if (-not (Test-Path $ConfigDir)) {
  New-Item -ItemType Directory -Path $ConfigDir -Force | Out-Null
  Write-Host "Created config directory: $ConfigDir"
}

# Convert Windows path to proper format for Docker volume mount
$ConfigDirForDocker = $ConfigDir -replace '\\', '/' -replace '^([A-Za-z]):', '/$1'

Write-Host "Creating password file for user: $Username"
Write-Host "Config directory: $ConfigDir"

if ($Password) {
  # non-interactive: pass password on command line (note: visible in process list)
  Write-Host "Running: docker run --rm -v `"${ConfigDir}:/mosquitto/config`" eclipse-mosquitto mosquitto_passwd -c -b /mosquitto/config/passwords $Username [password hidden]"
  docker run --rm -v "${ConfigDir}:/mosquitto/config" eclipse-mosquitto mosquitto_passwd -c -b /mosquitto/config/passwords $Username $Password
} else {
  Write-Host "No password provided. You will be prompted to enter it interactively."
  Write-Host "Running: docker run --rm -it -v `"${ConfigDir}:/mosquitto/config`" eclipse-mosquitto mosquitto_passwd -c /mosquitto/config/passwords $Username"
  docker run --rm -it -v "${ConfigDir}:/mosquitto/config" eclipse-mosquitto mosquitto_passwd -c /mosquitto/config/passwords $Username
}

if ($LASTEXITCODE -eq 0) {
  Write-Host "✅ Password file created successfully at: $PassFile"
  if (Test-Path $PassFile) {
    Write-Host "File details:"
    Get-ChildItem -Path $PassFile | Format-List Name, Length, LastWriteTime
    Write-Host ""
    Write-Host "Next steps:"
    Write-Host "1. Set environment variables:"
    Write-Host "   `$env:MQTT_USERNAME='$Username'"
    Write-Host "   `$env:MQTT_PASSWORD='[your_password]'"
    Write-Host "2. Run: docker-compose -f docker-compose.backend.yml up -d"
  } else {
    Write-Warning "Password file was not created at expected location: $PassFile"
  }
} else {
  Write-Error "Failed to create password file. Exit code: $LASTEXITCODE"
  Write-Host "Try running Docker Desktop as administrator or check Docker logs."
}
