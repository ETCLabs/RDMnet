"""
copy_vc_redist_to_install.py

Find the VC redistributable file and copy to the destination directory.

*** ASSUMES THE SCRIPT IS RUNNING IN THE DIRECTORY THAT cmake RAN FROM ***

Command line arguments
  - path to the CMakeCache.txt file
  - the destination path for "vc_redist.x86.exe" or "vc_redist.x64.exe"

  1. Open the file CMakeCache.txt
  2. Search for key text "CMAKE_LINKER:FILEPATH=", save the corresponding value as the path to the linker
  3. Search the linker path for "/x86/", save as is_x86 = True (or False)
  4. Search the linker path for "/VC/"
  5. Search the files system from the /VC directory for the "vc_redist.x86.exe" or "vc_redist.x64.exe" file
  6. Delete both "vc_redist.x86.exe" and "vc_redist.x64.exe" file from the destination directory
  7. Copy the "vc_redist.x86.exe" or "vc_redist.x64.exe" file to the destination directory
"""

import os
import sys

# -----------------------------------------------------------------------------
# constants

CMAKE_CACHE_FILE_NAME = "CMakeCache.txt"
LINKER_PATH_KEY = "CMAKE_LINKER:FILEPATH="
LINKER_PATH_NOT_FOUND = "LINKER PATH NOT FOUND"
X_86_MARKER = "/x86/"
VC_PATH_MARKER = "/VC/"
REDIST_DIR = "Redist/MSVC"
V_140_MARKER = "v"
X_86_REDIST_FILE_NAME = "vc_redist.x86.exe"
X_64_REDIST_FILE_NAME = "vc_redist.x64.exe"

# -----------------------------------------------------------------------------

def getLinkerPath(cmake_file_name_and_path):
    """
    Get the linker path from the CMakeCache.txt file. The path is found on a line like this:

    CMAKE_LINKER:FILEPATH=C:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Tools/MSVC/14.16.27023/bin/Hostx86/x86/link.exe
    """
    linker_path = LINKER_PATH_NOT_FOUND
    with open(cmake_file_name_and_path) as file:
        for line in file:
            if line.startswith(LINKER_PATH_KEY):
                linker_path = line[len(LINKER_PATH_KEY):].strip()
    file.closed
    return linker_path

# -----------------------------------------------------------------------------

def getRedistPath(linker_path):
    """
    From the linker path, find the path to the VC redistributables
    For example, from this:
      C:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Tools/MSVC/14.16.27023/bin/Hostx86/x86/link.exe
    To this
      C:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Redist/MSVC/14.16.27012
    """
    redist_path = ""
    index = linker_path.find(VC_PATH_MARKER)
    if index != -1:
        vc_path = linker_path[:(index+len(VC_PATH_MARKER))]
        redist_path = vc_path + REDIST_DIR
        found_subdir = False
        sub_dirs = os.listdir(redist_path)
        for sub_dir in sub_dirs:
            if not sub_dir.startswith(V_140_MARKER):
                # exclude directories like 'v140', 'v142', ...
                found_subdir = True
                redist_path = redist_path + "/" + sub_dir
                break
        if not found_subdir:
            redist_path = ""
    return redist_path

# -----------------------------------------------------------------------------

def deleteExistingVcRedistFiles(destination_path):
    """
    Delete the existing VC redistributable files from the 'destination_path'
    """
    destination_name_and_path = destination_path + "\\" + X_86_REDIST_FILE_NAME
    if os.path.isfile(destination_name_and_path):
        os.remove(destination_name_and_path)
    destination_name_and_path = destination_path + "\\" + X_64_REDIST_FILE_NAME
    if os.path.isfile(destination_name_and_path):
        os.remove(destination_name_and_path)

# -----------------------------------------------------------------------------

def copyVcRedistFile(redist_path, is_x86, destination_path):
    """
    Copy the VC redistributable file from the 'redist_path' to the 'destination_path'
    """
    if is_x86:
        file_name = X_86_REDIST_FILE_NAME
    else:
        file_name = X_64_REDIST_FILE_NAME
    file_name_and_path = redist_path + "\\" + file_name
    destination_name_and_path = destination_path + "\\" + file_name
    if os.path.isfile(file_name_and_path):
        command = f'copy "{file_name_and_path}" "{destination_name_and_path}"'
        print(command)
        os.system(command)
        return True
    print(f"Error: file '{file_name_and_path}' not found")
    return False

# -----------------------------------------------------------------------------

def main():
    if len(sys.argv) < 3:
        print("Usage: python copy_vc_redist_to_install.py cmake_file_name_and_path destination_path")
        sys.exit(-1)

    cmake_file_name_and_path = sys.argv[1]
    destination_path = sys.argv[2]

    success = False
    if os.path.isfile(cmake_file_name_and_path):
        linker_path = getLinkerPath(cmake_file_name_and_path)
        if linker_path != LINKER_PATH_NOT_FOUND:
            is_x86 = True
            if linker_path.find(X_86_MARKER) == -1:
                is_x86 = False
            redist_path = getRedistPath(linker_path)
            if redist_path != "":
                deleteExistingVcRedistFiles(destination_path)
                if copyVcRedistFile(redist_path, is_x86, destination_path):
                    success = True
            else:
                print(f"Error: redistributables path not found from linker path {linker_path}")
        else:
            print(f"Error: linker path key {LINKER_PATH_KEY} not found in file {CMAKE_CACHE_FILE_NAME}")
    else:
        print(f"Error: file {g_cmake_file_name_and_path} not found")
    
    if success:
        sys.exit(0)
    sys.exit(-1)

# -----------------------------------------------------------------------------

if __name__ == "__main__":
    main()