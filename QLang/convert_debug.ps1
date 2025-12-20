# Wrap debug statements with #if QLANG_DEBUG / #endif
# This script finds std::cout << "[DEBUG]..." lines and wraps them

$qlangDir = "c:\Quantum\Quantum3D\QLang"

$files = @(
    "QRunner.h",
    "QStatement.h",
    "QVariableDecl.h", 
    "QWhile.h",
    "Tokenizer.cpp"
)

foreach ($file in $files) {
    $path = Join-Path $qlangDir $file
    if (-not (Test-Path $path)) { continue }
    
    Write-Host "Processing $file..."
    $lines = Get-Content $path
    $result = New-Object System.Collections.ArrayList
    $i = 0
    $wrapped = 0
    
    while ($i -lt $lines.Count) {
        $line = $lines[$i]
        
        # Check if this line starts a debug statement
        if ($line -match 'std::cout\s*<<\s*"\[DEBUG\]') {
            # Find the end of this statement (semicolon)
            $startIndex = $i
            $statement = @($line)
            
            # Check if statement continues on next lines
            while ($line -notmatch ';\s*$' -and $i -lt $lines.Count - 1) {
                $i++
                $line = $lines[$i]
                $statement += $line
            }
            
            # Add the #if guard, the statement(s), and #endif
            [void]$result.Add("#if QLANG_DEBUG")
            foreach ($s in $statement) {
                [void]$result.Add($s)
            }
            [void]$result.Add("#endif")
            $wrapped++
        }
        else {
            [void]$result.Add($line)
        }
        $i++
    }
    
    # Write back
    $result | Set-Content $path -Encoding UTF8
    Write-Host "  Wrapped $wrapped debug statements"
}

Write-Host "`nDone! All debug statements wrapped with #if QLANG_DEBUG"
