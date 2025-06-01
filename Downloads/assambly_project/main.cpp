#include "main.hpp"
#include "set_initializer.hpp" // set_initializer fonksiyonunun tanımı burada olmalı
#include <iomanip>
#include <string>
#include <algorithm>
#include <vector> 

// Regex deseni aynı kalıyor
regex regexPattern = regex("^\\s*(?:(\\w+):\\s*)?(\\w+)\\s*(?:([#$]?[#$]?\\w+(?:\\s*,\\s*\\w+)?)(?:\\s*,\\s*([#$]?[#$]?\\w+))?)?(?:\\s*;\\s*(.*))?$");

SymbolTable symbolTable;
InstructionSet instructionSet;

string decimal_to_hex(string decimalStr)
{
    if (decimalStr.empty()) return "XX"; //
    try {
        istringstream iss(decimalStr);
        long long decimal_val; //
        iss >> decimal_val;
        if (iss.fail() || !iss.eof()) { // 
             // Eğer zaten hex ise (örn: "A", "B") ve decimal_to_hex'e geldiyse, bu bir mantık hatasıdır.
             // Ama #sayı formatında decimal bekliyoruz.
            // cerr << "Uyarı: decimal_to_hex geçersiz onluk sayı: " << decimalStr << endl;
            // Belki de zaten hex'tir? Bu fonksiyon sadece decimal -> hex yapmalı.
            // Şimdilik, eğer çevrim başarısızsa, orijinali döndürmeyi deneyebiliriz (riskli)
            // veya hata kodu. En iyisi çağıran yerin doğru formatta yollaması.
            // Biz yine de hex'e çevirmeye çalışalım, belki stoi daha iyi işler.
        }
        stringstream ss;
        // Eğer sayı 255'ten büyükse (tek byte'tan fazla), bu fonksiyonun amacı dışına çıkar.
        // Ama adresler için de kullanılabilir. Şimdilik 2 byte (FFFF) ile sınırlayalım.
        if (decimal_val > 0xFFFF) decimal_val = 0xFFFF; // Kırpma
        if (decimal_val < 0) decimal_val = 0; // Negatif olmamalı

        // Kaç byte'lık hex üreteceğimize karar verelim.
        // Eğer decimalStr bir adres ise ve büyükse, 4 karakter (2 byte) olabilir.
        // Eğer tek byte'lık bir değerse 2 karakter.
        // Bu fonksiyonun amacı tek byte'lık hex üretmekti.
        ss << hex << setw(2) << setfill('0') << uppercase << (static_cast<int>(decimal_val) & 0xFF) ; // Sadece son byte'ı al
        return ss.str();
    } catch (const std::exception& e) {
        cerr << "Hata: decimal_to_hex exception: " << e.what() << " girdi: " << decimalStr << endl;
        return "ER"; // Hata kodu
    }
}

string trim_whitespace(const string& str) {
    const string whitespace = " \t\n\r\f\v";
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == string::npos)
        return ""; 

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

vector<uint8_t> hex_string_to_bytes(const string& hex_str_in) {
    vector<uint8_t> bytes;
    string hex_str = hex_str_in; // Kopyasını alalım ki orijinali değiştirmeyelim
    
    // Başındaki olası '$' veya '0x' gibi hex belirteçlerini temizle (varsa)
    if (hex_str.rfind("$", 0) == 0) hex_str = hex_str.substr(1);
    else if (hex_str.rfind("0x", 0) == 0 || hex_str.rfind("0X", 0) == 0) hex_str = hex_str.substr(2);

    string clean_hex_str = "";
    for (char c : hex_str) { // Sadece geçerli hex karakterlerini al
        if (isxdigit(c)) {
            clean_hex_str += c;
        }
    }
    
    if (clean_hex_str.empty()) {
        // cerr << "Uyarı: hex_string_to_bytes boş veya geçersiz hex string aldı: " << hex_str_in << endl;
        return bytes; // Boş vektör döndür
    }

    if (clean_hex_str.length() % 2 != 0) {
        clean_hex_str = "0" + clean_hex_str;
    }

    for (size_t i = 0; i < clean_hex_str.length(); i += 2) {
        string byteString = clean_hex_str.substr(i, 2);
        try {
            uint8_t byte = static_cast<uint8_t>(stoi(byteString, nullptr, 16));
            bytes.push_back(byte);
        } catch (const std::exception& e) {
            cerr << "Hata: hex_string_to_bytes stoi exception: " << e.what() << " girdi: " << byteString << endl;
            // Hata durumunda işlemi durdurabilir veya devam edebiliriz. Şimdilik devam edip boş vektör döndürelim.
            return {}; 
        }
    }
    return bytes;
}

void parse(string line, int lineNumber, int &LC)
{
    string originalLineForDisplay = line; 

    if (line.find(';') != string::npos)
        line = line.substr(0, line.find(';'));

    string processedLineForRegex = trim_whitespace(line);

    if (processedLineForRegex.empty()) { // Tamamen boş veya sadece yorumdan oluşan satırları atla
        // Eğer orijinal satırda sadece etiket varsa, bu regex_match içinde ele alınacak.
        // Bu kontrol, regex'e gitmeden önce tamamen boş satırları filtreler.
        if (trim_whitespace(originalLineForDisplay).find_first_not_of(" \t\n\r\f\v;") == string::npos) {
            return;
        }
    }

    smatch matches;
    if (regex_match(processedLineForRegex, matches, regexPattern))
    {
        string label = trim_whitespace(matches[1].str());
        string instruction_mnemonic = trim_whitespace(matches[2].str());
        string operand1_raw = trim_whitespace(matches[3].str()); 
        string operand2_raw = trim_whitespace(matches[4].str()); 

        string actual_operand_value_str; 
        string register_operand_str;   

        // Operand ayrıştırma mantığı (Regex'inizin davranışına göre ayarlanmalı)
        if (!operand2_raw.empty()) { 
            register_operand_str = operand1_raw;       
            actual_operand_value_str = operand2_raw;   
        } else if (!operand1_raw.empty()) {        
            size_t comma_pos = operand1_raw.find(',');
            if (comma_pos != string::npos) {       
                register_operand_str = trim_whitespace(operand1_raw.substr(0, comma_pos));
                actual_operand_value_str = trim_whitespace(operand1_raw.substr(comma_pos + 1));
            } else {                              
                actual_operand_value_str = operand1_raw;
            }
        }
        // LDAA/STAA gibi komutlarda eğer register_operand_str boşsa ve komut "A" ile çalışıyorsa,
        // ve actual_operand_value_str bir adres veya immediate değerse, bu geçerli olabilir.
        // Örn: LDAA #$25 (register_operand_str boş, actual_operand_value_str = #$25)

        if (!label.empty() && instruction_mnemonic.empty() && actual_operand_value_str.empty() && register_operand_str.empty()) {
            if (symbolTable.get_symbol(label) == nullopt) {
                symbolTable.add_symbol(label, LC);
                cout << processedLineForRegex << " -> (Label Definition at $" << hex << uppercase << LC << ")" << endl;
            } else {
                cerr << "Error (Line " << lineNumber << "): Duplicate symbol '" << label << "'" << endl;
            }
            return; 
        }
        
        if (instruction_mnemonic.empty()) { // Etiket olabilir ama komut yoksa yukarıda işlendi.
            if (!label.empty() && !processedLineForRegex.empty() && processedLineForRegex != label + ":") {
                 // Etiket var ama komut yok ve satırda başka bir şey varsa (örn: ETIKET:   )
                 // Bu durum yukarıdaki label-only check ile yakalanmalı.
            } else if (processedLineForRegex.empty()){
                return; // Tamamen boş veya sadece yorum ise dön.
            }
            // Eğer buraya gelindiyse ve komut boşsa, muhtemelen bir regex eşleşme sorunu veya format hatasıdır.
            // cerr << "Warning (Line " << lineNumber << "): No instruction mnemonic found in: '" << processedLineForRegex << "'" << endl;
            return;
        }

        if (!label.empty())
        {
            if (symbolTable.get_symbol(label) == nullopt)
            {
                symbolTable.add_symbol(label, LC);
            }
            else
            {
                cerr << "Error (Line " << lineNumber << "): Duplicate symbol '" << label << "'" << endl;
            }
        }

        if (instruction_mnemonic == "ORG")
        {
            cout << processedLineForRegex << " -> (Directive)" << endl;
            if (!actual_operand_value_str.empty())
            {
                string org_val_str = actual_operand_value_str;
                if (org_val_str[0] == '$') {
                    org_val_str = org_val_str.substr(1);
                }
                try {
                    LC = stoi(org_val_str, nullptr, 16);
                } catch (const std::exception& e) {
                    cerr << "Error (Line " << lineNumber << "): Invalid ORG value '" << actual_operand_value_str << "'" << endl;
                }
            }
            return; 
        }
        
        if (instruction_mnemonic == "END") {
            cout << processedLineForRegex << " -> (Directive)" << endl;
            return; 
        }

        // Komut adını düzeltme (LDA -> LDAA, STA -> STAA)
        if (instruction_mnemonic == "LDA" && instructionSet.is_instruction("LDAA")) instruction_mnemonic = "LDAA";
        if (instruction_mnemonic == "STA" && instructionSet.is_instruction("STAA")) instruction_mnemonic = "STAA";
        // INCA için böyle bir durum yok, genellikle tek formattadır.

        if (!instructionSet.is_instruction(instruction_mnemonic))
        {
            cerr << "Error (Line " << lineNumber << "): Invalid instruction '" << instruction_mnemonic << "'" << endl;
            cout << processedLineForRegex << " -> ERROR (Invalid Instruction)" << endl;
            return;
        }

        AddressingMode determined_mode = AddressingMode::NONE;
        vector<uint8_t> operand_bytes_vec; 

        if (instruction_mnemonic == "LDAA") { 
            if (!actual_operand_value_str.empty() && actual_operand_value_str[0] == '#') { 
                determined_mode = AddressingMode::IMMEDIATE;
                string val_str = actual_operand_value_str.substr(1); 
                if (!val_str.empty() && val_str[0] == '$') val_str = val_str.substr(1); 
                else if (!val_str.empty()) val_str = decimal_to_hex(val_str); 
                
                if (!val_str.empty() && val_str.length() <= 2) {
                    operand_bytes_vec = hex_string_to_bytes(val_str);
                } else if (!val_str.empty()) {
                     cerr << "Error (Line " << lineNumber << "): Immediate value '"+actual_operand_value_str+"' too large or invalid for LDAA." << endl;
                } else {
                    cerr << "Error (Line " << lineNumber << "): Immediate value missing for LDAA." << endl;
                }
            }
            else if (!actual_operand_value_str.empty() && actual_operand_value_str[0] == '$') { // LDAA $ADDR
                string addr_str = actual_operand_value_str.substr(1);
                vector<uint8_t> temp_addr_bytes = hex_string_to_bytes(addr_str);
                if (temp_addr_bytes.size() == 1) {
                    determined_mode = AddressingMode::DIRECT;
                    operand_bytes_vec = temp_addr_bytes;
                } else if (temp_addr_bytes.size() == 2) {
                    determined_mode = AddressingMode::EXTENDED;
                    operand_bytes_vec = temp_addr_bytes;
                } else {
                    cerr << "Error (Line " << lineNumber << "): Address '"+actual_operand_value_str+"' invalid for LDAA." << endl;
                }
            }
            // TODO: LDAA için INDEXED modu eklenebilir.
            else { // Etiket veya tanımsız operand olabilir
                 // Şimdilik etiketleri DIRECT veya EXTENDED olarak varsayalım (JMP'deki gibi)
                auto symbol_val = symbolTable.get_symbol(actual_operand_value_str);
                if (symbol_val.has_value()) {
                    // Komut setinden LDAA'nın DIRECT ve EXTENDED versiyonlarını kontrol et
                    bool has_direct = instructionSet.get_instruction_wrt_address_mode(instruction_mnemonic, AddressingMode::DIRECT).has_value();
                    bool has_extended = instructionSet.get_instruction_wrt_address_mode(instruction_mnemonic, AddressingMode::EXTENDED).has_value();

                    if (has_direct && symbol_val.value() <= 0xFF) {
                        determined_mode = AddressingMode::DIRECT;
                        operand_bytes_vec = hex_string_to_bytes(decimal_to_hex(to_string(symbol_val.value() & 0xFF)));
                    } else if (has_extended) {
                        determined_mode = AddressingMode::EXTENDED;
                        stringstream ss_label_hex;
                        ss_label_hex << hex << setw(4) << setfill('0') << uppercase << symbol_val.value();
                        operand_bytes_vec = hex_string_to_bytes(ss_label_hex.str());
                    } else {
                         cerr << "Error (Line " << lineNumber << "): No suitable addressing mode for LDAA with label '" << actual_operand_value_str << "'" << endl;
                    }
                } else if (!actual_operand_value_str.empty()){ // Etiket değil ve # veya $ ile başlamıyorsa
                     cerr << "Error (Line " << lineNumber << "): Invalid or unsupported operand for LDAA: " << actual_operand_value_str << endl;
                     cout << processedLineForRegex << " -> ERROR (LDAA Operand)" << endl;
                     return;
                } else { // Operandsız LDAA (hatalı)
                    cerr << "Error (Line " << lineNumber << "): Operand missing for LDAA." << endl;
                    cout << processedLineForRegex << " -> ERROR (LDAA Operand)" << endl;
                    return;
                }
            }
        } else if (instruction_mnemonic == "STAA") { 
            if (!actual_operand_value_str.empty() && actual_operand_value_str[0] == '$') { 
                string addr_str = actual_operand_value_str.substr(1); 
                vector<uint8_t> temp_addr_bytes = hex_string_to_bytes(addr_str);

                if (temp_addr_bytes.size() == 1) { 
                    determined_mode = AddressingMode::DIRECT;
                    operand_bytes_vec = temp_addr_bytes; 
                } else if (temp_addr_bytes.size() == 2) { 
                    determined_mode = AddressingMode::EXTENDED;
                    operand_bytes_vec = temp_addr_bytes; 
                } else if (!addr_str.empty()){ 
                    cerr << "Error (Line " << lineNumber << "): Address '"+actual_operand_value_str+"' invalid size or format for STAA." << endl;
                } else {
                     cerr << "Error (Line " << lineNumber << "): Address missing for STAA." << endl;
                }
            }
            else { // Etiket veya tanımsız operand
                auto symbol_val = symbolTable.get_symbol(actual_operand_value_str);
                if (symbol_val.has_value()) {
                    bool has_direct = instructionSet.get_instruction_wrt_address_mode(instruction_mnemonic, AddressingMode::DIRECT).has_value();
                    bool has_extended = instructionSet.get_instruction_wrt_address_mode(instruction_mnemonic, AddressingMode::EXTENDED).has_value();

                    if (has_direct && symbol_val.value() <= 0xFF) {
                        determined_mode = AddressingMode::DIRECT;
                        operand_bytes_vec = hex_string_to_bytes(decimal_to_hex(to_string(symbol_val.value() & 0xFF)));
                    } else if (has_extended) {
                        determined_mode = AddressingMode::EXTENDED;
                        stringstream ss_label_hex;
                        ss_label_hex << hex << setw(4) << setfill('0') << uppercase << symbol_val.value();
                        operand_bytes_vec = hex_string_to_bytes(ss_label_hex.str());
                    } else {
                         cerr << "Error (Line " << lineNumber << "): No suitable addressing mode for STAA with label '" << actual_operand_value_str << "'" << endl;
                    }
                } else if (!actual_operand_value_str.empty()){
                     cerr << "Error (Line " << lineNumber << "): Invalid or unsupported operand for STAA: " << actual_operand_value_str << endl;
                     cout << processedLineForRegex << " -> ERROR (STAA Operand)" << endl;
                     return;
                } else {
                    cerr << "Error (Line " << lineNumber << "): Operand missing for STAA." << endl;
                    cout << processedLineForRegex << " -> ERROR (STAA Operand)" << endl;
                    return;
                }
            }
        } else if (instruction_mnemonic == "NOP") {
            determined_mode = AddressingMode::IMPLIED;
        } else if (instruction_mnemonic == "INCA") { // INCA EKLENDİ
            determined_mode = AddressingMode::IMPLIED;
            // INCA operand almaz, operand_bytes_vec boş kalır.
            // register_operand_str "A" olmalıydı, ama IMPLIED modda bu zaten bellidir.
        } else if (instruction_mnemonic == "JMP") { 
            determined_mode = AddressingMode::EXTENDED; // JMP için genellikle Extended veya Indexed
            auto symbol_addr = symbolTable.get_symbol(actual_operand_value_str);
            if (symbol_addr.has_value()) {
                stringstream ss_label_hex;
                ss_label_hex << hex << setw(4) << setfill('0') << uppercase << symbol_addr.value(); 
                operand_bytes_vec = hex_string_to_bytes(ss_label_hex.str()); 
            } else {
                if (!actual_operand_value_str.empty() && actual_operand_value_str[0] == '$') {
                    string addr_str = actual_operand_value_str.substr(1);
                    operand_bytes_vec = hex_string_to_bytes(addr_str);
                    if (operand_bytes_vec.size() != 2) { 
                        cerr << "Error (Line " << lineNumber << "): JMP address '"+actual_operand_value_str+"' must be 2 bytes for EXTENDED mode." << endl;
                        operand_bytes_vec.assign(2, 0xEE); // Hata göstergesi
                    }
                } else if (!actual_operand_value_str.empty()){
                    cerr << "Error (Line " << lineNumber << "): Undefined symbol or invalid address '" << actual_operand_value_str << "' for JMP" << endl;
                    operand_bytes_vec.assign(2, 0xEE); 
                } else {
                     cerr << "Error (Line " << lineNumber << "): Operand missing for JMP." << endl;
                     operand_bytes_vec.assign(2, 0xEE);
                }
            }
        } else {
            // Diğer komutlar için genel bir fallback (varsa)
            auto variants = instructionSet.get_instruction(instruction_mnemonic);
            if(!variants.empty()){
                 // En yaygın olanı veya ilk bulduğunu alabiliriz.
                 // Bu kısım daha sofistike bir adresleme modu belirleme gerektirir.
                 // Şimdilik, eğer operandsız bir komutsa IMPLIED varsayalım.
                 bool operand_expected = false;
                 for(const auto& var : variants) {
                     if (var.addressing_mode != AddressingMode::IMPLIED && var.addressing_mode != AddressingMode::NONE) {
                         operand_expected = true;
                         break;
                     }
                 }
                 if (!operand_expected && !actual_operand_value_str.empty()) {
                     cerr << "Warning (Line " << lineNumber << "): Operand given for an implied mode instruction '" << instruction_mnemonic << "'" << endl;
                 }
                 if (actual_operand_value_str.empty()) { // Operandsızsa IMPLIED deneyelim
                    determined_mode = AddressingMode::IMPLIED;
                 } else {
                    // Daha karmaşık mod belirleme veya hata
                    cerr << "Warning (Line " << lineNumber << "): Addressing mode determination for '" << instruction_mnemonic << "' with operand '" << actual_operand_value_str << "' not fully implemented for demo." << endl;
                    determined_mode = variants[0].addressing_mode; // İlkini al, muhtemelen XX basar
                }
            } else { 
                 // Bu durum is_instruction() tarafından yakalanmalıydı.
                cout << processedLineForRegex << " -> ERROR (Unknown instruction variant after check)" << endl;
                return;
            }
        }

        optional<Instruction> ins_data_opt = instructionSet.get_instruction_wrt_address_mode(instruction_mnemonic, determined_mode);
        
        if (!ins_data_opt.has_value())
        {
            // Eğer belirlenen modla komut bulunamazsa, alternatif modları dene (örn: LDAA $ADR -> DIRECT veya EXTENDED)
            // Bu kısım, adresleme modu belirleme mantığının daha da geliştirilmesini gerektirir.
            // Şimdilik, hata mesajını daha bilgilendirici yapalım.
            cerr << "Error (Line " << lineNumber << "): No instruction variant found for '" << instruction_mnemonic 
                << "' that matches determined addressing mode (" << static_cast<int>(determined_mode) 
                << "). Check instructions.txt or parsing logic for this operand: '" << actual_operand_value_str << "'" << endl;
            cout << processedLineForRegex << " -> ERROR (Opcode/Mode Mismatch)" << endl;
            // LC'yi artıralım ki sonraki satırlar kaymasın, ama bu satır hatalı.
            auto any_variant = instructionSet.get_instruction(instruction_mnemonic);
            if (!any_variant.empty()) LC += any_variant[0].no_of_bytes; else LC +=1; // En az 1 byte ilerle
            return;
        }
        
        Instruction instructionData = ins_data_opt.value();

        cout << processedLineForRegex << " ->";
        cout << " " << decimal_to_hex(to_string(instructionData.opcode));

        for(uint8_t byte_val : operand_bytes_vec) {
            cout << " " << decimal_to_hex(to_string(byte_val));
        }
        
        int expected_operand_bytes = instructionData.no_of_bytes - 1;
        if (operand_bytes_vec.size() < expected_operand_bytes) {
            for (int i = operand_bytes_vec.size(); i < expected_operand_bytes; ++i) {
                cout << " XX"; 
            }
        } else if (operand_bytes_vec.size() > expected_operand_bytes && expected_operand_bytes >= 0) {
            cerr << "Warning (Line " << lineNumber << "): Too many operand bytes generated for " << instruction_mnemonic 
                << ". Expected " << expected_operand_bytes << ", got " << operand_bytes_vec.size() << endl;
             // Sadece beklenen kadarını basmaya gerek yok, zaten for döngüsü doğru sayıda basar.
             // Bu, operand_bytes_vec'in yanlış doldurulduğunu gösterir.
        }
        
        cout << endl;
        
        LC += instructionData.no_of_bytes;
    } else { 
        if (!processedLineForRegex.empty()) { 
            cerr << "Error (Line " << lineNumber << "): Syntax error (no regex match): '" << processedLineForRegex << "'" << endl;
            cout << processedLineForRegex << " -> ERROR (Syntax)" << endl;
        }
    }
}

// main fonksiyonu aynı kalıyor
int main(int argc, char *argv[])
{
    if (argc != 2) 
    {
        cerr << "Usage: " << argv[0] << " <assembly_source_file>" << endl;
        return 1;
    }

    set_initializer(instructionSet); 

    ifstream file(argv[1]); 
    if (!file.is_open()) {
        cerr << "Error: Could not open assembly file " << argv[1] << endl;
        return 1;
    }

    string line;
    int LC = 0;         
    int lineNumber = 0; 

    while (getline(file, line))
    {
        lineNumber++;
        parse(line, lineNumber, LC);
    }

    file.close();
    return 0;
}
