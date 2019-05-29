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
- [ ] Generate docs for the version
  * Update `TAGFILES` in Doxyfile to refer to correct versions of dependencies
  * Follow `tools/ci/publish_docs.sh` manually, except replace "head" with version number in the
    form "vM.m"
  * Add the version number to `versions.txt` on the `gh-pages` branch and mark it as latest
  * `PROJECT_NUMBER` in the Doxyfile should be correct but might need to be updated for patch
    versions
- [ ] Merge changes to `master`
  * `git checkout master`
  * `git merge --no-ff [develop|release/v*]`
- [ ] Create build and tag with `tools/version/create_build.py`
  * Include `-r` flag to specify a release build
- [ ] Mark as release on the releases page
- [ ] Create build report
- [ ] Merge `master` back into `develop`

## Starting work on a new release

- [ ] Update 3-digit versions to next planned version in all relevant places:
  * CMakeLists.txt
  * Doxyfile
- [ ] Pick a new color for Doxygen
