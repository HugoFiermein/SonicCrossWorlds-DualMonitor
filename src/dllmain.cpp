#include "stdafx.h"
#include "version.h"
#include "helper.hpp"

#ifdef _DEBUG
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#else
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <inipp/inipp.h>
#include <safetyhook.hpp>

#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/UMG_classes.hpp"

#include "SDK/WBP_Race_HUD_Player01_classes.hpp"
#include "SDK/WBP_Race_HUD_Player02_classes.hpp"
#include "SDK/WBP_Race_HUD_Sub_Aming_classes.hpp"
#include "SDK/WBP_Ready_M2_classes.hpp"
#include "SDK/WBP_Ready_Sub_CharaWindow_P1P3_classes.hpp"
#include "SDK/WBP_Ready_Sub_CharaWindow_P2P4_classes.hpp"
#include "SDK/WBP_Ready_Sub_Gadget_classes.hpp"
#include "SDK/WBP_Window_MachineParameter_classes.hpp"
#include "SDK/WBP_PauseMenu_classes.hpp"
#include "SDK/WBP_Popup_Window_classes.hpp"
#include "SDK/WBP_Result_GP_Player0204_classes.hpp"
#include "SDK/WBP_Ready_Sub_Decide_classes.hpp"
#include "SDK/WBP_Ready_Sub_StandByText_classes.hpp"
#include "SDK/WBP_CMN_Sub_BoxBtn_classes.hpp"
#include "SDK/WBP_CMN_PlayerNumber_Big_classes.hpp"
#include "SDK/WBP_FooterMenu_classes.hpp"
#include "SDK/WBP_GadgetCustom_Sub_GadgetPlate_classes.hpp"

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Ini
inipp::Ini<char> ini;
std::string sConfigFile = sFixName + ".ini";
std::filesystem::path FixPath;

// Logger
std::shared_ptr<spdlog::logger> logger;
std::string sLogFile = sFixName + ".log";
std::filesystem::path ExePath;
std::string sExeName;

// Aspect ratio / FOV / HUD
std::pair<int, int> DesktopDimensions = { 0, 0 };
constexpr float fPi = std::numbers::pi_v<float>;
constexpr float fNativeAspect = 16.0f / 9.0f;
float fAspectRatio = 16.0f / 9.0f;
float fAspectMultiplier = 1.0f;
float fHUDWidth = 1920.0f;
float fHUDWidthOffset = 0.0f;
float fHUDHeight = 1080.0f;
float fHUDHeightOffset = 0.0f;

// Ini settings
bool bFixAspect = false;
bool bFixHUD = false;
bool bSpanRaceHUD = false;
bool bDualMonitor2P = true;
bool bHideCenterLine = true;
bool bMirror2PMenus = true;
bool bIs2PlayerRace = false;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;
SDK::UObject* WidgetObject = nullptr;
std::string sWidgetName;

template<typename T>
T GetConfigAndLog(inipp::Ini<char>& ini, const std::string& section, const std::string& key, T defaultValue)
{
    T value = defaultValue;
    inipp::get_value(ini.sections[section], key, value);
    SPDLOG_INFO("Config Parse: [{}]: {} = {}", section, key, value);
    return value;
}

void CalculateAspectRatio(bool bLog)
{
    if (iCurrentResX <= 0 || iCurrentResY <= 0)
        return;

    fAspectRatio = static_cast<float>(iCurrentResX) / iCurrentResY;
    fAspectMultiplier = fAspectRatio / fNativeAspect;

    fHUDWidth = static_cast<float>(iCurrentResY) * fNativeAspect;
    fHUDHeight = static_cast<float>(iCurrentResY);
    fHUDWidthOffset = (static_cast<float>(iCurrentResX) - fHUDWidth) / 2.0f;
    fHUDHeightOffset = 0.0f;

    if (fAspectRatio < fNativeAspect) {
        fHUDWidth = static_cast<float>(iCurrentResX);
        fHUDHeight = static_cast<float>(iCurrentResX) / fNativeAspect;
        fHUDWidthOffset = 0.0f;
        fHUDHeightOffset = (static_cast<float>(iCurrentResY) - fHUDHeight) / 2.0f;
    }

    if (bLog) {
        SPDLOG_INFO("----------");
        SPDLOG_INFO("Current Resolution: Resolution: {:d}x{:d}", iCurrentResX, iCurrentResY);
        SPDLOG_INFO("Current Resolution: fAspectRatio: {}", fAspectRatio);
        SPDLOG_INFO("Current Resolution: fAspectMultiplier: {}", fAspectMultiplier);
        SPDLOG_INFO("Current Resolution: fHUDWidth: {}", fHUDWidth);
        SPDLOG_INFO("Current Resolution: fHUDHeight: {}", fHUDHeight);
        SPDLOG_INFO("Current Resolution: fHUDWidthOffset: {}", fHUDWidthOffset);
        SPDLOG_INFO("Current Resolution: fHUDHeightOffset: {}", fHUDHeightOffset);
        SPDLOG_INFO("----------");
    }
}

void Logging()
{
    // Get path to DLL
    WCHAR dllPath[_MAX_PATH] = {0};
    GetModuleFileNameW(thisModule, dllPath, MAX_PATH);
    FixPath = dllPath;
    FixPath = FixPath.remove_filename();

    // Get game name and exe path
    WCHAR gameExePath[_MAX_PATH] = {0};
    GetModuleFileNameW(exeModule, gameExePath, MAX_PATH);
    ExePath = gameExePath;
    sExeName = ExePath.filename().string();
    ExePath = ExePath.remove_filename();

    // spdlog initialisation
    try
    {
        std::ofstream file(ExePath / sLogFile, std::ios::trunc);
        if (file.is_open()) file.close();

        logger = std::make_shared<spdlog::logger>(sFixName, std::make_shared<spdlog::sinks::rotating_file_sink_st>((ExePath / sLogFile).string(), 10 * 1024 * 1024, 1));
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
        spdlog::flush_on(spdlog::level::debug);
        spdlog::set_level(spdlog::level::debug); 
        
        SPDLOG_INFO("----------");
        SPDLOG_INFO("{:s} v{:s}", sFixName, sFixVersion);
        SPDLOG_INFO("----------");
        SPDLOG_INFO("Log File: {}", (FixPath / sLogFile).string());
        SPDLOG_INFO("----------");
        SPDLOG_INFO("Module Name: {:s}", sExeName);
        SPDLOG_INFO("Module Path: {}", ExePath.string());
        SPDLOG_INFO("Module Address: 0x{:x}", reinterpret_cast<uintptr_t>(exeModule));
        SPDLOG_INFO("Module Timestamp: {:d}", Memory::GetModuleTimestamp(exeModule));
        SPDLOG_INFO("----------");
    }
    catch (const spdlog::spdlog_ex &ex) {
        Util::ConsoleExit(std::string("Log initialisation failed: ") + ex.what(), thisModule);
    }
}

void Configuration()
{
    // Inipp initialisation
    std::ifstream iniFile(FixPath / sConfigFile);
    if (!iniFile) {
        spdlog::shutdown(); // Flush log
        Util::ConsoleExit("Could not locate config file.\nMake sure " + sConfigFile + " is located in " + FixPath.string() + sConfigFile, thisModule);
    }
    
    SPDLOG_INFO("Config File: {}", (FixPath / sConfigFile).string());
    ini.parse(iniFile);
    ini.strip_trailing_comments();
    
    // Read ini
    bFixAspect = GetConfigAndLog(ini, "Fix Aspect Ratio", "Enabled", false);
    bFixHUD = GetConfigAndLog(ini, "Fix HUD", "Enabled", false);
    bSpanRaceHUD = GetConfigAndLog(ini, "Span Racing HUD", "Enabled", false);
    bDualMonitor2P = GetConfigAndLog(ini, "Dual Monitor Splitscreen", "Enabled", true);
    bHideCenterLine = GetConfigAndLog(ini, "Dual Monitor Splitscreen", "HideCenterDividerLine", true);
    bMirror2PMenus = GetConfigAndLog(ini, "Dual Monitor Splitscreen", "Mirror2PMenus", true);
    
    SPDLOG_INFO("----------");
}

void UpdateOffsets()
{
    // GObjects
    std::uint8_t* GObjectsScanResult = Memory::PatternScan(exeModule, "48 8B ?? ?? ?? ?? ?? 48 8B ?? ?? 48 8D ?? ?? EB ?? 33 ?? 8B ?? ?? C1 ??");
    if (GObjectsScanResult) {
        SPDLOG_INFO("Offsets: GObjects: Address is {:s}+{:x}", sExeName, GObjectsScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
        std::uint8_t* GObjectsAddr = Memory::GetRelativeAddr(GObjectsScanResult + 0x3);
        SDK::Offsets::GObjects = static_cast<UC::uint32>(GObjectsAddr - reinterpret_cast<std::uint8_t*>(exeModule));
        SPDLOG_INFO("Offsets: GObjects: 0x{:x}", SDK::Offsets::GObjects);
    }
    else {
        SPDLOG_ERROR("Offsets: GObjects: Pattern scan failed.");
    }

    // AppendString
    std::uint8_t* AppendStringScanResult = Memory::PatternScan(exeModule, "48 8D ?? ?? ?? 48 89 ?? ?? ?? E8 ?? ?? ?? ?? 83 ?? ?? ?? 00 48 8D ?? ?? ?? ?? ?? 41 ?? 01 00 00 00 48 8B ??");
    if (AppendStringScanResult) {
        SPDLOG_INFO("Offsets: AppendString: Address is {:s}+{:x}", sExeName, AppendStringScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
        std::uint8_t* AppendStringAddr = Memory::GetRelativeAddr(AppendStringScanResult + 0xB);
        SDK::Offsets::AppendString = static_cast<UC::uint32>(AppendStringAddr - reinterpret_cast<std::uint8_t*>(exeModule));
        SPDLOG_INFO("Offsets: AppendString: 0x{:x}", SDK::Offsets::AppendString);
    }
    else {
        SPDLOG_ERROR("Offsets: AppendString: Pattern scan failed.");
    }

    // ProcessEvent
    std::uint8_t* ProcessEventScanResult = Memory::PatternScan(exeModule, "40 ?? 56 57 41 ?? 41 ?? 41 ?? 41 ?? 48 81 ?? ?? ?? ?? ?? 48 8D ?? ?? ?? 48 89 ?? ?? ?? ?? ?? 48 8B ?? ?? ?? ?? ?? 48 33 ?? 48 89 ?? ?? ?? ?? ?? 4D 8B ??");
    if (ProcessEventScanResult) {
        SPDLOG_INFO("Offsets: ProcessEvent: Address is {:s}+{:x}", sExeName, ProcessEventScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
        SDK::Offsets::ProcessEvent = static_cast<UC::uint32>(ProcessEventScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
        SPDLOG_INFO("Offsets: ProcessEvent: 0x{:x}", SDK::Offsets::ProcessEvent);
    }
    else {
        SPDLOG_ERROR("Offsets: ProcessEvent: Pattern scan failed.");
    }

    SPDLOG_INFO("----------");
}

void CurrentResolution() 
{
    // Get current resolution
    std::uint8_t* CurrentResolutionScanResult = Memory::PatternScan(exeModule, "89 ?? ?? ?? 48 85 ?? 0F 84 ?? ?? ?? ?? 48 8B ?? ?? ?? ?? ?? 48 85 ?? 4C 8D ?? ??");
    if (CurrentResolutionScanResult) {
        SPDLOG_INFO("Current Resolution: Address is {:s}+{:x}", sExeName.c_str(), CurrentResolutionScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
        static SafetyHookMid CurrentResolutionMidHook{};
        CurrentResolutionMidHook = safetyhook::create_mid(CurrentResolutionScanResult,
            [](SafetyHookContext& ctx) {
                if (!ctx.rdx) return;

                int ResX = *reinterpret_cast<int*>(ctx.rdx + 0xA0);
                int ResY = *reinterpret_cast<int*>(ctx.rdx + 0xA4);
  
                if (iCurrentResX != ResX || iCurrentResY != ResY) {
                    iCurrentResX = ResX;
                    iCurrentResY = ResY;
                    CalculateAspectRatio(true);
                }
            });
    }
    else {
        SPDLOG_ERROR("Current Resolution: Pattern scan failed.");
    }
}

void AspectRatioFOV()
{
    if (bFixAspect) 
    {
        // AspectRatioAxisConstraint
        std::uint8_t* AspectRatioAxisConstraintScanResult = Memory::PatternScan(exeModule, "48 ?? ?? ?? ?? 00 00 4C ?? ?? 4D ?? ?? E8 ?? ?? ?? ?? 48 8B ?? ?? ?? ?? ??");
        if (AspectRatioAxisConstraintScanResult) {
            SPDLOG_INFO("Aspect Ratio: FOV Axis Constraint: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioAxisConstraintScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid AspectRatioAxisConstraintMidHook{};
            AspectRatioAxisConstraintMidHook = safetyhook::create_mid(AspectRatioAxisConstraintScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.rdx = 0;
                });
        }
        else {
            SPDLOG_ERROR("Aspect Ratio: FOV Axis Constraint: Pattern scan failed.");
        } 

        // bConstrainAspectRatio
        std::uint8_t* ConstrainAspectRatioScanResult = Memory::PatternScan(exeModule, "89 ?? ?? 0F B6 ?? ?? ?? ?? ?? D1 ?? 33 ?? 83 ?? 02 33 ?? 89 ?? ??");
        if (ConstrainAspectRatioScanResult) {
            SPDLOG_INFO("Aspect Ratio: Constrain Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), ConstrainAspectRatioScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid ConstrainAspectRatioMidHook{};
            ConstrainAspectRatioMidHook = safetyhook::create_mid(ConstrainAspectRatioScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.rcx = 0;
                });
        }
        else {
            SPDLOG_ERROR("Aspect Ratio: Constrain Aspect Ratio: Pattern scan failed.");
        }   
    }
}

void HUD()
{
    if (bFixHUD) 
    {
        // Fix racing position markers for single-player ultrawide only.
        // DO NOT hook for Dual Monitor 2P mode, as native UE split-screen already has 16:9 viewports per screen!
        if (!bDualMonitor2P) {
            std::uint8_t* ProjectWorldToScreenScanResult = Memory::PatternScan(exeModule, "48 ?? ?? 20 F2 0F ?? ?? 66 0F ?? ?? F3 0F ?? ?? F2 0F ?? ?? F2 0F ?? ?? ?? F2 0F ?? ?? F2 0F ?? ?? ?? 84 ??");
            if (ProjectWorldToScreenScanResult) {
                SPDLOG_INFO("HUD: Project World To Screen: Address is {:s}+{:x}", sExeName.c_str(), ProjectWorldToScreenScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
                static SafetyHookMid ProjectWorldToScreenWidthMidHook{};
                ProjectWorldToScreenWidthMidHook = safetyhook::create_mid(ProjectWorldToScreenScanResult,
                    [](SafetyHookContext& ctx) {
                        if (!bSpanRaceHUD && fAspectRatio > fNativeAspect)
                            ctx.xmm0.f64[0] = fHUDWidthOffset;
                    });

                static SafetyHookMid ProjectWorldToScreenHeightMidHook{};
                ProjectWorldToScreenHeightMidHook = safetyhook::create_mid(ProjectWorldToScreenScanResult + 0x10,
                    [](SafetyHookContext& ctx) {
                        if (!bSpanRaceHUD && fAspectRatio < fNativeAspect)
                            ctx.xmm0.f64[0] = fHUDHeightOffset;
                    });
            }
            else {
                SPDLOG_ERROR("HUD: Project World To Screen: Pattern scan failed.");
            }
        }

        // Fades
        std::uint8_t* FadesScanResult = Memory::PatternScan(exeModule, "49 8B ?? 41 0F ?? ?? 0F ?? ?? 48 8B ?? 48 85 ?? 0F 84 ?? ?? ?? ?? 8B ?? ?? C1 ?? ?? F6 ??");
        if (FadesScanResult) {
            SPDLOG_INFO("HUD: Fades: Address is {:s}+{:x}", sExeName.c_str(), FadesScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid FadesMidHook{};
            FadesMidHook = safetyhook::create_mid(FadesScanResult,
                [](SafetyHookContext& ctx) {
                    if (!ctx.rax) return;

                    auto transition = reinterpret_cast<SDK::UUserWidget*>(ctx.rax);
                    if (transition->WidgetTree && transition->WidgetTree->RootWidget) {
                        auto panel = static_cast<SDK::UCanvasPanel*>(transition->WidgetTree->RootWidget);
                        auto panelSlot = static_cast<SDK::UCanvasPanelSlot*>(panel->Slots[0]);

                        if (fAspectRatio > fNativeAspect)
                            panelSlot->SetOffsets(SDK::FMargin(0.0f, 0.0f, 2160.0f * fAspectRatio, 2160.0f));
                        else
                            panelSlot->SetOffsets(SDK::FMargin(0.0f, 0.0f, 3840.0f, 3840.0f / fAspectRatio));
                    }
                });
        }
        else {
            SPDLOG_ERROR("HUD: Fades: Pattern scan failed.");
        }
    }

    if (bFixHUD || bSpanRaceHUD) 
    {
        // Popup Window
        std::uint8_t* CreatePopupWindowScanResult = Memory::PatternScan(exeModule, "49 8B ?? ?? 48 89 ?? ?? 41 ?? ?? ?? 00 7E ?? 49 ?? ?? ?? 41 0F ?? ?? E8 ?? ?? ?? ?? 48 8B ?? ?? 48 85 ?? 74 ??");
        if (CreatePopupWindowScanResult) {
            SPDLOG_INFO("HUD: Create Popup Window: Address is {:s}+{:x}", sExeName.c_str(), CreatePopupWindowScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid CreatePopupWindowMidHook{};
            CreatePopupWindowMidHook = safetyhook::create_mid(CreatePopupWindowScanResult,
                [](SafetyHookContext& ctx) {
                    if (!ctx.rsi) return;

                    auto widget = reinterpret_cast<SDK::UUserWidget*>(ctx.rsi);

                    if (widget->IsA(SDK::UWBP_Popup_Window_C::StaticClass())) {
                        auto popupWidget = static_cast<SDK::UWBP_Popup_Window_C*>(widget);
                        auto panel = static_cast<SDK::UCanvasPanel*>(popupWidget->WidgetTree->RootWidget);      

                        float widthOffset = (2160.0f * fAspectRatio - 3840.0f) / 2.0f;
                        float heightOffset = (3840.0f / fAspectRatio - 2160.0f) / 2.0f;

                        if (bFixHUD && panel->Slots.IsValidIndex(2)) {
                            auto backgroundBlurSlot = static_cast<SDK::UCanvasPanelSlot*>(panel->Slots[2]);

                            if (bFixHUD) {
                                if (fAspectRatio > fNativeAspect)
                                    backgroundBlurSlot->SetOffsets(SDK::FMargin(-1920.0f - widthOffset, -1080.0f, 2160.0f * fAspectRatio, 2160.0f));
                                else
                                    backgroundBlurSlot->SetOffsets(SDK::FMargin(-1920.0f, -1080.0f - heightOffset, 3840.0f, 3840.0f / fAspectRatio));
                            }

                            if (bSpanRaceHUD) {
                                if (fAspectRatio > fNativeAspect)
                                    backgroundBlurSlot->SetOffsets(SDK::FMargin(-1920.0f, -1080.0f, 2160.0f * fAspectRatio, 2160.0f));
                                else
                                    backgroundBlurSlot->SetOffsets(SDK::FMargin(-1920.0f, -1080.0f, 3840.0f, 3840.0f / fAspectRatio));
                            }
                        }
                        
                        if (bSpanRaceHUD && panel->Slots.IsValidIndex(3)) {
                            auto safeZone = static_cast<SDK::USafeZone*>(panel->Slots[3]->Content);
                            auto panel = static_cast<SDK::UCanvasPanel*>(safeZone->Slots[0]->Content);
                            auto panelSlot = static_cast<SDK::UCanvasPanelSlot*>(panel->Slots[0]);

                            if (fAspectRatio > fNativeAspect)
                                panelSlot->SetOffsets(SDK::FMargin(widthOffset, 0.0f, 3200.0f, 1000.0f));
                            else
                                panelSlot->SetOffsets(SDK::FMargin(0.0f, heightOffset, 3200.0f, 1000.0f));
                        }
                    }
                });
        }
        else {
            SPDLOG_ERROR("HUD: Create Popup Window: Pattern scan failed.");
        }

        // Widgets
        std::uint8_t* UWidgetAddToViewportScanResult = Memory::PatternScan(exeModule, "48 83 ?? ?? 4D 8B ?? 45 33 ?? E8 ?? ?? ?? ?? 48 83 ?? ?? C3");
        if (UWidgetAddToViewportScanResult) {
            SPDLOG_INFO("HUD: Widgets: Address is {:s}+{:x}", sExeName.c_str(), UWidgetAddToViewportScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid UWidgetAddToViewportMidHook{};
            UWidgetAddToViewportMidHook = safetyhook::create_mid(UWidgetAddToViewportScanResult,
                [](SafetyHookContext& ctx) {
                    if (!ctx.rdx) return;

                    auto obj = reinterpret_cast<SDK::UObject*>(ctx.rdx);

                    if (!obj->Class || !obj->Class->Class) 
                        return;

                    if (WidgetObject != obj) {
                        WidgetObject = obj;
                        sWidgetName = WidgetObject->GetName();
                        SPDLOG_INFO("HUD: Widgets: {} @ 0x{:x}", sWidgetName, reinterpret_cast<uintptr_t>(WidgetObject));
                    }

                    float widthOffset = (2160.0f * fAspectRatio - 3840.0f) / 2.0f;
                    float heightOffset = (3840.0f / fAspectRatio - 2160.0f) / 2.0f;

                    if (sWidgetName.contains("WBP_Race_HUD_Player01_C")) {
                        bIs2PlayerRace = false;
                        auto hud = static_cast<SDK::UWBP_Race_HUD_Player01_C*>(WidgetObject);

                        if (hud->WidgetTree->RootWidget->IsA(SDK::UScaleBox::StaticClass())) {
                            auto scaleBox = static_cast<SDK::UScaleBox*>(hud->WidgetTree->RootWidget);
                            auto sizeBox = static_cast<SDK::USizeBox*>(scaleBox->Slots[0]->Content);

                            if (bSpanRaceHUD) {
                                if (fAspectRatio > fNativeAspect) {
                                    sizeBox->SetWidthOverride(2160.0f * fAspectRatio);
                                    sizeBox->SetHeightOverride(2160.0f);
                                }
                                else {
                                    sizeBox->SetWidthOverride(3840.0f);
                                    sizeBox->SetHeightOverride(3840.0f / fAspectRatio);
                                }

                                if (hud->CLIP_TextDirection && hud->CLIP_TextDirection->Slot) {
                                    auto clippingPanel = static_cast<SDK::UCanvasPanelSlot*>(hud->CLIP_TextDirection->Slot);

                                    if (fAspectRatio > fNativeAspect)
                                        clippingPanel->SetOffsets(SDK::FMargin(0.0f, 0.0f, 2160.0f * fAspectRatio, 2160.0f));
                                    else
                                        clippingPanel->SetOffsets(SDK::FMargin(0.0f, 0.0f, 3840.0f, 3840.0f / fAspectRatio));
                                }
                            }

                            if (bFixHUD) {
                                // Disable clipping so that player markers don't disappear outside 16:9 bounds
                                sizeBox->SetClipping(SDK::EWidgetClipping::Inherit);

                                // Span background of pause menu
                                if (hud->WBP_PauseMenu) {
                                    auto pauseMenu = static_cast<SDK::UWBP_PauseMenu_C*>(hud->WBP_PauseMenu);
                                    auto pauseScaleBox = static_cast<SDK::UScaleBox*>(pauseMenu->WidgetTree->RootWidget);

                                    if (pauseScaleBox->Slots[0] && pauseScaleBox->Slots[0]->Content) {
                                        auto panel = static_cast<SDK::UCanvasPanel*>(pauseScaleBox->Slots[0]->Content);
                                        panel->SetClipping(SDK::EWidgetClipping::Inherit);

                                        if (panel->Slots.IsValidIndex(3)) {
                                            auto backgroundBlurSlot = static_cast<SDK::UCanvasPanelSlot*>(panel->Slots[2]);
                                            auto bgBlackSlot = static_cast<SDK::UCanvasPanelSlot*>(panel->Slots[3]);

                                            if (fAspectRatio > fNativeAspect) {
                                                backgroundBlurSlot->SetOffsets(SDK::FMargin(-widthOffset, 0.0f, -widthOffset, 0.0f));
                                                bgBlackSlot->SetOffsets(SDK::FMargin(-widthOffset, 0.0f, -widthOffset, 0.0f));
                                            }
                                            else {
                                                backgroundBlurSlot->SetOffsets(SDK::FMargin(0.0f, -heightOffset, 0.0f, -heightOffset));
                                                bgBlackSlot->SetOffsets(SDK::FMargin(0.0f, -heightOffset, 0.0f, -heightOffset));
                                            }
                                        }  
                                    }
                                }
                            }
                        }
                    }

                    static bool bIsMenuCloning = false;
                    static SDK::UUserWidget* pCurrentMenuClone = nullptr;

                    if (sWidgetName.contains("_M2") || sWidgetName.contains("Player02")) {
                        bIs2PlayerRace = true;
                    }

                    if (sWidgetName.contains("WBP_Ready_M1_C") || sWidgetName.contains("WBP_CMN_MainMenu") || sWidgetName.contains("WBP_TitleMenu") || sWidgetName.contains("WBP_TopMenu")) {
                        bIs2PlayerRace = false;
                        if (pCurrentMenuClone) {
                            pCurrentMenuClone->RemoveFromParent();
                            pCurrentMenuClone = nullptr;
                        }
                    }

                    if (sWidgetName.contains("WBP_Race_HUD_Player02_C")) {
                        bIs2PlayerRace = true;
                        if (pCurrentMenuClone) {
                            pCurrentMenuClone->RemoveFromParent();
                            pCurrentMenuClone = nullptr;
                        }
                        auto hud2P = static_cast<SDK::UWBP_Race_HUD_Player02_C*>(WidgetObject);

                        if (hud2P->WidgetTree && hud2P->WidgetTree->RootWidget && hud2P->WidgetTree->RootWidget->IsA(SDK::UScaleBox::StaticClass())) {
                            auto scaleBox = static_cast<SDK::UScaleBox*>(hud2P->WidgetTree->RootWidget);
                            if (scaleBox->Slots.IsValidIndex(0) && scaleBox->Slots[0]->Content) {
                                auto sizeBox = static_cast<SDK::USizeBox*>(scaleBox->Slots[0]->Content);

                                if (bDualMonitor2P || bSpanRaceHUD) {
                                    sizeBox->SetWidthOverride((float)iCurrentResX);
                                    sizeBox->SetHeightOverride((float)iCurrentResY);

                                    SDK::FAnchors anchorsP1{ SDK::FVector2D{ 0.0, 0.0 }, SDK::FVector2D{ 0.5, 1.0 } };
                                    SDK::FAnchors anchorsP2{ SDK::FVector2D{ 0.5, 0.0 }, SDK::FVector2D{ 1.0, 1.0 } };

                                    // Player 1 HUD -> Left Monitor (0.0 to 0.5 of virtual screen)
                                    if (hud2P->CanvasPanel_P1_HUD && hud2P->CanvasPanel_P1_HUD->Slot) {
                                        auto slotP1 = static_cast<SDK::UCanvasPanelSlot*>(hud2P->CanvasPanel_P1_HUD->Slot);
                                        slotP1->SetAnchors(anchorsP1);
                                        slotP1->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                                    }

                                    // Player 2 HUD -> Right Monitor (0.5 to 1.0 of virtual screen)
                                    if (hud2P->CanvasPanel_P2_HUD && hud2P->CanvasPanel_P2_HUD->Slot) {
                                        auto slotP2 = static_cast<SDK::UCanvasPanelSlot*>(hud2P->CanvasPanel_P2_HUD->Slot);
                                        slotP2->SetAnchors(anchorsP2);
                                        slotP2->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                                    }

                                    // Item Warning Areas
                                    if (hud2P->CanvasPanel_ItemWarningArea_P1 && hud2P->CanvasPanel_ItemWarningArea_P1->Slot) {
                                        auto slot = static_cast<SDK::UCanvasPanelSlot*>(hud2P->CanvasPanel_ItemWarningArea_P1->Slot);
                                        slot->SetAnchors(anchorsP1);
                                        slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                                    }
                                    if (hud2P->CanvasPanel_ItemWarningArea_P2 && hud2P->CanvasPanel_ItemWarningArea_P2->Slot) {
                                        auto slot = static_cast<SDK::UCanvasPanelSlot*>(hud2P->CanvasPanel_ItemWarningArea_P2->Slot);
                                        slot->SetAnchors(anchorsP2);
                                        slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                                    }

                                    // Start Info Areas
                                    if (hud2P->CanvasPanel_P1_StartInfo && hud2P->CanvasPanel_P1_StartInfo->Slot) {
                                        auto slot = static_cast<SDK::UCanvasPanelSlot*>(hud2P->CanvasPanel_P1_StartInfo->Slot);
                                        slot->SetAnchors(anchorsP1);
                                        slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                                    }
                                    if (hud2P->CanvasPanel_P2_StartInfo && hud2P->CanvasPanel_P2_StartInfo->Slot) {
                                        auto slot = static_cast<SDK::UCanvasPanelSlot*>(hud2P->CanvasPanel_P2_StartInfo->Slot);
                                        slot->SetAnchors(anchorsP2);
                                        slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                                    }

                                    // Center vertical divider line: hide since physical monitor bezel already separates the screens
                                    if (bHideCenterLine && hud2P->VerticalLine) {
                                        hud2P->VerticalLine->SetVisibility(SDK::ESlateVisibility::Hidden);
                                    }
                                }

                                if (bFixHUD) {
                                    sizeBox->SetClipping(SDK::EWidgetClipping::Inherit);
                                    if (hud2P->CanvasPanel_0) hud2P->CanvasPanel_0->SetClipping(SDK::EWidgetClipping::Inherit);
                                    if (hud2P->CanvasPanel_P1_HUD) hud2P->CanvasPanel_P1_HUD->SetClipping(SDK::EWidgetClipping::Inherit);
                                    if (hud2P->CanvasPanel_P2_HUD) hud2P->CanvasPanel_P2_HUD->SetClipping(SDK::EWidgetClipping::Inherit);
                                    if (hud2P->WBP_Race_HUD_Sub_Aiming_P1) hud2P->WBP_Race_HUD_Sub_Aiming_P1->SetClipping(SDK::EWidgetClipping::Inherit);
                                    if (hud2P->WBP_Race_HUD_Sub_Aiming_P2) hud2P->WBP_Race_HUD_Sub_Aiming_P2->SetClipping(SDK::EWidgetClipping::Inherit);
                                    if (hud2P->WBP_Race_HUD_Sub_Targeted_P1) hud2P->WBP_Race_HUD_Sub_Targeted_P1->SetClipping(SDK::EWidgetClipping::Inherit);
                                    if (hud2P->WBP_Race_HUD_Sub_Targeted_P2) hud2P->WBP_Race_HUD_Sub_Targeted_P2->SetClipping(SDK::EWidgetClipping::Inherit);
                                }
                            }
                        }
                    }

                    if (sWidgetName.contains("WBP_Ready_M2_C")) {
                        bIs2PlayerRace = true;
                        auto ready2P = static_cast<SDK::UWBP_Ready_M2_C*>(WidgetObject);

                        if (bDualMonitor2P && bMirror2PMenus) {
                            SPDLOG_INFO("HUD: Applying Dual-Monitor layout to WBP_Ready_M2_C");

                            auto unclipAndExpand = [](SDK::UWidget* w, auto& self) -> void {
                                if (!w) return;
                                w->SetClipping(SDK::EWidgetClipping::Inherit);
                                if (w->IsA(SDK::USafeZone::StaticClass())) {
                                    static_cast<SDK::USafeZone*>(w)->SetSidesToPad(false, false, false, false);
                                }
                                if (w->IsA(SDK::USizeBox::StaticClass())) {
                                    auto sb = static_cast<SDK::USizeBox*>(w);
                                    sb->SetWidthOverride(3840.0f);
                                    sb->SetHeightOverride(1080.0f);
                                }
                                if (w->IsA(SDK::UPanelWidget::StaticClass())) {
                                    auto p = static_cast<SDK::UPanelWidget*>(w);
                                    for (int i = 0; i < p->Slots.Num(); ++i) {
                                        if (p->Slots.IsValidIndex(i) && p->Slots[i] && p->Slots[i]->Content) {
                                            self(p->Slots[i]->Content, self);
                                        }
                                    }
                                }
                            };

                            if (ready2P->WidgetTree && ready2P->WidgetTree->RootWidget) {
                                unclipAndExpand(ready2P->WidgetTree->RootWidget, unclipAndExpand);
                            }
                            if (ready2P->SafeZone_Ready_M2) {
                                ready2P->SafeZone_Ready_M2->SetSidesToPad(false, false, false, false);
                                unclipAndExpand(ready2P->SafeZone_Ready_M2, unclipAndExpand);
                            }

                            // Fullscreen backgrounds
                            SDK::FAnchors anchorsFull{ SDK::FVector2D{ 0.0, 0.0 }, SDK::FVector2D{ 1.0, 1.0 } };
                            if (ready2P->BG_Black && ready2P->BG_Black->Slot && ready2P->BG_Black->Slot->IsA(SDK::UCanvasPanelSlot::StaticClass())) {
                                auto slot = static_cast<SDK::UCanvasPanelSlot*>(ready2P->BG_Black->Slot);
                                slot->SetAnchors(anchorsFull);
                                slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                            }
                            if (ready2P->BG_Light && ready2P->BG_Light->Slot && ready2P->BG_Light->Slot->IsA(SDK::UCanvasPanelSlot::StaticClass())) {
                                auto slot = static_cast<SDK::UCanvasPanelSlot*>(ready2P->BG_Light->Slot);
                                slot->SetAnchors(anchorsFull);
                                slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                            }

                            // P1 Elements -> Left Monitor (0.0 to 0.5)
                            SDK::FAnchors anchorsP1{ SDK::FVector2D{ 0.0, 0.0 }, SDK::FVector2D{ 0.5, 1.0 } };
                            if (ready2P->Base_BG_P1 && ready2P->Base_BG_P1->Slot && ready2P->Base_BG_P1->Slot->IsA(SDK::UCanvasPanelSlot::StaticClass())) {
                                auto slot = static_cast<SDK::UCanvasPanelSlot*>(ready2P->Base_BG_P1->Slot);
                                slot->SetAnchors(anchorsP1);
                                slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                            }
                            if (ready2P->Base_BG_Shadow_P1 && ready2P->Base_BG_Shadow_P1->Slot && ready2P->Base_BG_Shadow_P1->Slot->IsA(SDK::UCanvasPanelSlot::StaticClass())) {
                                auto slot = static_cast<SDK::UCanvasPanelSlot*>(ready2P->Base_BG_Shadow_P1->Slot);
                                slot->SetAnchors(anchorsP1);
                                slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                            }
                            if (ready2P->RaceFlag_Left_P1 && ready2P->RaceFlag_Left_P1->Slot && ready2P->RaceFlag_Left_P1->Slot->IsA(SDK::UCanvasPanelSlot::StaticClass())) {
                                auto slot = static_cast<SDK::UCanvasPanelSlot*>(ready2P->RaceFlag_Left_P1->Slot);
                                slot->SetAnchors(anchorsP1);
                                slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                            }

                            // P2 Elements -> Right Monitor (0.5 to 1.0)
                            SDK::FAnchors anchorsP2{ SDK::FVector2D{ 0.5, 0.0 }, SDK::FVector2D{ 1.0, 1.0 } };
                            if (ready2P->Base_BG_P2 && ready2P->Base_BG_P2->Slot && ready2P->Base_BG_P2->Slot->IsA(SDK::UCanvasPanelSlot::StaticClass())) {
                                auto slot = static_cast<SDK::UCanvasPanelSlot*>(ready2P->Base_BG_P2->Slot);
                                slot->SetAnchors(anchorsP2);
                                slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                            }
                            if (ready2P->Base_BG_Shadow_P2 && ready2P->Base_BG_Shadow_P2->Slot && ready2P->Base_BG_Shadow_P2->Slot->IsA(SDK::UCanvasPanelSlot::StaticClass())) {
                                auto slot = static_cast<SDK::UCanvasPanelSlot*>(ready2P->Base_BG_Shadow_P2->Slot);
                                slot->SetAnchors(anchorsP2);
                                slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                            }
                            if (ready2P->RaceFlag_Right_P2 && ready2P->RaceFlag_Right_P2->Slot && ready2P->RaceFlag_Right_P2->Slot->IsA(SDK::UCanvasPanelSlot::StaticClass())) {
                                auto slot = static_cast<SDK::UCanvasPanelSlot*>(ready2P->RaceFlag_Right_P2->Slot);
                                slot->SetAnchors(anchorsP2);
                                slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
                            }

                            // Position interactive selection cards:
                            // P1 to Monitor 1 (-960.0), P2 to Monitor 2 (+960.0)
                            auto setupP1Window = [](SDK::UWidget* w) {
                                if (!w) return;
                                w->SetClipping(SDK::EWidgetClipping::Inherit);
                                w->SetRenderTranslation(SDK::FVector2D{ -960.0, 0.0 });
                            };

                            auto setupP2Window = [](SDK::UWidget* w) {
                                if (!w) return;
                                w->SetClipping(SDK::EWidgetClipping::Inherit);
                                w->SetRenderTranslation(SDK::FVector2D{ 960.0, 0.0 });
                            };

                            setupP1Window(ready2P->WBP_Ready_Sub_CharaWindow_P1P3);
                            setupP1Window(ready2P->WBP_Ready_Sub_Gadget_P1);
                            setupP1Window(ready2P->WBP_Window_MachineParameter_P1);

                            setupP2Window(ready2P->WBP_Ready_Sub_CharaWindow_P2P4);
                            setupP2Window(ready2P->WBP_Ready_Sub_Gadget_P2);
                            setupP2Window(ready2P->WBP_Window_MachineParameter_P2);

                            // Standby text and buttons on Monitor 1
                            if (ready2P->WBP_Ready_Sub_StandByText) {
                                ready2P->WBP_Ready_Sub_StandByText->SetRenderTranslation(SDK::FVector2D{ -960.0, 0.0 });
                            }
                            if (ready2P->WBP_CMN_Sub_BoxBtn) {
                                ready2P->WBP_CMN_Sub_BoxBtn->SetRenderTranslation(SDK::FVector2D{ -960.0, 0.0 });
                            }

                            // "ВПЕРЁД!" banner: Monitor 1 primary (-960) and clone to Monitor 2 (+960)
                            if (ready2P->WBP_Ready_Sub_Decide) {
                                ready2P->WBP_Ready_Sub_Decide->SetClipping(SDK::EWidgetClipping::Inherit);
                                ready2P->WBP_Ready_Sub_Decide->SetRenderTranslation(SDK::FVector2D{ -960.0, 0.0 });

                                if (!bIsMenuCloning && ready2P->WBP_Ready_Sub_Decide->Class) {
                                    bIsMenuCloning = true;
                                    auto decideClone = SDK::UWidgetBlueprintLibrary::Create(ready2P, ready2P->WBP_Ready_Sub_Decide->Class, nullptr);
                                    bIsMenuCloning = false;
                                    if (decideClone) {
                                        decideClone->AddToViewport(10);
                                        decideClone->SetVisibility(SDK::ESlateVisibility::HitTestInvisible);
                                        if (decideClone->WidgetTree && decideClone->WidgetTree->RootWidget) {
                                            decideClone->WidgetTree->RootWidget->SetClipping(SDK::EWidgetClipping::Inherit);
                                            decideClone->WidgetTree->RootWidget->SetRenderTranslation(SDK::FVector2D{ 960.0, 0.0 });
                                        }
                                        pCurrentMenuClone = decideClone;
                                    }
                                }
                            }
                        }
                    }

                    if (sWidgetName.contains("WBP_PauseMenu_C")) {
                        auto pauseMenu = static_cast<SDK::UWBP_PauseMenu_C*>(WidgetObject);
                        if (bDualMonitor2P && bMirror2PMenus) {
                            SPDLOG_INFO("HUD: Applying Dual-Monitor layout to WBP_PauseMenu_C");
                            if (pauseMenu->WidgetTree && pauseMenu->WidgetTree->RootWidget) {
                                pauseMenu->WidgetTree->RootWidget->SetClipping(SDK::EWidgetClipping::Inherit);
                            }

                            // Expand pause background blur and black background to full 3840x1080
                            if (pauseMenu->WidgetTree && pauseMenu->WidgetTree->RootWidget->IsA(SDK::UScaleBox::StaticClass())) {
                                auto pauseScaleBox = static_cast<SDK::UScaleBox*>(pauseMenu->WidgetTree->RootWidget);
                                if (pauseScaleBox->Slots.IsValidIndex(0) && pauseScaleBox->Slots[0]->Content) {
                                    if (pauseScaleBox->Slots[0]->Content->IsA(SDK::UCanvasPanel::StaticClass())) {
                                        auto panel = static_cast<SDK::UCanvasPanel*>(pauseScaleBox->Slots[0]->Content);
                                        panel->SetClipping(SDK::EWidgetClipping::Inherit);
                                        for (int i = 0; i < panel->Slots.Num(); ++i) {
                                            if (panel->Slots.IsValidIndex(i) && panel->Slots[i]) {
                                                auto slot = static_cast<SDK::UCanvasPanelSlot*>(panel->Slots[i]);
                                                if (slot->Content && (slot->Content->GetName().contains("Blur") || slot->Content->GetName().contains("Black") || slot->Content->GetName().contains("BG"))) {
                                                    slot->SetOffsets(SDK::FMargin(-960.0f, 0.0f, -960.0f, 0.0f));
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Determine which player paused: P1 = -960 (Monitor 1), P2 = +960 (Monitor 2)
                            float shift = (pauseMenu->Owner_Player_Index == 1) ? 960.0f : -960.0f;
                            SDK::FVector2D pauseShift{ shift, 0.0 };

                            if (pauseMenu->VBButton) pauseMenu->VBButton->SetRenderTranslation(pauseShift);
                            if (pauseMenu->TXT_ClassName) pauseMenu->TXT_ClassName->SetRenderTranslation(pauseShift);
                            if (pauseMenu->Line) pauseMenu->Line->SetRenderTranslation(pauseShift);
                            if (pauseMenu->HorizontalBox_RSR) pauseMenu->HorizontalBox_RSR->SetRenderTranslation(pauseShift);
                            if (pauseMenu->WBP_CMN_PlayerNumber_Big) pauseMenu->WBP_CMN_PlayerNumber_Big->SetRenderTranslation(pauseShift);
                            if (pauseMenu->WBP_CMN_PlayerNumber_Big_1P) pauseMenu->WBP_CMN_PlayerNumber_Big_1P->SetRenderTranslation(pauseShift);
                            if (pauseMenu->WBP_CMN_PlayerNumber_Big_P2) pauseMenu->WBP_CMN_PlayerNumber_Big_P2->SetRenderTranslation(pauseShift);
                            if (pauseMenu->WS_Icon_Active) pauseMenu->WS_Icon_Active->SetRenderTranslation(pauseShift);
                            if (pauseMenu->WBP_FooterMenu) pauseMenu->WBP_FooterMenu->SetRenderTranslation(pauseShift);

                            // Player 1 gadget info -> Monitor 1 (-960)
                            if (pauseMenu->Overlay_Option_Btn_03_P1) pauseMenu->Overlay_Option_Btn_03_P1->SetRenderTranslation(SDK::FVector2D{ -960.0, 0.0 });
                            if (pauseMenu->WidgetSwitcher_P1) pauseMenu->WidgetSwitcher_P1->SetRenderTranslation(SDK::FVector2D{ -960.0, 0.0 });
                            if (pauseMenu->WBP_GadgetCustom_Sub_GadgetPlate_P1) pauseMenu->WBP_GadgetCustom_Sub_GadgetPlate_P1->SetRenderTranslation(SDK::FVector2D{ -960.0, 0.0 });

                            // Player 2 gadget info -> Monitor 2 (+960)
                            if (pauseMenu->Overlay_Option_Btn_03_P2) pauseMenu->Overlay_Option_Btn_03_P2->SetRenderTranslation(SDK::FVector2D{ 960.0, 0.0 });
                            if (pauseMenu->WidgetSwitcher_P2) pauseMenu->WidgetSwitcher_P2->SetRenderTranslation(SDK::FVector2D{ 960.0, 0.0 });
                            if (pauseMenu->WBP_GadgetCustom_Sub_GadgetPlate_P2) pauseMenu->WBP_GadgetCustom_Sub_GadgetPlate_P2->SetRenderTranslation(SDK::FVector2D{ 960.0, 0.0 });
                        }
                    }

                    if (bDualMonitor2P && bMirror2PMenus && bIs2PlayerRace && !bIsMenuCloning) {
                        bool bIsSharedMenu = sWidgetName.contains("ClassSelect") ||
                                             sWidgetName.contains("CourseSelect") ||
                                             sWidgetName.contains("RivalSelect") ||
                                             sWidgetName.contains("RivalCutin") ||
                                             sWidgetName.contains("RivalChoice") ||
                                             sWidgetName.contains("RaceBefore");

                        if (bIsSharedMenu) {
                            auto widget = static_cast<SDK::UUserWidget*>(WidgetObject);
                            SPDLOG_INFO("HUD: Dual-Monitor shared menu mirroring for {}", sWidgetName);
                            if (widget->WidgetTree && widget->WidgetTree->RootWidget) {
                                widget->WidgetTree->RootWidget->SetClipping(SDK::EWidgetClipping::Inherit);
                                widget->WidgetTree->RootWidget->SetRenderTranslation(SDK::FVector2D{ -960.0f, 0.0f });

                                if (pCurrentMenuClone) {
                                    pCurrentMenuClone->RemoveFromParent();
                                    pCurrentMenuClone = nullptr;
                                }

                                bIsMenuCloning = true;
                                auto clone = SDK::UWidgetBlueprintLibrary::Create(widget, widget->Class, nullptr);
                                bIsMenuCloning = false;
                                if (clone) {
                                    clone->AddToViewport(0);
                                    clone->SetVisibility(SDK::ESlateVisibility::HitTestInvisible);
                                    if (clone->WidgetTree && clone->WidgetTree->RootWidget) {
                                        clone->WidgetTree->RootWidget->SetClipping(SDK::EWidgetClipping::Inherit);
                                        clone->WidgetTree->RootWidget->SetRenderTranslation(SDK::FVector2D{ 960.0f, 0.0f });
                                    }
                                    pCurrentMenuClone = clone;
                                }
                            }
                        }
                    }
                });
        }
        else {
            SPDLOG_ERROR("HUD: Widgets: Pattern scan failed.");
        }
    }
}

DWORD __stdcall Main(void*)
{
    Logging();
    Configuration();
    UpdateOffsets();
    CurrentResolution();
    AspectRatioFOV();
    HUD();
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) 
    {
    case DLL_PROCESS_ATTACH:
    {  
        // Detach from "crashpad_handler.exe"
        char exeName[MAX_PATH];
        GetModuleFileNameA(NULL, exeName, MAX_PATH);
        std::string exeStr(exeName);
        if (exeStr.find("crashpad_handler.exe") != std::string::npos)
            return FALSE;

        DisableThreadLibraryCalls(hModule);
        thisModule = hModule;

        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle)
        {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
            CloseHandle(mainHandle);
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}