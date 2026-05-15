# Repository implementations for Phase 2
# Header-only in-memory storage for accounts, channels, games, and realms.
# Thread-safe with reader-writer locks (std::shared_mutex).

pvpgn_v3_add_library(infra_storage_repository
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC_DEPS
        core
        application_ports
        domain_shared
        domain_identity
        domain_chat
        domain_gameplay
        domain_realm
)
