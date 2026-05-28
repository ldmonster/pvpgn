# R257 — CMakeLists Drift Fix & Missing Build Targets

## Status: ✅ COMPLETE

### Fixed CMakeLists drift
- [x] auth/CMakeLists.txt — added 6 missing .cpp sources
- [x] chat/CMakeLists.txt — added 7 missing .cpp sources
- [x] realm/CMakeLists.txt — added 6 missing .cpp sources

### Created missing CMakeLists.txt
- [x] ads/CMakeLists.txt
- [x] anongame_infoply/CMakeLists.txt
- [x] anongame_lobby/CMakeLists.txt
- [x] bnet_packet_pump/CMakeLists.txt
- [x] email_management/CMakeLists.txt
- [x] i18n/CMakeLists.txt
- [x] icon_table/CMakeLists.txt
- [x] init/CMakeLists.txt
- [x] profile/CMakeLists.txt
- [x] tournament/CMakeLists.txt

### Wired into parent CMakeLists
- [x] src/v3/application/CMakeLists.txt updated with new add_subdirectory() calls
