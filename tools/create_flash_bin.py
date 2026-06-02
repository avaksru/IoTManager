#!/usr/bin/env python3
# Скрипт для создания объединенного bin файла прошивки ESP
# Поддерживает ESP32, ESP32-S3, ESP32-C6
#
# Использование: 
#   python tools/create_flash_bin.py ESP32-S3_16mbOTDisplay.json
#   python tools/create_flash_bin.py ESP32-c6_8mbOpentherm.json
#   python tools/create_flash_bin.py ESP32_4mb.json
#
# Последовательность действий:
# 1. Выполняет PrepareProject.py с указанным профилем
# 2. Выполняет компиляцию прошивки (без загрузки в ESP)
# 3. Копирует bootloader файлы из bin/esp32bootloader
# 4. Объединяет все bin файлы через esptool merge-bin
# 5. Копирует результаты в iotm/<output_folder>

import os
import sys
import subprocess
import shutil
import json

def get_platformio_path():
    """Возвращает путь к PlatformIO в зависимости от операционной системы."""
    if os.name == 'nt':  # Windows
        return os.path.join(os.environ['USERPROFILE'], '.platformio', 'penv', 'Scripts', 'pio.exe')
    else:  # Linux/MacOS
        return os.path.join(os.environ['HOME'], '.platformio', 'penv', 'bin', 'pio')

def run_command(command, description="", check=True):
    """Выполняет указанную команду в subprocess."""
    if isinstance(command, list):
        cmd_str = ' '.join(command)
    else:
        cmd_str = command
    
    print(f"\n{'='*60}")
    print(f" {description}")
    print(f" Команда: {cmd_str}")
    print(f"{'='*60}")
    
    try:
        result = subprocess.run(command, check=check, shell=isinstance(command, str))
        print(f" [OK] {description} - завершено успешно")
        return True
    except subprocess.CalledProcessError as e:
        print(f" [ERR] {description} - ошибка: {e.returncode}")
        return False

def find_project_root():
    """Находит корень проекта (где platformio.ini)."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    current = script_dir
    
    while current:
        if os.path.exists(os.path.join(current, "platformio.ini")):
            return current
        parent = os.path.dirname(current)
        if parent == current:
            break
        current = parent
    
    return os.path.dirname(script_dir)

def load_profile_config(profile_path):
    """Загружает конфигурацию из файла профиля."""
    if not os.path.isfile(profile_path):
        print(f"ОШИБКА: Файл профиля {profile_path} не найден.")
        return None
    
    try:
        with open(profile_path, 'r', encoding='utf-8') as file:
            profile_data = json.load(file)
            return profile_data
    except json.JSONDecodeError:
        print(f"ОШИБКА: Некорректный JSON в файле {profile_path}.")
        return None

def get_device_config(profile_data):
    """Извлекает конфигурацию устройства из профиля."""
    try:
        platformio = profile_data["projectProp"]["platformio"]
        default_envs = platformio["default_envs"]
        envs = platformio["envs"]
        
        # Ищем env по имени default_envs
        device_config = None
        for env in envs:
            if env["name"] == default_envs:
                device_config = env
                break
        
        if device_config is None:
            print(f"ОШИБКА: Устройство {default_envs} не найдено в списке envs")
            return None
        
        return {
            "device_name": default_envs,
            "boot_app0": device_config.get("boot_app0", "0xe000"),
            "bootloader": device_config.get("bootloader_qio_80m", "0x1000"),
            "firmware": device_config.get("firmware", "0x10000"),
            "partitions": device_config.get("partitions", "0x8000"),
            "littlefs": device_config.get("littlefs", "0x290000")
        }
    except KeyError as e:
        print(f"ОШИБКА: Отсутствует ключ {e} в профиле")
        return None

def get_chip_type(device_name):
    """Определяет тип чипа по имени устройства."""
    name_lower = device_name.lower()
    
    if "esp32s3" in name_lower or "s3" in name_lower:
        return "esp32s3"
    elif "esp32c6" in name_lower or "c6" in name_lower:
        return "esp32c6"
    elif "esp32c3" in name_lower or "c3" in name_lower:
        return "esp32c3"
    elif "esp32s2" in name_lower or "s2" in name_lower:
        return "esp32s2"
    elif "esp32" in name_lower:
        return "esp32"
    else:
        return "esp32"  # Default

def get_output_folder(profile_path):
    """Определяет папку вывода из имени профиля."""
    # Извлекаем имя файла без расширения и пути
    filename = os.path.basename(profile_path)
    name_without_ext = os.path.splitext(filename)[0]
    
    # Маппинг для создания красивых имен папок
    mappings = {
        "ESP32-S3_16mbOTDisplay": "OTDisplay",
        "ESP32-S3_16mbOTPanel": "OTPanel",
        "ESP32-c6_8mbOpentherm": "ESP32C6_8MB",
        "ESP32-c6_4mbSmartThermo": "ESP32C6_4MB_SmartThermo",
        "ESP32-c6_4mbSmartThermo_NO_OTA": "ESP32C6_4MB_SmartThermo_NO_OTA",
        "ESP32-c6_8mbPressureRelay": "ESP32C6_8MB_PressureRelay",
        "ESP32-c6_8mbEctoControl": "ESP32C6_8MB_EctoControl",
        "ESP32_4mb": "ESP32_4MB_OT",
        "ESP32_4mbPCF8574": "ESP32_4MB_PCF8574",
        "ESP32_4mb3f": "ESP32_4MB_3F",
        "ESP32_16mb": "ESP32_16MB",
        "ESP32_4mb3f": "ESP32_4MB_3F",
        "ESP32-c6_4mbOpentherm": "ESP32C6_4MB",
    }
    
    # Проверяем точное совпадение
    if name_without_ext in mappings:
        return mappings[name_without_ext]
    
    # Если нет точного совпадения, используем имя файла как есть
    # Заменяем underscores и дефисы на пробелы, потом PascalCase
    output_name = name_without_ext.replace("_", " ").replace("-", " ").title().replace(" ", "")
    
    return output_name

def parse_hex_address(addr_str):
    """Парсит hex адрес из строки (например '0x1000' или '1000')."""
    addr_str = addr_str.strip()
    if addr_str.startswith("0x") or addr_str.startswith("0X"):
        return int(addr_str, 16)
    return int(addr_str, 16)

def copy_bootloader_files(bootloader_dir, build_dir, device_name):
    """Копирует bootloader файлы в папку сборки."""
    files_to_copy = ['boot_app0.bin', 'bootloader_qio_80m.bin']
    
    print(f"\n{'='*60}")
    print(f" Копирование bootloader файлов")
    print(f" Источник: {bootloader_dir}")
    print(f" Назначение: {build_dir}")
    print(f"{'='*60}")
    
    for filename in files_to_copy:
        src = os.path.join(bootloader_dir, filename)
        dst = os.path.join(build_dir, filename)
        
        if os.path.exists(src):
            shutil.copy2(src, dst)
            size = os.path.getsize(dst)
            print(f"  [OK] {filename} -> {size} bytes")
        else:
            print(f"  [WARN] {filename} не найден в {bootloader_dir}")
    
    # Переименовываем/копируем bootloader для esptool
    bootloader_src = os.path.join(build_dir, 'bootloader_qio_80m.bin')
    bootloader_dst = os.path.join(build_dir, 'bootloader.bin')
    
    if os.path.exists(bootloader_src):
        if not os.path.exists(bootloader_dst):
            shutil.copy2(bootloader_src, bootloader_dst)
            print(f"  [OK] bootloader_qio_80m.bin -> bootloader.bin")
        else:
            print(f"  [OK] bootloader.bin уже существует")

def create_merge_bin(build_dir, output_dir, chip, config, output_filename):
    """Создает объединенный bin файл через esptool merge-bin."""
    print(f"\n{'='*60}")
    print(f" Объединение bin файлов через esptool")
    print(f"{'='*60}")
    
    output_file = os.path.join(output_dir, output_filename)
    
    # Парсим адреса из конфига
    firmware_addr = parse_hex_address(config["firmware"])
    partitions_addr = parse_hex_address(config["partitions"])
    littlefs_addr = parse_hex_address(config["littlefs"])
    
    # bootloader_addr из конфига (например 0x1000 для ESP32)
    bootloader_addr = parse_hex_address(config.get("bootloader", "0x1000"))
    
    # Формируем команду merge-bin: -o выходной_файл адрес файл ...
    # Порядок: bootloader, firmware, partitions, littlefs
    merge_files = [
        "-o", output_file,
        f"0x{bootloader_addr:08X}", "bootloader.bin",
        f"0x{firmware_addr:08X}", "firmware.bin",
        f"0x{partitions_addr:08X}", "partitions.bin",
        f"0x{littlefs_addr:08X}", "littlefs.bin"
    ]
    
    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", chip,
        "merge-bin"
    ] + merge_files
    
    # Формируем строку команды для лога
    cmd_str = f"{sys.executable} -m esptool --chip {chip} merge-bin"
    for item in merge_files:
        cmd_str += f" {item}"
    
    print(f"\n{'='*60}")
    print(f" Команда merge-bin:")
    print(f"{'='*60}")
    print(cmd_str)
    print(f"{'='*60}")
    
    # Переходим в папку build_dir для корректных относительных путей
    original_cwd = os.getcwd()
    
    try:
        os.chdir(build_dir)
        
        result = subprocess.run(
            cmd,
            check=True,
            capture_output=True,
            text=True
        )
        
        if result.stdout:
            print(f"\n stdout:\n{result.stdout}")
        if result.stderr:
            print(f"\n stderr:\n{result.stderr}")
        
        # Проверяем что файл создан
        if os.path.exists(output_file):
            size = os.path.getsize(output_file)
            print(f"\n [OK] {output_filename} создан: {size:,} bytes ({size/1024/1024:.2f} MB)")
            return True
        else:
            print(f"\n [ERR] {output_filename} не создан!")
            return False
            
    except subprocess.CalledProcessError as e:
        print(f"\n [ERR] Ошибка при объединении: {e}")
        if e.stdout:
            print(f" stdout: {e.stdout}")
        if e.stderr:
            print(f" stderr: {e.stderr}")
        return False
    finally:
        os.chdir(original_cwd)

def copy_result_files(source_dir, dest_dir):
    """Копирует все *.bin файлы из папки source в dest."""
    print(f"\n{'='*60}")
    print(f" Копирование результатов")
    print(f" Источник: {source_dir}")
    print(f" Назначение: {dest_dir}")
    print(f"{'='*60}")
    
    # Создаем папку назначения если её нет
    os.makedirs(dest_dir, exist_ok=True)
    
    files_copied = []
    
    # Копируем все *.bin файлы из source_dir
    for filename in os.listdir(source_dir):
        if filename.endswith('.bin'):
            src = os.path.join(source_dir, filename)
            dst = os.path.join(dest_dir, filename)
            shutil.copy2(src, dst)
            size = os.path.getsize(dst)
            files_copied.append((filename, size))
            print(f"  [OK] {filename} -> {size:,} bytes")
    
    if files_copied:
        print(f"\n Скопировано {len(files_copied)} файлов")
        return True
    else:
        print(f"\n [WARN] Нет bin файлов для копирования")
        return False

def find_com_port():
    """Автоматически находит доступный COM-порт."""
    skip_ports = {'COM1', 'COM2', 'COM3', 'COM4'}
    
    if os.name == 'nt':  # Windows
        try:
            result = subprocess.run(
                ['powershell', '-Command', 
                 '[System.IO.Ports.SerialPort]::GetPortNames()'],
                capture_output=True, text=True, timeout=5
            )
            if result.stdout.strip():
                ports = result.stdout.strip().split('\n')
                for port in ports:
                    port = port.strip()
                    if port and port not in skip_ports:
                        return port
                # fallback если все системные
                return ports[0].strip() if ports else None
        except:
            pass
    
    return None

def flash_esp32(flash_file, chip, port=None, flash_speed=921600):
    """Прошивает ESP32 объединенным bin файлом."""
    print(f"\n{'='*60}")
    print(f" Прошивка ESP32")
    print(f"{'='*60}")
    
    if not os.path.exists(flash_file):
        print(f"ОШИБКА: Файл {flash_file} не найден!")
        return None
    
    # Автоопределение порта
    if port is None:
        port = find_com_port()
        if port:
            print(f" Автоопределен порт: {port}")
        else:
            port = "COM3"
            print(f" Используем порт по умолчанию: {port}")
    
    print(f" Файл: {flash_file}")
    print(f" Порт: {port}")
    print(f" Скорость: {flash_speed}")
    
    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", chip,
        "--port", port,
        "--baud", str(flash_speed),
        "write_flash", "0x0", flash_file
    ]
    
    print(f"\n Команда: python {' '.join(cmd[1:])}")
    
    try:
        result = subprocess.run(cmd, check=True, capture_output=True, text=True)
        if result.stdout:
            print(f"\n stdout:\n{result.stdout}")
        if result.stderr:
            print(f"\n stderr:\n{result.stderr}")
        
        print(f"\n [OK] Прошивка завершена успешно!")
        return port  # Возвращаем порт для монитора
        
    except subprocess.CalledProcessError as e:
        print(f"\n [ERR] Ошибка при прошивке: {e}")
        if e.stdout:
            print(f" stdout: {e.stdout}")
        if e.stderr:
            print(f" stderr: {e.stderr}")
        return None

def start_monitor(port, baud=115200):
    """Запускает монитор порта PlatformIO."""
    print(f"\n{'='*60}")
    print(f" Запуск монитора порта")
    print(f"{'='*60}")
    print(f" Порт: {port}")
    print(f" Скорость: {baud}")
    print(f"\n Для выхода нажмите Ctrl+C")
    print(f"{'='*60}")
    
    pio_path = get_platformio_path()
    
    cmd = [pio_path, "device", "monitor", "--port", port, "--baud", str(baud)]
    
    print(f" Команда: {' '.join(cmd)}")
    
    try:
        subprocess.run(cmd)
    except KeyboardInterrupt:
        print("\n Монитор остановлен.")
    except Exception as e:
        print(f"\n [ERR] Ошибка монитора: {e}")
def main():
    print(f"{'='*60}")
    print(f" IoTManager - Создание объединенного bin файла")
    print(f"{'='*60}")
    
    # Определяем корень проекта
    project_root = find_project_root()
    print(f" Корень проекта: {project_root}")
    
    # Определяем путь к PlatformIO
    pio_path = get_platformio_path()
    if not os.path.isfile(pio_path):
        print(f"ОШИБКА: PlatformIO не найден по пути: {pio_path}")
        sys.exit(1)
    print(f" PlatformIO: {pio_path}")
    
    # Определяем профиль из аргументов командной строки
    if len(sys.argv) > 1:
        profile_file = sys.argv[1]
    else:
        print("ОШИБКА: Укажите файл профиля")
        print("Использование: python create_flash_bin.py <profile.json>")
        sys.exit(1)
    
    # Проверяем существование файла
    if not os.path.isfile(profile_file):
        # Пробуем найти файл в корне проекта
        alt_path = os.path.join(project_root, profile_file)
        if os.path.isfile(alt_path):
            profile_file = alt_path
        else:
            print(f"ОШИБКА: Файл профиля {profile_file} не найден")
            sys.exit(1)
    
    print(f" Профиль: {profile_file}")
    
    # Загружаем конфигурацию профиля
    profile_data = load_profile_config(profile_file)
    if not profile_data:
        sys.exit(1)
    
    # Получаем конфигурацию устройства
    device_config = get_device_config(profile_data)
    if not device_config:
        sys.exit(1)
    
    device_name = device_config["device_name"]
    chip_type = get_chip_type(device_name)
    
    print(f" Устройство: {device_name}")
    print(f" Чип: {chip_type}")
    print(f" Адреса:")
    print(f"   boot_app0:  {device_config['boot_app0']}")
    print(f"   bootloader: {device_config['bootloader']}")
    print(f"   firmware:   {device_config['firmware']}")
    print(f"   partitions: {device_config['partitions']}")
    print(f"   littlefs:   {device_config['littlefs']}")
    
    # Определяем имя выходного файла и папку
    output_folder = get_output_folder(profile_file)
    output_filename = f"{output_folder}.bin"
    
    print(f" Выходной файл: {output_filename}")
    
    # Определяем пути
    bootloader_dir = os.path.join(project_root, "bin", "esp32bootloader")
    build_dir = os.path.join(project_root, ".pio", "build", device_name)
    iotm_dir = os.path.join(project_root, "iotm", output_folder)
    
    print(f" Bootloader: {bootloader_dir}")
    print(f" Build: {build_dir}")
    print(f" IoTM: {iotm_dir}")
    
    # Шаг 1: Выполняем PrepareProject.py
    prepare_cmd = [sys.executable, os.path.join(project_root, "PrepareProject.py"), "-p", profile_file]
    if not run_command(prepare_cmd, "Подготовка проекта (PrepareProject.py)"):
        sys.exit(1)
    
    # Ожидаем завершения обновления конфигурации
    input(f"\n\x1b[1;33;40m Нажмите Enter для продолжения...\x1b[0m")
    
    # Шаг 2: Выполняем компиляцию (БЕЗ загрузки в ESP)
    print(f"\n{'='*60}")
    print(f" Компиляция прошивки")
    print(f"{'='*60}")
    
    run_command([pio_path, "run", "-t", "clean"], "Очистка проекта")
    run_command([pio_path, "run", "-t", "buildfs", "--environment", device_name], "Сборка файловой системы")
    run_command([pio_path, "run", "--environment", device_name], "Компиляция прошивки (без загрузки)")
    
    # Шаг 3: Копируем bootloader файлы
    if not os.path.exists(build_dir):
        print(f"ОШИБКА: Папка сборки {build_dir} не существует!")
        print("Сначала выполните сборку: platformio run")
        sys.exit(1)
    
    copy_bootloader_files(bootloader_dir, build_dir, device_name)
    
    # Проверяем наличие необходимых файлов
    required_files = ['bootloader.bin', 'partitions.bin', 'firmware.bin', 'littlefs.bin']
    print(f"\n Проверка файлов в {build_dir}:")
    
    missing_files = []
    for filename in required_files:
        filepath = os.path.join(build_dir, filename)
        if os.path.exists(filepath):
            size = os.path.getsize(filepath)
            print(f"  [OK] {filename}: {size:,} bytes")
        else:
            print(f"  [MISS] {filename}")
            missing_files.append(filename)
    
    if missing_files:
        print(f"\nОШИБКА: Отсутствуют файлы: {', '.join(missing_files)}")
        sys.exit(1)
    
    # Шаг 4: Создаем объединенный bin файл
    if not create_merge_bin(build_dir, build_dir, chip_type, device_config, output_filename):
        print("ОШИБКА: Не удалось создать объединенный bin файл.")
        sys.exit(1)
    
    # Шаг 5: Копируем все *.bin файлы в iotm/<output_folder>
    if not copy_result_files(build_dir, iotm_dir):
        print("ОШИБКА: Не удалось скопировать результаты.")
        sys.exit(1)
    
    print(f"\n{'='*60}")
    print(f" Готово! Все файлы скопированы в {iotm_dir}")
    print(f"{'='*60}")
    
    # Выводим список скопированных файлов
    print(f"\n Файлы в {iotm_dir}:")
    for filename in sorted(os.listdir(iotm_dir)):
        if filename.endswith('.bin'):
            filepath = os.path.join(iotm_dir, filename)
            size = os.path.getsize(filepath)
            print(f"  - {filename}: {size:,} bytes ({size/1024/1024:.2f} MB)")
    
    # Шаг 6: Прошиваем ESP32 объединенным файлом
    merged_file = os.path.join(iotm_dir, output_filename)
    if os.path.exists(merged_file):
        print(f"\n{'='*60}")
        print(f" Запуск прошивки ESP32")
        print(f"{'='*60}")
        
        # Запрос на прошивку
        response = input("\n Прошить ESP32 объединенным файлом? (y/n): ").strip().lower()
        if response == 'y' or response == 'yes':
            used_port = flash_esp32(merged_file, chip_type)
            if used_port:
                # После прошивки запускаем монитор
                monitor_response = input("\n Запустить монитор порта? (y/n): ").strip().lower()
                if monitor_response == 'y' or monitor_response == 'yes':
                    start_monitor(used_port)
            else:
                print("ОШИБКА: Прошивка не выполнена!")
                sys.exit(1)
        else:
            print(" Прошивка пропущена.")
    else:
        print(f"\n [WARN] Объединенный файл {merged_file} не найден, прошивка пропущена.")

if __name__ == "__main__":
    main()