#include "settings.hpp"
#include "app.hpp"

FreeRoamSettings::FreeRoamSettings()
{
    // nothing to do
}

FreeRoamSettings::~FreeRoamSettings()
{
    free();
}

void FreeRoamSettings::settings_item_selected_callback(void *context, uint32_t index)
{
    FreeRoamSettings *settings = (FreeRoamSettings *)context;
    settings->settingsItemSelected(index);
}

uint32_t FreeRoamSettings::callback_to_submenu(void *context)
{
    UNUSED(context);
    return FreeRoamViewSubmenu;
}

uint32_t FreeRoamSettings::callback_to_settings(void *context)
{
    UNUSED(context);
    return FreeRoamViewSettings;
}

bool FreeRoamSettings::init(ViewDispatcher **view_dispatcher, void *appContext)
{
    view_dispatcher_ref = view_dispatcher;
    this->appContext = appContext;

    if (!easy_flipper_set_variable_item_list(&variable_item_list, FreeRoamViewSettings,
                                             settings_item_selected_callback, callback_to_submenu, view_dispatcher, this))
    {
        return false;
    }

    variable_item_wifi_ssid = variable_item_list_add(variable_item_list, "WiFi SSID", 1, nullptr, nullptr);
    variable_item_wifi_pass = variable_item_list_add(variable_item_list, "WiFi Password", 1, nullptr, nullptr);
    variable_item_user_name = variable_item_list_add(variable_item_list, "User Name", 1, nullptr, nullptr);
    variable_item_user_pass = variable_item_list_add(variable_item_list, "User Password", 1, nullptr, nullptr);
    variable_item_set_current_value_text(variable_item_wifi_ssid, "");
    variable_item_set_current_value_text(variable_item_wifi_pass, "");
    variable_item_set_current_value_text(variable_item_user_name, "");
    variable_item_set_current_value_text(variable_item_user_pass, "");

    return true;
}

void FreeRoamSettings::free()
{
    // Free text input first
    free_text_input();

    if (variable_item_list && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, FreeRoamViewSettings);
        variable_item_list_free(variable_item_list);
        variable_item_list = nullptr;
        variable_item_wifi_ssid = nullptr;
        variable_item_wifi_pass = nullptr;
    }
}

// Text input callback wrappers
void FreeRoamSettings::text_updated_ssid_callback(void *context)
{
    FreeRoamSettings *settings = (FreeRoamSettings *)context;
    settings->text_updated(TextInputWiFiSSID);
}

void FreeRoamSettings::text_updated_pass_callback(void *context)
{
    FreeRoamSettings *settings = (FreeRoamSettings *)context;
    settings->text_updated(TextInputWiFiPassword);
}

void FreeRoamSettings::text_updated_user_name_callback(void *context)
{
    FreeRoamSettings *settings = (FreeRoamSettings *)context;
    settings->text_updated(TextInputUserName);
}

void FreeRoamSettings::text_updated_user_pass_callback(void *context)
{
    FreeRoamSettings *settings = (FreeRoamSettings *)context;
    settings->text_updated(TextInputUserPassword);
}

void FreeRoamSettings::text_updated(uint32_t view)
{
    // store the entered text
    strncpy(text_input_buffer.get(), text_input_temp_buffer.get(), text_input_buffer_size);

    // Ensure null-termination
    text_input_buffer[text_input_buffer_size - 1] = '\0';

    // app context
    FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);

    switch (view)
    {
    case TextInputWiFiSSID:
        if (variable_item_wifi_ssid)
        {
            variable_item_set_current_value_text(variable_item_wifi_ssid, text_input_buffer.get());
        }
        app->save_char("wifi_ssid", text_input_buffer.get());
        break;
    case TextInputWiFiPassword:
        if (variable_item_wifi_pass)
        {
            variable_item_set_current_value_text(variable_item_wifi_pass, text_input_buffer.get());
        }
        app->save_char("wifi_pass", text_input_buffer.get());
        break;
    case TextInputUserName:
        if (variable_item_user_name)
        {
            variable_item_set_current_value_text(variable_item_user_name, text_input_buffer.get());
        }
        app->save_char("user_name", text_input_buffer.get());
        break;
    case TextInputUserPassword:
        if (variable_item_user_pass)
        {
            variable_item_set_current_value_text(variable_item_user_pass, text_input_buffer.get());
        }
        app->save_char("user_pass", text_input_buffer.get());
        break;
    default:
        break;
    }

    // switch to the settings view
    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FreeRoamViewSettings);
    }
}

bool FreeRoamSettings::init_text_input(uint32_t view)
{
    // check if already initialized
    if (text_input_buffer || text_input_temp_buffer)
    {
        FURI_LOG_E(TAG, "init_text_input: already initialized");
        return false;
    }

    // init buffers
    text_input_buffer_size = 128;
    if (!easy_flipper_set_buffer(reinterpret_cast<char **>(&text_input_buffer), text_input_buffer_size))
    {
        return false;
    }
    if (!easy_flipper_set_buffer(reinterpret_cast<char **>(&text_input_temp_buffer), text_input_buffer_size))
    {
        return false;
    }

    // app context
    FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
    char loaded[256];

    if (view == TextInputWiFiSSID)
    {
        if (app->load_char("wifi_ssid", loaded, sizeof(loaded)))
        {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        }
        else
        {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
        return easy_flipper_set_text_input(&text_input, FreeRoamViewTextInput,
                                           "Enter SSID", text_input_temp_buffer.get(), text_input_buffer_size,
                                           text_updated_ssid_callback, callback_to_settings, view_dispatcher_ref, this);
    }
    else if (view == TextInputWiFiPassword)
    {
        if (app->load_char("wifi_pass", loaded, sizeof(loaded)))
        {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        }
        else
        {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
        return easy_flipper_set_text_input(&text_input, FreeRoamViewTextInput,
                                           "Enter Password", text_input_temp_buffer.get(), text_input_buffer_size,
                                           text_updated_pass_callback, callback_to_settings, view_dispatcher_ref, this);
    }
    else if (view == TextInputUserName)
    {
        if (app->load_char("user_name", loaded, sizeof(loaded)))
        {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        }
        else
        {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
        return easy_flipper_set_text_input(&text_input, FreeRoamViewTextInput,
                                           "Enter User Name", text_input_temp_buffer.get(), text_input_buffer_size,
                                           text_updated_user_name_callback, callback_to_settings, view_dispatcher_ref, this);
    }
    else if (view == TextInputUserPassword)
    {
        if (app->load_char("user_pass", loaded, sizeof(loaded)))
        {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        }
        else
        {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
        return easy_flipper_set_text_input(&text_input, FreeRoamViewTextInput,
                                           "Enter User Password", text_input_temp_buffer.get(), text_input_buffer_size,
                                           text_updated_user_pass_callback, callback_to_settings, view_dispatcher_ref, this);
    }
    return false;
}

void FreeRoamSettings::free_text_input()
{
    if (text_input && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, FreeRoamViewTextInput);
        text_input_free(text_input);
        text_input = nullptr;
    }
    text_input_buffer.reset();
    text_input_temp_buffer.reset();
}

bool FreeRoamSettings::start_text_input(uint32_t view)
{
    free_text_input();
    if (!init_text_input(view))
    {
        FURI_LOG_E(TAG, "Failed to initialize text input for view %lu", view);
        return false;
    }
    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FreeRoamViewTextInput);
        return true;
    }
    else
    {
        FURI_LOG_E(TAG, "View dispatcher reference is null or invalid");
        return false;
    }
}

void FreeRoamSettings::settingsItemSelected(uint32_t index)
{
    start_text_input(index);
}
