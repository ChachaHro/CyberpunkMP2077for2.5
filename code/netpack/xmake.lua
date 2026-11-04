add_requires("cxxopts")

target("NetPack")
    set_kind("binary")
    add_files("**.cpp", "**.cc")
    set_group("Tools")
    add_headerfiles("**.h", "**.hpp", "**.inl")
    set_policy("build.across_targets_in_parallel", false)

    set_pcxxheader("NetPackPCH.h")
    add_includedirs(
        ".", 
        "../../build", 
        "../../vendor"
    )

    add_deps("Common")

    add_packages(
        "mimalloc",
        "spdlog",
        "hopscotch-map",
        "cxxopts",
        "protobuf-cpp",
        "abseil")

    -- Linux (ld GNU, sensible à l'ordre) : libabsl_hash.a appelle LowLevelHashLenGt16 défini dans
    -- libabsl_low_level_hash.a, placée avant elle par le paquet. On la relie une fois de plus en fin
    -- de ligne (les syslinks passent après les liens des paquets).
    if is_plat("linux") then
        add_syslinks("absl_low_level_hash", "absl_city")
    end
