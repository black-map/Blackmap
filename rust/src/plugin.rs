//! Plugin system for BlackMap
//!
//! Allows dynamic loading of scanning modules and analyzers

use std::path::Path;
use libloading::{Library, Symbol};
use tracing::{info, warn};
use std::sync::Arc;
use tokio::sync::RwLock;

/// Plugin trait for extending BlackMap
pub trait Plugin: Send + Sync {
    /// Plugin name
    fn name(&self) -> &str;

    /// Plugin version
    fn version(&self) -> &str;

    /// Plugin description
    fn description(&self) -> &str;

    /// Called when a port is found open
    fn on_port_open(&self, _host: &str, _port: u16) {}

    /// Called when a service is detected
    fn on_service_detected(&self, _host: &str, _port: u16, _service: &str) {}

    /// Called when scan completes
    fn on_scan_complete(&self) {}
}

/// Type signature for the plugin constructor exported by dynamic libraries
pub type PluginCreateFn = unsafe extern "C" fn() -> *mut dyn Plugin;

/// Plugin manager
pub struct PluginManager {
    plugins: Vec<Box<dyn Plugin>>,
    // We must keep libraries alive as long as we use their plugins
    libraries: Vec<Arc<Library>>,
}

impl PluginManager {
    /// Create a new plugin manager
    pub fn new() -> Self {
        Self {
            plugins: Vec::new(),
            libraries: Vec::new(),
        }
    }

    /// Register a plugin statically
    pub fn register(&mut self, plugin: Box<dyn Plugin>) {
        info!("Registered static plugin: {} v{}", plugin.name(), plugin.version());
        self.plugins.push(plugin);
    }

    /// Dynamically load a plugin from a shared library (.so, .dll, .dylib)
    pub unsafe fn load_plugin<P: AsRef<Path>>(&mut self, path: P) -> Result<(), Box<dyn std::error::Error>> {
        let lib = Arc::new(Library::new(path.as_ref())?);
        
        // Expected symbol exported by the plugin: `_plugin_create`
        let constructor: Symbol<PluginCreateFn> = lib.get(b"_plugin_create\0")?;
        
        let plugin_ptr = constructor();
        if plugin_ptr.is_null() {
            return Err("Plugin constructor returned null".into());
        }
        
        let plugin = Box::from_raw(plugin_ptr);
        info!("Loaded dynamic plugin: {} v{}", plugin.name(), plugin.version());
        
        self.plugins.push(plugin);
        self.libraries.push(lib);
        
        Ok(())
    }

    /// Trigger port open event
    pub fn on_port_open(&self, host: &str, port: u16) {
        for plugin in &self.plugins {
            plugin.on_port_open(host, port);
        }
    }

    /// Trigger service detected event
    pub fn on_service_detected(&self, host: &str, port: u16, service: &str) {
        for plugin in &self.plugins {
            plugin.on_service_detected(host, port, service);
        }
    }

    /// Trigger scan complete event
    pub fn on_scan_complete(&self) {
        for plugin in &self.plugins {
            plugin.on_scan_complete();
        }
    }
}

impl Default for PluginManager {
    fn default() -> Self {
        Self::new()
    }
}

// A macro to help plugin authors export their plugin
#[macro_export]
macro_rules! export_plugin {
    ($plugin_type:ty) => {
        #[no_mangle]
        pub unsafe extern "C" fn _plugin_create() -> *mut dyn $crate::plugin::Plugin {
            let plugin = Box::new(<$plugin_type>::default());
            Box::into_raw(plugin)
        }
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestPlugin;

    impl Plugin for TestPlugin {
        fn name(&self) -> &str {
            "Test Plugin"
        }

        fn version(&self) -> &str {
            "1.0.0"
        }

        fn description(&self) -> &str {
            "A test plugin"
        }
    }

    #[test]
    fn test_plugin_manager() {
        let mut manager = PluginManager::new();
        let plugin = Box::new(TestPlugin);
        manager.register(plugin);
        assert_eq!(manager.plugins.len(), 1);
    }
}
