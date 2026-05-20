# R65 — bnetd v3 legacy-fallback coverage audit

Date: post-R64.

## Method

Scanned every `src/bnetd/*.cpp` for `packet_create(packet_class_bnet)` and
checked whether `PVPGN_V3_BNETD_INTEGRATION` appears within ±120 lines.

```pwsh
$results = @()
Get-ChildItem -Recurse src\bnetd\*.cpp | ForEach-Object {
    $f = $_; $lines = Get-Content $f.FullName
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match 'packet_create\(packet_class_bnet\)') {
            $lo = [Math]::Max(0, $i - 120)
            $hi = [Math]::Min($lines.Count - 1, $i + 120)
            $w  = ($lines[$lo..$hi] -join "`n")
            $results += [PSCustomObject]@{
                File = $f.Name; Line = ($i+1)
                HasV3 = ($w -match 'PVPGN_V3_BNETD_INTEGRATION')
            }
        }
    }
}
```

## Results

| Scope             | Total | Covered | Missing |
|-------------------|------:|--------:|--------:|
| `handle_*.cpp`    |    76 |      76 |       0 |
| All `src/bnetd`   |    97 |      87 |      10 |

**`handle_*.cpp` is 100% covered.** All packet-creation sites inside the
client packet-handler dispatch have a strangler-fig v3 hook in scope.

## Uncovered call sites (outside handle_*.cpp)

These are outbound replies/requests that the v3 strangler-fig has not yet
adopted. Listed in priority order (low effort first):

| # | File             | Line | Packet                                | Trigger                                 |
|---|------------------|-----:|---------------------------------------|-----------------------------------------|
| 1 | connection.cpp   |  259 | SERVER_ECHOREQ                        | server-initiated keepalive              |
| 2 | message.cpp      | 1861 | SERVER_MESSAGEBOX                     | message_send_text path                  |
| 3 | command.cpp      | 1544 | SERVER_FRIENDADD_ACK                  | /f a command                            |
| 4 | command.cpp      | 1655 | SERVER_FRIENDDEL_ACK                  | /f r command                            |
| 5 | command.cpp      | 1699 | SERVER_FRIENDMOVE_ACK (promote)       | /f promote                              |
| 6 | command.cpp      | 1742 | SERVER_FRIENDMOVE_ACK (demote)        | /f demote                               |
| 7 | anongame.cpp     |  441 | SERVER_ANONGAME_SEARCH_REPLY          | anongame matchmaker                     |
| 8 | connection.cpp   | 4264 | SERVER_READMEMORY                     | antihack probe                          |
| 9 | connection.cpp   | 4289 | SERVER_REQUIREDWORK                   | antihack probe                          |

### False positive

| File         | Line | Reason                                                                       |
|--------------|-----:|------------------------------------------------------------------------------|
| server.cpp   |  815 | Allocates a packet for INPUT (read buffer), not an outbound reply. Skip.     |

## Recommendation

Treat the 9 real gaps as candidates for future strangler-fig rounds. They
fall into 3 natural groupings:

- **R66 (suggested):** SERVER_ECHOREQ + SERVER_MESSAGEBOX
  (both are simple fixed-size headers; ECHOREQ matches d2dbs R64 pattern).
- **R67 (suggested):** the four friend-list acks in command.cpp
  (single struct family, can share a bridge or live in a friend_acks bridge).
- **R68 (suggested):** SERVER_ANONGAME_SEARCH_REPLY (anongame).
- **R69 (suggested):** the two antihack probes (READMEMORY, REQUIREDWORK).

No correctness regression risk from leaving them as-is: each site still
runs the existing legacy code; the strangler-fig contract is that
un-bridged sites fall back to legacy unchanged.
