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

if [ "$BUILD_REASON" == "PullRequest" ]; then
  echo 'Documentation will not be published because this is a pull request build.'
  exit 0
fi

if [ "$BRANCH" != "azure-pipelines" ]; then
  echo 'Documentation will not be published because this is not a azure-pipelines branch build.'
  exit 0
fi

echo 'Publishing documentation...'

# Create a clean working directory for this script.
cd $BUILD_DIR/docs
mkdir output
cd output

# Get the current gh-pages branch
git clone -b gh-pages https://git@$GH_REPO_REF
cd $GH_REPO_NAME

##### Configure git.
# Set the push default to simple i.e. push only the current branch.
git config --global push.default simple
# Pretend to be an user called Azure Pipelines.
git config user.name "Azure Pipelines"
git config user.email "azure@microsoft.com"

# Remove everything currently in the relevant documentation directory on the
# gh-pages branch. GitHub is smart enough to know which files have changed and
# which files have stayed the same and will only update the changed files. So
# the directory can be safely cleaned, and it is sure that everything pushed
# later is the new documentation.
rm -rf docs/head/*

# Allow access to lwpa and RDM repositories
git submodule init
git submodule update

################################################################################
##### Generate the Doxygen code documentation and log the output.          #####
echo 'Generating Doxygen code documentation...'
# Redirect both stderr and stdout to the log file AND the console.
cd ../..
( cat Doxyfile ; echo "PROJECT_NUMBER=\"HEAD (unstable)\"" ; \
                 echo "OUTPUT_DIRECTORY=${BUILD_DIR}/docs/output/${GH_REPO_NAME}/docs/head" ; \
                 echo "HTML_OUTPUT=." ) \
                 | doxygen - 2>&1 | tee doxygen.log

cd output/$GH_REPO_NAME

################################################################################
##### Upload the documentation to the gh-pages branch of the repository.   #####
# Only upload if Doxygen successfully created the documentation.
# Check this by verifying that the html directory and the file html/index.html
# both exist. This is a good indication that Doxygen did it's work.
if [ -d "docs/head" ] && [ -f "docs/head/index.html" ]; then

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
      git commit -m "Deploy code docs to GitHub Pages Azure Pipelines build: ${BUILD_NUMBER}" -m "Commit: ${COMMIT}"

      # Force push to the remote gh-pages branch.
      # The ouput is redirected to /dev/null to hide any sensitive credential data
      # that might otherwise be exposed.
      git push --force "https://${GH_REPO_TOKEN}@${GH_REPO_REF}" > /dev/null 2>&1
    else
      echo 'No documentation changes. Doing nothing.'
    fi
else
    echo '' >&2
    echo 'Warning: No documentation (html) files have been found!' >&2
    echo 'Warning: Not going to push the documentation to GitHub!' >&2
    exit 1
fi
