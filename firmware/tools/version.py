# SPDX-License-Identifier: GPL-3.0-or-later
# PlatformIO pre-build script: stamps the firmware version.
#
# Version = v.MAJOR.MINOR.YYYYMMDDTHHMMSSZ
#   MAJOR.MINOR comes from the VERSION file.
#   The last part is the committer time of HEAD, in UTC.
#   "-modified" is appended when `git status --porcelain` lists anything.
# The short sha of HEAD is stamped too.

import datetime
import os
import subprocess

Import("env")  # noqa: F821  (provided by PlatformIO)

project_dir = env["PROJECT_DIR"]  # noqa: F821


def git(*args):
    return subprocess.run(
        ["git", *args], cwd=project_dir, check=True, capture_output=True, text=True
    ).stdout.strip()


with open(os.path.join(project_dir, "VERSION"), encoding="utf-8") as f:
    major_minor = f.read().strip()

commit_time = int(git("show", "-s", "--format=%ct", "HEAD"))
stamp = datetime.datetime.fromtimestamp(commit_time, datetime.timezone.utc)
short_sha = git("rev-parse", "--short=7", "HEAD")
modified = git("status", "--porcelain") != ""

version = f"v.{major_minor}.{stamp:%Y%m%dT%H%M%SZ}" + ("-modified" if modified else "")
# Short form for the small display, for example "v.0.1 261104T2215Z".
short = f"v.{major_minor} {stamp:%y%m%dT%H%MZ}" + ("*" if modified else "")

print(f"GERARD firmware version {version} ({short_sha})")

env.Append(  # noqa: F821
    CPPDEFINES=[
        ("FW_VERSION", env.StringifyMacro(version)),  # noqa: F821
        ("FW_VERSION_SHORT", env.StringifyMacro(short)),  # noqa: F821
        ("FW_SHA", env.StringifyMacro(short_sha)),  # noqa: F821
    ]
)
