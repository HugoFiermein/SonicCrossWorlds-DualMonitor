#include "stdafx.h"

namespace Memory
{
    template<typename T>
    void Write(std::uint8_t* writeAddress, T value)
    {
        DWORD oldProtect;
        VirtualProtect((LPVOID)(writeAddress), sizeof(T), PAGE_EXECUTE_WRITECOPY, &oldProtect);
        *(reinterpret_cast<T*>(writeAddress)) = value;
        VirtualProtect((LPVOID)(writeAddress), sizeof(T), oldProtect, &oldProtect);
    }

    void PatchBytes(std::uint8_t* address, const char* pattern, unsigned int numBytes)
    {
        DWORD oldProtect;
        VirtualProtect((LPVOID)address, numBytes, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((LPVOID)address, pattern, numBytes);
        VirtualProtect((LPVOID)address, numBytes, oldProtect, &oldProtect);
    }

    std::vector<int> PatternToBytes(const char* pattern)
    {
        auto bytes = std::vector<int>{};
        auto start = const_cast<char*>(pattern);
        auto end = const_cast<char*>(pattern) + strlen(pattern);

        for (auto current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?')
                    ++current;
                bytes.push_back(-1);
            }
            else {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }
        return bytes;
    }

    std::vector<std::uint8_t*> PatternScanInternal(void* module, const char* signature, bool firstMatch)
    {
        auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
        auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<std::uint8_t*>(module) + dosHeader->e_lfanew);
        size_t sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;

        auto patternBytes = PatternToBytes(signature);
        auto scanBytes = reinterpret_cast<std::uint8_t*>(module);

        std::vector<std::uint8_t*> results;
        size_t s = patternBytes.size();
        auto* d = patternBytes.data();

        for (size_t i = 0; i < sizeOfImage - s; ++i)
        {
            bool found = true;
            for (size_t j = 0; j < s; ++j)
            {
                if (scanBytes[i + j] != d[j] && d[j] != -1)
                {
                    found = false;
                    break;
                }
            }
            if (found)
            {
                 if (firstMatch)
                    return { &scanBytes[i] };
                results.push_back(&scanBytes[i]);
            }
        }
        return results;
    }

    std::uint8_t* PatternScan(void* module, const char* signature)
    {
        auto results = PatternScanInternal(module, signature, true);
        return results.empty() ? nullptr : results.front();
    }

    std::vector<std::uint8_t*> PatternScanAll(void* module, const char* signature)
    {
        return PatternScanInternal(module, signature, false);
    }

    std::uint32_t GetModuleTimestamp(void* module)
    {
        auto dosHeader = (PIMAGE_DOS_HEADER)module;
        auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);
        return ntHeaders->FileHeader.TimeDateStamp;
    }

    std::uint8_t* GetRelativeAddr(std::uint8_t* address) noexcept
    {
        if (!address) return nullptr;

        std::int32_t offset = *reinterpret_cast<std::int32_t*>(address);
        return address + sizeof(offset) + offset;
    }

    BOOL HookIAT(HMODULE callerModule, char const* targetModule, const void* targetFunction, void* detourFunction)
    {
        auto* base = (uint8_t*)callerModule;
        const auto* dos_header = (IMAGE_DOS_HEADER*)base;
        const auto nt_headers = (IMAGE_NT_HEADERS*)(base + dos_header->e_lfanew);
        const auto* imports = (IMAGE_IMPORT_DESCRIPTOR*)(base + nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

        for (int i = 0; imports[i].Characteristics; i++)
        {
            const char* name = (const char*)(base + imports[i].Name);
            if (lstrcmpiA(name, targetModule) != 0)
                continue;

            void** thunk = (void**)(base + imports[i].FirstThunk);

            for (; *thunk; thunk++)
            {
                const void* import = *thunk;

                if (import != targetFunction)
                    continue;

                DWORD oldState;
                if (!VirtualProtect(thunk, sizeof(void*), PAGE_READWRITE, &oldState))
                    return FALSE;

                *thunk = detourFunction;

                VirtualProtect(thunk, sizeof(void*), oldState, &oldState);

                return TRUE;
            }
        }
        return FALSE;
    }
}

namespace Util
{
    std::pair<int, int> GetPhysicalDesktopDimensions() 
    {
        if (DEVMODE devMode{ .dmSize = sizeof(DEVMODE) }; EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode))
            return { devMode.dmPelsWidth, devMode.dmPelsHeight };

        return {};
    }

    std::string WStringToString(const std::wstring& wstr) 
    {
        if (wstr.empty()) return {};
        std::string str(wstr.size() * 2, '\0');
        size_t converted = 0;
        wcstombs_s(&converted, &str[0], str.size() + 1, wstr.c_str(), str.size());
        str.resize(converted - 1);
        return str;
    }

    [[noreturn]] void ConsoleExit(const std::string& message, HMODULE module = nullptr)
    {
        AllocConsole();
        freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
        freopen_s(reinterpret_cast<FILE**>(stderr), "CONOUT$", "w", stderr);

        std::cerr << "Fatal Error: " << message << std::endl;

        if (module)
            FreeLibraryAndExitThread(module, 1);

        std::terminate();
    }
}