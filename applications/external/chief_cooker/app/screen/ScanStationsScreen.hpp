#pragma once

#include "SettingsScreen.hpp"
#include "lib/hardware/subghz/data/SubGhzReceivedDataStub.hpp"
#include "lib/ui/view/ColumnOrientedListUiView.hpp"

#include "EditPagerScreen.hpp"
#include "PagerActionsScreen.hpp"

#include "lib/hardware/subghz/SubGhzModule.hpp"

#include "app/AppConfig.hpp"
#include "app/AppNotifications.hpp"
#include "app/pager/PagerReceiver.hpp"

static int8_t stationScreenColumnOffsets[]{
    3, // station name (if known)
    3, // hex
    49, // station
    72, // pager
    94, // action
    128 - 8 // repeats / edit flag
};
static Font stationScreenColumnFonts[]{
    FontSecondary, // station name (if known)
    FontBatteryPercent, // hex
    FontSecondary, // station
    FontSecondary, // pager
    FontBatteryPercent, // action
    FontBatteryPercent, // repeats
};

static Align stationScreenColumnAlignments[]{
    AlignLeft, // station name (if known)
    AlignLeft, // hex
    AlignCenter, // station
    AlignCenter, // pager
    AlignCenter, // action
    AlignRight, // repeats
};

class ScanStationsScreen {
private:
    AppConfig* config;
    ColumnOrientedListUiView* menuView;
    PagerReceiver* pagerReceiver;
    SubGhzModule* subghz;
    bool receiveMode = false;
    bool updateUserCategory = true;
    int scanForMoreButtonIndex = -1;
    uint32_t fromFilePagersCount = 0;

public:
    ScanStationsScreen(AppConfig* config) : ScanStationsScreen(config, true, NotSelected, NULL) {
    }

    ScanStationsScreen(AppConfig* config, CategoryType categoryType, const char* category) :
            ScanStationsScreen(config, false, categoryType, category) {
    }

    ScanStationsScreen(AppConfig* config, bool receiveNew, CategoryType categoryType, const char* category) {
        this->config = config;

        menuView = new ColumnOrientedListUiView(
            stationScreenColumnOffsets,
            sizeof(stationScreenColumnOffsets),
            HANDLER_3ARG(&ScanStationsScreen::getElementColumnName)
        );
        menuView->SetOnDestroyHandler(HANDLER(&ScanStationsScreen::destroy));
        menuView->SetOnReturnToViewHandler([this]() { this->menuView->Refresh(); });
        menuView->SetGoBackHandler(HANDLER(&ScanStationsScreen::goBack));

        menuView->SetColumnFonts(stationScreenColumnFonts);
        menuView->SetColumnAlignments(stationScreenColumnAlignments);

        menuView->SetLeftButton("Conf", HANDLER_1ARG(&ScanStationsScreen::showConfig));

        FURI_LOG_I("SCAN", "Step 1: creating PagerReceiver");
        pagerReceiver = new PagerReceiver(config);
        furi_check(pagerReceiver);
        FURI_LOG_I("SCAN", "Step 2: PagerReceiver created");
        if(categoryType == User) {
            pagerReceiver->SetUserCategory(category);
            updateUserCategory = false;

            if(category != NULL) {
                menuView->SetRightButton("Delete category", HANDLER_1ARG(&ScanStationsScreen::deleteCategory));
            }
        } else if(!receiveNew) {
            // Live scan doesn't need the saved-station DB up front; loading
            // it here fragments the heap right before the ~40KB RX burst.
            // Matching is unaffected (IsKnown safely returns false).
            FURI_LOG_I("SCAN", "Step 3: ReloadKnownStations");
            pagerReceiver->ReloadKnownStations();
            FURI_LOG_I("SCAN", "Step 4: ReloadKnownStations done");
        } else {
            FURI_LOG_I("SCAN", "Step 3-4: skipped ReloadKnownStations (live scan)");
        }

        if(!receiveNew) {
            pagerReceiver->LoadStationsFromDirectory(categoryType, category, HANDLER_1ARG(&ScanStationsScreen::pagerAdded));

            if(categoryType == User && menuView->GetElementsCount() > 0) {
                scanForMoreButtonIndex = menuView->GetElementsCount();
                fromFilePagersCount = menuView->GetElementsCount();
                menuView->AddElement();
            }
            menuView->SetNoElementCaption("No stations found!");
        }

        FURI_LOG_I("SCAN", "Step 5: creating SubGhzModule(%lu)", config->Frequency);
        subghz = new SubGhzModule(config->Frequency);
        furi_check(subghz);
        FURI_LOG_I("SCAN", "Step 6: setting receive handler");
        subghz->SetReceiveHandler(HANDLER_1ARG(&ScanStationsScreen::receive));

        if(!subghz->IsReady()) {
            // Global selection is External but the module is absent (or begin
            // failed): show it loudly instead of starting RX on nothing.
            menuView->SetNoElementCaption("External required!");
            FURI_LOG_E("SCAN", "Step 7: skipped, no radio device");
        } else if(receiveNew) {
            if(subghz->IsExternal()) {
                menuView->SetNoElementCaption("Receiving via EXT...");
            } else {
                menuView->SetNoElementCaption("Receiving...");
            }
            // RX starts in StartScan(), called after PushView: by then the
            // Loading popup is freed and construction is complete, so the
            // worker callback can never race ctor state.
            FURI_LOG_I("SCAN", "Step 7: RX deferred to StartScan()");
        }

        receiveMode = receiveNew;
        FURI_LOG_I("SCAN", "ScanStationsScreen constructor fully completed");
    }

    // Starts live RX. Must be called once, after PushView shows this view.
    void StartScan() {
        if(subghz == nullptr || !subghz->IsReady() || !receiveMode) {
            return;
        }
        FURI_LOG_I("SCAN", "Step 8: calling ReceiveAsync()");
        subghz->SetReceiveAfterTransmission(true);
        if(!subghz->ReceiveAsync()) {
            menuView->SetNoElementCaption("Low memory!");
            if(menuView->IsOnTop()) {
                menuView->Refresh();
            }
            FURI_LOG_E("SCAN", "Step 8: ReceiveAsync failed (low memory?)");
        } else {
            FURI_LOG_I("SCAN", "Step 8: ReceiveAsync() completed");
        }
    }

    UiView* GetView() {
        return menuView;
    }

private:
    void receive(SubGhzReceivedData* data) {
        if(pagerReceiver != nullptr) {
            pagerAdded(pagerReceiver->Receive(data));
        }
        delete data;
    }

    void pagerAdded(ReceivedPagerData* pagerData) {
        if(pagerData != NULL) {
            if(pagerData->IsNew()) {
                if(receiveMode) {
                    Notification::Play(&NOTIFICATION_PAGER_RECEIVE);
                }

                if(pagerData->GetIndex() == 0) { // add buttons after capturing the first transmission
                    menuView->SetCenterButton("Actions", HANDLER_1ARG(&ScanStationsScreen::showActions));
                    menuView->SetRightButton("Edit", HANDLER_1ARG(&ScanStationsScreen::editPagerMessage));
                }

                if(!receiveMode || scanForMoreButtonIndex == -1) {
                    menuView->AddElement();
                } else {
                    scanForMoreButtonIndex = -1;
                }
            }

            if(menuView->IsOnTop()) {
                menuView->Refresh();
            }

            delete pagerData;
        }
    }

    void getElementColumnName(int index, int column, String* str) {
        if(pagerReceiver == nullptr || str == nullptr) {
            return;
        }
        StoredPagerData* pagerData = pagerReceiver->PagerGetter(index)();
        if(pagerData == nullptr || pagerData->decoder >= PagerReceiver::decodersCount) {
            return;
        }
        PagerDecoder* decoder = pagerReceiver->decoders[pagerData->decoder];
        if(decoder == nullptr) {
            return;
        }
        String* name = pagerReceiver->GetName(pagerData);

        if(index == scanForMoreButtonIndex) {
            if(column == 0) {
                if(!receiveMode) {
                    str->format("> Scan here for more");
                } else {
                    str->format("Scanning...");
                }
            }
            return;
        }

        switch(column) {
        case 0: // station name
            if(name != NULL) {
                str->format("%s", name->cstr());
            }
            break;

        case 1: // hex
            if(name == NULL) {
                str->format("%06X", pagerData->data);
            }
            break;

        case 2: // station
            if(name == NULL) {
                str->format("%d", decoder->GetStation(pagerData->data));
            }
            break;

        case 3: // pager
            str->format("%d", decoder->GetPager(pagerData->data));
            break;

        case 4: // action
        {
            PagerAction action = decoder->GetAction(pagerData->data);
            if(action == UNKNOWN) {
                str->format("%d", decoder->GetActionValue(pagerData->data));
            } else {
                str->format("%.4s", PagerActions::GetDescription(action));
            }
        }; break;

        case 5: // repeats or edit flag
            if(pagerData->edited) {
                str->format("**");
            } else if(receiveMode) {
                str->format("x%d", pagerData->repeats);
            }
            break;

        default:
            break;
        }
    }

    void showConfig(uint32_t) {
        FURI_LOG_I("SCAN", "showConfig: 1 creating SettingsScreen");
        SettingsScreen* screen = new SettingsScreen(config, pagerReceiver, subghz, updateUserCategory);
        FURI_LOG_I("SCAN", "showConfig: 2 pushing SettingsScreen view");
        UiManager::GetInstance()->PushView(screen->GetView());
        FURI_LOG_I("SCAN", "showConfig: 3 completed");
    }

    void editPagerMessage(uint32_t index) {
        if((int)index == scanForMoreButtonIndex) {
            return;
        }
        if(pagerReceiver == nullptr || subghz == nullptr) {
            return;
        }

        PagerDataGetter getPager = pagerReceiver->PagerGetter(index);
        if(getPager() == nullptr) {
            return;
        }
        EditPagerScreen* screen = new EditPagerScreen(config, subghz, pagerReceiver, getPager, index < fromFilePagersCount);
        UiManager::GetInstance()->PushView(screen->GetView());
    }

    void showActions(uint32_t index) {
        if(subghz == nullptr || pagerReceiver == nullptr) {
            return;
        }
        if((int)index == scanForMoreButtonIndex) {
            if(!receiveMode) {
                subghz->SetReceiveAfterTransmission(true);
                if(!subghz->ReceiveAsync()) {
                    menuView->SetNoElementCaption("Low memory!");
                    if(menuView->IsOnTop()) {
                        menuView->Refresh();
                    }
                }

                receiveMode = true;
            }
            return;
        }

        PagerDataGetter getPager = pagerReceiver->PagerGetter(index);
        StoredPagerData* pagerData = getPager();
        if(pagerData == nullptr || pagerData->decoder >= PagerReceiver::decodersCount ||
           pagerData->protocol >= PagerReceiver::protocolsCount) {
            return;
        }
        PagerDecoder* decoder = pagerReceiver->decoders[pagerData->decoder];
        PagerProtocol* protocol = pagerReceiver->protocols[pagerData->protocol];
        if(decoder == nullptr || protocol == nullptr) {
            return;
        }

        PagerActionsScreen* screen = new PagerActionsScreen(config, getPager, decoder, protocol, subghz);
        UiManager::GetInstance()->PushView(screen->GetView());
    }

    bool goBack() {
        if(receiveMode && menuView->GetElementsCount() > 0) {
            DialogUiView* confirmGoBack = new DialogUiView("Really stop scan?", "You may loose captured signals");
            confirmGoBack->AddLeftButton("No");
            confirmGoBack->AddRightButton("Yes");
            confirmGoBack->SetResultHandler(HANDLER_1ARG(&ScanStationsScreen::goBackConfirmationHandler));
            UiManager::GetInstance()->PushView(confirmGoBack);
            return false;
        }
        return true;
    }

    void deleteCategory(int) {
        AppFileSysytem().DeleteCategory(pagerReceiver->GetCurrentUserCategory());
        menuView->SetRightButton("Deleted", NULL);
    }

    void goBackConfirmationHandler(DialogExResult dialogResult) {
        if(dialogResult == DialogExResultRight) {
            UiManager::GetInstance()->PopView(false);
        }
    }

    void destroy() {
        // Stop RX and unhook before deleting: the worker callback may still
        // be in flight when Back is pressed.
        // Delete pagerReceiver FIRST (matches reference) — it may hold
        // callbacks that reference subghz indirectly.
        if(pagerReceiver != nullptr) {
            delete pagerReceiver;
            pagerReceiver = nullptr;
        }
        if(subghz != nullptr) {
            subghz->StopReceive();
            subghz->SetReceiveHandler(nullptr);
            delete subghz;
            subghz = nullptr;
        }
        delete this;
    }
};
