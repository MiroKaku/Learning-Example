$shell  = New-Object -COM Shell.Application
$files  = Get-ChildItem "." -Include ".vs","Output" -Force -Recurse

foreach ($file in $files)
{
    $path = Resolve-Path "$file";
    Write-Host "delete " "$path";
    $shell.NameSpace(10).MoveHere("$path");
}

Read-Host -Prompt "Press Enter to continue"
