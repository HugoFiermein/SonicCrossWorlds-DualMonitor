local name = "CrossWorldsFix"

set_project(name)
add_rules("mode.debug", "mode.release")
set_languages("cxxlatest", "clatest")
set_optimize("fastest")

if is_mode("release") then
    set_symbols("hidden")
    set_strip("all")
else
    set_symbols("debug")
    set_strip("none")
end

-- ===================== Zydis =====================
-- hashes Zydis submodule version after a successful build
-- so we only ever rebuild it if the module changes or the folder gets deleted
target("zydis")
    set_kind("static")
    set_policy("build.fence", true)

    before_build(function (target)
        local arch       = is_arch("x64") and "x64" or "Win32"
        local builddir   = ".build_zydis/build"
        local hashfile   = path.join(builddir, ".zydis_build_hash")

        -- current commit hash of submodule
        local zy_hash = os.iorun("git -C external/zydis rev-parse HEAD"):gsub("%s+$", "")
        if not zy_hash or #zy_hash == 0 then
            raise("Could not determine Zydis git hash. Submodule initialized?")
        end

        -- old hash
        local old_hash = nil
        if os.isfile(hashfile) then
            local f = io.open(hashfile, "r")
            if f then
                old_hash = f:read("*l")
                f:close()
            end
        end

        local function build_cfg(cfg)
            cprint("[Zydis] configuring (%s)...", cfg)
            os.exec("cmake -S external/zydis -B " .. builddir ..
                    " -A " .. arch ..
                    " -DZYDIS_BUILD_SHARED_LIB=OFF")
            cprint("[Zydis] building (%s)...", cfg)
            os.exec("cmake --build " .. builddir .. " --config " .. cfg)
        end

        local need_rebuild = (old_hash ~= zy_hash)

        -- Release build
        if need_rebuild or not os.isfile(path.join(builddir, "Release/Zydis.lib")) then
            build_cfg("Release")
            local f = io.open(hashfile, "w")
            f:write(zy_hash)
            f:close()
        else
            cprint("[Zydis] Release build up to date, skipping.")
        end

        -- Debug build (skip in release script)
        if os.getenv("MGD_RELEASE_BUILD") ~= "true" then
            if need_rebuild or not os.isfile(path.join(builddir, "Debug/Zydis.lib")) then
                build_cfg("Debug")
            else
                cprint("[Zydis] Debug build up to date, skipping.")
            end
        else
            cprint("[Zydis] Skipping Debug build in release script.")
        end
    end)

    on_load(function (target)
        local builddir = ".build_zydis/build"
        local cfg      = is_mode("debug") and "Debug" or "Release"

        target:add("includedirs",
            "external/zydis/include",
            "external/zydis/dependencies/zycore/include",
            {public = true}
        )

        target:add("links",
            path.join(builddir, cfg, "Zydis.lib"),
            path.join(builddir, "zycore", cfg, "Zycore.lib"),
            {public = true}
        )

        target:add("defines", "ZYDIS_STATIC_BUILD", "ZYCORE_STATIC_BUILD", {public = true})
    end)

-- ===================== ASI =====================
target(name)
    set_kind("shared")
    set_prefixname("")
    set_extension(".asi")

    add_includedirs("src", "src/resources", "external/spdlog/include", "external/inipp", "external/safetyhook/include")
    add_headerfiles("src/**.h", "src/**.hpp")
    add_files("src/*.cpp", "src/resources/version.rc", "external/safetyhook/src/**.cpp", "src/SDK/Basic.cpp", "src/SDK/CoreUObject_functions.cpp", "src/SDK/Engine_functions.cpp", "src/SDK/UMG_functions.cpp")

    add_deps("zydis")
    add_syslinks("user32")

    if is_plat("windows") then
        set_toolchains("msvc")
        if is_mode("release") then
            set_runtimes("MT")
            add_cxflags("/utf-8")
            add_ldflags("/LTCG", "/GL", "/OPT:REF", "/OPT:ICF")
        else
            set_runtimes("MDd")
            add_cxflags("/utf-8", "/Zi")
            add_defines("_DEBUG")
        end
    end
	
