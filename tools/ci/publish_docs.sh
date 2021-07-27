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
echo 'Publishing documentation...'
# Exit with nonzero exit code if anything fails
set -e

# Create a clean working directory for this script.
cd ${CI_PROJECT_DIR}/docs
mkdir build
cd build

# Get the current gh-pages branch
git clone https://git@${GH_REPO_REF}
cd ${GH_REPO_NAME}

##### Configure git.
# Set the push default to simple i.e. push only the current branch.
git config --global push.default simple
# Pretend to be an user called ETCLabs CI.
git config user.name "ETCLabs CI"
git config user.email "noreply.etclabs@etcconnect.com"

### Remove all existing files (except the .git directory).
git rm -rf .
git clean -fxd
git reset

################################################################################
##### Generate the Doxygen code documentation and log the output.          #####
cd ../../..
etclibtool docs -c tools/ci/etclibtool_config.json -o docs/build/${GH_REPO_NAME} . 1.9.1
cd docs/build/${GH_REPO_NAME}

################################################################################
##### Upload the documentation to the gh-pages branch of the repository.   #####

# Add everything in this directory (the Doxygen code documentation) to the
# gh-pages branch.
# GitHub is smart enough to know which files have changed and which files have
# stayed the same and will only update the changed files.
git add --all

# Check to see if there are any differences in the documentation.
if ! git diff-index --quiet HEAD; then
  echo 'Uploading documentation...'
  # Commit the added files with a title and description containing the pipeline
  # number and the commit reference that issued this build.
  git commit -m "Deploy code docs to GitHub Pages " -m "Pipeline: ${CI_PIPELINE_ID}" -m "Commit: ${CI_COMMIT_SHA}"

  # Force push to the remote gh-pages branch.
  git push --force "https://${GH_REPO_TOKEN}@${GH_REPO_REF}"
else
  echo 'No documentation changes. Doing nothing.'
fi
