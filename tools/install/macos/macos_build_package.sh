#!/bin/sh

# This script requires an unlocked keychain on the keychain search path with the ETC macOS identities pre-installed.
# It is mostly run in the environment of Azure Pipelines CI, where those prerequisites are met.
MACOS_APPLICATION_SIGNING_IDENTITY=${MACOS_APPLICATION_SIGNING_IDENTITY:-"Developer ID Application: Electronic Theatre Controls, Inc. (8AVSFD7ZED)"}
MACOS_INSTALLER_SIGNING_IDENTITY=${MACOS_INSTALLER_SIGNING_IDENTITY:-"Developer ID Installer: Electronic Theatre Controls, Inc. (8AVSFD7ZED)"}

echo "Codesigning binaries..."
codesign --force --sign "${MACOS_APPLICATION_SIGNING_IDENTITY}" --deep --timestamp -o runtime build/install/RDMnet\ Controller\ Example.app || exit 1
codesign --force --sign "${MACOS_APPLICATION_SIGNING_IDENTITY}" --timestamp -o runtime build/install/bin/rdmnet_broker_example || exit 1
codesign --force --sign "${MACOS_APPLICATION_SIGNING_IDENTITY}" --timestamp -o runtime build/install/bin/rdmnet_device_example || exit 1
codesign --force --sign "${MACOS_APPLICATION_SIGNING_IDENTITY}" --timestamp -o runtime build/install/bin/llrp_manager_example || exit 1

echo "Building installer package..."
packagesbuild tools/install/macos/RDMnet.pkgproj || exit 1

# echo "Signing installer package..."
productsign --sign "Developer ID Installer: Electronic Theatre Controls, Inc. (8AVSFD7ZED)" tools/install/macos/build/RDMnet.pkg RDMnet.pkg || exit 1
pkgutil --check-signature RDMnet.pkg || exit 1

echo "Done."
