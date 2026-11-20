param([string]$Executable = "$PSScriptRoot/../build/PromptManager.exe", [string]$OutputPath = "$PSScriptRoot/../build/benchmark.json")
python "$PSScriptRoot/benchmark.py" $Executable $OutputPath
exit $LASTEXITCODE
