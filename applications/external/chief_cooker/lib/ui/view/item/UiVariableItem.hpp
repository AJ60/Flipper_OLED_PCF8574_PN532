#pragma once

#include <functional>

#include "gui/modules/variable_item_list.h"

using namespace std;

class UiVariableItem {
private:
    VariableItemList* varItemList = nullptr;
    uint8_t position = 0;
    const char* label;

    uint8_t selectedIndex;
    uint8_t valuesCount;

    function<const char*(uint8_t)> changeHandler;

    static void itemChangeCallback(VariableItem* item) {
        if(item == NULL) {
            return;
        }
        UiVariableItem* uiItem = (UiVariableItem*)variable_item_get_context(item);
        if(uiItem == NULL || !uiItem->changeHandler) {
            return;
        }
        uint8_t index = variable_item_get_current_value_index(item);
        const char* text = uiItem->changeHandler(index);
        variable_item_set_current_value_text(item, text ? text : "");
    }

public:
    UiVariableItem(const char* label, const char* staticValue) :
            UiVariableItem(label, [staticValue](uint8_t) { return staticValue; }) {
    }

    UiVariableItem(const char* label, function<const char*(uint8_t)> changeHandler) : UiVariableItem(label, 0, 1, changeHandler) {
    }

    UiVariableItem(const char* label, uint8_t selectedIndex, uint8_t valuesCount, function<const char*(uint8_t)> changeHandler) {
        this->label = label;
        this->selectedIndex = selectedIndex;
        this->valuesCount = valuesCount;
        this->changeHandler = changeHandler;
    }

    void AddTo(VariableItemList* varItemList, uint8_t pos) {
        this->varItemList = varItemList;
        this->position = pos;
        variable_item_list_add(varItemList, label, valuesCount, itemChangeCallback, this);
        Refresh();
    }

    void AddTo(VariableItemList* varItemList) {
        AddTo(varItemList, 0);
    }

    void SetSelectedItem(uint8_t selectedIndex, uint8_t valuesCount) {
        this->selectedIndex = selectedIndex;
        this->valuesCount = valuesCount;

        Refresh();
    }

    void Refresh() {
        if(varItemList == NULL) {
            return;
        }
        VariableItem* item = variable_item_list_get(varItemList, position);
        if(item == NULL) {
            return;
        }

        variable_item_set_values_count(item, valuesCount);
        variable_item_set_current_value_index(item, selectedIndex);
        itemChangeCallback(item);
    }

    bool Editable() {
        return valuesCount > 1;
    }

    ~UiVariableItem() {
    }
};
