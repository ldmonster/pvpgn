#!/usr/bin/env python3
"""Plan 05 finalization: eliminate src/application/ports/.

- Category A (domain-owned, aliased by the facade): rewrite
  `application::ports::X` -> `domain::<ctx>::X`.
- Category B (metrics shim): `application::ports::X` -> `core::X`.
- Category C (genuinely-application ports defined in application/ports/):
  relocated to domain/shared/ports/; keep the `application::ports` namespace,
  only the include path changes.
- Category D (defined elsewhere, already `application::ports`): unchanged
  namespace; consumers just need the defining header instead of the facade.

Then every `#include "application/ports/ports.hpp"` /
`".../metrics_registry.hpp"` is replaced by the specific headers the file
actually needs, and the relocated cat-C include paths are rewritten.
"""
import os
import re
import sys

SRC_ROOTS = ["src", "tests"]

# type -> (new_namespace_or_None, header)
CAT_A = {
    "IAccountRepository": ("domain::identity", "domain/identity/ports.hpp"),
    "IPasswordHasher": ("domain::identity", "domain/identity/ports.hpp"),
    "ISessionRegistry": ("domain::identity", "domain/identity/ports.hpp"),
    "ISessionTokenIssuer": ("domain::identity", "domain/identity/ports.hpp"),
    "IConnectionEgress": ("domain::connection", "domain/connection/ports.hpp"),
    "IMessageRouter": ("domain::connection", "domain/connection/ports.hpp"),
    "IConnectionHandler": ("domain::connection", "domain/connection/ports.hpp"),
    "IPermissionChecker": ("domain::moderation", "domain/moderation/ports.hpp"),
    "IAccountBanRepository": ("domain::moderation", "domain/moderation/ports.hpp"),
    "IIpBanRepository": ("domain::moderation", "domain/moderation/ports.hpp"),
    "Permission": ("domain::moderation", "domain/moderation/ports.hpp"),
    "AccountBan": ("domain::moderation", "domain/moderation/ports.hpp"),
    "AuditAction": ("domain::moderation", "domain/moderation/ports.hpp"),
    "AuditEntry": ("domain::moderation", "domain/moderation/ports.hpp"),
    "IAuditLog": ("domain::moderation", "domain/moderation/ports.hpp"),
    "IChannelRepository": ("domain::chat", "domain/chat/ports.hpp"),
    "IChannelStore": ("domain::chat", "domain/chat/ports.hpp"),
    "ChannelDefinition": ("domain::chat", "domain/chat/ports.hpp"),
    "IHelpfileSource": ("domain::chat", "domain/chat/ports.hpp"),
    "IMessageBroadcaster": ("domain::chat", "domain/chat/ports.hpp"),
    "IGameRepository": ("domain::gameplay", "domain/gameplay/ports.hpp"),
    "IFriendListRepository": ("domain::social", "domain/social/ports.hpp"),
    "IClanRepository": ("domain::social", "domain/social/ports.hpp"),
    "ITeamRepository": ("domain::social", "domain/social/ports.hpp"),
    "IMailStore": ("domain::social", "domain/social/ports.hpp"),
    "MailMessage": ("domain::social", "domain/social/ports.hpp"),
    "ILadderRepository": ("domain::ladder", "domain/ladder/ports.hpp"),
    "IAnonGameCompressor": ("domain::matchmaking", "domain/matchmaking/ports.hpp"),
    "IRealmRepository": ("domain::realm", "domain/realm/ports.hpp"),
}
CAT_B = {
    "IMetricsRegistry": ("core", "core/metrics.hpp"),
    "ICounter": ("core", "core/metrics.hpp"),
    "IGauge": ("core", "core/metrics.hpp"),
    "IHistogram": ("core", "core/metrics.hpp"),
    "MetricLabels": ("core", "core/metrics.hpp"),
}
# cat C: namespace stays application::ports, header relocates
CAT_C = {
    "IEventLoop": "domain/shared/ports/event_loop.hpp",
    "ITraceSink": "domain/shared/ports/trace_sink.hpp",
    "IResolver": "domain/shared/ports/resolver.hpp",
    "ResolvedAddress": "domain/shared/ports/resolver.hpp",
    "IRandomSource": "domain/shared/ports/random_source.hpp",
    "IIconProvider": "domain/shared/ports/icon_provider.hpp",
}
# cat D: namespace stays application::ports, defining header
CAT_D = {
    "IEventBus": "domain/shared/event_bus.hpp",
    "SubscriptionId": "domain/shared/event_bus.hpp",
    "EventHandler": "domain/shared/event_bus.hpp",
    "ICommandRegistry": "domain/chat/ports/command_registry.hpp",
    "INewsStore": "domain/social/ports/news_store.hpp",
    "NewsItem": "domain/social/ports/news_store.hpp",
    "IUnitOfWork": "application/persistence/unit_of_work.hpp",
    "IUnitOfWorkFactory": "application/persistence/unit_of_work_factory.hpp",
    "UnitOfWorkGuard": "application/persistence/unit_of_work.hpp",
    "IConfigSubscriber": "domain/shared/ports/config_subscriber.hpp",
}

NS_MAP = {}
HDR_MAP = {}
for t, (ns, h) in {**CAT_A, **CAT_B}.items():
    NS_MAP[t] = ns
    HDR_MAP[t] = h
for t, h in CAT_C.items():
    HDR_MAP[t] = h  # ns unchanged
for t, h in CAT_D.items():
    HDR_MAP[t] = h  # ns unchanged

ALL_TYPES = set(HDR_MAP)

FACADE_INCLUDES = {
    "application/ports/ports.hpp",
    "application/ports/metrics_registry.hpp",
}
CAT_C_OLD = {
    "application/ports/event_loop.hpp": "domain/shared/ports/event_loop.hpp",
    "application/ports/trace_sink.hpp": "domain/shared/ports/trace_sink.hpp",
    "application/ports/resolver.hpp": "domain/shared/ports/resolver.hpp",
    "application/ports/random_source.hpp": "domain/shared/ports/random_source.hpp",
    "application/ports/icon_provider.hpp": "domain/shared/ports/icon_provider.hpp",
}

inc_re = re.compile(r'^\s*#\s*include\s*"([^"]+)"\s*$')


def process(path):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    orig = text

    # which application::ports::TYPE appear
    used = set(re.findall(r'application::ports::([A-Za-z_][A-Za-z0-9_]*)', text))

    # 1) namespace rewrites for cat A/B
    for t in used:
        ns = NS_MAP.get(t)
        if ns:
            text = re.sub(r'application::ports::' + re.escape(t) + r'\b',
                          ns + '::' + t, text)

    # 2) rewrite relocated cat-C include paths
    for old, new in CAT_C_OLD.items():
        text = text.replace('"%s"' % old, '"%s"' % new)

    # 3) facade include replacement
    lines = text.split("\n")
    has_facade = any(
        (m := inc_re.match(ln)) and m.group(1) in FACADE_INCLUDES for ln in lines
    )
    # headers this file needs for every application::ports type it referenced
    needed = set()
    for t in used:
        h = HDR_MAP.get(t)
        if h:
            needed.add(h)
    # don't self-include
    selfhdr = None
    for h in list(needed):
        if path.replace("\\", "/").endswith(h):
            selfhdr = h
    if selfhdr:
        needed.discard(selfhdr)

    out = []
    inserted = False
    existing_incs = {m.group(1) for ln in lines if (m := inc_re.match(ln))}
    for ln in lines:
        m = inc_re.match(ln)
        if m and m.group(1) in FACADE_INCLUDES:
            if not inserted:
                for h in sorted(needed):
                    if h not in existing_incs:
                        out.append('#include "%s"' % h)
                inserted = True
            continue  # drop the facade include
        out.append(ln)
    text2 = "\n".join(out)

    # if file used types but had NO facade include and is now missing headers,
    # append needed headers after the first include block.
    if not has_facade and needed:
        miss = [h for h in sorted(needed) if h not in existing_incs]
        if miss:
            o2 = []
            done = False
            for i, ln in enumerate(out):
                o2.append(ln)
                if not done and inc_re.match(ln):
                    nxt = out[i + 1] if i + 1 < len(out) else ""
                    if not inc_re.match(nxt):
                        for h in miss:
                            o2.append('#include "%s"' % h)
                        done = True
            if not done:  # no includes at all; put after pragma once / first line
                o2 = []
                for i, ln in enumerate(out):
                    o2.append(ln)
                    if i == 0:
                        for h in miss:
                            o2.append('#include "%s"' % h)
                done = True
            text2 = "\n".join(o2)

    if text2 != orig:
        with open(path, "w", encoding="utf-8") as f:
            f.write(text2)
        return True
    return False


def main():
    changed = 0
    for root in SRC_ROOTS:
        for dirpath, _dirs, files in os.walk(root):
            if "/application/ports" in dirpath.replace("\\", "/"):
                continue  # skip the dir being deleted
            for fn in files:
                if fn.endswith((".cpp", ".hpp", ".h", ".cc", ".cxx")):
                    p = os.path.join(dirpath, fn)
                    if process(p):
                        changed += 1
    print("files changed:", changed)


if __name__ == "__main__":
    main()
