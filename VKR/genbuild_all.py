import os
import subprocess
import multiprocessing
import platform

genbuild_tag = "[GENBUILD]"

# Path of this script file
main_path = os.path.dirname(os.path.abspath(__file__))

# Working directory path
working_dir_path = os.path.abspath(os.getcwd())

# Path to external libraries directory
external_path = "External/"

def genbuild_print(message):

    print(f"{genbuild_tag}: {message}")

# Clone a git repository (identified via URL) into the provided folder at a commit specified by commit_sha1
def clone_repo(repository_url, folder_name, commit_sha1):

    subprocess.call(f"git clone {repository_url} {folder_name}", shell=True)
    os.chdir(folder_name)
    subprocess.call(f"git reset --hard {commit_sha1}", shell=True)
    os.chdir("../")

def fetch_dependencies():

    # !-- Add calls here to fetch external dependencies via git. --!
    clone_repo("https://github.com/nothings/stb.git", "stb", "5a0bb8b")
    clone_repo("https://github.com/tinyobjloader/tinyobjloader.git", "tinyobjloader", "51908fb")
    clone_repo("https://github.com/glfw/glfw.git", "glfw", "201400b")
    clone_repo("https://github.com/g-truc/glm.git", "glm", "06ed280")
    clone_repo("https://github.com/google/shaderc.git", "shaderc", "e0a5092")
    clone_repo("https://github.com/zeux/meshoptimizer.git", "meshoptimizer", "536f296")
    clone_repo("https://github.com/ocornut/imgui.git", "imgui", "87c1ab7")

def cmake_configure_external_libs():

    if not os.path.exists("build"):
        os.mkdir("build")

    subprocess.call("cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build", shell=True)

def cmake_build(job_count):

    if platform.system() == 'Windows':
        subprocess.call(f"cmake --build build --config Debug --target ALL_BUILD -j {job_count}")
        subprocess.call(f"cmake --build build --config Release --target ALL_BUILD -j {job_count}")
    elif platform.system() == 'Linux':
        subprocess.call(f"cmake --build build -j {job_count}", shell=True)
    

def cmake_configure_engine():

    if platform.system() == 'Windows': # Only needed on Windows to generate the Visual Studio solution. On Linux we use VSCode with CMakeTools.
        subprocess.call("cmake -S . -B engine-build", shell=True)

def main():

    os.chdir(main_path)

    os.chdir(external_path)
    genbuild_print("Fetching dependencies...")
    fetch_dependencies()

    genbuild_print("Generating build files for external dependencies...")
    cmake_configure_external_libs()

    genbuild_print("Building dependencies...")
    build_job_count = multiprocessing.cpu_count()
    cmake_build(build_job_count)

    genbuild_print("Generating engine build files...")
    os.chdir(main_path)
    cmake_configure_engine()



# !-- entry point --!
main()
os.chdir(working_dir_path)