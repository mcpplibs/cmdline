add_rules("mode.debug", "mode.release")
set_languages("c++23")

add_requires("gtest", { configs = { main = true, gmock = false } })

target("cmdline_test")
    set_kind("binary")
    add_files("cmdline_test.cpp")
    add_deps("cmdline")
    add_packages("gtest")
    set_policy("build.c++.modules", true)
    -- MSVC 不会从静态库扫描入口点，需要 /WHOLEARCHIVE 把 gtest_main.lib
    -- 强制整体链入，否则报 LNK1561: entry point must be defined。
    if is_plat("windows") then
        add_ldflags("/wholearchive:gtest_main.lib", { force = true })
    end
