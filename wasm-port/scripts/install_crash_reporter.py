#!/usr/bin/env python3
"""Make the crash reporter part of the build instead of something added by hand.

WHAT IT IS. When the wasm module traps, the player sees a frozen screen and the
one line that explains it sits in a console they will never open. crash-report.js
offers to send it: a dialog with the trace, every PYEXC line of the session, the
tail of the console log, the character name, and a box to say what they were
doing. Nothing is sent unless the button is pressed, and the complete payload is
one click away before it goes.

WHY THIS SCRIPT EXISTS. The file was first hung into a SERVED index.html by hand,
which works exactly until the next link -- emcc regenerates index.html from
shell.html and the tag is gone without a word. So the tag belongs in shell.html,
the copy belongs in CMake beside webfs.js, and the name belongs in the engine
archive's file list. All three, or the reporter is present on one build and
missing on the next.

The <script> tag goes in <head>, not at the end of <body>: the handlers have to
be installed before the module can trap, and a trap during boot is exactly the
case worth reporting. It is a plain tag rather than the shell's own
loadScriptRetry, because a crash reporter that fails to load must cost the page
nothing at all -- there is nothing to retry for and nothing to wait on.

The engine archive's contents are a LIST and not a glob, for a reason recorded in
package-web-client.sh: the first archive was packed by hand one file short
(index.dev), everything passed, and the client stopped at "starting..." on a 404.
A file that is present and unlisted is reported by that script too, so adding the
file without adding the name would be caught -- but it would be caught after the
fact, which is not the same as being right.

Idempotent. Run against /opt/m2wasm; a second run reports `already patched'.
"""
import io
import os
import shutil
import sys

ROOT = os.environ.get("M2WASM", "/opt/m2wasm")
REPO = os.environ.get(
    "M2REPO",
    os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

SHELL = os.path.join(ROOT, "tools/wasm/shell.html")
CMAKE = os.path.join(ROOT, "src/UserInterface/CMakeLists.txt")
PACK = os.path.join(REPO, "linux-port/package-web-client.sh")

SHELL_OLD = "</head>\n"
SHELL_NEW = """
<!--
  The crash reporter. In <head> and before everything else, because the traps
  worth reporting include the ones that happen while the module is still coming
  up. A plain tag rather than loadScriptRetry: if this file does not arrive, the
  page must be exactly as it was without it.
-->
<script src="crash-report.js"></script>
</head>
"""

CMAKE_OLD = """        add_custom_command(TARGET Metin2ClientLinux POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_SOURCE_DIR}/tools/wasm/webfs.js"
                    "$<TARGET_FILE_DIR:Metin2ClientLinux>/webfs.js"
            COMMENT "copying webfs.js beside the page")
"""

CMAKE_NEW = """        add_custom_command(TARGET Metin2ClientLinux POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_SOURCE_DIR}/tools/wasm/webfs.js"
                    "$<TARGET_FILE_DIR:Metin2ClientLinux>/webfs.js"
            COMMENT "copying webfs.js beside the page")

        # crash-report.js is a page script for the same reason webfs.js is: the
        # shell loads it with its own tag and emcc never sees it. Missing, the
        # page still works and simply never offers to report anything -- which
        # is the quiet failure this copy exists to prevent.
        add_custom_command(TARGET Metin2ClientLinux POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_SOURCE_DIR}/tools/wasm/crash-report.js"
                    "$<TARGET_FILE_DIR:Metin2ClientLinux>/crash-report.js"
            COMMENT "copying crash-report.js beside the page")
"""

PACK_OLD = "webfs.js\nserve-webfs.py\n"
PACK_NEW = "webfs.js\ncrash-report.js\nserve-webfs.py\n"


def patch(path, old, new, marker, label):
    if not os.path.isfile(path):
        sys.exit("not found: %s" % path)
    s = io.open(path, encoding="utf-8", errors="surrogateescape").read()
    if marker in s:
        print("already patched: %s" % label)
        return False
    if s.count(old) != 1:
        sys.exit("anchor not found exactly once in %s" % path)
    io.open(path, "w", encoding="utf-8", errors="surrogateescape", newline="").write(
        s.replace(old, new, 1))
    print("patched: %s" % label)
    return True


def main():
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "crash-report.js")
    dst = os.path.join(ROOT, "tools/wasm/crash-report.js")
    if os.path.isfile(src):
        shutil.copyfile(src, dst)
        print("placed: tools/wasm/crash-report.js")
    elif not os.path.isfile(dst):
        sys.exit("crash-report.js is neither beside this script nor in the client tree")

    patch(SHELL, SHELL_OLD, SHELL_NEW, 'src="crash-report.js"', "shell.html")
    patch(CMAKE, CMAKE_OLD, CMAKE_NEW, "crash-report.js beside the page", "CMakeLists.txt")
    patch(PACK, PACK_OLD, PACK_NEW, "crash-report.js", "package-web-client.sh")


if __name__ == "__main__":
    main()
