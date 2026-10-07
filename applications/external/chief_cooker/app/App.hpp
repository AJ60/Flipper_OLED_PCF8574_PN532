#pragma once

#include "lib/ui/UiManager.hpp"
#include "lib/hardware/notification/Notification.hpp"
#include "lib/hardware/subghz/FrequencyManager.hpp"

#include "AppConfig.hpp"
#include "app/screen/MainMenuScreen.hpp"

using namespace std;

class App {
public:
    void Run() {
        UiManager* ui = UiManager::GetInstance();
        furi_check(ui);
        ui->InitGui();

        FrequencyManager* frequencyManager = FrequencyManager::GetInstance();
        furi_check(frequencyManager);
        (void)frequencyManager;
        AppConfig* config = new AppConfig();
        furi_check(config);
        config->Load();

        MainMenuScreen* mainMenuScreen = new MainMenuScreen(config);
        furi_check(mainMenuScreen);
        furi_check(mainMenuScreen->GetView());
        ui->PushView(mainMenuScreen->GetView());
        ui->RunEventLoop();

        // All screens are owned by UiManager's view stack (PopView deletes
        // with preserveView=false); destroying the singletons releases any
        // remaining views. Must run before deleting config (screens borrow it).
        FrequencyManager::DestroyInstance();
        UiManager::DestroyInstance();
        delete config;

        Notification::Dispose();
    }
};
