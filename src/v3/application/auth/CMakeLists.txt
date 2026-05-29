# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer auth module CMakeLists.txt

add_library(pvpgn_application_auth)

target_sources(pvpgn_application_auth
  PRIVATE
    src/login_user.cpp
    src/login_user_nls.cpp
    src/permission_checker.cpp
    src/create_account.cpp
    src/logout_user.cpp
    src/change_password.cpp
    src/account_lock.cpp
    src/login_classifier.cpp
    src/password_rotation_observer.cpp
    src/lookup_account.cpp
    src/list_sessions.cpp
)

target_include_directories(pvpgn_application_auth
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_auth
  PUBLIC
    pvpgn_application_ports
    pvpgn_domain_identity
    pvpgn_core
  PRIVATE
    infra_crypto_nls
    application_chat
)
