import PySimpleGUI as sg
import subprocess
import os
import ctypes
import re 

# --- Tekrarlanan Mesajlar için Sabitler ---
MSG_PROGRAM_YUKLENMEDI_BASLIK = "Program Yüklenmedi"
MSG_PROGRAM_YUKLENMEDI_ICERIK = "Önce programı 'Çevir & Yükle' butonu ile yüklemelisiniz."
MSG_MOTOR_YUKLENEMEDI_BASLIK = "Motor Hatası"
MSG_MOTOR_YUKLENEMEDI_ICERIK_KISMI = "Simülatör motoru yüklenemedi."
MSG_MOTOR_YUKLENEMEDI_ADIM_ATILAMAZ = f"{MSG_MOTOR_YUKLENEMEDI_ICERIK_KISMI} Adım atılamaz."
MSG_MOTOR_YUKLENEMEDI_CEVIR_YUKLE = f"{MSG_MOTOR_YUKLENEMEDI_ICERIK_KISMI} 'Çevir & Yükle' işlemi yapılamaz."
MSG_MOTOR_YUKLENEMEDI_RESET = MSG_MOTOR_YUKLENEMEDI_ICERIK_KISMI
MSG_MOTOR_VEYA_PROGRAM_YOK = "Motor yüklenmedi veya program yüklenmedi."
MSG_ISLEM_HATASI_BASLIK = "İşlem Hatası"

# --- C++ DLL ve Fonksiyon Tanımlamaları ---
script_dir = os.path.dirname(os.path.abspath(__file__))
DLL_NAME_BASE = "sim_engine.dll" if os.name == 'nt' else "sim_engine.so"
DLL_PATH = os.path.join(script_dir, DLL_NAME_BASE)

class CppCPUState(ctypes.Structure):
    _fields_ = [
        ("pc", ctypes.c_uint16),
        ("sp", ctypes.c_uint16),
        ("ix", ctypes.c_uint16),
        ("accA", ctypes.c_uint8),
        ("accB", ctypes.c_uint8),
        ("ccr", ctypes.c_uint8) 
    ]

    def get_flag(self, bit_pos):
        return (self.ccr >> bit_pos) & 1

    @property
    def H_flag(self): return self.get_flag(5)
    @property
    def I_flag(self): return self.get_flag(4)
    @property
    def N_flag(self): return self.get_flag(3)
    @property
    def Z_flag(self): return self.get_flag(2)
    @property
    def V_flag(self): return self.get_flag(1)
    @property
    def C_flag(self): return self.get_flag(0)

engine_lib = None
engine_initialized = False

try:
    engine_lib = ctypes.CDLL(DLL_PATH)

    engine_lib.initialize_engine_dll.restype = None
    engine_lib.reset_cpu_dll.restype = None
    engine_lib.load_program_dll.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_int, ctypes.c_uint16]
    engine_lib.load_program_dll.restype = None
    engine_lib.step_cpu_dll.restype = None
    engine_lib.get_cpu_state_dll.restype = CppCPUState
    engine_lib.read_memory_dll.argtypes = [ctypes.c_uint16]
    engine_lib.read_memory_dll.restype = ctypes.c_uint8
    engine_lib.write_memory_dll.argtypes = [ctypes.c_uint16, ctypes.c_uint8]
    engine_lib.write_memory_dll.restype = None

    engine_lib.initialize_engine_dll()
    engine_initialized = True
    print("C++ Simülatör Motoru başarıyla yüklendi ve başlatıldı.")

except OSError as e:
    print(f"HATA (OSError): C++ Simülatör Motoru ({DLL_PATH}) yüklenemedi: {e}")
    sg.popup_error(f"HATA: C++ Simülatör Motoru ({DLL_PATH}) yüklenemedi.\nLütfen C++ projesini bir DLL/.so olarak derleyip bu script ile aynı klasöre koyduğunuzdan ve GEREKLİ TÜM BAĞIMLILIK DLL'LERİNİN (libgcc, libstdc++ vb.) de aynı klasörde olduğundan emin olun.\n\nDetay: {e}", title="Motor Yükleme Hatası")
except AttributeError as e:
    print(f"HATA (AttributeError): C++ Simülatör Motorunda fonksiyon bulunamadı: {e}")
    sg.popup_error(f"HATA: C++ Simülatör Motorunda fonksiyon bulunamadı.\nDLL fonksiyon adlarını (engine_wrapper.cpp) ve Python'daki tanımları kontrol edin.\n\nDetay: {e}", title="Motor Fonksiyon Hatası")
except Exception as e: 
    print(f"HATA (Genel): C++ Simülatör Motoru yüklenirken beklenmedik bir hata: {e}")
    sg.popup_error(f"HATA: C++ Simülatör Motoru yüklenirken beklenmedik bir hata oluştu.\n\nDetay: {e}", title="Genel Motor Hatası")

# --- Arayüz Fonksiyonları ---
def update_gui_registers(window, current_cpu_state):
    if not window or not current_cpu_state:
        return
    window['-PC-'].update(f"{current_cpu_state.pc:04X}")
    window['-SP-'].update(f"{current_cpu_state.sp:04X}")
    window['-IX-'].update(f"{current_cpu_state.ix:04X}")
    window['-A-'].update(f"{current_cpu_state.accA:02X}")
    window['-B-'].update(f"{current_cpu_state.accB:02X}")
    window['-CCR_BYTE-'].update(f"{current_cpu_state.ccr:02X}")
    
    window['-H-'].update(bool(current_cpu_state.H_flag))
    window['-I-'].update(bool(current_cpu_state.I_flag))
    window['-N-'].update(bool(current_cpu_state.N_flag))
    window['-Z-'].update(bool(current_cpu_state.Z_flag))
    window['-V-'].update(bool(current_cpu_state.V_flag))
    window['-C-'].update(bool(current_cpu_state.C_flag))

def _find_org_address(lines: list) -> int:
    org_address = 0 
    for line in lines:
        if "ORG" in line.upper() and "->" in line and "(Directive)" in line:
            try:
                match = re.search(r'ORG\s+\$(\w+)', line, re.IGNORECASE)
                if match:
                    org_address = int(match.group(1), 16)
                    print(f"Parsed ORG address: ${org_address:04X}")
                    return org_address 
            except ValueError:
                print(f"Uyarı: ORG adresi ayrıştırılamadı (ValueError): {line}")
            except Exception as e:
                print(f"Uyarı: ORG adresi ayrıştırılırken genel hata: {e} - Satır: {line}")
    return org_address

def _parse_machine_code_from_line(line_part: str) -> list[int]:
    instruction_bytes = []
    hex_bytes_str_list = line_part.strip().split()
    
    for hex_byte_str in hex_bytes_str_list:
        if hex_byte_str.upper() == "XX":
            instruction_bytes.append(0xEE) 
        else:
            try:
                instruction_bytes.append(int(hex_byte_str, 16))
            except ValueError:
                instruction_bytes.append(0xFF) 
    return instruction_bytes

def parse_assembler_output(output_str: str) -> tuple[list[int], int]:
    if not isinstance(output_str, str): 
        print(f"Hata: parse_assembler_output geçersiz girdi tipi aldı: {type(output_str)}")
        return [], 0

    machine_code_bytes_final = []
    lines = output_str.strip().split('\n')
    
    org_address = _find_org_address(lines)
    
    for line_content in lines:
        if "->" not in line_content or \
           "(Directive)" in line_content or \
           "ERROR" in line_content.upper() or \
           "HATA" in line_content.upper() or \
           not line_content.strip(): 
            continue
        
        parts = line_content.split('->', 1) 
        if len(parts) < 2:
            continue
            
        code_part_str = parts[1] 
        current_line_mc_bytes = _parse_machine_code_from_line(code_part_str)
        machine_code_bytes_final.extend(current_line_mc_bytes)
                    
    print(f"Ayrıştırılan toplam makine kodu: {[f'{b:02X}' for b in machine_code_bytes_final]}")
    return machine_code_bytes_final, org_address

def format_memory_dump(start_address, byte_list, bytes_per_line=16):
    dump_str = ""
    for i in range(0, len(byte_list), bytes_per_line):
        chunk = byte_list[i:i + bytes_per_line]
        dump_str += f"{start_address + i:04X}: "
        hex_part = " ".join([f"{b:02X}" for b in chunk])
        dump_str += f"{hex_part:<{bytes_per_line * 3 -1}}  " 
        ascii_part = "".join([chr(b) if 32 <= b <= 126 else "." for b in chunk])
        dump_str += ascii_part + "\n"
    return dump_str

# --- PySimpleGUI Arayüz Tanımlamaları ---
THEME_BACKGROUND_COLOR = 'black'
TEXT_COLOR = 'white'
INPUT_TEXT_COLOR = '#B0E0E6'  
INPUT_BG_COLOR = '#1C1C1C'    
DISABLED_TEXT_COLOR = '#777777' 
HEADER_TEXT_COLOR = '#6495ED' 
MEM_DUMP_BG_COLOR = '#0D0D0D'   
MEM_DUMP_TEXT_COLOR = '#ADD8E6' 
BUTTON_TEXT_COLOR = 'white'
BUTTON_BG_COLOR = '#2A3B4C'   

sg.theme('Black') 

# sg.set_options() çağrısını doğru parametrelerle yapın
sg.set_options(
    background_color=THEME_BACKGROUND_COLOR,
    text_color=TEXT_COLOR,
    button_color=(BUTTON_TEXT_COLOR, BUTTON_BG_COLOR)
    # input_elements_background_color ve input_elements_text_color burada olmamalı
)
# Input elemanları için varsayılan renkleri tema seviyesinde ayarlayın
sg.theme_input_background_color(INPUT_BG_COLOR)
sg.theme_input_text_color(INPUT_TEXT_COLOR)


example_asm_content = ""
example_asm_file_path = "example.asm" 
try:
    with open(example_asm_file_path, "r", encoding='utf-8') as f:
        example_asm_content = f.read()
except FileNotFoundError:
    example_asm_content = (
        "; example.asm bulunamadı.\nORG $0050\nLDAA #$12\nSTAA $0060\nLOOP: JMP LOOP\nEND"
    )

layout_sol = [
    [sg.Text("Assembly Kodunu Girin:")],
    [sg.Multiline(size=(60, 25), key='-ASM_INPUT-', default_text=example_asm_content)]
]

layout_sag = [
    [sg.Text("Çıktı ve Eşleştirme (Assembler):", text_color=HEADER_TEXT_COLOR)],
    [sg.Multiline(size=(60, 8), key='-ASM_OUTPUT-', disabled=True, autoscroll=True, text_color=DISABLED_TEXT_COLOR, background_color=INPUT_BG_COLOR)],
    [sg.HorizontalSeparator(color=HEADER_TEXT_COLOR)],
    [sg.Text("Yürütme Simülatörü", font=('Helvetica', 12, 'bold'), text_color=HEADER_TEXT_COLOR)],
    [
        sg.Text("PC:"), sg.Input("0000", size=(6,1), key='-PC-', disabled=True, text_color=INPUT_TEXT_COLOR, background_color=INPUT_BG_COLOR, disabled_readonly_background_color=INPUT_BG_COLOR, disabled_readonly_text_color=DISABLED_TEXT_COLOR),
        sg.Text("SP:"), sg.Input("0000", size=(6,1), key='-SP-', disabled=True, text_color=INPUT_TEXT_COLOR, background_color=INPUT_BG_COLOR, disabled_readonly_background_color=INPUT_BG_COLOR, disabled_readonly_text_color=DISABLED_TEXT_COLOR),
        sg.Text("IX:"), sg.Input("0000", size=(6,1), key='-IX-', disabled=True, text_color=INPUT_TEXT_COLOR, background_color=INPUT_BG_COLOR, disabled_readonly_background_color=INPUT_BG_COLOR, disabled_readonly_text_color=DISABLED_TEXT_COLOR)
    ],
    [
        sg.Text("A:"), sg.Input("00", size=(4,1), key='-A-', disabled=True, text_color=INPUT_TEXT_COLOR, background_color=INPUT_BG_COLOR, disabled_readonly_background_color=INPUT_BG_COLOR, disabled_readonly_text_color=DISABLED_TEXT_COLOR),
        sg.Text("B:"), sg.Input("00", size=(4,1), key='-B-', disabled=True, text_color=INPUT_TEXT_COLOR, background_color=INPUT_BG_COLOR, disabled_readonly_background_color=INPUT_BG_COLOR, disabled_readonly_text_color=DISABLED_TEXT_COLOR)
    ],
    [sg.Text("CCR:"), sg.Input("C0", size=(4,1), key='-CCR_BYTE-', disabled=True, text_color=INPUT_TEXT_COLOR, background_color=INPUT_BG_COLOR, disabled_readonly_background_color=INPUT_BG_COLOR, disabled_readonly_text_color=DISABLED_TEXT_COLOR),
     sg.Checkbox("H", key='-H-', disabled=True), sg.Checkbox("I", key='-I-', disabled=True), 
     sg.Checkbox("N", key='-N-', disabled=True), sg.Checkbox("Z", key='-Z-', disabled=True), 
     sg.Checkbox("V", key='-V-', disabled=True), sg.Checkbox("C", key='-C-', disabled=True)
    ],
    [sg.HorizontalSeparator(color=HEADER_TEXT_COLOR)],
    [sg.Text("Bellek Görüntüleyici", font=('Helvetica', 11, 'bold' ), text_color=HEADER_TEXT_COLOR)],
    [sg.Text("Başlangıç Adresi: $"), sg.Input("0000", size=(6,1), key='-MEM_DUMP_ADDR_IN-'), 
     sg.Text("Satır Sayısı:"), sg.Spin([i for i in range(1, 33)], initial_value=8, size=(3,1), key='-MEM_DUMP_LINES-'),
     sg.Button("Belleği Göster", key='-MEM_SHOW-', disabled=True) 
    ],
    [sg.Multiline(size=(60, 10), key='-MEM_OUTPUT-', disabled=True, font=('Courier New', 10), background_color=MEM_DUMP_BG_COLOR, text_color=MEM_DUMP_TEXT_COLOR, autoscroll=True)]
]

layout_alt_butonlar = [ 
    sg.Button("Çevir & Yükle", key='-ASSEMBLE_LOAD-'), 
    sg.Button("Adım At (Step)", key='-STEP-', disabled=True), 
    sg.Button("Reset CPU", key='-RESET_CPU-', disabled=True),
    sg.Button("Binary .txt Oluştur", key='-CREATE_BINARY_TXT-', disabled=True), 
    sg.Button("Çıkış", key='-EXIT-')
]

layout = [
    [
        sg.Column(layout_sol),
        sg.VSeperator(color=HEADER_TEXT_COLOR), 
        sg.Column(layout_sag)
    ],
    [sg.HorizontalSeparator(color=HEADER_TEXT_COLOR)], 
    layout_alt_butonlar 
]

window = sg.Window("Motorola 6800 Assembler & Simülatör", layout, finalize=True)

if engine_initialized:
    engine_lib.reset_cpu_dll() 
    cpu_s = engine_lib.get_cpu_state_dll()
    update_gui_registers(window, cpu_s)
else: 
    window['-ASSEMBLE_LOAD-'].update(disabled=True)
    window['-CREATE_BINARY_TXT-'].update(disabled=True)


# --- Olay Döngüsü ---
program_loaded = False
current_org_address = 0 
last_dump_start_addr = 0 
last_dump_num_lines = 8 
machine_code_bytes_cache = [] 

while True:
    event, values = window.read()

    if event == sg.WIN_CLOSED or event == '-EXIT-':
        break

    if not engine_initialized: 
        if event != sg.WIN_CLOSED and event != '-EXIT-': 
             sg.popup_error(MSG_MOTOR_YUKLENEMEDI_ICERIK_KISMI + " Lütfen programı kapatıp tekrar açın veya DLL sorununu çözün.", title="Kritik Motor Hatası")
        continue

    if event == '-ASSEMBLE_LOAD-':
        assembly_kodu = values['-ASM_INPUT-']
        temp_asm_file = "temp_code.asm" 
        with open(temp_asm_file, "w", encoding='utf-8') as f:
            f.write(assembly_kodu)
        
        assembler_executable = 'main.exe' if os.name == 'nt' else './main' 
        
        try:
            result = subprocess.run(
                [assembler_executable, temp_asm_file, "object_output.txt"], # Çıktı dosyası adı eklendi
                capture_output=True, text=True, check=False, encoding='utf-8'
            )
            assembler_stdout = result.stdout
            assembler_stderr = result.stderr
            
            full_assembler_output = "--- STDOUT ---\n" + assembler_stdout + "\n--- STDERR ---\n" + assembler_stderr
            window['-ASM_OUTPUT-'].update(full_assembler_output)

            # Hata kontrolü STDERR'e ve return code'a göre yapılıyor
            if result.returncode != 0 or "ERROR" in assembler_stderr.upper() or "HATA" in assembler_stderr.upper():
                error_message_to_show = assembler_stderr if assembler_stderr.strip() else assembler_stdout
                sg.popup_error(f"Assembly çevirme hatası!\n{error_message_to_show}", title="Assembler Hatası")
                program_loaded = False
                machine_code_bytes_cache = []
            else: 
                machine_code_bytes_cache, org_addr = parse_assembler_output(assembler_stdout)
                current_org_address = org_addr 
                
                if machine_code_bytes_cache:
                    code_array = (ctypes.c_uint8 * len(machine_code_bytes_cache))(*machine_code_bytes_cache)
                    engine_lib.load_program_dll(code_array, len(machine_code_bytes_cache), org_addr)
                    
                    current_cpu_state = engine_lib.get_cpu_state_dll() 
                    update_gui_registers(window, current_cpu_state)
                    
                    sg.popup_quick_message(f"Program belleğe ${org_addr:04X} adresinden yüklendi. PC = ${current_cpu_state.pc:04X}", auto_close_duration=3)
                    program_loaded = True
                else:
                    sg.popup_error("Assembler çıktısından makine kodu ayrıştırılamadı (STDOUT boş veya hatalı).", title="Ayrıştırma Hatası")
                    program_loaded = False
                    machine_code_bytes_cache = []
            
            window['-STEP-'].update(disabled=not program_loaded)
            window['-RESET_CPU-'].update(disabled=not program_loaded)
            window['-MEM_SHOW-'].update(disabled=not program_loaded)
            window['-CREATE_BINARY_TXT-'].update(disabled=not program_loaded)


        except FileNotFoundError:
            output_msg = f"HATA: C++ Assembler ('{assembler_executable}') bulunamadı."
            window['-ASM_OUTPUT-'].update(output_msg)
            sg.popup_error(output_msg, title="Dosya Bulunamadı")
        except Exception as e:
            output_msg = f"Beklenmedik bir hata (Çevir & Yükle): {str(e)}"
            window['-ASM_OUTPUT-'].update(output_msg)
            sg.popup_error(output_msg, title="Genel Hata")

    elif event == '-STEP-':
        if program_loaded:
            engine_lib.step_cpu_dll()
            current_cpu_state = engine_lib.get_cpu_state_dll()
            update_gui_registers(window, current_cpu_state)
            if window['-MEM_OUTPUT-'].get().strip():
                start_addr_to_refresh = last_dump_start_addr
                num_lines_to_refresh = last_dump_num_lines
                
                memory_bytes_list = []
                total_bytes_to_read = num_lines_to_refresh * 16 
                for i in range(total_bytes_to_read):
                    current_addr = start_addr_to_refresh + i
                    if current_addr < 65536:
                         memory_bytes_list.append(engine_lib.read_memory_dll(current_addr))
                    else:
                        break
                formatted_dump = format_memory_dump(start_addr_to_refresh, memory_bytes_list)
                window['-MEM_OUTPUT-'].update(formatted_dump)
        else:
            sg.popup_error(MSG_PROGRAM_YUKLENMEDI_ICERIK, title=MSG_PROGRAM_YUKLENMEDI_BASLIK)
            
    elif event == '-RESET_CPU-':
        if program_loaded: 
            engine_lib.reset_cpu_dll() 
            if machine_code_bytes_cache and current_org_address is not None:
                 print(f"Reset sonrası programı ORG ${current_org_address:04X} adresine tekrar yüklüyorum.")
                 code_array = (ctypes.c_uint8 * len(machine_code_bytes_cache))(*machine_code_bytes_cache)
                 engine_lib.load_program_dll(code_array, len(machine_code_bytes_cache), current_org_address)

            current_cpu_state = engine_lib.get_cpu_state_dll()
            update_gui_registers(window, current_cpu_state)
            sg.popup_quick_message(f"CPU sıfırlandı. PC = ${current_cpu_state.pc:04X}", auto_close_duration=2)
            if window['-MEM_OUTPUT-'].get().strip():
                 window.write_event_value('-MEM_SHOW-', None) 
        else:
             sg.popup_error(MSG_PROGRAM_YUKLENMEDI_ICERIK, title=MSG_PROGRAM_YUKLENMEDI_BASLIK)
    
    elif event == '-CREATE_BINARY_TXT-': 
        if program_loaded and machine_code_bytes_cache:
            save_filename = sg.popup_get_file(
                "Binary String Dosyasını Kaydet",
                save_as=True,
                default_extension=".txt",
                file_types=(("Text Dosyaları", "*.txt"), ("Tüm Dosyalar", "*.*")),
                no_window=True 
            )
            if save_filename:
                try:
                    with open(save_filename, "w", encoding='utf-8') as f:
                        for byte_val in machine_code_bytes_cache:
                            binary_string = format(byte_val, '08b') 
                            f.write(binary_string + "\n")
                    sg.popup(f"Binary string dosyası başarıyla kaydedildi:\n{save_filename}", title="Kaydetme Başarılı")
                except Exception as e:
                    sg.popup_error(f"Dosya kaydedilirken hata oluştu:\n{e}", title="Kaydetme Hatası")
        elif not machine_code_bytes_cache:
            sg.popup_error("Kaydedilecek makine kodu bulunmuyor. Lütfen önce 'Çevir & Yükle' yapın.", title="Veri Yok")
        else: 
            sg.popup_error(MSG_PROGRAM_YUKLENMEDI_ICERIK, title=MSG_PROGRAM_YUKLENMEDI_BASLIK)

    elif event == '-MEM_SHOW-': 
        if program_loaded:
            try:
                start_addr_str = values['-MEM_DUMP_ADDR_IN-']
                last_dump_num_lines = int(values['-MEM_DUMP_LINES-']) 
                bytes_per_line = 16 
                total_bytes_to_read = last_dump_num_lines * bytes_per_line

                last_dump_start_addr = int(start_addr_str, 16) 
                
                if not (0 <= last_dump_start_addr < 65536): 
                     sg.popup_error(f"Başlangıç adresi ${last_dump_start_addr:04X} bellek sınırları dışında.", title="Adres Hatası")
                     continue
                
                if last_dump_start_addr + total_bytes_to_read > 65536:
                    total_bytes_to_read = 65536 - last_dump_start_addr
                
                if total_bytes_to_read <= 0:
                    window['-MEM_OUTPUT-'].update(f"${last_dump_start_addr:04X} adresinden itibaren gösterilecek veri yok (bellek sonu).")
                    continue

                memory_bytes_list = [] 
                for i in range(total_bytes_to_read):
                    current_addr = last_dump_start_addr + i
                    memory_bytes_list.append(engine_lib.read_memory_dll(current_addr))
                
                formatted_dump = format_memory_dump(last_dump_start_addr, memory_bytes_list, bytes_per_line)
                window['-MEM_OUTPUT-'].update(formatted_dump)

            except ValueError:
                sg.popup_error("Lütfen geçerli bir hex başlangıç adresi girin.", title="Giriş Hatası")
            except Exception as e:
                sg.popup_error(f"Bellek görüntüleme hatası: {e}", title="Bellek Hatası")
        else:
            sg.popup_error(MSG_PROGRAM_YUKLENMEDI_ICERIK, title=MSG_PROGRAM_YUKLENMEDI_BASLIK)
    
window.close()
