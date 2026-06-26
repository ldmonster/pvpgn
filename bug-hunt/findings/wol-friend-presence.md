# WOL friend presence (DEFERRED — wave 57)

Status: confirmed divergence, deferred (needs cross-protocol delivery).

Observable (scratchpad/probe_wol_presence.py): with a mutual friendship, when a
user logs in/out via WOL, the oracle whispers that user's online friends "Your
friend X has entered/left <server>." (the watch system is cross-protocol — fires
on conn_set_account/conn_destroy regardless of connection protocol). v3 sends
nothing on a WOL login/logout.

v3 root cause: the presence helper BnetFsm::notify_friends_presence (wave 56) is
BNCS-only. WOL's wol_auth.cpp (login) and wol_fsm.cpp on_close (logout) never
invoke any presence path.

Why deferred (not a quick fix): correct delivery is PER-RECIPIENT protocol-
specific. notify_friends_presence encodes a BNCS SID_CHATEVENT and ships raw
bytes via message_router->broadcast. The WOL->BNCS-recipient case happens to work
with BNCS encoding, but sending those BNCS bytes to a WOL/IRC recipient would
corrupt that client's stream. v3's router is byte-oriented (not protocol-aware),
and the session registry carries no per-session protocol tag, so there is no
clean way today to encode the notice differently per recipient.

Fix shape: introduce a protocol-aware presence-delivery mechanism — e.g. a shared
application use-case notify_friend_presence(account, username, entered) that, for
each mutual+online friend, resolves that recipient's protocol and asks the
recipient's own egress/FSM to encode the chat/whisper natively (BNCS ChatEvent vs
IRC NOTICE/PRIVMSG). Then call it from both the BNCS and WOL login/disconnect
paths. (WOL already has the on_close hook from wave 51 for the logout side.)
