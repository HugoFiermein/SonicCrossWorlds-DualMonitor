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
#include <cmath>
#include <unordered_map>
#include <unordered_set>

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

// Dual monitor: two 16:9 (or 16:10) panels side by side give ~3.2-3.56 aspect. 21:9 ultrawide is ~2.4.
constexpr float fDualMonitorMinAspect = 3.0f;
constexpr int32_t kRF_BeginDestroyed = 0x00008000;
constexpr int32_t kRF_FinishDestroyed = 0x00010000;

// Menu clones shown on Monitor 2. The engine knows nothing about them, so we own their lifetime.
struct MenuClone
{
    SDK::UUserWidget* Clone = nullptr;   int32_t CloneIdx = -1;
    SDK::UUserWidget* Owner = nullptr;   int32_t OwnerIdx = -1;   // clone lives while this stays in viewport
    SDK::UWidget* Source = nullptr;      int32_t SourceIdx = -1;  // clone mirrors visibility/opacity of this
    std::string Key;
    bool bExactVisibility = false;       // false -> visible states are forced to HitTestInvisible (no input on clone)
    bool bOwnerSeenInViewport = false;
};
std::vector<MenuClone> g_MenuClones;
bool g_bIsMenuCloning = false;
DWORD g_GameThreadId = 0;

// Anything we do to clones (SetVisibility, RemoveFromParent, IsInViewport...) goes through ProcessEvent,
// which would re-enter SyncMenuClones while g_MenuClones is being iterated. This blocks that.
int g_CloneOpDepth = 0;
struct CloneOpGuard
{
    CloneOpGuard() { ++g_CloneOpDepth; }
    ~CloneOpGuard() { --g_CloneOpDepth; }
};

bool IsDualMonitorLayout()
{
    return bDualMonitor2P && fAspectRatio >= fDualMonitorMinAspect;
}

bool IsObjectAlive(const SDK::UObject* obj, int32_t index)
{
    if (!obj || index < 0)
        return false;
    if (SDK::UObject::GObjects->GetByIndex(index) != obj)
        return false;
    return (static_cast<int32_t>(obj->Flags) & (kRF_BeginDestroyed | kRF_FinishDestroyed)) == 0;
}

bool IsMenuClone(const SDK::UObject* obj)
{
    for (const auto& e : g_MenuClones)
        if (e.Clone == obj) return true;
    return false;
}

void DestroyMenuClone(MenuClone& e)
{
    if (IsObjectAlive(e.Clone, e.CloneIdx))
        e.Clone->RemoveFromParent();
}

void DestroyAllMenuClones()
{
    CloneOpGuard guard;
    for (auto& e : g_MenuClones)
        DestroyMenuClone(e);
    g_MenuClones.clear();
}

void MirrorCloneVisibility(MenuClone& e)
{
    bool sourceAlive = IsObjectAlive(e.Source, e.SourceIdx);
    auto srcVis = sourceAlive ? e.Source->Visibility : SDK::ESlateVisibility::Collapsed;
    bool srcVisible = srcVis != SDK::ESlateVisibility::Collapsed && srcVis != SDK::ESlateVisibility::Hidden;

    auto desired = e.bExactVisibility ? srcVis : (srcVisible ? SDK::ESlateVisibility::HitTestInvisible : SDK::ESlateVisibility::Collapsed);
    if (e.Clone->Visibility != desired)
        e.Clone->SetVisibility(desired);

    float opacity = sourceAlive ? e.Source->RenderOpacity : 0.0f;
    if (std::fabs(e.Clone->RenderOpacity - opacity) > 0.001f)
        e.Clone->SetRenderOpacity(opacity);
}

// bCheckViewport: also drop clones whose owner left the viewport (costs a ProcessEvent per clone).
void SyncMenuClones(bool bCheckViewport)
{
    CloneOpGuard guard;
    std::erase_if(g_MenuClones, [bCheckViewport](MenuClone& e) {
        if (!IsObjectAlive(e.Clone, e.CloneIdx))
            return true;

        bool ownerOk = IsObjectAlive(e.Owner, e.OwnerIdx);
        if (ownerOk && bCheckViewport) {
            // Owner is not in viewport yet while its own AddToViewport is running,
            // so only treat "not in viewport" as removal after we've seen it there once.
            if (e.Owner->IsInViewport())
                e.bOwnerSeenInViewport = true;
            else if (e.bOwnerSeenInViewport)
                ownerOk = false;
        }

        if (!ownerOk) {
            SPDLOG_INFO("HUD: Dual-Monitor removing stale clone {}", e.Key);
            e.Clone->RemoveFromParent();
            return true;
        }

        MirrorCloneVisibility(e);
        return false;
    });
}

void RegisterMenuClone(SDK::UUserWidget* clone, SDK::UUserWidget* owner, SDK::UWidget* source, const std::string& key, bool bExactVisibility)
{
    CloneOpGuard guard;
    // Same menu re-added -> replace the old clone instead of stacking a second one on Monitor 2
    std::erase_if(g_MenuClones, [&key](MenuClone& e) {
        if (e.Key != key) return false;
        DestroyMenuClone(e);
        return true;
    });

    MenuClone e;
    e.Clone = clone;   e.CloneIdx = clone->Index;
    e.Owner = owner;   e.OwnerIdx = owner->Index;
    e.Source = source; e.SourceIdx = source->Index;
    e.Key = key;
    e.bExactVisibility = bExactVisibility;
    g_MenuClones.push_back(std::move(e));
    MirrorCloneVisibility(g_MenuClones.back());
}

// SEH wrapper (no C++ objects with destructors allowed here). CloneOpGuard's destructor is skipped
// when an SEH exception unwinds, so the depth counter is reset by hand.
static void SafeSyncMenuClones(bool bCheckViewport)
{
    __try {
        SyncMenuClones(bCheckViewport);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        g_CloneOpDepth = 0;
        g_MenuClones.clear();   // pointers are suspect now, forget them rather than touch them again
    }
}

SafetyHookInline ProcessEventHook{};

void ProcessEvent_Hook(SDK::UObject* obj, SDK::UFunction* fn, void* parms)
{
    // unsafe_call: call() locks a mutex around the whole original, which would serialise every ProcessEvent in the game
    ProcessEventHook.unsafe_call<void>(obj, fn, parms);

    if (g_CloneOpDepth > 0 || g_bIsMenuCloning || GetCurrentThreadId() != g_GameThreadId || g_MenuClones.empty())
        return;

    static ULONGLONG lastViewportCheck = 0;
    ULONGLONG now = GetTickCount64();
    bool bCheckViewport = now - lastViewportCheck >= 100;
    if (bCheckViewport)
        lastViewportCheck = now;
    SafeSyncMenuClones(bCheckViewport);
}

// Clears clipping and safe-zone padding on the whole tree and stretches only the OUTERMOST SizeBox
// of each branch to 3840x1080. Nested SizeBoxes (icons, buttons, frames) keep their own sizes.
void UnclipAndExpand(SDK::UWidget* w, bool bInsideSizeBox = false)
{
    if (!w) return;
    w->SetClipping(SDK::EWidgetClipping::Inherit);

    if (w->IsA(SDK::USafeZone::StaticClass()))
        static_cast<SDK::USafeZone*>(w)->SetSidesToPad(false, false, false, false);

    if (!bInsideSizeBox && w->IsA(SDK::USizeBox::StaticClass())) {
        auto sb = static_cast<SDK::USizeBox*>(w);
        sb->SetWidthOverride(3840.0f);
        sb->SetHeightOverride(1080.0f);
        bInsideSizeBox = true;
    }

    if (w->IsA(SDK::UPanelWidget::StaticClass())) {
        auto p = static_cast<SDK::UPanelWidget*>(w);
        for (int i = 0; i < p->Slots.Num(); ++i) {
            if (p->Slots[i] && p->Slots[i]->Content)
                UnclipAndExpand(p->Slots[i]->Content, bInsideSizeBox);
        }
    }
}

// Shift P1 parts to Monitor 1 and P2 parts to Monitor 2 by name.
// RenderTranslation is inherited by children, so once a branch is shifted we don't shift its children again.
void ShiftPlayerSections(SDK::UWidget* w)
{
    if (!w) return;
    w->SetClipping(SDK::EWidgetClipping::Inherit);

    std::string name = w->GetName();
    if (name.contains("P1") || name.contains("1P") || name.contains("Left")) {
        w->SetRenderTranslation(SDK::FVector2D{ -960.0f, 0.0f });
        return;
    }
    if (name.contains("P2") || name.contains("2P") || name.contains("Right")) {
        w->SetRenderTranslation(SDK::FVector2D{ 960.0f, 0.0f });
        return;
    }

    if (w->IsA(SDK::UPanelWidget::StaticClass())) {
        auto p = static_cast<SDK::UPanelWidget*>(w);
        for (int i = 0; i < p->Slots.Num(); ++i) {
            if (p->Slots[i] && p->Slots[i]->Content)
                ShiftPlayerSections(p->Slots[i]->Content);
        }
    }
}

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
        SPDLOG_INFO("Log File: {}", (ExePath / sLogFile).string());
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

// ---------- Widget lookup by name ----------
// Blueprint widget classes here all derive from game classes (UUnionRaceUI, UReadyWindow, UUnionUISceneBase...).
// When the game updates, those parents change size and every member offset in the dumped SDK shifts:
// reading e.g. hud2P->CanvasPanel_P1_HUD then returns garbage and crashes (race start crash).
// The widget tree itself (UUserWidget::WidgetTree, UPanelWidget::Slots, UPanelSlot::Content) is engine-side
// and stable, and designer widgets are named exactly like the BP members, so we look them up by name instead.
class WidgetIndex
{
public:
    explicit WidgetIndex(SDK::UUserWidget* owner)
    {
        if (owner && owner->WidgetTree && owner->WidgetTree->RootWidget)
            Add(owner->WidgetTree->RootWidget);
    }

    SDK::UWidget* operator[](const char* name) const
    {
        auto it = m_byName.find(name);
        return it != m_byName.end() ? it->second : nullptr;
    }

    template<typename T>
    T* Get(const char* name) const { return static_cast<T*>((*this)[name]); }

    bool Contains(const SDK::UObject* w) const { return m_all.contains(w); }

private:
    void Add(SDK::UWidget* w)
    {
        if (!w || m_all.contains(w)) return;
        m_all.insert(w);
        m_byName.emplace(w->GetName(), w);
        if (w->IsA(SDK::UPanelWidget::StaticClass())) {
            auto p = static_cast<SDK::UPanelWidget*>(w);
            for (int i = 0; i < p->Slots.Num(); ++i)
                if (p->Slots[i] && p->Slots[i]->Content)
                    Add(p->Slots[i]->Content);
        }
    }

    std::unordered_map<std::string, SDK::UWidget*> m_byName;
    std::unordered_set<const SDK::UObject*> m_all;
};

// Anchors a widget's CanvasPanelSlot to the given region and zeroes its offsets
static void SetSlotAnchors(SDK::UWidget* widget, const SDK::FAnchors& anchors)
{
    if (!widget || !widget->Slot || !widget->Slot->IsA(SDK::UCanvasPanelSlot::StaticClass()))
        return;
    auto slot = static_cast<SDK::UCanvasPanelSlot*>(widget->Slot);
    slot->SetAnchors(anchors);
    slot->SetOffsets(SDK::FMargin{ 0.0f, 0.0f, 0.0f, 0.0f });
}

static void SetTranslation(SDK::UWidget* w, float x, bool bUnclip = false)
{
    if (!w) return;
    if (bUnclip) w->SetClipping(SDK::EWidgetClipping::Inherit);
    w->SetRenderTranslation(SDK::FVector2D{ x, 0.0 });
}

// Creates a passive copy of `widgetClass` for Monitor 2.
// Note: AddToViewport on the clone re-enters OnWidgetAddToViewport, g_bIsMenuCloning stops that.
static SDK::UUserWidget* CreateMonitor2Clone(SDK::UUserWidget* outer, SDK::UClass* widgetClass, int zOrder, float translationX)
{
    g_bIsMenuCloning = true;
    auto clone = SDK::UWidgetBlueprintLibrary::Create(outer, widgetClass, nullptr);
    if (clone) {
        clone->AddToViewport(zOrder);
        if (clone->WidgetTree && clone->WidgetTree->RootWidget) {
            clone->WidgetTree->RootWidget->SetClipping(SDK::EWidgetClipping::Inherit);
            clone->WidgetTree->RootWidget->SetRenderTranslation(SDK::FVector2D{ translationX, 0.0 });
        }
    }
    g_bIsMenuCloning = false;
    return clone;
}

static SDK::USizeBox* GetRootScaleBoxSizeBox(SDK::UUserWidget* uw)
{
    if (!uw->WidgetTree || !uw->WidgetTree->RootWidget || !uw->WidgetTree->RootWidget->IsA(SDK::UScaleBox::StaticClass()))
        return nullptr;
    auto scaleBox = static_cast<SDK::UScaleBox*>(uw->WidgetTree->RootWidget);
    if (!scaleBox->Slots.IsValidIndex(0) || !scaleBox->Slots[0] || !scaleBox->Slots[0]->Content
        || !scaleBox->Slots[0]->Content->IsA(SDK::USizeBox::StaticClass()))
        return nullptr;
    return static_cast<SDK::USizeBox*>(scaleBox->Slots[0]->Content);
}

void OnWidgetAddToViewport(SDK::UObject* obj)
{
    if (!obj->Class || !obj->Class->Class)
        return;

    g_GameThreadId = GetCurrentThreadId();

    // Our own clones (and anything created while we are cloning) must not be processed
    if (g_bIsMenuCloning || IsMenuClone(obj))
        return;

    // Locals: the code below can re-enter this function (clone AddToViewport)
    const std::string name = obj->GetName();
    if (WidgetObject != obj) {
        WidgetObject = obj;
        sWidgetName = name;
        SPDLOG_INFO("HUD: Widgets: {} @ 0x{:x}", name, reinterpret_cast<uintptr_t>(obj));
    }

    if (!obj->IsA(SDK::UUserWidget::StaticClass()))
        return;
    auto userWidget = static_cast<SDK::UUserWidget*>(obj);

    const float widthOffset = (2160.0f * fAspectRatio - 3840.0f) / 2.0f;
    const float heightOffset = (3840.0f / fAspectRatio - 2160.0f) / 2.0f;
    const bool bDualLayout = IsDualMonitorLayout();
    const bool bDualMenus = bDualLayout && bMirror2PMenus;

    // ---------- 1P race HUD ----------
    if (name.contains("WBP_Race_HUD_Player01_C")) {
        bIs2PlayerRace = false;
        WidgetIndex w(userWidget);

        if (auto sizeBox = GetRootScaleBoxSizeBox(userWidget)) {
            if (bSpanRaceHUD) {
                if (fAspectRatio > fNativeAspect) {
                    sizeBox->SetWidthOverride(2160.0f * fAspectRatio);
                    sizeBox->SetHeightOverride(2160.0f);
                }
                else {
                    sizeBox->SetWidthOverride(3840.0f);
                    sizeBox->SetHeightOverride(3840.0f / fAspectRatio);
                }

                auto clip = w["CLIP_TextDirection"];
                if (clip && clip->Slot && clip->Slot->IsA(SDK::UCanvasPanelSlot::StaticClass())) {
                    auto clippingPanel = static_cast<SDK::UCanvasPanelSlot*>(clip->Slot);
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
                auto pauseMenu = w.Get<SDK::UUserWidget>("WBP_PauseMenu");
                if (pauseMenu && pauseMenu->IsA(SDK::UUserWidget::StaticClass()) && pauseMenu->WidgetTree && pauseMenu->WidgetTree->RootWidget) {
                    auto pauseScaleBox = static_cast<SDK::UScaleBox*>(pauseMenu->WidgetTree->RootWidget);
                    if (pauseScaleBox->IsA(SDK::UScaleBox::StaticClass()) && pauseScaleBox->Slots.IsValidIndex(0)
                        && pauseScaleBox->Slots[0] && pauseScaleBox->Slots[0]->Content) {
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

    // ---------- Mode tracking / clone housekeeping ----------
    if (name.contains("_M2") || name.contains("Player02"))
        bIs2PlayerRace = true;

    if (name.contains("WBP_Ready_M1_C") || name.contains("WBP_CMN_MainMenu") || name.contains("WBP_TitleMenu") || name.contains("WBP_TopMenu")) {
        bIs2PlayerRace = false;
        DestroyAllMenuClones();
    }
    else if (!g_MenuClones.empty()) {
        // Also covers Loading/Transition/BlackBoard and the 2P race HUD: drop clones whose original is gone.
        SyncMenuClones(true);
    }

    // ---------- 2P race HUD ----------
    if (name.contains("WBP_Race_HUD_Player02_C")) {
        bIs2PlayerRace = true;
        WidgetIndex w(userWidget);

        if (auto sizeBox = GetRootScaleBoxSizeBox(userWidget)) {
            // Resolution not known yet -> overriding with 0x0 would make the whole HUD vanish
            if ((bDualLayout || bSpanRaceHUD) && iCurrentResX > 0 && iCurrentResY > 0) {
                sizeBox->SetWidthOverride(static_cast<float>(iCurrentResX));
                sizeBox->SetHeightOverride(static_cast<float>(iCurrentResY));

                const SDK::FAnchors anchorsP1{ SDK::FVector2D{ 0.0, 0.0 }, SDK::FVector2D{ 0.5, 1.0 } };
                const SDK::FAnchors anchorsP2{ SDK::FVector2D{ 0.5, 0.0 }, SDK::FVector2D{ 1.0, 1.0 } };

                // Player 1 -> left monitor, Player 2 -> right monitor
                SetSlotAnchors(w["CanvasPanel_P1_HUD"], anchorsP1);
                SetSlotAnchors(w["CanvasPanel_P2_HUD"], anchorsP2);
                SetSlotAnchors(w["CanvasPanel_ItemWarningArea_P1"], anchorsP1);
                SetSlotAnchors(w["CanvasPanel_ItemWarningArea_P2"], anchorsP2);
                SetSlotAnchors(w["CanvasPanel_P1_StartInfo"], anchorsP1);
                SetSlotAnchors(w["CanvasPanel_P2_StartInfo"], anchorsP2);

                // Physical monitor bezels already separate the screens
                if (bDualLayout && bHideCenterLine)
                    if (auto line = w["VerticalLine"])
                        line->SetVisibility(SDK::ESlateVisibility::Hidden);
            }

            if (bFixHUD) {
                sizeBox->SetClipping(SDK::EWidgetClipping::Inherit);
                for (auto n : { "CanvasPanel_0", "CanvasPanel_P1_HUD", "CanvasPanel_P2_HUD",
                                "WBP_Race_HUD_Sub_Aiming_P1", "WBP_Race_HUD_Sub_Aiming_P2",
                                "WBP_Race_HUD_Sub_Targeted_P1", "WBP_Race_HUD_Sub_Targeted_P2" })
                    if (auto x = w[n]) x->SetClipping(SDK::EWidgetClipping::Inherit);
            }
        }
    }

    // ---------- 2P character / machine select ----------
    if (name.contains("WBP_MachineCharaSelect_M2")) {
        bIs2PlayerRace = true;

        if (bDualMenus) {
            SPDLOG_INFO("HUD: Applying Dual-Monitor layout to {}", name);
            WidgetIndex w(userWidget);

            if (userWidget->WidgetTree && userWidget->WidgetTree->RootWidget)
                UnclipAndExpand(userWidget->WidgetTree->RootWidget);

            // Per-player machine windows: CharaMachineSelectList is a native member, so only trust
            // entries that really are widgets of this tree. Index 0 = P1 -> Monitor 1, 1 = P2 -> Monitor 2.
            if (obj->IsA(SDK::UCharaMachineSelectsBase::StaticClass())) {
                auto& list = static_cast<SDK::UCharaMachineSelectsBase*>(obj)->CharaMachineSelectList;
                int num = list.Num();
                if (num >= 0 && num <= 8) {
                    for (int i = 0; i < num && i < 2; ++i)
                        if (w.Contains(list[i]))
                            SetTranslation(list[i], i == 0 ? -960.0f : 960.0f, true);
                }
            }

            // Shared character icon grid and gadget counter: Monitor 1
            SetTranslation(w["WBP_CharaSelect_Sub_Window"], -960.0f, true);
            SetTranslation(w["WBP_CMN_GadgetCount"], -960.0f);
        }
    }

    // ---------- 2P gadget customisation ----------
    if (name.contains("GadgetCutom_M2") || name.contains("GadgetCustom_M2")) {
        bIs2PlayerRace = true;
        if (bDualMenus && userWidget->WidgetTree && userWidget->WidgetTree->RootWidget) {
            SPDLOG_INFO("HUD: Applying Dual-Monitor layout to {}", name);
            UnclipAndExpand(userWidget->WidgetTree->RootWidget);
            ShiftPlayerSections(userWidget->WidgetTree->RootWidget);
        }
    }

    // ---------- 2P ready screen ----------
    if (name.contains("WBP_Ready_M2_C")) {
        bIs2PlayerRace = true;

        if (bDualMenus) {
            SPDLOG_INFO("HUD: Applying Dual-Monitor layout to WBP_Ready_M2_C");
            WidgetIndex w(userWidget);

            if (userWidget->WidgetTree && userWidget->WidgetTree->RootWidget)
                UnclipAndExpand(userWidget->WidgetTree->RootWidget);

            const SDK::FAnchors anchorsFull{ SDK::FVector2D{ 0.0, 0.0 }, SDK::FVector2D{ 1.0, 1.0 } };
            const SDK::FAnchors anchorsP1{ SDK::FVector2D{ 0.0, 0.0 }, SDK::FVector2D{ 0.5, 1.0 } };
            const SDK::FAnchors anchorsP2{ SDK::FVector2D{ 0.5, 0.0 }, SDK::FVector2D{ 1.0, 1.0 } };

            // Fullscreen backgrounds
            SetSlotAnchors(w["BG_Black"], anchorsFull);
            SetSlotAnchors(w["BG_Light"], anchorsFull);

            // P1 -> left monitor, P2 -> right monitor
            for (auto n : { "Base_BG_P1", "Base_BG_Shadow_P1", "RaceFlag_Left_P1" })
                SetSlotAnchors(w[n], anchorsP1);
            for (auto n : { "Base_BG_P2", "Base_BG_Shadow_P2", "RaceFlag_Right_P2" })
                SetSlotAnchors(w[n], anchorsP2);

            // Selection cards
            for (auto n : { "WBP_Ready_Sub_CharaWindow_P1P3", "WBP_Ready_Sub_Gadget_P1", "WBP_Window_MachineParameter_P1" })
                SetTranslation(w[n], -960.0f, true);
            for (auto n : { "WBP_Ready_Sub_CharaWindow_P2P4", "WBP_Ready_Sub_Gadget_P2", "WBP_Window_MachineParameter_P2" })
                SetTranslation(w[n], 960.0f, true);

            // Standby text and buttons on Monitor 1
            SetTranslation(w["WBP_Ready_Sub_StandByText"], -960.0f);
            SetTranslation(w["WBP_CMN_Sub_BoxBtn"], -960.0f);

            // "GO!" banner: original on Monitor 1, clone on Monitor 2 that copies the original's visibility
            auto decide = w["WBP_Ready_Sub_Decide"];
            if (decide && decide->Class) {
                SetTranslation(decide, -960.0f, true);
                if (auto clone = CreateMonitor2Clone(userWidget, decide->Class, 10, 960.0f))
                    RegisterMenuClone(clone, userWidget, decide, decide->Class->GetName(), true);
            }
        }
    }

    // ---------- Pause menu (2P only: in 1P it must stay centred) ----------
    if (name.contains("WBP_PauseMenu_C") && bDualMenus && bIs2PlayerRace) {
        SPDLOG_INFO("HUD: Applying Dual-Monitor layout to WBP_PauseMenu_C");
        WidgetIndex w(userWidget);

        if (userWidget->WidgetTree && userWidget->WidgetTree->RootWidget) {
            auto root = userWidget->WidgetTree->RootWidget;
            root->SetClipping(SDK::EWidgetClipping::Inherit);

            // Expand blur / black background to both monitors
            if (root->IsA(SDK::UScaleBox::StaticClass())) {
                auto pauseScaleBox = static_cast<SDK::UScaleBox*>(root);
                if (pauseScaleBox->Slots.IsValidIndex(0) && pauseScaleBox->Slots[0] && pauseScaleBox->Slots[0]->Content
                    && pauseScaleBox->Slots[0]->Content->IsA(SDK::UCanvasPanel::StaticClass())) {
                    auto panel = static_cast<SDK::UCanvasPanel*>(pauseScaleBox->Slots[0]->Content);
                    panel->SetClipping(SDK::EWidgetClipping::Inherit);
                    for (int i = 0; i < panel->Slots.Num(); ++i) {
                        auto slot = static_cast<SDK::UCanvasPanelSlot*>(panel->Slots[i]);
                        if (!slot || !slot->Content) continue;
                        std::string childName = slot->Content->GetName();
                        if (childName.contains("Blur") || childName.contains("Black") || childName.contains("BG"))
                            slot->SetOffsets(SDK::FMargin(-960.0f, 0.0f, -960.0f, 0.0f));
                    }
                }
            }
        }

        // Owner_Player_Index (0-based) is a plain BP field, its offset may be stale after a game update:
        // only trust sane values. 0 = P1 -> Monitor 1, 1 = P2 -> Monitor 2.
        int ownerIdx = static_cast<SDK::UWBP_PauseMenu_C*>(obj)->Owner_Player_Index;
        const float shift = (ownerIdx == 1) ? 960.0f : -960.0f;
        for (auto n : { "VBButton", "TXT_ClassName", "Line", "HorizontalBox_RSR",
                        "WBP_CMN_PlayerNumber_Big", "WBP_CMN_PlayerNumber_Big_1P", "WBP_CMN_PlayerNumber_Big_P2",
                        "WS_Icon_Active", "WBP_FooterMenu" })
            SetTranslation(w[n], shift);

        for (auto n : { "Overlay_Option_Btn_03_P1", "WidgetSwitcher_P1", "WBP_GadgetCustom_Sub_GadgetPlate_P1" })
            SetTranslation(w[n], -960.0f);
        for (auto n : { "Overlay_Option_Btn_03_P2", "WidgetSwitcher_P2", "WBP_GadgetCustom_Sub_GadgetPlate_P2" })
            SetTranslation(w[n], 960.0f);
    }

    // ---------- Shared full-screen menus: original on Monitor 1, passive clone on Monitor 2 ----------
    if (bDualMenus && bIs2PlayerRace) {
        const bool bIsSharedMenu = name.contains("ClassSelect") ||
                                   name.contains("CourseSelect") ||
                                   name.contains("RivalSelect") ||
                                   name.contains("RivalCutin") ||
                                   name.contains("RivalChoice") ||
                                   name.contains("GrandPrixTips") ||
                                   name.contains("RaceBefore");

        if (bIsSharedMenu && userWidget->WidgetTree && userWidget->WidgetTree->RootWidget && userWidget->Class) {
            SPDLOG_INFO("HUD: Dual-Monitor shared menu duplicating for {}", name);
            userWidget->WidgetTree->RootWidget->SetClipping(SDK::EWidgetClipping::Inherit);
            // -1920 / +1920 design units = left / right monitor at this widget's DPI scale
            userWidget->WidgetTree->RootWidget->SetRenderTranslation(SDK::FVector2D{ -1920.0f, 0.0f });

            const std::string className = userWidget->Class->GetName();
            if (auto clone = CreateMonitor2Clone(userWidget, userWidget->Class, 0, 1920.0f)) {
                RegisterMenuClone(clone, userWidget, userWidget, className, false);
                SPDLOG_INFO("HUD: Successfully duplicated {} to Monitor 2", className);
            }
        }
    }
}

// SEH safety net: a bad read (e.g. SDK layout drift after a game update) is logged instead of killing the game.
// Kept free of C++ objects with destructors so __try is allowed here.
static void LogWidgetHandlerCrash(SDK::UObject* obj, DWORD code)
{
    SPDLOG_ERROR("HUD: Widgets: exception 0x{:x} while processing widget @ 0x{:x} (last: {}). Skipped.",
        code, reinterpret_cast<uintptr_t>(obj), sWidgetName);
}

static void SafeOnWidgetAddToViewport(SDK::UObject* obj)
{
    DWORD code = 0;
    __try {
        OnWidgetAddToViewport(obj);
        return;
    }
    __except (code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
    }
    g_bIsMenuCloning = false;
    g_CloneOpDepth = 0;
    LogWidgetHandlerCrash(obj, code);
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
                        if (!panel->Slots.IsValidIndex(0) || !panel->Slots[0]) return;
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
                        if (!popupWidget->WidgetTree || !popupWidget->WidgetTree->RootWidget) return;
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
                            if (!safeZone || !safeZone->Slots.IsValidIndex(0) || !safeZone->Slots[0]->Content) return;
                            auto innerPanel = static_cast<SDK::UCanvasPanel*>(safeZone->Slots[0]->Content);
                            if (!innerPanel->Slots.IsValidIndex(0) || !innerPanel->Slots[0]) return;
                            auto panelSlot = static_cast<SDK::UCanvasPanelSlot*>(innerPanel->Slots[0]);

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
                    if (ctx.rdx)
                        SafeOnWidgetAddToViewport(reinterpret_cast<SDK::UObject*>(ctx.rdx));
                });

            // ProcessEvent runs every frame on the game thread: used to keep Monitor 2 clones in sync
            // with their originals and to remove them when the original menu goes away.
            if (bDualMonitor2P && bMirror2PMenus) {
                if (SDK::Offsets::ProcessEvent) {
                    auto peAddr = reinterpret_cast<std::uint8_t*>(exeModule) + SDK::Offsets::ProcessEvent;
                    ProcessEventHook = safetyhook::create_inline(peAddr, reinterpret_cast<void*>(&ProcessEvent_Hook));
                    if (ProcessEventHook)
                        SPDLOG_INFO("HUD: Dual-Monitor clone sync hooked on ProcessEvent");
                    else
                        SPDLOG_ERROR("HUD: Dual-Monitor clone sync: failed to hook ProcessEvent, clones will only be cleaned on menu changes.");
                }
                else {
                    SPDLOG_ERROR("HUD: Dual-Monitor clone sync: ProcessEvent offset unknown, clones will only be cleaned on menu changes.");
                }
            }
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