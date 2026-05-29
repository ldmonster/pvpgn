# infra/xml — thin C++20 wrapper over pugixml
#
# INTERFACE library: header-only from the consumer's perspective.
# Links pugixml::static so consumers get the pugixml headers transitively.

add_library(infra_xml INTERFACE)

target_include_directories(infra_xml INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(infra_xml INTERFACE cxx_std_20)

target_link_libraries(infra_xml INTERFACE pugixml::static)

add_library(pvpgn::v3::infra_xml ALIAS infra_xml)
