# Snapshot lifetime safety — scoping note

> Written R166. Frames the residual lifetime hazard left by the R165
> atomic-shared_ptr swap, and proposes options. No code in R166.

## The hazard

Each `pvpgn_v3_<svc>_prefs_get_<field>()` accessor does:

```cpp
auto p = g_prefs.load();                          // local strong ref
return p ? p->field().c_str() : (default_);       // pointer into p
```

The returned `const char*` aliases an `std::string` owned by `*p`.
But `p` (the local `shared_ptr`) drops its reference the moment the
function returns. If no other strong reference exists when the next
SIGHUP swaps in a new snapshot, the old `std::string` is freed and
any caller still holding the `const char*` reads freed memory.

Today most callers use the pointer synchronously (`fopen(path)`,
`eventlog_set_logfile(path)`) -- the SIGHUP race window is small but
not zero.

## Why atomic alone does not fix it

`std::atomic<std::shared_ptr<...>>` makes the pointer swap and the
matching readers race-free in the C++ memory-model sense, but the
*payload* lifetime is governed by reference counting, not the atomic.
A reader holding only a raw `const char*` is invisible to the
ref-counter.

## Options ranked by invasiveness

### A. Status quo + comments (current — R165)

Each bridge documents the constraint. Callers are expected to use
the pointer immediately. Acceptable until a real bug appears.

### B. Per-call shared_ptr handle (small invasion)

Wrap each accessor's return in a small RAII handle:

```cpp
struct StringHandle {
    std::shared_ptr<const LegacyPrefs> snap;
    std::string_view view;
    operator const char*() const noexcept { return view.data(); }
};

StringHandle pvpgn_v3_prefs_get_filedir_h() noexcept {
    auto p = g_prefs.load();
    return p ? StringHandle{p, p->filedir()}
             : StringHandle{nullptr, ""};
}
```

Pros: lifetime safe by construction.
Cons: API change -- every caller updated; can't be `extern "C"`.

### C. Immortal strings (medium invasion)

Intern each unique config string in a process-global pool that
never deallocates. Snapshots store `string_view` into the pool.
Pool grows monotonically; SIGHUP appends, never frees.

Pros: `const char*` stays valid forever -- no API change.
Cons: monotonic memory growth; not safe for long-running daemons
with hot-reloaded configs.

### D. Caller-side copy (zero invasion at bridge)

Document that callers must `std::string copy = path;` immediately.
Audit each caller; convert any that store the pointer long-term.

Pros: keeps the bridge API stable.
Cons: ~200 caller sites to audit; easy to regress.

## Recommendation

D (caller-side copy) is the lowest-risk path for the legacy bnetd
binary which is being phased out anyway (see
`legacy-retirement-scope.md`). For `pvpgn_v3_bnetd` the migration
is a one-time refactor while v3 is still small.

When `bnetd_legacy` is gone (Phase 3 closeout), option B becomes
trivially feasible because all callers are v3 C++ code.

## What we will do this round (R166)

Nothing in code -- this scoping note + a clear comment in each
bridge. The hazard is documented; tackling it is gated on Phase 3.
