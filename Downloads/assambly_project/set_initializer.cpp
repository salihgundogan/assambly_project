#include "set_initializer.hpp" // Kendi başlık dosyasını include eder (veya doğrudan main.hpp)

#include <sstream>   // istringstream için
#include <fstream>   // ifstream için
#include <string>    // std::string işlemleri için
#include <vector>    // std::vector (InstructionSet içinde kullanılıyorsa)
#include <iostream>  // cerr ve cout için
#include <stdexcept> // std::invalid_argument, std::out_of_range için

// trim_whitespace fonksiyonunun tanımı burada olmalı veya
// ortak bir utility dosyasından include edilmeli.
// Şimdilik, main.cpp'de olduğunu ve link edileceğini varsayıyoruz.
// Eğer ayrı derleniyorsa ve main.cpp'ye bağımlıysa, bu fonksiyonun
// bildirimi de set_initializer.hpp'ye veya ortak bir başlığa eklenebilir.
// En temizi, eğer bu fonksiyon sadece burada kullanılacaksa, buraya kopyalamaktır
// ya da static inline olarak başlıkta tanımlamaktır.
// Hızlı çözüm için, main.cpp'deki trim_whitespace'in linkleneceğini varsayalım.
// VEYA, eğer main.cpp'deki trim_whitespace'i burada kullanmak istiyorsanız,
// main.hpp içine bildirimini ekleyebilirsiniz:
// extern std::string trim_whitespace(const std::string& str); // main.hpp'ye eklenebilir
// Ya da en basiti, fonksiyonu buraya da kopyalayalım (DRY prensibine aykırı ama hızlı):

// String'in başındaki ve sonundaki boşlukları temizleyen yardımcı fonksiyon
// Bu fonksiyon main.cpp'de de vardı. İdealde tek bir yerde tanımlanmalı.
// Eğer main.cpp ile birlikte link ediliyorsa sorun olmaz.
// Aksi takdirde, buraya da kopyalayabilir veya ortak bir utility dosyasına taşıyabilirsiniz.
// Şimdilik, main.cpp'deki ile aynı olduğunu varsayarak buraya eklemiyorum.
// Eğer link hatası alırsanız (undefined reference to trim_whitespace),
// main.cpp'deki trim_whitespace fonksiyonunu buraya kopyalayın.
// Örnek olarak, main.cpp'deki trim_whitespace'i buraya kopyalayalım:
std::string trim_whitespace_local(const std::string& str) { // İsim çakışmasını önlemek için _local ekledim
    const std::string whitespace = " \t\n\r\f\v";
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; 
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}


// set_initializer fonksiyonunun tam tanımı (gövdesi)
void set_initializer(InstructionSet &set)
{
    std::ifstream file("instructions.txt");
    std::string line;

    if (!file.is_open()) {
        std::cerr << "HATA: instructions.txt dosyasi acilamadi! Program sonlandiriliyor." << std::endl;
        return;
    }

    int line_number = 0;
    while (std::getline(file, line))
    {
        line_number++;
        line = trim_whitespace_local(line); // Yerel trim_whitespace'i kullan

        if (line.empty() || line[0] == ';') { // Boş satırları veya yorum satırlarını atla
            continue;
        }

        std::istringstream iss(line);
        std::string instruction_mnemonic; 
        std::string opcode_str;
        int opcode_val; 
        int num_of_bytes; 
        std::string addressing_mode_str; 

        if (!(iss >> instruction_mnemonic >> opcode_str >> num_of_bytes >> addressing_mode_str)) {
            std::cerr << "UYARI: instructions.txt dosyasinda satir " << line_number << " hatali format: '" << line << "'. Bu satir atlandi." << std::endl;
            continue; 
        }
        
        try {
            opcode_val = std::stoi(opcode_str, nullptr, 16); 
        } catch (const std::invalid_argument& ia) {
            std::cerr << "UYARI: instructions.txt dosyasinda satir " << line_number << " gecersiz opcode degeri: '" << opcode_str << "'. Bu satir atlandi." << std::endl;
            continue;
        } catch (const std::out_of_range& oor) {
            std::cerr << "UYARI: instructions.txt dosyasinda satir " << line_number << " opcode degeri aralik disi: '" << opcode_str << "'. Bu satir atlandi." << std::endl;
            continue;
        }

        AddressingMode mode = AddressingMode::NONE; 

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
        else if (addressing_mode_str == "IMPLIED" || addressing_mode_str == "ACCUMULATOR")
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
        else
        {
            std::cerr << "UYARI: instructions.txt dosyasinda satir " << line_number
                << " taninmayan veya desteklenmeyen adresleme modu: '" << addressing_mode_str
                << "'. NONE olarak ayarlandi." << std::endl;
            mode = AddressingMode::NONE; 
        }

        Instruction instr;
        instr.instruction = instruction_mnemonic;
        instr.opcode = opcode_val;
        instr.no_of_bytes = num_of_bytes;
        instr.addressing_mode = mode;
        
        set.add_instruction(instr); 
    }

    file.close();
}
