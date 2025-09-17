Import("env")
import json
import os
import shutil
from sys import platform

pio_home = env.subst("$PROJECT_CORE_DIR")
print("PLATFORMIO_DIR" + pio_home)

if platform == "linux" or platform == "linux2":
    # linux
    #devkitm = '/home/rise/.platformio/platforms/espressif32/boards/esp32-c6-devkitm-1.json'
    #devkitc = '/home/rise/.platformio/platforms/espressif32/boards/esp32-c6-devkitc-1.json'    
    devkitm = pio_home + '/platforms/espressif32/boards/esp32-c6-devkitm-1.json'
    devkitc = pio_home + '/platforms/espressif32/boards/esp32-c6-devkitc-1.json'     
else:
    # windows
    devkitm = pio_home + '\\platforms\\espressif32\\boards\\esp32-c6-devkitm-1.json'
    devkitc = pio_home + '\\platforms\\espressif32\\boards\\esp32-c6-devkitc-1.json'    

def add_arduino_to_frameworks(file_name):
    try:
        with open(file_name, 'r+') as f:
            data = json.load(f)
            frameworks = data['frameworks']
            if 'arduino' not in frameworks:
                frameworks.insert(frameworks.index('espidf') + 1, 'arduino')
                data['frameworks'] = frameworks
                f.seek(0)
                json.dump(data, f, indent=4)
                f.truncate()
                print(f"Файл изменён, ОК! {file_name}")
            else:
                print(f"Arduino already exists in {file_name}")
    except FileNotFoundError:
        print("Файл не найден или не удается открыть")

if os.path.exists(devkitm) and os.path.exists(devkitc):
    add_arduino_to_frameworks(devkitm)
    add_arduino_to_frameworks(devkitc)
else:
    print("One or both files do not exist.")