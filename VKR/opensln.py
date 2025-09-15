import os

genbuild_tag = "[GENBUILD]"

# Path of this script file
main_path = os.path.dirname(os.path.abspath(__file__))

# Working directory path
working_dir_path = os.path.abspath(os.getcwd())

# Build path
build_path = "/engine-build"
sln_name = "NeuroverseEngine.sln"
sln_path = build_path + "/" + sln_name

def genbuild_print(message):

    print(f"{genbuild_tag}: {message}")

def open_sln():

    sln_full_path = main_path + build_path + "/NeuroverseEngine.sln"
    os.startfile(sln_full_path)

def main():

    os.chdir(main_path)

    genbuild_print("Opening engine solution...")
    open_sln()



# !-- entry point --!
main()
os.chdir(working_dir_path)