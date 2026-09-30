#pragma once

#include "SelectCategoryScreen.hpp"
#include "app/AppConfig.hpp"
#include "app/pager/PagerReceiver.hpp"
#include "lib/String.hpp"
#include "lib/hardware/subghz/SubGhzModule.hpp"
#include "lib/ui/UiManager.hpp"
#include "lib/ui/view/VariableItemListUiView.hpp"

class SettingsScreen {
private:
    AppConfig* config;
    SubGhzModule* subghz;
    PagerReceiver* receiver;
    VariableItemListUiView* varItemList;

    UiVariableItem* currentCategoryItem = nullptr;
    UiVariableItem* frequencyItem = nullptr;
    UiVariableItem* maxPagerItem = nullptr;
    UiVariableItem* signalRepeatItem = nullptr;
    UiVariableItem* ignoreSavedItem = nullptr;
    UiVariableItem* autosaveFoundItem = nullptr;
    UiVariableItem* debugModeItem = nullptr;

    String frequencyStr;
    String maxPagerStr;
    String signalRepeatStr;
    bool updateUserCategory;
    uint32_t categoryItemIndex;

public:
    SettingsScreen(AppConfig* config, PagerReceiver* receiver, SubGhzModule* subghz, bool updateUserCategory) {
        FURI_LOG_I("SETTINGS", "SettingsScreen constructor begin");
        this->config = config;
        this->receiver = receiver;
        this->subghz = subghz;
        this->updateUserCategory = updateUserCategory;

        varItemList = new VariableItemListUiView();
        varItemList->SetOnDestroyHandler(HANDLER(&SettingsScreen::destroy));
        varItemList->SetEnterPressHandler(HANDLER_1ARG(&SettingsScreen::enterPressHandler));

        FURI_LOG_I("SETTINGS", "Adding Category item");
        categoryItemIndex = varItemList->AddItem(
            currentCategoryItem = new UiVariableItem("Category", HANDLER_1ARG(&SettingsScreen::categoryChangedHandler))
        );

        FURI_LOG_I("SETTINGS", "Adding Frequency item");
        varItemList->AddItem(
            frequencyItem = new UiVariableItem(
                "Scan frequency",
                FrequencyManager::GetInstance()->GetFrequencyIndex(config->Frequency),
                FrequencyManager::GetInstance()->GetFrequencyCount(),
                [this](uint8_t val) {
                    uint32_t freq = this->config->Frequency = FrequencyManager::GetInstance()->GetFrequency(val);
                    this->subghz->SetReceiveFrequency(this->config->Frequency);
                    return frequencyStr.format("%lu.%02lu", freq / 1000000, (freq % 1000000) / 10000);
                }
            )
        );

        FURI_LOG_I("SETTINGS", "Adding Max pager item");
        uint8_t maxPagerIndex = config->MaxPagerForBatchOrDetection > 0 ? (config->MaxPagerForBatchOrDetection - 1) : 0;
        varItemList->AddItem(
            maxPagerItem = new UiVariableItem(
                "Max pager value",
                maxPagerIndex,
                UINT8_MAX,
                [this](uint8_t val) {
                    this->config->MaxPagerForBatchOrDetection = val + 1;
                    return maxPagerStr.fromInt(this->config->MaxPagerForBatchOrDetection);
                }
            )
        );

        FURI_LOG_I("SETTINGS", "Adding Signal repeats item");
        uint8_t repeatsIndex = config->SignalRepeats > 0 ? (config->SignalRepeats - 1) : 0;
        varItemList->AddItem(
            signalRepeatItem = new UiVariableItem(
                "Times to repeat signal",
                repeatsIndex,
                UINT8_MAX,
                [this](uint8_t val) {
                    this->config->SignalRepeats = val + 1;
                    return signalRepeatStr.fromInt(this->config->SignalRepeats);
                }
            )
        );

        FURI_LOG_I("SETTINGS", "Adding Saved stations item");
        uint8_t savedStrategyIdx = config->SavedStrategy < SavedStationStrategyValuesCount ? config->SavedStrategy : 0;
        varItemList->AddItem(
            ignoreSavedItem = new UiVariableItem(
                "Saved stations",
                savedStrategyIdx,
                SavedStationStrategyValuesCount,
                [this](uint8_t val) {
                    this->config->SavedStrategy = (val < SavedStationStrategyValuesCount) ?
                        static_cast<enum SavedStationStrategy>(val) : SHOW_NAME;
                    return savedStationsStrategy(this->config->SavedStrategy);
                }
            )
        );

        FURI_LOG_I("SETTINGS", "Adding Autosave item");
        varItemList->AddItem(
            autosaveFoundItem = new UiVariableItem(
                "Autosave found signals",
                config->AutosaveFoundSignals ? 1 : 0,
                2,
                [this](uint8_t val) {
                    this->config->AutosaveFoundSignals = (val != 0);
                    return boolOption(val);
                }
            )
        );
        FURI_LOG_I("SETTINGS", "SettingsScreen constructor end");
    }

    UiView* GetView() {
        return varItemList;
    }

private:
    void enterPressHandler(uint32_t index) {
        if(index != categoryItemIndex) {
            return;
        }
        UiManager::GetInstance()->PushView(
            (new SelectCategoryScreen(false, User, HANDLER_2ARG(&SettingsScreen::categorySelected)))->GetView()
        );
    }

    void categorySelected(CategoryType, const char* category) {
        if(config->CurrentUserCategory != NULL) {
            delete config->CurrentUserCategory;
        }
        config->CurrentUserCategory = category != NULL ? new String("%s", category) : NULL;
        UiManager::GetInstance()->PopView(false);
        currentCategoryItem->Refresh();
    }

    const char* categoryChangedHandler(uint8_t) {
        const char* category = config->GetCurrentUserCategoryCstr();
        if(category == NULL) {
            category = "Default";
        }
        return category;
    }

    const char* boolOption(uint8_t value) {
        return value ? "ON" : "OFF";
    }

    const char* savedStationsStrategy(SavedStationStrategy value) {
        switch(value) {
        case IGNORE:
            return "Ignore";

        case SHOW_NAME:
            return "Show name";

        case HIDE:
            return "Hide";

        default:
            return "Ignore";
        }
    }

    void destroy() {
        config->Save();
        if(updateUserCategory) {
            receiver->SetUserCategory(config->CurrentUserCategory);
            receiver->ReloadKnownStations();
        }

        if(currentCategoryItem != nullptr) delete currentCategoryItem;
        if(frequencyItem != nullptr) delete frequencyItem;
        if(maxPagerItem != nullptr) delete maxPagerItem;
        if(signalRepeatItem != nullptr) delete signalRepeatItem;
        if(ignoreSavedItem != nullptr) delete ignoreSavedItem;
        if(autosaveFoundItem != nullptr) delete autosaveFoundItem;
        if(debugModeItem != nullptr) delete debugModeItem;

        delete this;
    }
};
