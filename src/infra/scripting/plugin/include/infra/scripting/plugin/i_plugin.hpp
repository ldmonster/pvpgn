#pragma once

#include <memory>
#include <string>
#include "plugin_manifest.hpp"
#include "capability.hpp"

namespace pvpgn::infra::scripting {

// Forward declarations
class PluginContext;

/// Base interface for all plugins (Lua and native C++)
class IPlugin {
public:
    virtual ~IPlugin() = default;
    
    /// Get plugin manifest
    virtual const PluginManifest& manifest() const = 0;
    
    /// Initialize plugin with context
    /// Called after plugin is loaded
    virtual void init(PluginContext& context) = 0;
    
    /// Shutdown plugin
    /// Called before plugin is unloaded
    virtual void shutdown() = 0;
    
    /// Get capabilities required by this plugin
    virtual CapabilitySet required_capabilities() const = 0;
    
    /// Check if plugin is ready
    virtual bool is_ready() const = 0;
};

/// Plugin context passed to init()
/// Provides access to DI container filtered by capabilities
class PluginContext {
public:
    virtual ~PluginContext() = default;
    
    /// Get the manifest
    virtual const PluginManifest& manifest() const = 0;
    
    /// Get granted capabilities
    virtual const CapabilitySet& capabilities() const = 0;
    
    /// Check if a capability is granted
    virtual bool has_capability(Capability cap) const = 0;
    
    /// Get a service from the DI container (if capability allows)
    /// Returns nullptr if capability is not granted or service not found
    template<typename T>
    T* get_service() {
        return static_cast<T*>(get_service_impl(typeid(T)));
    }
    
protected:
    virtual void* get_service_impl(const std::type_info& type) = 0;
};

/// Factory function for native plugins
/// Implemented by native plugin .so/.dll
extern "C" {
    IPlugin* pvpgn_plugin_create();
    void pvpgn_plugin_destroy(IPlugin* plugin);
}

} // namespace pvpgn::infra::scripting
