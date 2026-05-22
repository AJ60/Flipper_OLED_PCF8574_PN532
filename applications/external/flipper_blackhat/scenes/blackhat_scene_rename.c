#include "../blackhat_app_i.h"

void blackhat_text_input_callback(void* context)
{
    furi_assert(context);
    BlackhatApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, BlackhatEventTextInput
    );
}

void blackhat_scene_rename_on_enter(void* context)
{
    BlackhatApp* app = context;
    TextInput* text_input = app->text_input;

    text_input_set_result_callback(
        text_input,
        blackhat_text_input_callback,
        context,
        app->text_input_ch,
        ENTER_NAME_LENGTH,
        false
    );

    view_dispatcher_switch_to_view(
        app->view_dispatcher, BlackhatAppViewTextInput
    );
}

bool blackhat_scene_rename_on_event(void* context, SceneManagerEvent event)
{
    BlackhatApp* app = context;
    bool consumed = false;
    if (event.type == SceneManagerEventTypeCustom) {
        snprintf(
            app->text_store,
            sizeof(app->text_store),
            "%s '%s'\n",
            app->selected_tx_string,
            app->text_input_ch
        );

        blackhat_uart_tx(app->uart, app->text_store, strlen(app->text_store));

        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, BlackhatSceneStart
        );

        consumed = true;
    }
    return consumed;
}

void blackhat_scene_rename_on_exit(void* context)
{
    BlackhatApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
