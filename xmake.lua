set_languages("c++23")

target("cmdline")
    set_kind("static")
    add_files("src/*.cppm", { public = true, install = true })
    set_policy("build.c++.modules", true)

includes("examples", "tests")
