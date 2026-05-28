# SPDX-License-Identifier: GPL-2.0-or-later

# In-memory persistence implementations
# Thread-safe repositories suitable for tests, development, and CI
# Note: Library is defined in parent CMakeLists.txt as STATIC with unit_of_work_factory.cpp

# R267: null/fake I/O adapters (header-only, included via the parent STATIC target)
# include/infra/inmemory/null_event_loop.hpp
# include/infra/inmemory/null_resolver.hpp
