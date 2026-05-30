# Bulk xalloc -> std/new[] sweep for a single source file.
# Conservative: only converts patterns that are 1:1 safe.
# Usage: powershell -File scripts/xalloc_sweep.ps1 -Path src/bnetd/message.cpp
param([Parameter(Mandatory=$true)][string]$Path)

$f = (Resolve-Path $Path).Path
$t = [System.IO.File]::ReadAllText($f)

# 1. (char*)xmalloc(EXPR)  ->  new char[EXPR]
$t = [regex]::Replace($t, '\(char\s*\*\)\s*xmalloc\(', 'new char[REPLACE_EXPR_START')
# Wait - need balanced paren handling. Skip regex with arbitrary expressions.
# Instead: revert, do targeted patterns line-by-line.
$t = [System.IO.File]::ReadAllText($f)

# Simple pattern: (char*)xmalloc(N) where N has no nested parens
$t = [regex]::Replace($t, '\(char\s*\*\)xmalloc\(([^()]+)\);', 'new char[$1];')
$t = [regex]::Replace($t, '\(char\s*\*\)xmalloc\(([^()]+\([^()]*\)[^()]*)\);', 'new char[$1];')

# (T*)xmalloc(sizeof(T))  ->  new T{}
$t = [regex]::Replace($t, '\(([A-Za-z_][A-Za-z_0-9 :]*)\s*\*\)xmalloc\(sizeof\(\1\)\);', 'new $1{};')

# xstrdup("") -> conn_strdup is per-file; use generic: new char[1]{}; (caller writes nothing)
# Skip: handled per-file.

# xfree((void*)EXPR); /* avoid warning */  ->  delete[] const_cast<char*>(EXPR);
$t = [regex]::Replace($t, 'xfree\(\(void\s*\*\)([^;]+?)\);\s*/\*\s*avoid warning\s*\*/', 'delete[] const_cast<char*>($1);')
# xfree((void*)EXPR);  ->  delete[] const_cast<char*>(EXPR);
$t = [regex]::Replace($t, 'xfree\(\(void\s*\*\)([^;]+?)\);', 'delete[] const_cast<char*>($1);')
# xfree((void *) ...)
$t = [regex]::Replace($t, 'xfree\(\(void \*\)([^;]+?)\);', 'delete[] const_cast<char*>($1);')

[System.IO.File]::WriteAllText($f, $t, [System.Text.UTF8Encoding]::new($false))

# Report what remains
Write-Host "--- remaining xalloc references in $Path ---"
Select-String -Path $f -Pattern 'xmalloc|xfree|xstrdup|xrealloc|xcalloc' | ForEach-Object { "{0,5}: {1}" -f $_.LineNumber, $_.Line.Trim() }
