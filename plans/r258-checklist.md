# R258 — Missing Port Interfaces

## Status: ✅ COMPLETE

### New port headers created
- [x] mail_store.hpp — IMailStore (send, inbox, delete_message)
- [x] news_store.hpp — INewsStore (get_news, add_news)
- [x] icon_provider.hpp — IIconProvider (icon_for, raw_icon_data)
- [x] helpfile_source.hpp — IHelpfileSource (lookup, all_commands)
- [x] random_source.hpp — IRandomSource (next_uint, fill_bytes)
- [x] session_token_issuer.hpp — ISessionTokenIssuer (issue, validate, revoke)
- [x] message_broadcaster.hpp — IMessageBroadcaster (broadcast_to_channel, send_info, send_error)

### Build
- [x] ports/CMakeLists.txt updated
