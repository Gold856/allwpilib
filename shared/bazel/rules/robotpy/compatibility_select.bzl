def robotpy_compatibility_select():
    return select({
        "@wpilib_toolchains//constraints/is_systemcore:systemcore": ["@platforms//:incompatible"],
        "//conditions:default": [],
    })
