"""Deploys binaries to a GitHub release given the specified tag name."""
import argparse
import os
import time

from github import Github

THIS_FILE_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
GH_REPO_IDENT = "ETCLabs/RDMnet"
GH_USERNAME = "svc-etclabs"
GH_API_TOKEN = os.getenv("SVC_ETCLABS_REPO_TOKEN")

def deploy_binaries(version: str):
    """Deploys staged binaries to a new GitHub Release."""
    g = Github(login_or_token=GH_USERNAME, password=GH_API_TOKEN)
    repo = g.get_repo(GH_REPO_IDENT)

    print(f"Waiting for the correct GitHub tag v{version} to become available...")

    keep_trying = True
    while keep_trying:
        for tag in repo.get_tags():
            if tag.name == f"v{version}":
                keep_trying = False  # Tag now exists
                break

        if keep_trying:
            time.sleep(5)

    print(f"Tag v{version} available. Creating release...")
    new_release = repo.create_git_release(
        tag=f"v{version}",
        name=f"RDMnet v{version}",
        message=f"Automated release of RDMnet for v{version}",
    )
    new_release.upload_asset("RDMnetSetup_x86.msi")
    new_release.upload_asset("RDMnetSetup_x64.msi")
    new_release.upload_asset("RDMnet.pkg")


def main():
    parser = argparse.ArgumentParser(
        description="Deploy RDMnet artifacts to GitHub Release"
    )
    parser.add_argument("version", help="Artifact version being deployed")
    args = parser.parse_args()

    # Make sure our cwd is the root of the repository
    os.chdir(os.path.abspath(os.path.join(THIS_FILE_DIRECTORY, "..", "..")))

    deploy_binaries(args.version)


if __name__ == "__main__":
    main()
