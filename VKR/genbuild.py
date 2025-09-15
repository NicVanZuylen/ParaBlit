import os
import subprocess

genbuild_tag = "[GENBUILD]"

# Path of this script file
main_path = os.path.dirname(os.path.abspath(__file__))

# Working directory path
working_dir_path = os.path.abspath(os.getcwd())

def genbuild_print(message):

    print(f"{genbuild_tag}: {message}")

def cmake_configure_engine():

    subprocess.call("cmake -S . -B engine-build", shell=True)

def main():

    os.chdir(main_path)

    genbuild_print("Generating engine build files...")
    cmake_configure_engine()



# !-- entry point --!
main()
os.chdir(working_dir_path)