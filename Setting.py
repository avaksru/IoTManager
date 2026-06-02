import tkinter as tk
from tkinter import ttk, messagebox
import json
import os
from pathlib import Path

# Динамическое определение пути к файлу в папке со скриптом
BASE_DIR = Path(__file__).resolve().parent
CONFIG_FILE = BASE_DIR / 'myProfile.json'
MODULES_BASE_DIR = BASE_DIR / 'src' / 'modules'


class ConfigApp:
    def __init__(self, root):
        self.root = root
        self.root.title("IoT Manager Configurator")
        self.root.geometry("900x900")

        self.load_data()
        self.load_module_descriptions()
        self.vars = {}  # Хранилище для переменных интерфейса
        self.desc_labels = []  # Список всех меток с описаниями
        self.wraplength = 600  # Начальное значение

        if hasattr(self, 'data'):
            self.create_widgets()
            # Привязываем обработчик изменения размера окна
            self.root.bind('<Configure>', self.on_window_resize)


    def load_data(self):
        """Загрузка данных из JSON файла с проверкой пути."""
        if not CONFIG_FILE.exists():
            messagebox.showerror("Ошибка", 
                f"Файл не найден!\n\nОжидаемый путь:\n{CONFIG_FILE}\n\n"
                f"Убедитесь, что файл myProfile.json лежит в той же папке, что и скрипт.")
            self.root.destroy()
            return
        
        try:
            with open(CONFIG_FILE, 'r', encoding='utf-8') as f:
                self.data = json.load(f)
        except Exception as e:
            messagebox.showerror("Ошибка чтения", f"Не удалось прочитать JSON: {e}")
            self.root.destroy()

    def load_module_descriptions(self):
        """Загрузка описаний модулей из файлов modinfo.json."""
        self.module_descriptions = {}
        
        for category in ['virtual_elments', 'sensors', 'executive_devices', 'screens']:
            if category not in self.data.get('modules', {}):
                continue
                
            for module in self.data['modules'][category]:
                path = module.get('path', '')
                if not path:
                    continue
                
                # Путь уже содержит src/modules/, например: src/modules/virtual/Benchmark
                # Просто добавляем /modinfo.json
                modinfo_path = BASE_DIR / path / 'modinfo.json'
                
                if modinfo_path.exists():
                    try:
                        with open(modinfo_path, 'r', encoding='utf-8') as f:
                            modinfo = json.load(f)
                            # Получаем moduleDesc из about или moduleDesc напрямую
                            desc = ''
                            if 'about' in modinfo:
                                desc = modinfo['about'].get('moduleDesc', '')
                            if not desc and 'moduleDesc' in modinfo:
                                desc = modinfo['moduleDesc']
                            self.module_descriptions[path] = desc
                    except Exception:
                        pass

    def save_data(self):
        """Сбор данных из интерфейса и запись в файл."""
        # 1. Обновляем iotmSettings[cite: 1]
        for key in self.data['iotmSettings']:
            val = self.vars[f"iotm_{key}"].get()
            # Преобразование типов (если в оригинале было число)
            orig_val = self.data['iotmSettings'][key]
            if isinstance(orig_val, int):
                try: val = int(val)
                except: val = 0
            self.data['iotmSettings'][key] = val

        # 2. Обновляем модули[cite: 1]
        for category in ['virtual_elments', 'sensors', 'executive_devices', 'screens']:
            for i, module in enumerate(self.data['modules'][category]):
                path = module['path']
                self.data['modules'][category][i]['active'] = self.vars[f"mod_{path}"].get()

        try:
            with open(CONFIG_FILE, 'w', encoding='utf-8') as f:
                json.dump(self.data, f, indent=4, ensure_ascii=False)
            messagebox.showinfo("Успех", "Настройки сохранены в myProfile.json")
        except Exception as e:
            messagebox.showerror("Ошибка", f"Не удалось сохранить: {e}")

    def create_widgets(self):
        notebook = ttk.Notebook(self.root)
        notebook.pack(expand=True, fill='both', padx=5, pady=5)

        # Вкладка 1: Основные настройки (iotmSettings)[cite: 1]
        settings_frame = self.create_scrollable_frame(notebook, "Основные")
        for key, value in self.data['iotmSettings'].items():
            frame = tk.Frame(settings_frame)
            frame.pack(fill='x', padx=5, pady=2)
            tk.Label(frame, text=key, width=24, anchor='w', font=('Arial', 10)).pack(side='left')
            
            var = tk.StringVar(value=str(value))
            self.vars[f"iotm_{key}"] = var
            tk.Entry(frame, textvariable=var, font=('Arial', 10)).pack(side='right', expand=True, fill='x')

        # Вкладки для категорий модулей[cite: 1]
        module_categories = {
            "Виртуальные": "virtual_elments",
            "Датчики": "sensors",
            "Исполнители": "executive_devices",
            "Экраны": "screens"
        }

        for tab_name, cat_key in module_categories.items():
            frame = self.create_scrollable_frame(notebook, tab_name)
            for module in self.data['modules'][cat_key]:
                path = module['path']
                name = path.split('/')[-1] 
                
                var = tk.BooleanVar(value=module['active'])
                self.vars[f"mod_{path}"] = var
                
                # Фрейм для модуля с описанием
                mod_frame = tk.Frame(frame)
                mod_frame.pack(anchor='w', padx=10, pady=2, fill='x')
                
                tk.Checkbutton(mod_frame, text=name, variable=var, font=('Arial', 11, 'bold')).pack(anchor='w')
                
                # Добавляем описание модуля если есть
                desc = self.module_descriptions.get(path, '')
                if desc:
                    desc_label = tk.Label(mod_frame, text=f"  {desc}", 
                                         font=('Arial', 10), fg='#555555', 
                                         wraplength=self.wraplength, justify='left')
                    desc_label.pack(anchor='w', padx=20)
                    self.desc_labels.append(desc_label)


        # Нижняя панель с кнопкой
        bottom_frame = tk.Frame(self.root)
        bottom_frame.pack(fill='x')
        
        save_btn = tk.Button(bottom_frame, text="СОХРАНИТЬ ИЗМЕНЕНИЯ", 
                            command=self.save_data, bg="#2E7D32", fg="white", 
                            font=('Arial', 12, 'bold'), pady=12)
        save_btn.pack(fill='x', padx=10, pady=10)

    def on_window_resize(self, event):
        """Обновление wraplength при изменении размера окна."""
        if event.widget == self.root:
            # Вычисляем wraplength: ширина окна минус отступы
            new_wraplength = max(200, event.width - 120)
            if new_wraplength != self.wraplength:
                self.wraplength = new_wraplength
                for label in self.desc_labels:
                    label.configure(wraplength=self.wraplength)

    def create_scrollable_frame(self, notebook, title):
        outer_frame = tk.Frame(notebook)
        notebook.add(outer_frame, text=title)

        canvas = tk.Canvas(outer_frame, highlightthickness=0)
        scrollbar = ttk.Scrollbar(outer_frame, orient="vertical", command=canvas.yview)
        scrollable_frame = tk.Frame(canvas)

        scrollable_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)

        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        
        # Поддержка прокрутки колесиком мыши
        def _on_mousewheel(event):
            canvas.yview_scroll(int(-1*(event.delta/120)), "units")
        canvas.bind_all("<MouseWheel>", _on_mousewheel)

        return scrollable_frame

if __name__ == "__main__":
    root = tk.Tk()
    # Установка иконки, если нужно (пропускаем для универсальности)
    app = ConfigApp(root)
    root.mainloop()