pvpgn_v3_add_test(test_application_auth_login_user
    SOURCES login_user_test.cpp
    DEPS    application_auth infra_inmemory)

# Plan 08: transparent password-at-rest upgrade policy (plaintext-bearing auth)
pvpgn_v3_add_test(test_application_auth_password_upgrade
    SOURCES password_upgrade_test.cpp
    DEPS    application_auth)

pvpgn_v3_add_test(test_application_auth_login_classifier
    SOURCES login_classifier_test.cpp
    DEPS    application_auth)

pvpgn_v3_add_test(test_application_auth_login_user_must_change_password
    SOURCES login_user_must_change_password_test.cpp
    DEPS    application_auth)

pvpgn_v3_add_test(test_application_auth_password_rotation_observer
    SOURCES password_rotation_observer_test.cpp
    DEPS    application_auth infra_inmemory)

pvpgn_v3_add_test(test_application_auth_change_password
    SOURCES change_password_test.cpp
    DEPS    application_auth infra_inmemory)

pvpgn_v3_add_test(test_application_auth_create_account
    SOURCES create_account_test.cpp
    DEPS    application_auth infra_inmemory)

pvpgn_v3_add_test(test_application_auth_logout_user
    SOURCES logout_user_test.cpp
    DEPS    application_auth infra_inmemory)

pvpgn_v3_add_test(test_application_auth_account_lock
    SOURCES account_lock_test.cpp
    DEPS    application_auth infra_inmemory)

pvpgn_v3_add_test(test_application_auth_lookup_account
    SOURCES lookup_account_test.cpp
    DEPS    application_auth infra_inmemory)

pvpgn_v3_add_test(test_application_auth_list_sessions
    SOURCES list_sessions_test.cpp
    DEPS    application_auth infra_inmemory)

pvpgn_v3_add_test(test_application_auth_login_user_nls
    SOURCES login_user_nls_test.cpp
    DEPS    application_auth infra_crypto_nls infra_crypto_nls_adapter)

pvpgn_v3_add_test(test_application_auth_permission_checker
    SOURCES permission_checker_test.cpp
    DEPS    application_auth infra_inmemory)
