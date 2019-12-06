"""Script to create a build report in Jira from Azure Pipelines."""
import os
import requests
import sys

BN_PASSWORD = os.getenv("BUILDNETWORKING_PASSWORD")
THIS_SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
BUILD_DESCRIPTION_TEMPLATE="""
{{noformat}}
{}
{{noformat}}

Installers for RDMnet are available on Bintray:
https://bintray.com/etclabs/rdmnet_bin/latest
"""

def main():
    with open(os.path.join(THIS_SCRIPT_DIR, "..", "version", "imports.txt")) as imports_file:
        jira_imports = imports_file.read().strip()

    with open(os.path.join(THIS_SCRIPT_DIR, "..", "version", "current_version.txt")) as version_file:
        current_version = version_file.read().strip()

    post_body = {
        "fields": {
            "project": {
                "key": "RDMNET"
            },
            "summary": current_version,
            "description": BUILD_DESCRIPTION_TEMPLATE.format(jira_imports),
            "issuetype": {
                "name": "Build"
            }
        }
    }
    post_url = "https://jira.etcconnect.com/rest/api/2/issue"
    try:
        response = requests.post(post_url, json=post_body, auth=("buildnetworking", BN_PASSWORD))
        if response.status_code < 200 or response.status_code >= 300:
            print("An error HTTP response was received with code {}.".format(response.status_code))
            sys.exit(1)
    except Exception:
        # Do not print the exception - risks printing secrets
        print("An error occurred while attempting to invoke the REST API.")
        sys.exit(1)

if __name__ == "__main__":
    main()
