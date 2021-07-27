# Making a release of RDMnet

- [ ] Make sure submodule dependencies are up-to-date.
- [ ] Make sure copyright year is up-to-date:
  - in all source files
  - in `tools/install/windows/license.rtf`
- [ ] Review and update `CHANGELOG.md`
- [ ] Check `docs/mainpage.md` and `README.md` to make sure they are still accurate
- [ ] Check `ThirdPartySoftware.txt` to make sure it is still accurate and doesn't need any
      additions
- [ ] Check the [Github page](https://etclabs.github.io/RDMnetDocs) and make sure it still has
      accurate information

## After making any changes necessary to address the above:

- [ ] Make sure there is a clean/working build on the `main` branch.
- [ ] Make sure a 4-digit tag `vM.m.p.b` is pointing at the latest commit on the develop branch. If
      not, make a 4-digit build first.
- [ ] Create the release tag in the form `vM.m.p`
  - `git tag -a vM.m.p -m "RDMnet release version M.m.p"`
  - `git push origin --tags`
- [ ] Mark as release on the releases page
- [ ] Copy the release artifacts from the most recent 4-digit build to the 3-digit release
- [ ] Send a notification that the version has been released

## Starting work on a new release

- [ ] Update 3-digit versions to next planned version in all relevant places:
  - CMakeLists.txt
  - Doxyfile
- [ ] Add the previous release to `docs.multiVersion` in `etclibtool_config.json`.
