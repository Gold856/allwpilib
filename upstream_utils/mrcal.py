#!/usr/bin/env python3

import os
import shutil
import subprocess

from upstream_utils import (
    walk_cwd_and_copy_if,
    Lib,
)


def copy_upstream_src(wpilib_root):
    wpical = os.path.join(wpilib_root, "wpimath")

    # Delete old install
    for d in [
        "src/main/native/thirdparty/mrcal/src",
        "src/main/native/thirdparty/mrcal/include",
    ]:
        shutil.rmtree(os.path.join(wpical, d), ignore_errors=True)

    walk_cwd_and_copy_if(
        lambda dp, f: (f.endswith(".h") or f.endswith(".hh"))
        and not f.endswith("mrcal-image.h")
        and not f.endswith("stereo.h")
        and not f.endswith("stereo-matching-libelas.h")
        and not dp.startswith(os.path.join(".", "test")),
        os.path.join(wpical, "src/main/native/thirdparty/mrcal/include"),
    )
    walk_cwd_and_copy_if(
        lambda dp, f: (f.endswith(".c") or f.endswith(".cc") or f.endswith(".pl"))
        and not f.endswith("mrcal-pywrap.c")
        and not f.endswith("image.c")
        and not f.endswith("stereo.c")
        and not f.endswith("stereo-matching-libelas.cc")
        and not f.endswith("uncertainty.c")
        and not dp.startswith(os.path.join(".", "doc"))
        and not dp.startswith(os.path.join(".", "test")),
        os.path.join(wpical, "src/main/native/thirdparty/mrcal/src"),
    )


def main():
    name = "mrcal"
    url = "https://github.com/dkogan/mrcal"
    tag = "71c89c4e9f268a0f4fb950325e7d551986a281ec"

    mrcal = Lib(name, url, tag, copy_upstream_src)
    mrcal.main()


if __name__ == "__main__":
    main()
