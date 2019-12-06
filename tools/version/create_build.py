"""Script to create a new versioned build of RDMnet.

Updates the versioned input files
Prompts the user to review before committing and tagging

This script is meant to be run by an RDMnet developer on a development workstation. It assumes that
git is already configured globally and git commands can be run without further configuration.
"""
import argparse
import os
import sys
import datetime

try:
    import git
except ImportError as ie:
    print('ImportError: {}'.format(ie))
    print('Try "pip install gitpython" before retrying.')
    sys.exit(1)


FILE_TEMPLATE_DIR = os.path.join('tools', 'version', 'templates')
FILE_IN_PATHS = [
    os.path.join(FILE_TEMPLATE_DIR, 'version.h.in'),
    os.path.join(FILE_TEMPLATE_DIR, 'vars.wxi.in'),
    os.path.join(FILE_TEMPLATE_DIR, 'current_version.txt.in'),
]
FILE_OUT_PATHS = [
    os.path.join('include', 'rdmnet', 'version.h'),
    os.path.join('tools', 'install', 'windows', 'vars.wxi'),
    os.path.join('tools', 'version', 'current_version.txt')
]
COMMIT_MSG_TEMPLATE = 'Update version files for RDMnet build {}'
TAG_MSG_TEMPLATE = 'RDMnet version {}'
RELEASE_TAG_MSG_TEMPLATE = 'RDMnet release version {}'


def parse_version(vers_string):
    """Parse a version string into its component parts. Returns a list of ints representing the
    version, or None on parse error."""
    vers_string = vers_string.strip()
    vers_parts = vers_string.split('.')
    if len(vers_parts) != 4:
        return None

    try:
        major = int(vers_parts[0])
        minor = int(vers_parts[1])
        patch = int(vers_parts[2])
        build = int(vers_parts[3])
    except ValueError:
        return None

    return [major, minor, patch, build]

IMPORTS = {
    'EtcPal': '@ETCPAL_VERSION_STRING@',
    'RDM': '@RDM_VERSION_STRING@'
}
IMPORT_TEMPLATE_FILE = os.path.join(FILE_TEMPLATE_DIR, 'imports.txt.in')
IMPORT_OUT_FILE = os.path.join('tools', 'version', 'imports.txt')

def resolve_import_versions(repo_root, version):
    """Update the version record of the submodule dependencies."""
    version_str = '{}.{}.{}.{}'.format(version[0], version[1], version[2], version[3])

    with open(os.path.join(repo_root, IMPORT_TEMPLATE_FILE), 'r') as import_file:
        import_file_contents = import_file.read()

    import_file_contents = import_file_contents.replace('@RDMNET_VERSION_STRING@', version_str)

    for import_name, import_var in IMPORTS.items():
        import_version_file_path = os.path.join(repo_root, 'external', import_name, 'tools',
                                                'version', 'current_version.txt')
        with open(import_version_file_path, 'r') as import_version_file:
            import_version = import_version_file.read().strip()
            import_file_contents = import_file_contents.replace(import_var, import_version)

    with open(os.path.join(repo_root, IMPORT_OUT_FILE), 'w') as out_file:
        out_file.write(import_file_contents)


def update_version_files(repo_root, version):
    """Update RDMnet files with version information in them with the new version information.
    Returns True on success, false otherwise."""

    in_file_handles = []
    out_file_handles = []

    # First make sure we can open each file for reading or writing as appropriate.
    for in_file, out_file in zip(FILE_IN_PATHS, FILE_OUT_PATHS):
        in_file_path = os.path.join(repo_root, in_file)
        out_file_path = os.path.join(repo_root, out_file)
        try:
            in_file_handles.append(open(in_file_path, mode='r', encoding='utf8'))
            out_file_handles.append(open(out_file_path, mode='w', encoding='utf8'))
        except OSError as e:
            print('Error while trying to open files {} and {}: {}'.format(in_file_path, out_file_path, e))
            return False

    today = datetime.date.today()
    version_str = '{}.{}.{}.{}'.format(
        version[0], version[1], version[2], version[3])

    # Then copy each template file to the output file, replacing the flagged values.
    for in_file, out_file in zip(in_file_handles, out_file_handles):
        for line in in_file.readlines():
            line = line.replace('@RDMNET_VERSION_MAJOR@', str(version[0]))
            line = line.replace('@RDMNET_VERSION_MINOR@', str(version[1]))
            line = line.replace('@RDMNET_VERSION_PATCH@', str(version[2]))
            line = line.replace('@RDMNET_VERSION_BUILD@', str(version[3]))
            line = line.replace('@RDMNET_VERSION_STRING@', version_str)
            line = line.replace('@RDMNET_VERSION_DATESTR@', today.strftime('%d.%b.%Y'))
            line = line.replace('@RDMNET_VERSION_COPYRIGHT@', 'Copyright ' + str(today.year) + ' ETC Inc.')
            out_file.write(line)
        in_file.close()
        out_file.close()

    return True


def prompt_to_continue():
    """Prompt the user for whether they would like to continue with the commit and tag operation."""
    print("Check the updated version files, then press 'y' to commit and tag. " +
          "Press any other key to abort.")
    choice = input().strip().lower()
    return True if choice == 'y' else False


def commit_and_tag(repo, new_version):
    """Commit the updated version files and tag the version."""
    index = repo.index

    # Add all of our version files
    out_file_abs_paths = [os.path.join(repo.working_tree_dir, out_file) for out_file in FILE_OUT_PATHS]
    out_file_abs_paths.append(os.path.join(repo.working_tree_dir, IMPORT_OUT_FILE))
    index.add(out_file_abs_paths)

    # Commit and tag
    vers_string = '{}.{}.{}.{}'.format(new_version[0], new_version[1], new_version[2], new_version[3])
    index.commit(COMMIT_MSG_TEMPLATE.format(vers_string))
    repo.create_tag('v' + vers_string, message=TAG_MSG_TEMPLATE.format(vers_string))


def main():
    """The script entry point."""

    # Parse the command-line arguments.
    parser = argparse.ArgumentParser(description='Create a new versioned build of RDMnet')
    parser.add_argument('new_version', help='New version number (format M.m.p.b)')
    args = parser.parse_args()

    new_version = parse_version(args.new_version)
    if not new_version:
        parser.print_usage()
        print('new_version: Format error.')
        sys.exit(1)

    repo_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
    rdmnet_repo = git.Repo(repo_root)

    if not update_version_files(repo_root, new_version):
        sys.exit(1)

    if not prompt_to_continue():
        sys.exit(0)

    commit_and_tag(rdmnet_repo, new_version)

    print("Done - now push using 'git push origin [branch] --tags'.")


if __name__ == '__main__':
    main()
