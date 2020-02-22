"""Script to build, sign and notarize the macOS RDMnet binary package."""
import datetime
import os
import re
import subprocess
import sys
import time

# This script requires an unlocked keychain on the keychain search path with the ETC macOS identities pre-installed.
# It is mostly run in the environment of Azure Pipelines CI, where those prerequisites are met.
MACOS_APPLICATION_SIGNING_IDENTITY = os.getenv(
    "MACOS_APPLICATION_SIGNING_IDENTITY",
    "Developer ID Application: Electronic Theatre Controls, Inc. (8AVSFD7ZED)",
)
MACOS_INSTALLER_SIGNING_IDENTITY = os.getenv(
    "MACOS_INSTALLER_SIGNING_IDENTITY",
    "Developer ID Installer: Electronic Theatre Controls, Inc. (8AVSFD7ZED)",
)

PACKAGE_BUNDLE_ID = "com.etcconnect.pkg.RDMnet"

DEVEL_ID_USERNAME = os.getenv("RDMNET_APPLE_DEVELOPER_ID_USER")
DEVEL_ID_PASSWORD = os.getenv("RDMNET_APPLE_DEVELOPER_ID_PW")

if not DEVEL_ID_USERNAME or not DEVEL_ID_PASSWORD:
    print("Couldn't get credentials to notarize application!")
    sys.exit(1)

###############################################################################
# Codesign
###############################################################################

print("Codesigning binaries...")

subprocess.run(
    [
        "codesign",
        "--force",
        "--sign",
        f"{MACOS_APPLICATION_SIGNING_IDENTITY}",
        "--deep",
        "--timestamp",
        "-o",
        "runtime",
        "build/install/RDMnet Controller Example.app",
    ],
    check=True,
)
subprocess.run(
    [
        "codesign",
        "--force",
        "--sign",
        f"{MACOS_APPLICATION_SIGNING_IDENTITY}",
        "--timestamp",
        "-o",
        "runtime",
        "build/install/bin/rdmnet_broker_example",
    ],
    check=True,
)
subprocess.run(
    [
        "codesign",
        "--force",
        "--sign",
        f"{MACOS_APPLICATION_SIGNING_IDENTITY}",
        "--timestamp",
        "-o",
        "runtime",
        "build/install/bin/rdmnet_device_example",
    ],
    check=True,
)
subprocess.run(
    [
        "codesign",
        "--force",
        "--sign",
        f"{MACOS_APPLICATION_SIGNING_IDENTITY}",
        "--timestamp",
        "-o",
        "runtime",
        "build/install/bin/llrp_manager_example",
    ],
    check=True,
)

###############################################################################
# Build and sign pkg
###############################################################################

print("Building installer package...")
subprocess.run(["packagesbuild", "tools/install/macos/RDMnet.pkgproj"], check=True)

print("Signing installer package...")
subprocess.run(
    [
        "productsign",
        "--sign",
        f"{MACOS_INSTALLER_SIGNING_IDENTITY}",
        "tools/install/macos/build/RDMnet.pkg",
        "RDMnet.pkg",
    ],
    check=True,
)
subprocess.run(["pkgutil", "--check-signature", "RDMnet.pkg"], check=True)

###############################################################################
# Notarize pkg
###############################################################################

print("Notarizing...")
notarize_result = subprocess.run(
    [
        "xcrun",
        "altool",
        "--notarize-app",
        "--primary-bundle-id",
        f"{PACKAGE_BUNDLE_ID}",
        "--username",
        f"{DEVEL_ID_USERNAME}",
        "--password",
        f"{DEVEL_ID_PASSWORD}",
        "--file",
        "RDMnet.pkg",
    ],
    capture_output=True,
    encoding="utf-8",
)

print(notarize_result.stdout)
if notarize_result.returncode != 0:
    sys.exit(1)

notarize_uuid = re.search(
    r"[0-9a-fA-F]{8}\-[0-9a-fA-F]{4}\-[0-9a-fA-F]{4}\-[0-9a-fA-F]{4}\-[0-9a-fA-F]{12}",
    notarize_result.stdout,
)
if not notarize_uuid:
    print("UUID not found in notarization output: {}".format(notarize_result.stdout))
    sys.exit(1)

notarize_uuid = notarize_uuid.group(0)
# check the status of the notarization process

# Slightly delay getting the notarization status to give apple servers time to update their
# status of our pkg upload. If we don't do this, we'll get a UUID not found error since we're
# requesting the status of our pkg a tad bit too fast right after uploading.
time.sleep(5)

# Check notarization status every 30 seconds for up to 20 minutes
for half_minutes in range(0, 40):
    time_str = datetime.time(
        minute=int(half_minutes / 2), second=int((half_minutes % 2) * 30)
    ).strftime("%M:%S")
    print(
        f"Checking notarization request UUID: {notarize_uuid} at {time_str} since notarization upload..."
    )

    notarize_status = subprocess.run(
        [
            "xcrun",
            "altool",
            "--notarization-info",
            f"{notarize_uuid}",
            "-u",
            f"{DEVEL_ID_USERNAME}",
            "-p",
            f"{DEVEL_ID_PASSWORD}",
        ],
        capture_output=True,
        encoding="utf-8",
    )

    print(notarize_status.stdout)
    if notarize_status.returncode != 0:
        sys.exit(1)

    notarize_status_str = None
    for line in notarize_status.stdout.splitlines():
        notarize_status_str = re.search("Status: (.+)", line)
        if notarize_status_str:
            notarize_status_str = notarize_status_str.group(1).strip()
            break

    if notarize_status_str:
        print("Got notarization status: '{}'".format(notarize_status_str))
    else:
        print(
            "Notarization status not found in status request output: {}".format(
                notarize_status.stdout
            )
        )
        sys.exit(1)

    if notarize_status_str == "success":
        # staple the ticket onto the notarized pkg
        print("Stapling ticket to pkg file...")
        staple_result = subprocess.run(
            ["xcrun", "stapler", "staple", "RDMnet.pkg"],
            capture_output=True,
            encoding="utf-8",
        )
        staple_result.check_returncode()
        print(staple_result.stdout)

        if re.search("The staple and validate action worked!", staple_result.stdout):
            print("Done.")
            sys.exit(0)
        else:
            print("Unknown ticket staple status. Notarization failed.")
            sys.exit(1)

    time.sleep(30)

# If we got here, the notarization was not approved; error.
print("Unable to obtain confirmation of notarization approval.")
sys.exit(1)
