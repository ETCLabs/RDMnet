#!/bin/sh

# Adapted from the EXTREMELY helpful example script by Jeroen de Bruijn,
# available here: https://gist.github.com/vidavidorra/548ffbcdae99d752da02
#
# This script will generate Doxygen documentation and push the documentation to
# the gh-pages branch of a repository specified by GH_REPO_REF.
# Before this script is used there should already be a gh-pages branch in the
# repository.
# 
################################################################################

################################################################################
##### Setup this script and get the current gh-pages branch.               #####

# Exit with nonzero exit code if anything fails
set -e

# Allow access to EtcPal and RDM repositories
cd ${BUILD_SOURCESDIRECTORY}
git submodule init
git submodule update

echo 'Publishing documentation...'

# Create a clean working directory for this script.
cd docs
mkdir build
cd build

# Get the current gh-pages branch
git clone -b gh-pages https://git@${GH_REPO_REF}
cd ${GH_REPO_NAME}

##### Configure git.
# Set the push default to simple i.e. push only the current branch.
git config --global push.default simple
# Pretend to be an user called Azure Pipelines.
git config user.name "Azure Pipelines"
git config user.email "azure@microsoft.com"

################################################################################
##### Generate the Doxygen code documentation and log the output.          #####
cd ../..
python doxygen_generate_from_ci.py
cd build/${GH_REPO_NAME}

################################################################################
##### Upload the documentation to the gh-pages branch of the repository.   #####

# Add everything in this directory (the Doxygen code documentation) to the
# gh-pages branch.
# GitHub is smart enough to know which files have changed and which files have
# stayed the same and will only update the changed files.
git add --all

# Check to see if there are any differences in the documentation.
if ! git diff-index --quiet HEAD; then
  echo 'Uploading documentation to the gh-pages branch...'
  # Commit the added files with a title and description containing the Azure Pipelines
  # build number and the GitHub commit reference that issued this build.
  git commit -m "Deploy code docs to GitHub Pages from Azure Pipelines" -m "Azure Pipelines build: ${BUILD_BUILDNUMBER}" -m "Commit: ${BUILD_SOURCEVERSION}"

  # Force push to the remote gh-pages branch.
  # The ouput is redirected to /dev/null to hide any sensitive credential data
  # that might otherwise be exposed.
  git push --force "https://${SVC_ETCLABS_CREDENTIALS}@${GH_REPO_REF}" > /dev/null 2>&1
else
  echo 'No documentation changes. Doing nothing.'
fi
