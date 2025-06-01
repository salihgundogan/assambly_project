#include "main.hpp"
#include "set_initializer.hpp" // set_initializer fonksiyonunun bildirimi burada olmalı
#include <iomanip>
#include <string>
#include <algorithm>
#include <vector> 
#include <fstream> 
#include <bitset>  
#include <regex> // std::regex için (main.hpp'de zaten var ama burada da olması zarar vermez)

// regexPattern'i static yaparak bu dosya için yerel hale getirelim.
// Ve std:: ön ekini açıkça kullanalım.
static const std::regex regexPattern = std::regex("^\\s*(?:(\\w+):\\s*)?(\\w+)\\s*(?:([#$]?[#$]?\\w+(?:\\s*,\\s*\\w+)?)(?:\\s*,\\s*([#$]?[#$]?\\w+))?)?(?:\\s*;\\s*(.*))?$");

SymbolTable symbolTable;
InstructionSet instructionSet;
std::vector<uint8_t> programData; 

// decimal_to_hex, trim_whitespace, hex_string_to_bytes fonksiyonları
// bir önceki mesajınızdaki gibi doğru görünüyor. Onları tekrar eklemiyorum
// ama bu dosyanın içinde veya doğru şekilde link edildiklerinden emin olun.
// Kolaylık olması için buraya kopyalıyorum:

std::string decimal_to_hex(std::string decimalStr) {
    if (decimalStr.empty()) return "XX";
    try {
        std::istringstream iss(decimalStr);
        long long decimal_val; 
        iss >> decimal_val;
        std::stringstream ss;
        if (decimal_val > 0xFFFF) decimal_val = 0xFFFF; 
        if (decimal_val < 0) decimal_val = 0; 
        ss << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << (static_cast<int>(decimal_val) & 0xFF) ; 
        return ss.str();
    } catch (const std::exception& e) {
        std::cerr << "Hata: decimal_to_hex exception: " << e.what() << " girdi: " << decimalStr << std::endl;
        return "ER"; 
    }
}

std::string trim_whitespace(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; 
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

std::vector<uint8_t> hex_string_to_bytes(const std::string& hex_str_in) {
    std::vector<uint8_t> bytes;
    std::string hex_str = hex_str_in; 
    if (hex_str.rfind("$", 0) == 0) hex_str = hex_str.substr(1);
    else if (hex_str.rfind("0x", 0) == 0 || hex_str.rfind("0X", 0) == 0) hex_str = hex_str.substr(2);
    std::string clean_hex_str = "";
    for (char c : hex_str) { 
        if (isxdigit(c)) {
            clean_hex_str += c;
        }
    }
    if (clean_hex_str.empty()) return bytes; 
    if (clean_hex_str.length() % 2 != 0) clean_hex_str = "0" + clean_hex_str;
    for (size_t i = 0; i < clean_hex_str.length(); i += 2) {
        std::string byteString = clean_hex_str.substr(i, 2);
        try {
            uint8_t byte = static_cast<uint8_t>(std::stoi(byteString, nullptr, 16));
            bytes.push_back(byte);
        } catch (const std::exception& e) {
            std::cerr << "Hata: hex_string_to_bytes stoi exception: " << e.what() << " girdi: '" << byteString << "' (orijinal: '" << hex_str_in << "')" << std::endl;
            return {}; 
        }
    }
    return bytes;
}


void parse(std::string line, int lineNumber, int &LC)
{
    std::string originalLineForDisplay = line; 

    if (line.find(';') != std::string::npos)
        line = line.substr(0, line.find(';'));

    std::string processedLineForRegex = trim_whitespace(line);

    if (processedLineForRegex.empty()) { 
        if (trim_whitespace(originalLineForDisplay).find_first_not_of(" \t\n\r\f\v;") == std::string::npos) {
            return;
        }
    }

    std::smatch matches; // std::smatch olarak tanımla
    // regexPattern global olduğu için doğrudan kullanılabilir.
    if (std::regex_match(processedLineForRegex, matches, regexPattern))
    {
        std::string label = trim_whitespace(matches[1].str());
        std::string instruction_mnemonic = trim_whitespace(matches[2].str());
        std::string operand1_raw = trim_whitespace(matches[3].str()); 
        std::string operand2_raw = trim_whitespace(matches[4].str()); 

        std::string actual_operand_value_str; 
        std::string register_operand_str;   

        if (!operand2_raw.empty()) { 
            register_operand_str = operand1_raw;       
            actual_operand_value_str = operand2_raw;   
        } else if (!operand1_raw.empty()) {        
            size_t comma_pos = operand1_raw.find(',');
            if (comma_pos != std::string::npos) {       
                register_operand_str = trim_whitespace(operand1_raw.substr(0, comma_pos));
                actual_operand_value_str = trim_whitespace(operand1_raw.substr(comma_pos + 1));
            } else {                              
                actual_operand_value_str = operand1_raw;
            }
        }

        if (!label.empty() && instruction_mnemonic.empty() && actual_operand_value_str.empty() && register_operand_str.empty()) {
            if (symbolTable.get_symbol(label) == std::nullopt) { 
                symbolTable.add_symbol(label, LC);
                std::cout << processedLineForRegex << " -> (Label Definition at $" << std::hex << std::uppercase << LC << ")" << std::endl;
            } else {
                std::cerr << "Error (Line " << lineNumber << "): Duplicate symbol '" << label << "'" << std::endl;
            }
            return; 
        }
        
        if (instruction_mnemonic.empty()) { 
            return;
        }

        if (!label.empty())
        {
            if (symbolTable.get_symbol(label) == std::nullopt)
            {
                symbolTable.add_symbol(label, LC);
            }
            else
            {
                std::cerr << "Error (Line " << lineNumber << "): Duplicate symbol '" << label << "'" << std::endl;
            }
        }

        if (instruction_mnemonic == "ORG")
        {
            std::cout << processedLineForRegex << " -> (Directive)" << std::endl;
            if (!actual_operand_value_str.empty())
            {
                std::string org_val_str = actual_operand_value_str;
                if (org_val_str[0] == '$') {
                    org_val_str = org_val_str.substr(1);
                }
                try {
                    LC = std::stoi(org_val_str, nullptr, 16);
                } catch (const std::exception& e) {
                    std::cerr << "Error (Line " << lineNumber << "): Invalid ORG value '" << actual_operand_value_str << "'" << std::endl;
                }
            }
            return; 
        }
        
        if (instruction_mnemonic == "END") {
            std::cout << processedLineForRegex << " -> (Directive)" << std::endl;
            return; 
        }

        if (instruction_mnemonic == "LDA" && instructionSet.is_instruction("LDAA")) instruction_mnemonic = "LDAA";
        if (instruction_mnemonic == "STA" && instructionSet.is_instruction("STAA")) instruction_mnemonic = "STAA";

        if (!instructionSet.is_instruction(instruction_mnemonic))
        {
            std::cerr << "Error (Line " << lineNumber << "): Invalid instruction '" << instruction_mnemonic << "'" << std::endl;
            std::cout << processedLineForRegex << " -> ERROR (Invalid Instruction)" << std::endl;
            return;
        }

        AddressingMode determined_mode = AddressingMode::NONE;
        std::vector<uint8_t> operand_bytes_for_this_instruction; 

        // Adresleme modu belirleme ve operand byte'larını oluşturma mantığı
        // (Bir önceki mesajdaki gibi demo odaklı veya daha da geliştirilmiş hali)
        // Örnek olarak INCA'yı da içeren demo odaklı mantık:
        if (instruction_mnemonic == "LDAA") { 
            if (!actual_operand_value_str.empty() && actual_operand_value_str[0] == '#') { 
                determined_mode = AddressingMode::IMMEDIATE;
                std::string val_str = actual_operand_value_str.substr(1); 
                if (!val_str.empty() && val_str[0] == '$') val_str = val_str.substr(1); 
                else if (!val_str.empty()) val_str = decimal_to_hex(val_str); 
                if (!val_str.empty() && val_str.length() <= 2) {
                    operand_bytes_for_this_instruction = hex_string_to_bytes(val_str);
                } // Hata mesajları eklenebilir
            }
            else if (!actual_operand_value_str.empty() && actual_operand_value_str[0] == '$') { 
                std::string addr_str = actual_operand_value_str.substr(1);
                std::vector<uint8_t> temp_addr_bytes = hex_string_to_bytes(addr_str);
                if (temp_addr_bytes.size() == 1) { determined_mode = AddressingMode::DIRECT; operand_bytes_for_this_instruction = temp_addr_bytes; }
                else if (temp_addr_bytes.size() == 2) { determined_mode = AddressingMode::EXTENDED; operand_bytes_for_this_instruction = temp_addr_bytes; }
            } else { /* Etiket işleme */ 
                auto symbol_val = symbolTable.get_symbol(actual_operand_value_str);
                if (symbol_val.has_value()) {
                     if (instructionSet.get_instruction_wrt_address_mode(instruction_mnemonic, AddressingMode::DIRECT).has_value() && symbol_val.value() <= 0xFF) {
                        determined_mode = AddressingMode::DIRECT;
                        operand_bytes_for_this_instruction = hex_string_to_bytes(decimal_to_hex(std::to_string(symbol_val.value() & 0xFF)));
                    } else if (instructionSet.get_instruction_wrt_address_mode(instruction_mnemonic, AddressingMode::EXTENDED).has_value()) {
                        determined_mode = AddressingMode::EXTENDED;
                        std::stringstream ss_label_hex; ss_label_hex << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << symbol_val.value();
                        operand_bytes_for_this_instruction = hex_string_to_bytes(ss_label_hex.str());
                    }
                } else if (!actual_operand_value_str.empty()) { /* Hata */ } else { /* Hata */ }
            }
        } else if (instruction_mnemonic == "STAA") { 
             if (!actual_operand_value_str.empty() && actual_operand_value_str[0] == '$') {
                std::string addr_str = actual_operand_value_str.substr(1); 
                std::vector<uint8_t> temp_addr_bytes = hex_string_to_bytes(addr_str);
                if (temp_addr_bytes.size() == 1) { determined_mode = AddressingMode::DIRECT; operand_bytes_for_this_instruction = temp_addr_bytes; }
                else if (temp_addr_bytes.size() == 2) { determined_mode = AddressingMode::EXTENDED; operand_bytes_for_this_instruction = temp_addr_bytes; }
            } else { /* Etiket işleme */
                auto symbol_val = symbolTable.get_symbol(actual_operand_value_str);
                 if (symbol_val.has_value()) {
                    if (instructionSet.get_instruction_wrt_address_mode(instruction_mnemonic, AddressingMode::DIRECT).has_value() && symbol_val.value() <= 0xFF) {
                        determined_mode = AddressingMode::DIRECT;
                        operand_bytes_for_this_instruction = hex_string_to_bytes(decimal_to_hex(std::to_string(symbol_val.value() & 0xFF)));
                    } else if (instructionSet.get_instruction_wrt_address_mode(instruction_mnemonic, AddressingMode::EXTENDED).has_value()) {
                        determined_mode = AddressingMode::EXTENDED;
                        std::stringstream ss_label_hex; ss_label_hex << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << symbol_val.value();
                        operand_bytes_for_this_instruction = hex_string_to_bytes(ss_label_hex.str());
                    }
                } else if (!actual_operand_value_str.empty()) { /* Hata */ } else { /* Hata */ }
            }
        } else if (instruction_mnemonic == "NOP" || instruction_mnemonic == "INCA" || instruction_mnemonic == "SWI") { 
            determined_mode = AddressingMode::IMPLIED;
        } else if (instruction_mnemonic == "JMP") { 
            determined_mode = AddressingMode::EXTENDED; 
            auto symbol_addr = symbolTable.get_symbol(actual_operand_value_str);
            if (symbol_addr.has_value()) {
                std::stringstream ss_label_hex;
                ss_label_hex << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << symbol_addr.value(); 
                operand_bytes_for_this_instruction = hex_string_to_bytes(ss_label_hex.str()); 
            } else if (!actual_operand_value_str.empty() && actual_operand_value_str[0] == '$') {
                std::string addr_str = actual_operand_value_str.substr(1);
                operand_bytes_for_this_instruction = hex_string_to_bytes(addr_str);
                if (operand_bytes_for_this_instruction.size() != 2 && !operand_bytes_for_this_instruction.empty()) { 
                    operand_bytes_for_this_instruction.assign(2, 0xEE); 
                } else if (operand_bytes_for_this_instruction.empty() && !addr_str.empty()){
                     operand_bytes_for_this_instruction.assign(2, 0xEE); 
                }
            } else if (!actual_operand_value_str.empty()){ 
                operand_bytes_for_this_instruction.assign(2, 0xEE); 
            } else { 
                 operand_bytes_for_this_instruction.assign(2, 0xEE);
            }
        } else { 
            auto variants = instructionSet.get_instruction(instruction_mnemonic);
            if(!variants.empty()){
                if (actual_operand_value_str.empty()) determined_mode = AddressingMode::IMPLIED; 
                else determined_mode = variants[0].addressing_mode; 
            }
        }
        
        std::optional<Instruction> ins_data_opt = instructionSet.get_instruction_wrt_address_mode(instruction_mnemonic, determined_mode);
        
        if (!ins_data_opt.has_value())
        {
            std::cerr << "Error (Line " << lineNumber << "): No instruction variant found for '" << instruction_mnemonic 
                 << "' that matches determined addressing mode (" << static_cast<int>(determined_mode) 
                 << "). Operand: '" << actual_operand_value_str << "'" << std::endl;
            std::cout << processedLineForRegex << " -> ERROR (Opcode/Mode Mismatch)" << std::endl;
            auto any_variant = instructionSet.get_instruction(instruction_mnemonic);
            if (!any_variant.empty()) LC += any_variant[0].no_of_bytes; else LC +=1; 
            return;
        }
        
        Instruction instructionData = ins_data_opt.value();

        std::cout << processedLineForRegex << " ->";
        std::cout << " " << decimal_to_hex(std::to_string(instructionData.opcode)); 
        programData.push_back(instructionData.opcode); 

        for(uint8_t byte_val : operand_bytes_for_this_instruction) { 
            std::cout << " " << decimal_to_hex(std::to_string(byte_val));
            programData.push_back(byte_val); 
        }
        
        int expected_operand_bytes = instructionData.no_of_bytes - 1;
        if (operand_bytes_for_this_instruction.size() < expected_operand_bytes) {
             for (int i = operand_bytes_for_this_instruction.size(); i < expected_operand_bytes; ++i) {
                std::cout << " XX"; 
                programData.push_back(0xEE); 
            }
        } else if (operand_bytes_for_this_instruction.size() > expected_operand_bytes && expected_operand_bytes >= 0) {
             std::cerr << "Warning (Line " << lineNumber << "): Too many operand bytes generated for " << instruction_mnemonic << std::endl;
        }
        
        std::cout << std::endl;
        LC += instructionData.no_of_bytes;

    } else { 
        if (!processedLineForRegex.empty()) { 
            std::cerr << "Error (Line " << lineNumber << "): Syntax error (no regex match): '" << processedLineForRegex << "'" << std::endl;
            std::cout << processedLineForRegex << " -> ERROR (Syntax)" << std::endl;
        }
    }
}
int main(int argc, char *argv[])
{
    // Argüman sayısını kontrol et: program_adı <giriş_dosyası> <çıktı_binary_string_dosyası.txt>
    // Çıktı dosyası adı için 3. argümanı kullanacağız.
    if (argc != 3) 
    {
        std::cerr << "Usage: " << argv[0] << " <assembly_source_file> <output_text_binary_file.txt>" << std::endl;
        return 1;
    }

    set_initializer(instructionSet); // Komut setini yükle

    std::ifstream file(argv[1]); // Assembly kaynak dosyasını aç
    if (!file.is_open()) {
        std::cerr << "Error: Could not open assembly file '" << argv[1] << "'" << std::endl;
        return 1;
    }

    std::string line;
    int LC = 0;         
    int lineNumber = 0; 
    programData.clear(); // Her çalıştırmada programData'yı temizle

    while (std::getline(file, line))
    {
        lineNumber++;
        parse(line, lineNumber, LC); // Bu fonksiyon programData vektörünü doldurmalı
    }
    file.close();

    // --- YENİ DOSYAYA YAZMA KISMI (Text formatında 0 ve 1'ler) ---
    std::string output_text_filename = argv[2]; // Komut satırından gelen dosya adını kullan
    std::ofstream txt_outfile(output_text_filename); // Binary modda DEĞİL, text modda aç

    if (!txt_outfile.is_open()) {
        std::cerr << "HATA: Metin tabanlı binary cikti dosyasi '" << output_text_filename << "' olusturulamadi!" << std::endl;
        return 1; 
    }

    if (!programData.empty()) { 
        for (uint8_t byte_val : programData) { // programData global vektörünüzü kullanıyoruz
            std::bitset<8> bits(byte_val); // uint8_t'yi 8-bitlik bir bitset'e çevir
            txt_outfile << bits.to_string() << std::endl; // "01011010" formatında yaz, her byte yeni satırda
        }
        std::cout << "Metin tabanlı binary (0 ve 1) dosyasi '" << output_text_filename << "' (" << programData.size() << " bytes represented) basariyla olusturuldu." << std::endl;
    } else if (lineNumber > 0) { 
         std::cout << "Metin tabanlı binary (txt) dosyasi '" << output_text_filename << "' olusturuldu (0 bytes represented - sadece direktifler veya hatalar olabilir)." << std::endl;
         txt_outfile << ""; // Boş dosya oluşturulsun
    } else { 
        std::cout << "Metin tabanlı binary (txt) dosyasi '" << output_text_filename << "' olusturuldu (0 bytes represented)." << std::endl;
        txt_outfile << ""; 
    }
    txt_outfile.close();
    // --- YENİ DOSYAYA YAZMA KISMI BİTTİ ---
    
    return 0;
}
