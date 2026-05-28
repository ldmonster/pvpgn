# SPDX-License-Identifier: GPL-2.0-or-later

# In-memory persistence implementations
# Thread-safe repositories suitable for tests, development, and CI
# Note: Library is defined in parent CMakeLists.txt as STATIC with unit_of_work_factory.cpp

# R267: null/fake I/O adapters (header-only, included via the parent STATIC target)
# include/infra/inmemory/null_event_loop.hpp
# include/infra/inmemory/null_resolver.hpp

# R268: in-memory fakes for missing ports (header-only, included via the parent STATIC target)
# include/infra/inmemory/in_memory_team_repository.hpp
# include/infra/inmemory/in_memory_mail_store.hpp
# include/infra/inmemory/in_memory_news_store.hpp
# include/infra/inmemory/in_memory_helpfile_source.hpp
# include/infra/inmemory/in_memory_icon_provider.hpp
# include/infra/inmemory/in_memory_random_source.hpp
# include/infra/inmemory/in_memory_session_token_issuer.hpp
# include/infra/inmemory/in_memory_message_broadcaster.hpp
# include/infra/inmemory/in_memory_permission_checker.hpp
# include/infra/inmemory/in_memory_config_subscriber.hpp
# include/infra/inmemory/in_memory_channel_store.hpp
