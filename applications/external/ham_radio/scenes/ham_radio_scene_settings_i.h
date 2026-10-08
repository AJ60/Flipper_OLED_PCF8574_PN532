#pragma once

#include "../ham_radio_app_i.h"

/** Rebuild the settings list from app->settings. */
void ham_radio_scene_settings_refresh(HamRadioApp* app);

/** Variable item list callback shared by every settings row. */
void ham_radio_scene_settings_callback(void* context, uint32_t index);

/* The SettingsValue scene handlers are declared by ham_radio_scene.h through the
 * ADD_SCENE macro, so they are not repeated here. */
