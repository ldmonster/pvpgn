// SPDX-License-Identifier: GPL-2.0-or-later
//
// Thin C++20 wrapper over pugixml.
//
// v3 XML document API — replaces direct pugixml usage in legacy code.
//
// The legacy code used pugixml directly (src/common/pugixml.h) in
// src/bnetd/i18n.cpp and a few other places.  This wrapper provides a
// clean, exception-free, std::optional-based API that fits the v3 style.
//
// ## Types
//
//   `XmlNode`     — wraps `pugi::xml_node`; value type, cheap to copy
//   `XmlDocument` — owns a parsed XML document; move-only
//
// ## XmlDocument factory functions (free functions)
//
//   `load_file(path)`   → `std::optional<XmlDocument>` — parse from file
//   `load_string(text)` → `std::optional<XmlDocument>` — parse from string
//
// ## XmlDocument member API
//
//   `root()` → `XmlNode` — the document root element
//
// ## XmlNode member API
//
//   `name()`                    → `std::string_view`
//   `text()`                    → `std::string_view`
//   `attribute(name)`           → `std::optional<std::string_view>`
//   `child(name)`               → `std::optional<XmlNode>`
//   `children()`                → range (iterable, yields `XmlNode`)
//   `children(name)`            → range (filtered by element name)
//
// ## Design decisions
//
//   - No exceptions — all error paths return `std::nullopt` or empty ranges.
//   - `[[nodiscard]]` throughout.
//   - `XmlDocument` is move-only (owns the `pugi::xml_document`).
//   - `XmlNode` is a lightweight value type wrapping `pugi::xml_node`.
//   - The `children()` range is a lazy view; it does not allocate.
//   - `children(name)` uses pugixml's named-child iterator for efficiency.

#pragma once

#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>

#include <pugixml.hpp>

namespace pvpgn::v3::infra::xml {

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

class XmlNode;
class XmlDocument;

// ---------------------------------------------------------------------------
// XmlChildRange — lazy range over all child elements
// ---------------------------------------------------------------------------

/// A lazy, non-owning range over the direct children of an `XmlNode`.
///
/// Supports range-for iteration.  Each element is an `XmlNode`.
class XmlChildRange {
public:
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = XmlNode;
        using difference_type   = std::ptrdiff_t;
        using pointer           = XmlNode*;
        using reference         = XmlNode;

        explicit iterator(pugi::xml_node node) noexcept : node_{node} {}

        [[nodiscard]] XmlNode operator*() const noexcept;

        iterator& operator++() noexcept {
            node_ = node_.next_sibling();
            return *this;
        }

        iterator operator++(int) noexcept {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        [[nodiscard]] bool operator==(const iterator& other) const noexcept {
            return node_ == other.node_;
        }

        [[nodiscard]] bool operator!=(const iterator& other) const noexcept {
            return !(*this == other);
        }

    private:
        pugi::xml_node node_;
    };

    explicit XmlChildRange(pugi::xml_node parent) noexcept
        : parent_{parent} {}

    [[nodiscard]] iterator begin() const noexcept {
        return iterator{parent_.first_child()};
    }

    [[nodiscard]] iterator end() const noexcept {
        return iterator{pugi::xml_node{}};
    }

private:
    pugi::xml_node parent_;
};

// ---------------------------------------------------------------------------
// XmlNamedChildRange — lazy range over children with a specific element name
// ---------------------------------------------------------------------------

/// A lazy, non-owning range over direct children whose element name matches.
///
/// Supports range-for iteration.  Each element is an `XmlNode`.
class XmlNamedChildRange {
public:
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = XmlNode;
        using difference_type   = std::ptrdiff_t;
        using pointer           = XmlNode*;
        using reference         = XmlNode;

        iterator(pugi::xml_node node, std::string_view name) noexcept
            : node_{node}, name_{name} {}

        [[nodiscard]] XmlNode operator*() const noexcept;

        iterator& operator++() noexcept {
            node_ = node_.next_sibling(name_.data());
            return *this;
        }

        iterator operator++(int) noexcept {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        [[nodiscard]] bool operator==(const iterator& other) const noexcept {
            return node_ == other.node_;
        }

        [[nodiscard]] bool operator!=(const iterator& other) const noexcept {
            return !(*this == other);
        }

    private:
        pugi::xml_node node_;
        std::string_view name_;
    };

    XmlNamedChildRange(pugi::xml_node parent, std::string_view name) noexcept
        : parent_{parent}, name_{name} {}

    [[nodiscard]] iterator begin() const noexcept {
        return iterator{parent_.child(name_.data()), name_};
    }

    [[nodiscard]] iterator end() const noexcept {
        return iterator{pugi::xml_node{}, name_};
    }

private:
    pugi::xml_node parent_;
    std::string_view name_;
};

// ---------------------------------------------------------------------------
// XmlNode — lightweight wrapper around pugi::xml_node
// ---------------------------------------------------------------------------

/// Wraps a `pugi::xml_node`.  Value type — cheap to copy.
///
/// All accessors are `[[nodiscard]]` and never throw.
class XmlNode {
public:
    explicit XmlNode(pugi::xml_node node) noexcept : node_{node} {}

    /// The element tag name (e.g. `"root"`, `"item"`).
    /// Returns an empty string_view for null nodes.
    [[nodiscard]] std::string_view name() const noexcept {
        return node_.name();
    }

    /// The text content of this element (the first text child node).
    /// Returns an empty string_view if there is no text content.
    [[nodiscard]] std::string_view text() const noexcept {
        return node_.text().get();
    }

    /// Look up an attribute by name.
    /// Returns `std::nullopt` if the attribute does not exist.
    [[nodiscard]] std::optional<std::string_view>
    attribute(std::string_view attr_name) const noexcept {
        auto a = node_.attribute(attr_name.data());
        if (!a) return std::nullopt;
        return std::string_view{a.value()};
    }

    /// Find the first direct child element with the given tag name.
    /// Returns `std::nullopt` if no such child exists.
    [[nodiscard]] std::optional<XmlNode>
    child(std::string_view child_name) const noexcept {
        auto c = node_.child(child_name.data());
        if (!c) return std::nullopt;
        return XmlNode{c};
    }

    /// A lazy range over all direct child elements.
    [[nodiscard]] XmlChildRange children() const noexcept {
        return XmlChildRange{node_};
    }

    /// A lazy range over direct child elements whose tag name matches `name`.
    [[nodiscard]] XmlNamedChildRange
    children(std::string_view child_name) const noexcept {
        return XmlNamedChildRange{node_, child_name};
    }

    /// Returns `true` if this node is non-null (i.e. valid).
    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(node_);
    }

    /// Expose the underlying pugixml node for advanced use.
    [[nodiscard]] pugi::xml_node raw() const noexcept { return node_; }

private:
    pugi::xml_node node_;
};

// ---------------------------------------------------------------------------
// Iterator operator* definitions (need XmlNode to be complete)
// ---------------------------------------------------------------------------

inline XmlNode XmlChildRange::iterator::operator*() const noexcept {
    return XmlNode{node_};
}

inline XmlNode XmlNamedChildRange::iterator::operator*() const noexcept {
    return XmlNode{node_};
}

// ---------------------------------------------------------------------------
// XmlDocument — owns a parsed XML document
// ---------------------------------------------------------------------------

/// Owns a parsed XML document.  Move-only.
///
/// Construct via the `load_file()` or `load_string()` free functions.
class XmlDocument {
public:
    XmlDocument() = default;

    // Move-only
    XmlDocument(const XmlDocument&)            = delete;
    XmlDocument& operator=(const XmlDocument&) = delete;
    XmlDocument(XmlDocument&&)                 = default;
    XmlDocument& operator=(XmlDocument&&)      = default;

    ~XmlDocument() = default;

    /// Returns the document root element.
    ///
    /// Note: `pugi::xml_document::document_element()` returns the single
    /// top-level element (skipping the XML declaration and processing
    /// instructions), which is what callers almost always want.
    [[nodiscard]] XmlNode root() const noexcept {
        return XmlNode{doc_->document_element()};
    }

    /// Returns `true` if the document was successfully parsed.
    [[nodiscard]] explicit operator bool() const noexcept {
        return doc_ != nullptr;
    }

private:
    // Private constructor — only the factory functions may create instances.
    explicit XmlDocument(std::unique_ptr<pugi::xml_document> doc) noexcept
        : doc_{std::move(doc)} {}

    friend std::optional<XmlDocument> load_file(const std::filesystem::path&) noexcept;
    friend std::optional<XmlDocument> load_string(std::string_view) noexcept;

    std::unique_ptr<pugi::xml_document> doc_;
};

// ---------------------------------------------------------------------------
// Factory functions
// ---------------------------------------------------------------------------

/// Parse an XML document from a file.
///
/// Returns `std::nullopt` if the file does not exist, cannot be read, or
/// contains malformed XML.  Never throws.
[[nodiscard]] inline std::optional<XmlDocument>
load_file(const std::filesystem::path& path) noexcept {
    auto doc = std::make_unique<pugi::xml_document>();
    auto result = doc->load_file(path.c_str());
    if (!result) return std::nullopt;
    return XmlDocument{std::move(doc)};
}

/// Parse an XML document from a string.
///
/// Returns `std::nullopt` if the input is malformed XML.  Never throws.
[[nodiscard]] inline std::optional<XmlDocument>
load_string(std::string_view text) noexcept {
    auto doc = std::make_unique<pugi::xml_document>();
    // pugixml's load_buffer takes a pointer + size; no null-terminator needed.
    auto result = doc->load_buffer(text.data(), text.size());
    if (!result) return std::nullopt;
    return XmlDocument{std::move(doc)};
}

} // namespace pvpgn::v3::infra::xml
