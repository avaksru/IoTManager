# правим  %USERPROFILE%\.platformio\platforms\espressif8266\builder\main.py 103-115
# для добавления возможности прошивки 16мб модуля esp8266
Import("env")

import os
import shutil
from sys import platform

pio_home = env.subst("$PROJECT_CORE_DIR")
print("PLATFORMIO_DIR" + pio_home)

if platform == "linux" or platform == "linux2":
    #mainPyPath = '/home/rise/.platformio/platforms/espressif8266@4.0.1/builder/main.py'
    mainPyPath = pio_home + '/platforms/espressif8266@4.0.1/builder/main.py'
else:
    #mainPyPath = os.environ['USERPROFILE'] + '\\.platformio\\platforms\\espressif8266@4.0.1\\builder\\main.py'
    mainPyPath = pio_home +  '\\platforms\\espressif8266@4.0.1\\builder\\main.py'

print("FIX 16Mb path: " + mainPyPath)

try:
    with open(mainPyPath) as fr:
        oldData = fr.read()
        if not 'if _value == -0x6000:' in oldData:
            shutil.copyfile(mainPyPath, mainPyPath+'.bak')
            newData = oldData.replace('_value += 0xE00000  # correction', '_value += 0xE00000  # correction\n\n        if _value == -0x6000:\n            _value = env[k]-0x40200000')
            with open(mainPyPath, 'w') as fw:
                fw.write(newData)
                print(f"Файл изменён, ОК! {mainPyPath}")
except FileNotFoundError:
    print("Файл не найден или не удается открыть")
