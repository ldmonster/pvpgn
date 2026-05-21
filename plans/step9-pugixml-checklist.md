# Phase 1 Step 9: pugixml via FetchContent — Checklist

Created: 2026-05-21 (Round 118)
Last updated: 2026-05-21 (Round 119)

## Overview

Integrate pugixml as a v3 FetchContent dependency and create a clean C++20 wrapper
(`infra_xml`) that replaces direct use of the legacy bundled `src/common/pugixml.h`.

---

## 1. FetchContent Integration

| Task | Status | Notes |
|------|--------|-------|
| Add `PVPGN_V3_WITH_PUGIXML` option to `src/v3/CMakeLists.txt` | ✅ R118 | Alongside existing spdlog/tomlpp/ankerl options |
| `FetchContent_Declare(pugixml GIT_TAG v1.14 GIT_SHALLOW TRUE)` | ✅ R118 | In `src/v3/CMakeLists.txt` third-party section |
| `FetchContent_MakeAvailable(pugixml)` | ✅ R118 | |
| `pugixml::static` alias guard | ✅ R118 | pugixml exports `pugixml-static`; alias created if needed |
| `PUGIXML_BUILD_SHARED_LIBS OFF` cache override | ✅ R118 | Ensures static build |
| `set_target_properties(pugixml-static PROPERTIES SYSTEM TRUE)` | ✅ R118 | Suppresses warnings from pugixml headers |

---

## 2. `xml_document.hpp` Wrapper

File: `src/v3/infra/xml/include/infra/xml/xml_document.hpp`

| Feature | Status | Notes |
|---------|--------|-------|
| `XmlDocument` class (move-only, owns `pugi::xml_document`) | ✅ R118 | |
| `XmlNode` class (value type, wraps `pugi::xml_node`) | ✅ R118 | |
| `XmlChildRange` — lazy forward-iterator range over all children | ✅ R118 | |
| `XmlNamedChildRange` — lazy forward-iterator range filtered by name | ✅ R118 | |
| `load_file(std::filesystem::path)` → `std::optional<XmlDocument>` | ✅ R118 | `[[nodiscard]]`, no exceptions |
| `load_string(std::string_view)` → `std::optional<XmlDocument>` | ✅ R118 | `[[nodiscard]]`, no exceptions |
| `XmlDocument::root()` → `XmlNode` | ✅ R118 | Returns `document_element()` |
| `XmlNode::name()` → `std::string_view` | ✅ R118 | |
| `XmlNode::text()` → `std::string_view` | ✅ R118 | |
| `XmlNode::attribute(name)` → `std::optional<std::string_view>` | ✅ R118 | |
| `XmlNode::child(name)` → `std::optional<XmlNode>` | ✅ R118 | |
| `XmlNode::children()` → `XmlChildRange` | ✅ R118 | Lazy, no allocation |
| `XmlNode::children(name)` → `XmlNamedChildRange` | ✅ R118 | Lazy, filtered |
| `XmlNode::operator bool()` | ✅ R118 | |
| `XmlNode::raw()` → `pugi::xml_node` | ✅ R118 | Escape hatch for advanced use |
| Namespace: `pvpgn::v3::infra::xml` | ✅ R118 | |
| `[[nodiscard]]` throughout | ✅ R118 | |
| No exceptions — `std::optional` for all error paths | ✅ R118 | |
| C++20 (`-std=c++20`) | ✅ R118 | |

---

## 3. CMakeLists

| Task | Status | Notes |
|------|--------|-------|
| `src/v3/infra/xml/CMakeLists.txt` created | ✅ R118 | `infra_xml` INTERFACE target |
| `target_link_libraries(infra_xml INTERFACE pugixml::static)` | ✅ R118 | |
| `target_compile_features(infra_xml INTERFACE cxx_std_20)` | ✅ R118 | |
| `pvpgn::v3::infra_xml` alias | ✅ R118 | |
| `add_subdirectory(infra/xml)` in `src/v3/CMakeLists.txt` | ✅ R118 | Guarded by `if(PVPGN_V3_WITH_PUGIXML)` |

---

## 4. Unit Tests

File: `tests/unit/infra/xml/test_xml_document.cpp`
Target: `test_infra_xml_document`

| Test Case | Status |
|-----------|--------|
| `load_string` valid XML returns non-empty optional | ✅ R118 |
| `load_string` invalid XML (`<<<not xml>>>`) returns `nullopt` | ✅ R118 |
| `load_string` empty string returns `nullopt` | ✅ R118 |
| `load_string` whitespace-only returns `nullopt` | ✅ R118 |
| `load_file` non-existent path returns `nullopt` | ✅ R118 |
| `root().name()` returns correct element name | ✅ R118 |
| `root().name()` for nested XML returns outer element name | ✅ R118 |
| `root().text()` returns text content | ✅ R118 |
| `root().text()` does not crash on element-only root | ✅ R118 |
| `child().text()` returns correct text | ✅ R118 |
| `root().attribute()` finds existing attribute | ✅ R118 |
| `root().attribute()` finds numeric attribute as string | ✅ R118 |
| `root().attribute()` returns `nullopt` for nonexistent | ✅ R118 |
| Multiple attributes on same element | ✅ R118 |
| `root().child()` finds existing child element | ✅ R118 |
| `root().child()` returns `nullopt` for nonexistent | ✅ R118 |
| `root().child()` returns first matching child | ✅ R118 |
| Nested child access via chained `child()` | ✅ R118 |
| `root().children()` iterates all direct child elements | ✅ R118 |
| `root().children()` on empty element yields empty range | ✅ R118 |
| `root().children()` count matches expected | ✅ R118 |
| `root().children(name)` filters children by element name | ✅ R118 |
| `root().children(name)` with no matching children yields empty range | ✅ R118 |
| `root().children(name)` collects all matching elements | ✅ R118 |
| `XmlNode` bool conversion: valid node is truthy | ✅ R118 |
| `XmlNode` bool conversion: child result is truthy when found | ✅ R118 |
| `XmlDocument` is movable | ✅ R118 |
| XML with declaration parses correctly | ✅ R118 |
| XML with CDATA section parses correctly | ✅ R118 |
| Single-element XML with no children | ✅ R118 |

**Total: 30 test cases**

---

## 5. Dockerfile.v3

| Task | Status | Notes |
|------|--------|-------|
| `infra_xml` added to `cmake --build` target list | ✅ R118 | In v3-build stage |
| `test_infra_xml_document` added to `cmake --build` target list | ✅ R118 | In v3-build stage |
| `test_infra_xml_document` runner added to v3-test stage | ✅ R118 | After `test_infra_compat_process` |

---

## 6. Consumer Migration (Round 119)

### Consumer Classification

| File | Classification | Status | Notes |
|------|---------------|--------|-------|
| `src/bnetd/i18n.cpp` | Moderate | ✅ R119 | Uses read-only API + `find_child_by_attribute` (replaced with manual loop) |
| `src/bnetd/output.cpp` | N/A | ✅ R119 (no-op) | No pugixml usage found |
| `src/bnetd/ladder.cpp` | N/A | ✅ R119 (no-op) | No pugixml usage found |
| `src/d2dbs/d2ladder.cpp` | N/A | ✅ R119 (no-op) | No pugixml usage found |
| `src/bnetd/tracker.cpp` (bntrackd) | N/A | ✅ R119 (no-op) | No pugixml usage found |

### Migration Tasks

| Task | Status | Notes |
|------|--------|-------|
| Audit `src/bnetd/i18n.cpp` pugixml usage | ✅ R118 | Uses `pugi::xml_document`, `xml_node`, `xml_attribute` |
| Audit other consumers (`output.cpp`, `ladder.cpp`, `d2ladder.cpp`, `bntrackd`) | ✅ R119 | All have zero pugixml usage — no migration needed |
| Migrate `i18n.cpp` to use `pvpgn::v3::infra::xml::XmlDocument` | ✅ R119 | Done — `load_file()`, `children()`, manual `find_child_by_attribute` replacement |
| Remove `#include "common/pugixml.h"` from `i18n.cpp` | ✅ R119 | Replaced with `#include "infra/xml/xml_document.hpp"` |
| Add `infra_xml` guard to `src/bnetd/CMakeLists.txt` | ✅ R119 | `if(TARGET infra_xml) target_link_libraries(bnetd_legacy PUBLIC infra_xml) endif()` |
| Verify: `grep -r 'pugi::' src/bnetd/ src/d2cs/ src/d2dbs/` | ✅ R119 | Zero results — all consumers migrated |

### Key Migration Details for `i18n.cpp`

- `pugi::xml_document doc` (reused across loop) → `auto doc = xml::load_file(lang_filename)` (per-iteration `optional<XmlDocument>`)
- `doc.child("root")` → `doc->root().child("root")` returning `optional<XmlNode>`
- `node.child_value("x")` → `node.child("x")->text()` (returns `string_view`)
- `node.attribute("tag").as_string()` → `node->attribute("tag").value_or("")`
- `for (pugi::xml_node n = x.child("y"); n; n = n.next_sibling("y"))` → `for (auto n : x->children("y"))`
- `item_nodes.find_child_by_attribute("id", attr.value())` → manual loop over `item_nodes.children("item")` checking `n.attribute("id").value_or("") == refid`

---

## 7. Step 9 Completion Status

**Phase 1 Step 9: COMPLETE ✅ (Round 119)**

All pugixml consumers in `src/bnetd/`, `src/d2cs/`, `src/d2dbs/` have been audited.
The only real consumer (`i18n.cpp`) has been fully migrated to `pvpgn::v3::infra::xml`.

---

## Legacy Files to Eventually Delete

| File | Blocked By |
|------|-----------|
| `src/common/pugixml.h` | ✅ No remaining consumers — safe to delete in Step 10 |
| `src/common/pugixml.cpp` | ✅ No remaining consumers — safe to delete in Step 10 |
| `src/common/pugiconfig.h` | ✅ No remaining consumers — safe to delete in Step 10 |
