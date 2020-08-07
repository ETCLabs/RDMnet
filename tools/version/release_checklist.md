## Making a release of RDMnet

- [ ] Make sure submodule dependencies are up-to-date.
- [ ] Make sure copyright year is up-to-date:
  * in all source files
  * in `tools/install/windows/license.rtf`
- [ ] Review and update `CHANGELOG.md`
- [ ] Check `docs/mainpage.md` and `README.md` to make sure they are still accurate
- [ ] Check `ThirdPartySoftware.txt` to make sure it is still accurate and doesn't need any
      additions
- [ ] Check the Github page and make sure it still has accurate information

### After making any changes necessary to address the above:

- [ ] Make sure there is a clean/working build on the `develop` or `release/v*` branch.
- [ ] Create a new version in the "rdmnet_bin/stable" repository on Bintray.
  * Copy the files from the latest development build to the new stable version, renaming them to
    remove the build number.
  * Publish the files and enable them to be shown in the download list.
- [ ] Generate docs for the version
  * Update `TAGFILES` in Doxyfile to refer to correct versions of dependencies
    - You will want to reference the appropriate released versions of the EtcPal and RDM
      dependencies for this generation, but don't check in that change - it should reference "head"
      for the auto-deployed docs.
  * Follow `tools/ci/publish_docs.sh` manually, except replace "head" with version number in the
    form "vM.m"
  * Add the version number to `versions.txt` on the `gh-pages` branch and mark it as latest
  * `PROJECT_NUMBER` in the Doxyfile should be correct but might need to be updated for patch
    versions
- [ ] Merge changes to `stable`
  * `git checkout stable`
  * `git merge --no-ff [develop|release/v*]`
- [ ] Create the release tag in the form `vM.m.p`
  * `git tag -a vM.m.p -m "RDMnet release version M.m.p"`
- [ ] Mark as release on the releases page
- [ ] Create build report
- [ ] Merge `stable` back into `develop`

## Starting work on a new release

- [ ] Update 3-digit versions to next planned version in all relevant places:
  * CMakeLists.txt
  * Doxyfile
- [ ] Pick a new color for Doxygen
