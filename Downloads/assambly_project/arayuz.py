import PySimpleGUI as sg
import subprocess
import os # Dosya işlemleri için eklendi

# Arayüzün temasını ayarla
sg.theme('DarkGrey13')

# example.asm dosyasının içeriğini oku
example_asm_content = ""
example_asm_file_path = "example.asm" # example.asm dosyasının adını ve yolunu kontrol edin

try:
    with open(example_asm_file_path, "r", encoding='utf-8') as f:
        example_asm_content = f.read()
except FileNotFoundError:
    # Eğer example.asm bulunamazsa, varsayılan bir metin kullan
    example_asm_content = (
        "; example.asm bulunamadı.\n"
        "; Lütfen manuel olarak girin veya dosyayı oluşturun.\n\n"
        "ORG $100\n"
        "START: LDA A, #$25\n"
        "       STA A, $0200\n"
        "       NOP\n"
        "       JMP START\n"
        "       END"
    )

# Sol taraf (Assembly kodu girişi)
layout_sol = [
    [sg.Text("Assembly Kodunu Girin:")],
    [sg.Multiline(size=(60, 25), key='-ASM_INPUT-', default_text=example_asm_content)] # default_text güncellendi
]

# Sağ taraf (Makine kodu çıktısı ve simülatör) - Bu kısım aynı kalıyor
layout_sag = [
    [sg.Text("Çıktı ve Eşleştirme:")],
    [sg.Multiline(size=(60, 15), key='-OUTPUT-', disabled=True)],
    [sg.HorizontalSeparator()],
    [sg.Text("Yürütme Simülatörü (Execution Simulator)", font=('Helvetica', 12, 'bold'))],
    [
        sg.Text("PC:"), sg.Input("0000", size=(6,1), key='-PC-', disabled=True),
        sg.Text("SP:"), sg.Input("0000", size=(6,1), key='-SP-', disabled=True),
        sg.Text("IX:"), sg.Input("0000", size=(6,1), key='-IX-', disabled=True)
    ],
    [
        sg.Text("A:"), sg.Input("00", size=(4,1), key='-A-', disabled=True),
        sg.Text("B:"), sg.Input("00", size=(4,1), key='-B-', disabled=True)
    ],
    [sg.Text("Flags:"), 
    sg.Checkbox("H", key='-H-', disabled=True), sg.Checkbox("I", key='-I-', disabled=True), 
    sg.Checkbox("N", key='-N-', disabled=True), sg.Checkbox("Z", key='-Z-', disabled=True), 
    sg.Checkbox("V", key='-V-', disabled=True), sg.Checkbox("C", key='-C-', disabled=True)
    ]
]

# Ana pencere düzeni - Bu kısım aynı kalıyor
layout = [
    [
        sg.Column(layout_sol),
        sg.VSeperator(),
        sg.Column(layout_sag)
    ],
    [sg.Button("Çevir (Assemble)", key='-ASSEMBLE-'), sg.Button("Çıkış", key='-EXIT-')]
]

window = sg.Window("Motorola 6800 Assembler & Simulator", layout)

# Olay döngüsü - Bu kısım aynı kalıyor
while True:
    event, values = window.read()

    if event == sg.WIN_CLOSED or event == '-EXIT-':
        break

    if event == '-ASSEMBLE-':
        assembly_kodu = values['-ASM_INPUT-']
        # Windows'ta geçici dosya adı farklı olabilir, ya da aynı klasörde temp_code.asm oluşturulabilir
        temp_asm_file = "temp_code.asm" 
        with open(temp_asm_file, "w", encoding='utf-8') as f: # encoding eklendi
            f.write(assembly_kodu)
        
            executable_name = 'main.exe' # Windows için        if os.name != 'nt': # Eğer Windows değilse (Linux, macOS)
            executable_name = './assembler'


        try:
            result = subprocess.run(
                [executable_name, temp_asm_file], 
                capture_output=True,
                text=True,
                check=True,
                encoding='utf-8' # encoding eklendi
            )
            window['-OUTPUT-'].update(result.stdout)

        except FileNotFoundError:
            window['-OUTPUT-'].update(f"HATA: '{executable_name}' bulunamadı.\nC++ projesini derlediğinizden ve bu dizinde olduğundan emin olun.")
        except subprocess.CalledProcessError as e:
            error_output = e.stderr
            if not error_output: # Bazen C++ hataları stdout'a da gidebilir
                error_output = e.stdout
            window['-OUTPUT-'].update(f"ASSEMBLER HATASI (Return Code: {e.returncode}):\n{error_output}")
        except Exception as e: # Diğer olası hatalar için
            window['-OUTPUT-'].update(f"Beklenmedik bir hata oluştu: {str(e)}")


window.close()