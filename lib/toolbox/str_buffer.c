#include "str_buffer.h"

const char* str_buffer_make_owned_clone(StrBuffer* buffer, const char* str) {
    if(!buffer || !str) return NULL;
    char* owned = strdup(str);
    if(!owned) return NULL;
    char** new_strings =
        realloc(buffer->owned_strings, (buffer->n_owned_strings + 1) * sizeof(char*));
    if(!new_strings) {
        free(owned);
        return NULL;
    }
    buffer->owned_strings = new_strings;
    buffer->owned_strings[buffer->n_owned_strings] = owned;
    buffer->n_owned_strings++;
    return owned;
}

void str_buffer_clear_all_clones(StrBuffer* buffer) {
    if(!buffer) return;
    for(size_t i = 0; i < buffer->n_owned_strings; i++) {
        free(buffer->owned_strings[i]);
    }
    free(buffer->owned_strings);
    buffer->owned_strings = NULL;
    buffer->n_owned_strings = 0;
}
