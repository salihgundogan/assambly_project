#include "main.hpp" // AddressingMode enum'u ve Instruction sınıfları burada tanımlı olmalı
#include <sstream>   // istringstream için
#include <fstream>   // ifstream için
#include <string>    // string işlemleri için
#include <vector>    // InstructionSet muhtemelen std::vector kullanıyor
#include <iostream>  // cerr ve cout için

// set_initializer fonksiyonu, InstructionSet nesnesini instructions.txt dosyasından okuyarak doldurur.
void set_initializer(InstructionSet &set)
{
    ifstream file("instructions.txt");
    string line;

    if (!file.is_open()) {
        cerr << "HATA: instructions.txt dosyasi acilamadi! Program sonlandiriliyor." << endl;
        // Burada programı sonlandırmak veya bir hata durumuyla dönmek daha uygun olabilir.
        // Şimdilik sadece mesaj basıp dönüyor.
        return;
    }

    int line_number = 0;
    while (getline(file, line))
    {
        line_number++;
        // Satır başındaki ve sonundaki boşlukları temizle (isteğe bağlı ama iyi bir pratik)
        // line = trim_whitespace(line); // trim_whitespace fonksiyonunuzun main.cpp'de olduğunu varsayıyorum,
                                      // eğer değilse, buraya da eklenmeli veya ortak bir utility dosyasına taşınmalı.
                                      // Şimdilik, main.cpp'deki trim_whitespace'in kullanıldığını varsayalım.
                                      // Eğer set_initializer.cpp ayrı derleniyorsa bu fonksiyonun tanımına ihtiyacı olur.
                                      // Hızlı çözüm için, bu satırı yorumlayıp instructions.txt'nin temiz olduğunu varsayabiliriz.

        if (line.empty() || line[0] == ';') { // Boş satırları veya yorum satırlarını atla
            continue;
        }

        istringstream iss(line);
        string instruction_mnemonic; 
        string opcode_str;
        int opcode_val; 
        int num_of_bytes; 
        string addressing_mode_str; 

        // Satırdan verileri oku
        if (!(iss >> instruction_mnemonic >> opcode_str >> num_of_bytes >> addressing_mode_str)) {
            cerr << "UYARI: instructions.txt dosyasinda satir " << line_number << " hatali format: '" << line << "'. Bu satir atlandi." << endl;
            continue; // Hatalı satırı atla ve devam et
        }
        
        try {
            opcode_val = stoi(opcode_str, nullptr, 16); // Hex string'i integer'a çevir (nullptr eklendi)
        } catch (const std::invalid_argument& ia) {
            cerr << "UYARI: instructions.txt dosyasinda satir " << line_number << " gecersiz opcode degeri: '" << opcode_str << "'. Bu satir atlandi." << endl;
            continue;
        } catch (const std::out_of_range& oor) {
            cerr << "UYARI: instructions.txt dosyasinda satir " << line_number << " opcode degeri aralik disi: '" << opcode_str << "'. Bu satir atlandi." << endl;
            continue;
        }

        AddressingMode mode = AddressingMode::NONE; // Varsayılan olarak NONE ata

        // Adresleme modu string'ini enum değerine çevir
        if (addressing_mode_str == "-" || addressing_mode_str == "NONE") 
        {
            mode = AddressingMode::NONE;
        }
        else if (addressing_mode_str == "IMMEDIATE")
        {
            mode = AddressingMode::IMMEDIATE;
        }
        else if (addressing_mode_str == "DIRECT")
        {
            mode = AddressingMode::DIRECT;
        }
        else if (addressing_mode_str == "IMPLIED" || addressing_mode_str == "ACCUMULATOR") // ACCUMULATOR'ı da IMPLIED olarak kabul edebiliriz
        {
            mode = AddressingMode::IMPLIED;
        }
        else if (addressing_mode_str == "EXTENDED") 
        {
            mode = AddressingMode::EXTENDED;
        }
        else if (addressing_mode_str == "INDEXED")  
        {
            mode = AddressingMode::INDEXED;
        }
        else if (addressing_mode_str == "RELATIVE") 
        {
            mode = AddressingMode::RELATIVE;
        }
        // Eski "REGISTER" ve "REGISTER_INDIRECT" modları için:
        // Eğer instructions.txt'de hala bu string'ler varsa ve enum'da yoksa,
        // aşağıdaki else bloğuna düşecektir.
        // Bu durumda instructions.txt'nin de güncellenmesi en doğrusudur.
        else
        {
            cerr << "UYARI: instructions.txt dosyasinda satir " << line_number
                << " taninmayan veya desteklenmeyen adresleme modu: '" << addressing_mode_str
                << "'. NONE olarak ayarlandi." << endl;
            mode = AddressingMode::NONE; // Tanınmayan modlar için NONE ata
        }

        Instruction instr;
        instr.instruction = instruction_mnemonic;
        instr.opcode = opcode_val;
        instr.no_of_bytes = num_of_bytes;
        instr.addressing_mode = mode;

        // Debug için kullanılan cout satırı yorumda bırakıldı, gerekirse açılabilir.
        // cout << "Loaded: " << instr.instruction << " Opcode: " << hex << instr.opcode << dec 
        //      << " Bytes: " << instr.no_of_bytes << " Mode: " << static_cast<int>(instr.addressing_mode) 
        //      << " (from string: '" << addressing_mode_str << "')" << endl;

        set.add_instruction(instr); // Hazırlanan komutu InstructionSet'e ekle
    }

    file.close();
    // cout << "instructions.txt dosyasi basariyla islendi." << endl; // İşlem bitti mesajı (isteğe bağlı)
}