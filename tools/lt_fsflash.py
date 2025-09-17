
# c:\Users\bmw\.platformio\penv\Scripts\pio run -t buildfs 
#c:\Users\bmw\.platformio\penv\.libretiny\Scripts\ltchiptool -v flash write lt_littlefs.bin --start 0x1db000 -f BK7231N -c



Import("env")

# Определите имя таргета
target_name = "flashfs"

# Получите путь к выходному файлу LittleFS из переменной среды env
output_file = env.subst("$PROJECT_DIR") + "/lt_littlefs.bin"

# Получите путь к mklittlefs.exe из переменной среды env
ltchiptool_path = "${LTCHIPTOOL}"
# Определите команду, которая будет выполнена при вызове таргета
command = "{} flash write {} -s 0x1db000 -c -f BK7231N ".format(ltchiptool_path, output_file)

# Добавьте кастомный таргет
env.AddCustomTarget(
    name=target_name,
    dependencies=None,
    actions=[command],
    title="Flash LittleFS",
    description="Flash LittleFS file system"
)


# Определите путь к исполняемому файлу ltchiptool



print("Custom target '{}' added!".format(target_name))