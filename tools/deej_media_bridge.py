"""
deej_media_bridge.py
====================
GUI-мост между Deej32Led (WebSocket) и горячими клавишами/медиа-клавишами.
Позволяет захватывать шорткаты и привязывать их к жестам (вверх-вниз / вниз-вверх).
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import asyncio
import threading
import json
import os
import sys
import logging

try:
    import websockets
    import keyboard
    import pystray
    from PIL import Image, ImageTk
    import webbrowser
except ImportError:
    # Use built-in Tkinter interface to show error before crash
    root = tk.Tk()
    root.withdraw()
    messagebox.showerror("Dependency Error", "Please install the required libraries:\npip install websockets keyboard pystray pillow")
    sys.exit(1)

CONFIG_FILE = "bridge_config.json"

I18N = {
    'ru': {
        'dep_err': 'Ошибка зависимостей. Установите библиотеки:\npip install websockets keyboard',
        'saved': '💾 Настройки сохранены.',
        'init': 'GUI инициализирован. Можно запустить мост.',
        'net_settings': 'Настройки подключения',
        'ip_host': 'IP / Host:',
        'port': 'Порт:',
        'save': 'Сохранить',
        'gesture_bind': 'Привязка жестов к кнопкам',
        'channel': 'Канал',
        'up_down': 'Движение ВВЕРХ-ВНИЗ (bak)',
        'down_up': 'Движение ВНИЗ-ВВЕРХ (con)',
        'fader': 'Фейдер',
        'capture': 'Считать',
        'waiting': 'Жду...',
        'start_bridge': '▶ Запустить мост',
        'stop_bridge': '⏹ Остановить мост',
        'cap_err': 'Ошибка захвата: ',
        'arrêt': 'Остановка... (полностью остановится при следующем пинге/сообщении)',
        'conn_to': '🔄 Подключение к {}...',
        'conn_ok': '✅ Успешно подключено к Deej!',
        'gesture_ud': '⚡ Жест ↑↓ (Канал {}) -> отправляем {}',
        'gesture_du': '⚡ Жест ↓↑ (Канал {}) -> отправляем {}',
        'unk_key': 'Неизвестная клавиша: {}',
        'conn_err': '❌ Ошибка соединения: {}. Переподключение через 3с...',
        'crit_err': '❌ Критическая ошибка: {}',
        'lang': 'Language / Язык:',
        'tray_open_web': '🌐 Открыть Dashboard',
        'tray_show': 'Развернуть окно',
        'tray_exit': 'Выход'
    },
    'en': {
        'dep_err': 'Dependency Error. Please install:\npip install websockets keyboard',
        'saved': '💾 Settings saved.',
        'init': 'GUI initialized. You can now start the bridge.',
        'net_settings': 'Connection Settings',
        'ip_host': 'IP / Host:',
        'port': 'Port:',
        'save': 'Save',
        'gesture_bind': 'Gesture Key-binding',
        'channel': 'Channel',
        'up_down': 'UP-DOWN gesture (bak)',
        'down_up': 'DOWN-UP gesture (con)',
        'fader': 'Fader',
        'capture': 'Capture',
        'waiting': 'Waiting...',
        'start_bridge': '▶ Start Bridge',
        'stop_bridge': '⏹ Stop Bridge',
        'cap_err': 'Capture error: ',
        'arrêt': 'Stopping... (will fully stop on the next message)',
        'conn_to': '🔄 Connecting to {}...',
        'conn_ok': '✅ Successfully connected to Deej!',
        'gesture_ud': '⚡ Gesture ↑↓ (Channel {}) -> sending {}',
        'gesture_du': '⚡ Gesture ↓↑ (Channel {}) -> sending {}',
        'unk_key': 'Unknown key: {}',
        'conn_err': '❌ Connection error: {}. Reconnecting in 3s...',
        'crit_err': '❌ Critical error: {}',
        'lang': 'Language / Язык:',
        'tray_open_web': '🌐 Open Dashboard',
        'tray_show': 'Show Window',
        'tray_exit': 'Exit'
    }
}

def T(config, key, *args):
    lang = config.get('lang', 'ru')
    text = I18N.get(lang, I18N['ru']).get(key, key)
    if args: return text.format(*args)
    return text

class TextHandler(logging.Handler):
    def __init__(self, text_widget):
        super().__init__()
        self.text_widget = text_widget

    def emit(self, record):
        msg = self.format(record)
        def append():
            self.text_widget.configure(state='normal')
            self.text_widget.insert(tk.END, msg + '\n')
            self.text_widget.configure(state='disabled')
            self.text_widget.yview(tk.END)
        self.text_widget.after(0, append)

class DeejBridgeApp:
    def __init__(self, root):
        self.root = root
        self.root.title("DeejESP32 Bridge")
        self.root.geometry("700x700")
        
        # Загружаем настройки
        self.config = self.load_config()
        self.is_running = False
        self.loop = None
        self.ws_thread = None
        
        self.tray = None
        self.tray_running = False
        
        self.action_vars = {}
        
        self.create_widgets()
        self.setup_logging()
        self.setup_tray()

        self.root.protocol('WM_DELETE_WINDOW', self.hide_window)
        
    def load_config(self):
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE, 'r', encoding='utf-8') as f:
                    return json.load(f)
            except Exception as e:
                pass # Игнорируем или используем дефолтные значения
        # Значения по умолчанию
        return {
            "host": "deej32led.local",
            "port": 8765,
            "lang": "ru",
            "actions": {
                "bak_0": "previous track",
                "con_0": "",
                "bak_1": "",
                "con_1": "",
                "bak_2": "play/pause media",
                "con_2": "play/pause media",
                "bak_3": "",
                "con_3": "",
                "bak_4": "next track",
                "con_4": ""
            }
        }

    def save_config(self):
        self.config["host"] = self.host_var.get()
        self.config["lang"] = self.lang_var.get()
        self.config["port"] = int(self.port_var.get())
        for key, tk_var in self.action_vars.items():
            self.config["actions"][key] = tk_var.get().strip()
        with open(CONFIG_FILE, 'w', encoding='utf-8') as f:
            json.dump(self.config, f, indent=4, ensure_ascii=False)
        self.logger.info(T(self.config, "saved"))

    def setup_logging(self):
        self.log_widget = scrolledtext.ScrolledText(self.root, height=12, state='disabled', bg="#1e1e1e", fg="#d4d4d4", font=("Consolas", 10))
        self.log_widget.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))
        
        handler = TextHandler(self.log_widget)
        handler.setFormatter(logging.Formatter("[%(asctime)s] %(message)s", datefmt="%H:%M:%S"))
        
        logger = logging.getLogger("Bridge")
        logger.setLevel(logging.INFO)
        logger.addHandler(handler)
        self.logger = logger
        self.logger.info(T(self.config, "init"))

    def create_widgets(self):
        # Network settings frame
        frame_top = ttk.LabelFrame(self.root, text=T(self.config, "net_settings"))
        frame_top.pack(fill=tk.X, padx=10, pady=5)
        
        ttk.Label(frame_top, text=T(self.config, "ip_host")).grid(row=0, column=0, padx=5, pady=5)
        self.host_var = tk.StringVar(value=self.config.get("host", "deej32led.local"))
        ttk.Entry(frame_top, textvariable=self.host_var).grid(row=0, column=1, padx=5, pady=5)
        
        ttk.Label(frame_top, text=T(self.config, "port")).grid(row=0, column=2, padx=5, pady=5)
        self.port_var = tk.StringVar(value=str(self.config.get("port", 8765)))
        ttk.Entry(frame_top, textvariable=self.port_var, width=6).grid(row=0, column=3, padx=5, pady=5)
        
        ttk.Button(frame_top, text=T(self.config, "save"), command=self.save_config).grid(row=0, column=4, padx=10, pady=5)
        # Language selector
        ttk.Label(frame_top, text=T(self.config, "lang")).grid(row=1, column=0, padx=5, pady=5)
        self.lang_var = tk.StringVar(value=self.config.get("lang", "ru"))
        lang_cb = ttk.Combobox(frame_top, textvariable=self.lang_var, values=["ru", "en"], width=5, state="readonly")
        lang_cb.grid(row=1, column=1, padx=5, pady=5)
        def on_lang_change(e):
            self.save_config()
            messagebox.showinfo("Restart Required / Требуется перезапуск", "Please restart the application to apply the language change. / Пожалуйста, перезапустите приложение для смены языка.")
        lang_cb.bind('<<ComboboxSelected>>', on_lang_change)


        # Button mapping frame
        frame_middle = ttk.LabelFrame(self.root, text=T(self.config, "gesture_bind"))
        frame_middle.pack(fill=tk.X, padx=10, pady=5)

        # Headers
        ttk.Label(frame_middle, text=T(self.config, "channel"), font=('Arial', 9, 'bold')).grid(row=0, column=0, padx=5, pady=5)
        ttk.Label(frame_middle, text=T(self.config, "up_down"), font=('Arial', 9, 'bold')).grid(row=0, column=1, padx=5, pady=5, columnspan=2)
        ttk.Label(frame_middle, text=T(self.config, "down_up"), font=('Arial', 9, 'bold')).grid(row=0, column=3, padx=5, pady=5, columnspan=2)

        MEDIA_KEYS = [
            "",
            "play/pause media",
            "previous track",
            "next track",
            "volume mute",
            "volume up",
            "volume down",
            "stop media",
            "enter",
            "esc",
            "space"
        ]

        for ch in range(5):
            ttk.Label(frame_middle, text=f"{T(self.config, 'fader')} {ch+1}:").grid(row=ch+1, column=0, padx=5, pady=5)
            
            # --- Up-down ---
            kb_bak = f"bak_{ch}"
            self.action_vars[kb_bak] = tk.StringVar(value=self.config["actions"].get(kb_bak, ""))
            cb_bak = ttk.Combobox(frame_middle, textvariable=self.action_vars[kb_bak], values=MEDIA_KEYS, width=20)
            cb_bak.grid(row=ch+1, column=1, padx=5, pady=5)
            
            btn_bak = ttk.Button(frame_middle, text=T(self.config, "capture"))
            btn_bak.grid(row=ch+1, column=2, padx=2, pady=5)
            # Pass the button and variable to update UI during capture
            btn_bak.config(command=lambda b=btn_bak, v=self.action_vars[kb_bak]: self.capture_hotkey(b, v))

            # --- Down-up ---
            kb_con = f"con_{ch}"
            self.action_vars[kb_con] = tk.StringVar(value=self.config["actions"].get(kb_con, ""))
            cb_con = ttk.Combobox(frame_middle, textvariable=self.action_vars[kb_con], values=MEDIA_KEYS, width=20)
            cb_con.grid(row=ch+1, column=3, padx=15, pady=5)
            
            btn_con = ttk.Button(frame_middle, text=T(self.config, "capture"))
            btn_con.grid(row=ch+1, column=4, padx=2, pady=5)
            btn_con.config(command=lambda b=btn_con, v=self.action_vars[kb_con]: self.capture_hotkey(b, v))

        # Action buttons frame
        frame_btns = ttk.Frame(self.root)
        frame_btns.pack(fill=tk.X, padx=10, pady=10)
        
        self.btn_start = ttk.Button(frame_btns, text=T(self.config, "start_bridge"), command=self.toggle_server)
        self.btn_start.pack(fill=tk.X, pady=5)

    def capture_hotkey(self, btn, tk_var):
        """Starts a thread to capture key press to avoid blocking UI"""
        def on_capture():
            self.root.after(0, lambda: btn.config(text=T(self.config, "waiting"), state="disabled"))
            try:
                # Read any combo/key
                hk = keyboard.read_hotkey(suppress=False)
                self.root.after(0, lambda: tk_var.set(hk))
            except Exception as e:
                self.logger.error(f"{T(self.config, 'cap_err')}{e}")
            finally:
                self.root.after(0, lambda: btn.config(text=T(self.config, "capture"), state="normal"))
                
        threading.Thread(target=on_capture, daemon=True).start()

    def toggle_server(self):
        self.save_config()
        if not self.is_running:
            self.is_running = True
            self.btn_start.config(text=T(self.config, "stop_bridge"))
            self.ws_thread = threading.Thread(target=self.start_async_loop, daemon=True)
            self.ws_thread.start()
        else:
            self.is_running = False
            self.btn_start.config(text=T(self.config, "start_bridge"))
            self.logger.info(T(self.config, 'arrêt'))
            
            if self.loop is not None and self.loop.is_running():
                self.loop.call_soon_threadsafe(self.loop.stop)


    def start_async_loop(self):
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        try:
            self.loop.run_until_complete(self.ws_client())
        except asyncio.CancelledError:
            pass
        except Exception as e:
            self.root.after(0, lambda: self.logger.error(f"Async loop error: {e}"))
        finally:
            self.loop.close()
            self.loop = None
            self.is_running = False
            self.root.after(0, lambda: self.btn_start.config(text="▶ Запустить мост"))

    async def ws_client(self):
        url = f"ws://{self.config['host']}:{self.config['port']}/ws"
        
        prev_bak = {}
        prev_con = {}
        
        while self.is_running:
            try:
                self.logger.info(T(self.config, 'conn_to', url))
                async with websockets.connect(url, ping_interval=10) as ws:
                    self.logger.info(T(self.config, 'conn_ok'))
                    
                    while self.is_running:
                        try:
                            # Таймаут позволяет прервать цикл при остановке программы
                            try:
                                raw = await asyncio.wait_for(ws.recv(), timeout=1.0)
                            except asyncio.TimeoutError:
                                await ws.send(json.dumps({"type": "ping"}))
                                continue
                            
                        try:
                            msg = json.loads(raw)
                            if msg.get("type") != "update":
                                continue
                                
                            bak = msg.get("gbak", [])
                            con = msg.get("gcon", [])
                            
                            actions = self.config["actions"]
                            
                            # Проверяем жесты "вверх-вниз"
                            for ch, val in enumerate(bak):
                                if ch < 5 and prev_bak.get(ch) is not None and val != prev_bak[ch]:
                                    hk = actions.get(f"bak_{ch}", "")
                                    if hk:
                                        self.logger.info(T(self.config, 'gesture_ud', ch+1, f"'{hk}'"))
                                        try:
                                            keyboard.send(hk)
                                        except ValueError as ve:
                                            self.logger.error(T(self.config, 'unk_key', hk))
                                prev_bak[ch] = val

                            # Проверяем жесты "вниз-вверх"
                            for ch, val in enumerate(con):
                                if ch < 5 and prev_con.get(ch) is not None and val != prev_con[ch]:
                                    hk = actions.get(f"con_{ch}", "")
                                    if hk:
                                        self.logger.info(T(self.config, 'gesture_du', ch+1, f"'{hk}'"))
                                        try:
                                            keyboard.send(hk)
                                        except ValueError as ve:
                                            self.logger.error(T(self.config, 'unk_key', hk))
                                prev_con[ch] = val

                        except json.JSONDecodeError:
                            pass
            except (OSError, websockets.exceptions.WebSocketException) as e:
                self.logger.warning(T(self.config, 'conn_err', e))
                if self.is_running:
                    await asyncio.sleep(3)
            except Exception as e:
                self.logger.error(T(self.config, 'crit_err', e))
                if self.is_running:
                    await asyncio.sleep(5)

    def setup_tray(self):
        try:
            self.tray_icon_img = Image.open(os.path.join("tools", "icon.png"))
            self.tk_icon = ImageTk.PhotoImage(self.tray_icon_img)
            self.root.iconphoto(False, self.tk_icon)
        except Exception:
            self.tray_icon_img = Image.new('RGB', (64, 64), color = (73, 109, 137))
            
        menu = pystray.Menu(
            pystray.MenuItem(T(self.config, 'tray_open_web'), self.open_web),
            pystray.MenuItem(T(self.config, 'start_bridge'), self.start_bridge_tray),
            pystray.MenuItem(T(self.config, 'stop_bridge'), self.stop_bridge_tray),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem(T(self.config, 'tray_show'), self.show_window),
            pystray.MenuItem(T(self.config, 'tray_exit'), self.exit_app)
        )
        self.tray = pystray.Icon("DeejBridge", self.tray_icon_img, "Deej32Led Companion", menu)

    def hide_window(self):
        self.root.withdraw()
        if not self.tray_running:
            self.tray_thread = threading.Thread(target=self.run_tray, daemon=True)
            self.tray_thread.start()

    def run_tray(self):
        self.tray_running = True
        self.tray.run()

    def show_window(self, icon=None, item=None):
        if self.tray_running:
            self.tray.stop()
            self.tray_running = False
        self.root.after(0, self.root.deiconify)

    def exit_app(self, icon=None, item=None):
        if self.tray_running:
            self.tray.stop()
        self.is_running = False
        if self.loop:
            self.loop.stop()
        self.root.after(0, self.root.destroy)
            
    def open_web(self, icon=None, item=None):
        webbrowser.open(f"http://{self.config['host']}")
        
    def start_bridge_tray(self, icon=None, item=None):
        if not self.is_running:
            self.root.after(0, self.toggle_server)

    def stop_bridge_tray(self, icon=None, item=None):
        if self.is_running:
            self.root.after(0, self.toggle_server)

if __name__ == "__main__":
    root = tk.Tk()
    app = DeejBridgeApp(root)
    root.mainloop()
