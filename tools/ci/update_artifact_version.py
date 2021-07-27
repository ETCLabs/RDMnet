"""This script checks to see if an artifact version update is necessary before
RDMnet artifacts are built.

If so, it uses EtcLibTool to update versioned input files.
"""
import os
import pathlib
import re
import sys
from etclibtool.version import Version, _read_and_validate_config, update_files

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.realpath(__file__)), "..", ".."))

new_version = os.getenv("NEW_BUILD_VERSION")
pipeline_source = os.getenv("CI_PIPELINE_SOURCE")
if pipeline_source == "web" and re.match("^\d+\.\d+\.\d+\.\d+$", new_version):
    print("Updating versioned input files for versioned build...")

    try:
        new_version = Version.from_string(new_version)
    except ValueError:
        print(f"Error: {new_version} is not a valid version.")
        sys.exit(1)

    config = _read_and_validate_config(
        os.path.join(REPO_ROOT, "tools", "ci", "etclibtool_config.json"),
        pathlib.Path(REPO_ROOT),
        "update-files",
    )
    if not config:
        print("Error: Could not read config at tools/ci/etclibtool_config.json")
        sys.exit(1)

    if not update_files(REPO_ROOT, config["fileTemplates"], new_version):
        print("Error: Could not update versioned input files")
        sys.exit(1)
else:
    print("Not versioned build; skipping versioned input file update")
