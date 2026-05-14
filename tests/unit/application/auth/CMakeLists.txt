pvpgn_v3_add_test(test_application_auth_login_user
    SOURCES login_user_test.cpp
    DEPS    application_auth infra_inmemory)

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
