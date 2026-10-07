#include "../metroflip_i.h"
#include <furi.h>
#include "../metroflip_plugins.h"
#include "../api/metroflip/metroflip_api.h"
#define TAG "Metroflip:Scene:Parse"
#include <stdio.h>

void metroflip_scene_parse_on_enter(void* context) {
    Metroflip* app = context;
    

    // Check if card_type is empty or unknown
    FURI_LOG_I(TAG, "test1");

    if(!app->card_type ||
   (app->card_type[0] == '\0') ||
   (strcmp(app->card_type, "unknown") == 0) ||
   (strcmp(app->card_type, "Unknown Card") == 0) ||
   (app->is_desfire && is_desfire_locked(app->card_type))) {
        FURI_LOG_I(TAG, "bad card");
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventWrongCard);
    } else {
        metroflip_plugin_manager_alloc(app);
        char path[128]; // Adjust size as needed
        // Dist layout first (non-embedded .fal deploys here), then the legacy
        // extracted-bundle layout for users upgrading with old assets.
        snprintf(
            path, sizeof(path), "/ext/apps_data/metroflip/plugins/%s_plugin.fal", app->card_type);

        FURI_LOG_I(TAG, "path %s", path);

        // Try loading the plugin
        if(plugin_manager_load_single(app->plugin_manager, path) != PluginManagerErrorNone) {
            snprintf(
                path,
                sizeof(path),
                "/ext/apps_assets/metroflip/plugins/%s_plugin.fal",
                app->card_type);
            FURI_LOG_I(TAG, "fallback path %s", path);
            if(plugin_manager_load_single(app->plugin_manager, path) != PluginManagerErrorNone) {
                FURI_LOG_E(TAG, "Failed to load parse plugin");
                plugin_manager_free(app->plugin_manager);
                composite_api_resolver_free(app->resolver);
                app->plugin_manager = NULL;
                app->resolver = NULL;
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, MetroflipCustomEventWrongCard);
                return;
            }
        }

        // Get and run the plugin's on_enter function
        const MetroflipPlugin* plugin = plugin_manager_get_ep(app->plugin_manager, 0);
        if(!plugin) {
            FURI_LOG_E(TAG, "Parse plugin has no entry point");
            plugin_manager_free(app->plugin_manager);
            composite_api_resolver_free(app->resolver);
            app->plugin_manager = NULL;
            app->resolver = NULL;
            view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventWrongCard);
            return;
        }
        plugin->plugin_on_enter(app);
    }
}

bool metroflip_scene_parse_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventWrongCard) {
            scene_manager_next_scene(app->scene_manager, MetroflipSceneUnknown);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        return true;
    }

    if(!app->plugin_manager) {
        return false;
    }
    // Get and run the plugin's on_event function
    const MetroflipPlugin* plugin = plugin_manager_get_ep(app->plugin_manager, 0);
    if(!plugin) {
        return false;
    }
    return plugin->plugin_on_event(app, event);
}

void metroflip_scene_parse_on_exit(void* context) {
    Metroflip* app = context;
    if(!(!app->card_type ||
   (app->card_type[0] == '\0') ||
   (strcmp(app->card_type, "unknown") == 0) ||
   (strcmp(app->card_type, "Unknown Card") == 0) ||
    (app->is_desfire && is_desfire_locked(app->card_type)))) {
        // Plugin may have failed to load in on_enter (already freed there).
        if(app->plugin_manager) {
            // Get and run the plugin's on_exit function
            const MetroflipPlugin* plugin = plugin_manager_get_ep(app->plugin_manager, 0);
            if(plugin) {
                plugin->plugin_on_exit(app);
            }

            plugin_manager_free(app->plugin_manager);
            app->plugin_manager = NULL;
        }
        if(app->resolver) {
            composite_api_resolver_free(app->resolver);
            app->resolver = NULL;
        }
        app->is_desfire = false;
    }
    app->data_loaded = false;
}
