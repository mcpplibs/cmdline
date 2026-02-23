set_languages("c++23")

target("cmdline")
    set_kind("static")
    add_files("src/*.cppm", { public = true, install = true })
    set_policy("build.c++.modules", true)

if not is_host("macosx") then
    includes("examples", "tests")
end
