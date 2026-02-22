add_rules("mode.debug", "mode.release")
set_languages("c++23")

add_requires("gtest", { configs = { main = true } })

target("cmdline_test")
    set_kind("binary")
    add_files("cmdline_test.cpp")
    add_deps("cmdline")
    add_packages("gtest")
    set_policy("build.c++.modules", true)
