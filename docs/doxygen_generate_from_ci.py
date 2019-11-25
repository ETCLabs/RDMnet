import os
import re
import shutil
import subprocess
import sys

THIS_SCRIPT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))

print("Generating Doxygen and capturing warnings...")

if os.getenv("BUILD_REASON", "IndividualCI") == "PullRequest":
    project_number = "Staging for pull request {}".format(
        os.getenv("SYSTEM_PULLREQUEST_PULLREQUESTNUMBER")
    )
    output_dir = "build/{}/docs/stage/{}".format(
        os.getenv("GH_REPO_NAME", "RDMnet"),
        os.getenv("SYSTEM_PULLREQUEST_PULLREQUESTNUMBER"),
    )
else:
    project_number = "HEAD (unstable)"
    output_dir = "build/{}/docs/head".format(os.getenv("GH_REPO_NAME", "RDMnet"))

# Remove the relevant documentation directory - git will resolve the changes when the documentation
# is regenerated.
shutil.rmtree(output_dir, ignore_errors=True)
os.makedirs(output_dir)

with open("Doxyfile", "r") as doxyfile:
    dox_input = doxyfile.read()
    dox_input += '\nPROJECT_NUMBER="{}"'.format(project_number)
    dox_input += '\nOUTPUT_DIRECTORY="{}"'.format(output_dir)
    dox_input += "\nHTML_OUTPUT=."
    process_result = subprocess.run(
        ["doxygen", "-"], input=dox_input.encode("utf-8"), capture_output=True
    )

# Print normal output, then print warnings or errors formatted for the Azure Pipelines task.
print(process_result.stdout.decode("utf-8"))

print("\nISSUES:\n")
num_issues = 0
# Check to see if we are running on Azure Pipelines.
if os.getenv("BUILD_BUILDID"):
    for line in process_result.stderr.decode("utf-8").splitlines():
        num_issues += 1
        if "error:" in line:
            print("##vso[task.logissue type=error]{}".format(line))
        else:
            match = re.match(r"(.+):(\d+):(.+)", line)
            if match:
                print(
                    "##vso[task.logissue type=warning;sourcepath={};linenumber={};columnnumber=0]{}".format(
                        match.group(1), match.group(2), match.group(3)
                    )
                )
            else:
                print("##vso[task.logissue type=warning]{}".format(line))
else:
    decoded = process_result.stderr.decode("utf-8")
    num_issues = len(decoded.splitlines())
    print(decoded)

print("\n{} issues captured.".format(num_issues))

sys.exit(process_result.returncode)
