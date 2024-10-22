#!/usr/bin/env python3

import os
import shutil

from upstream_utils import Lib, comment_out_invalid_includes, walk_cwd_and_copy_if


def copy_upstream_src(wpilib_root):
    wpical = os.path.join(wpilib_root, "wpical")

    # Delete old install
    for d in [
        "src/main/native/thirdparty/mrcal/src",
        "src/main/native/thirdparty/mrcal/include",
    ]:
        shutil.rmtree(os.path.join(wpical, d), ignore_errors=True)

    files = walk_cwd_and_copy_if(
        lambda dp, f: (f.endswith(".h") or f.endswith(".hh"))
        and not f.endswith("stereo.h")
        and not f.endswith("stereo-matching-libelas.h")
        and not dp.startswith(os.path.join(".", "test")),
        os.path.join(wpical, "src/main/native/thirdparty/mrcal/include"),
    )
    for f in files:
        comment_out_invalid_includes(
            f,
            [
                os.path.join(wpical, "src/main/native/thirdparty/mrcal/include"),
                os.path.join(wpical, "src/main/native/thirdparty/mrcal/generated"),
            ],
        )
        with open(f) as file:
            content = file.read()
        content = content.replace("__attribute__((unused))", "")
        content = content.replace('__attribute__ ((visibility ("hidden")))', "")
        with open(f, "w") as file:
            file.write(content)

    files = walk_cwd_and_copy_if(
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
    for f in files:
        comment_out_invalid_includes(
            f,
            [
                os.path.join(wpical, "src/main/native/thirdparty/mrcal/include"),
                os.path.join(wpical, "src/main/native/thirdparty/mrcal/generated"),
            ],
        )
        with open(f) as file:
            content = file.read()
        content = content.replace('__attribute__ ((visibility ("hidden")))', "")
        with open(f, "w") as file:
            file.write(content)


def main():
    name = "mrcal"
    url = "https://github.com/dkogan/mrcal"
    tag = "0d5426b5851be80dd8e51470a0784a73565a3006"

    mrcal = Lib(name, url, tag, copy_upstream_src)
    mrcal.main()


if __name__ == "__main__":
    main()
