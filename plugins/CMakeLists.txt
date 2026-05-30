# Install plugin directories to ${LOCALSTATEDIR}/plugins/
# Each plugin is a self-contained directory with plugin.toml + Lua files.
# The 'native/' subdirectory (if present) contains C source only and is excluded.

set(PLUGIN_DIRS
    example-quiz
    quiz
    ghost
    antihack-starcraft
    extra-commands
)

foreach(PLUGIN ${PLUGIN_DIRS})
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/plugins/${PLUGIN}/"
        DESTINATION "${LOCALSTATEDIR}/plugins/${PLUGIN}"
        PATTERN "native" EXCLUDE
    )
endforeach()
