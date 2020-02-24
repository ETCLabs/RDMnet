"""
Generate the RDMnet Doxygen.

Does a few value adds over basic doxygen, like configuration of where the doxygen is deployed in
various CI configurations, printing warnings in a format that can be parsed by Azure Pipelines, and
the addition of language selector panels on the how-to markdown pages.
"""

import os
import re
import shutil
import subprocess
import sys
import time

THIS_SCRIPT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))

LANGUAGE_SELECTOR_TEXT = r"""
\htmlonly
<div class="tab">
  <button id="button_{0}_c" class="tablinks" onclick="selectAllC()">Show All C Snippets</button>
  <button id="button_{0}_cpp" class="tablinks" onclick="selectAllCpp()">Show All C++ Snippets</button>
</div>
\endhtmlonly
"""
PANEL_START_TEXT = r"""
\htmlonly
<div class="tab">
  <button id="button_{0}_c" class="tablinks" onclick="selectLanguage(event, '{0}', 'c')">C</button>
  <button id="button_{0}_cpp" class="tablinks" onclick="selectLanguage(event, '{0}', 'cpp')">C++</button>
</div>
<div id="div_{0}_c" class="tabcontent">
\endhtmlonly
"""
PANEL_MID_TEXT = r"""
\htmlonly
</div>
<div id="div_{0}_cpp" class="tabcontent">
\endhtmlonly
"""
PANEL_END_TEXT = r"""
\htmlonly
</div>
\endhtmlonly
"""

# Substitute special sentinels in the how-to markdown files with constructs which will render as
# C/C++ language-selector panels on the HTML output.

print("Converting markdown files...")

try:
    shutil.rmtree(os.path.join(THIS_SCRIPT_DIRECTORY, "GeneratedFiles"), ignore_errors=True)
    os.makedirs(os.path.join(THIS_SCRIPT_DIRECTORY, "GeneratedFiles"))
except OSError:
    pass

markdown_source_directories = [
    os.path.join(THIS_SCRIPT_DIRECTORY, "getting_started"),
    os.path.join(THIS_SCRIPT_DIRECTORY, "how_rdmnet_works"),
]

markdown_files = []
for directory in markdown_source_directories:
    markdown_files.extend(
        os.path.join(directory, file_name)
        for file_name in os.listdir(directory)
        if os.path.isfile(os.path.join(directory, file_name)) and file_name.endswith(".md")
    )

for file_path in markdown_files:
    panel_number = 1
    with open(file_path, "r") as file_handle:
        with open(
            os.path.join(THIS_SCRIPT_DIRECTORY, "GeneratedFiles", os.path.basename(file_path)), "w"
        ) as out_file:
            for line in file_handle.readlines():
                new_line = line.replace(
                    "<!-- CODE_BLOCK_START -->", PANEL_START_TEXT.format(panel_number)
                )
                new_line = new_line.replace(
                    "<!-- CODE_BLOCK_MID -->", PANEL_MID_TEXT.format(panel_number)
                )
                new_line = new_line.replace(
                    "<!-- CODE_BLOCK_END -->", PANEL_END_TEXT.format(panel_number)
                )
                new_line = new_line.replace(
                    "<!-- LANGUAGE_SELECTOR -->", LANGUAGE_SELECTOR_TEXT.format(panel_number)
                )
                if "<!-- CODE_BLOCK_END -->" in line or "<!-- LANGUAGE_SELECTOR -->" in line:
                    panel_number += 1
                out_file.write(new_line)

# Generate the docs.

print("Generating Doxygen and capturing warnings...")

if os.getenv("BUILD_REASON", "IndividualCI") == "PullRequest":
    project_number = "Staging for pull request {}".format(
        os.getenv("SYSTEM_PULLREQUEST_PULLREQUESTNUMBER")
    )
    output_dir = "build/{}/docs/stage/{}".format(
        os.getenv("GH_REPO_NAME", "RDMnet"), os.getenv("SYSTEM_PULLREQUEST_PULLREQUESTNUMBER"),
    )
else:
    project_number = "HEAD (unstable)"
    output_dir = "build/{}/docs/head".format(os.getenv("GH_REPO_NAME", "RDMnet"))

# Remove the relevant documentation directory - git will resolve the changes when the documentation
# is regenerated.
shutil.rmtree(output_dir, ignore_errors=True)
os.makedirs(output_dir)

with open(os.path.join(THIS_SCRIPT_DIRECTORY, "Doxyfile"), "r") as doxyfile:
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
                        match.group(1), match.group(2), match.group(3).replace("warning: ", "")
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
