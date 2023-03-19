import subprocess
from datetime import datetime

Import("env")

def is_custom():
    ret = subprocess.run(["git", "diff","--name-only"], stdout=subprocess.PIPE, text=True) #Uses only annotated tags
    #ret = subprocess.run(["git", "describe", "--tags"], stdout=subprocess.PIPE, text=True) #Uses any tags
    if len(ret.stdout.strip())>0:
        return True
    return False

def get_firmware_specifier_build_flag():
    version = subprocess.run(["git", "describe", "--tags"], stdout=subprocess.PIPE, text=True).stdout.strip() #Uses only annotated tags
    hash = subprocess.run(["git", "rev-parse", "--short", "HEAD"], stdout=subprocess.PIPE, text=True).stdout.strip() #Uses only annotated tags
    #ret = subprocess.run(["git", "describe", "--tags"], stdout=subprocess.PIPE, text=True) #Uses any tags
    build_version = version+hash

    if is_custom():
        build_version="custom-"+datetime.now().strftime("%d.%m.%Y.%H.%M.%S")

    build_flag = "-D AUTO_VERSION=\\\"" + build_version + "\\\""
    print ("Firmware Revision: " + build_version)
    return (build_flag)

env.Append(
    BUILD_FLAGS=[get_firmware_specifier_build_flag()]
)