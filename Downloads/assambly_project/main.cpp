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
    // Gelen string boşsa veya sayıya çevrilemiyorsa hata yönetimi eklenebilir.
    if (decimalStr.empty()) return "XX"; // Veya hata fırlat
    try {
        istringstream iss(decimalStr);
        long long decimal_val; // Daha büyük sayılar için long long
        iss >> decimal_val;
        if (iss.fail() || !iss.eof()) { // Tamamen sayıya çevrilemediyse
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

    if (processedLineForRegex.empty()) {
        return;
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

        // Regex'inizin operandları nasıl yakaladığına göre bu kısım çok önemlidir.
        // LDAA #$25 -> regex'in matches[3]="LDAA", matches[4]="#$25" yakalaması beklenir.
        // VEYA matches[3]="A", matches[4]="#$25" (eğer LDAA yerine LDA A yazılırsa)
        // VEYA matches[3]="A, #$25" (tek grup)
        // Bu kısım sizin regex'inize göre ayarlanmalı.
        // Önceki kodunuzda LDA/STA için bir mantık vardı, onu temel alalım.
        // Komut adını da düzeltelim (LDA -> LDAA)
        if ((instruction_mnemonic == "LDA" || instruction_mnemonic == "LDAA") || (instruction_mnemonic == "STA" || instruction_mnemonic == "STAA")) {
            if (instruction_mnemonic == "LDA") instruction_mnemonic = "LDAA";
            if (instruction_mnemonic == "STA") instruction_mnemonic = "STAA";

            if (!operand2_raw.empty()) { // Eğer regex operand2'yi (matches[4]) yakaladıysa (örn: LDA A, #$25)
                register_operand_str = operand1_raw;       // "A"
                actual_operand_value_str = operand2_raw;   // "#$25"
            } else if (!operand1_raw.empty()) {        // Sadece operand1_raw (matches[3]) doluysa
                size_t comma_pos = operand1_raw.find(',');
                if (comma_pos != string::npos) {       // "A,#$25" gibi virgülle ayrılmışsa
                    register_operand_str = trim_whitespace(operand1_raw.substr(0, comma_pos));
                    actual_operand_value_str = trim_whitespace(operand1_raw.substr(comma_pos + 1));
                } else {                              // Bu durum LDAA #$25 (register belirtilmemiş) veya LDAA $ADRES için.
                                                      // Eğer register_operand_str boşsa ve komut A/B ile çalışıyorsa, A varsayalım.
                    if (instruction_mnemonic == "LDAA" || instruction_mnemonic == "STAA") {
                        // register_operand_str = "A"; // Bunu varsaymak yerine, komut setinden kontrol edilebilir.
                                                    // Şimdilik, eğer operand1_raw '#' veya '$' ile başlamıyorsa,
                                                    // ve register değilse, bu bir etiket olabilir.
                                                    // Ama LDAA/STAA için bu durum biraz farklı.
                                                    // Eğer LDAA #$25 ise, register_operand_str boş, actual_operand_value_str = #$25
                                                    // Eğer LDAA $ADR ise, register_operand_str boş, actual_operand_value_str = $ADR
                        actual_operand_value_str = operand1_raw;
                    } else { // JMP gibi durumlar
                         actual_operand_value_str = operand1_raw;
                    }
                }
            }
        } else { // NOP, JMP gibi durumlar
            actual_operand_value_str = operand1_raw; 
        }


        if (!label.empty() && instruction_mnemonic.empty() && actual_operand_value_str.empty()) {
            if (symbolTable.get_symbol(label) == nullopt) {
                symbolTable.add_symbol(label, LC);
                cout << processedLineForRegex << " -> (Label Definition at $" << hex << uppercase << LC << ")" << endl;
            } else {
                cerr << "Error (Line " << lineNumber << "): Duplicate symbol '" << label << "'" << endl;
            }
            return; 
        }
        
        if (instruction_mnemonic.empty()) {
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
            // Diğer LDAA modları (DIRECT, EXTENDED, INDEXED) buraya eklenebilir.
            // Örnek: else if (actual_operand_value_str[0] == '$') { ... }
            else {
                 cerr << "Error (Line " << lineNumber << "): Invalid or unsupported operand for LDAA: " << actual_operand_value_str << endl;
                 cout << processedLineForRegex << " -> ERROR (LDAA Operand)" << endl;
                 return;
            }
        } else if (instruction_mnemonic == "STAA") { 
            if (!actual_operand_value_str.empty() && actual_operand_value_str[0] == '$') { 
                string addr_str = actual_operand_value_str.substr(1); 
                vector<uint8_t> temp_addr_bytes = hex_string_to_bytes(addr_str);

                if (temp_addr_bytes.size() == 1) { // 00-FF arası
                    determined_mode = AddressingMode::DIRECT;
                    operand_bytes_vec = temp_addr_bytes; 
                } else if (temp_addr_bytes.size() == 2) { // 0000-FFFF arası
                    determined_mode = AddressingMode::EXTENDED;
                    operand_bytes_vec = temp_addr_bytes; 
                } else if (!addr_str.empty()){ // addr_str boş değil ama byte'a çevrilemedi veya boyutu yanlış
                    cerr << "Error (Line " << lineNumber << "): Address '"+actual_operand_value_str+"' invalid size or format for STAA." << endl;
                } else {
                     cerr << "Error (Line " << lineNumber << "): Address missing for STAA." << endl;
                }
            }
             // Diğer STAA modları (INDEXED) buraya eklenebilir.
            else {
                 cerr << "Error (Line " << lineNumber << "): Invalid or unsupported operand for STAA: " << actual_operand_value_str << endl;
                 cout << processedLineForRegex << " -> ERROR (STAA Operand)" << endl;
                 return;
            }
        } else if (instruction_mnemonic == "NOP") {
            determined_mode = AddressingMode::IMPLIED;
        } else if (instruction_mnemonic == "JMP") { 
            determined_mode = AddressingMode::EXTENDED; 
            auto symbol_addr = symbolTable.get_symbol(actual_operand_value_str);
            if (symbol_addr.has_value()) {
                stringstream ss_label_hex;
                ss_label_hex << hex << setw(4) << setfill('0') << uppercase << symbol_addr.value(); 
                operand_bytes_vec = hex_string_to_bytes(ss_label_hex.str()); 
            } else {
                // Eğer doğrudan adres verilmişse (JMP $ADDR)
                if (actual_operand_value_str[0] == '$') {
                    string addr_str = actual_operand_value_str.substr(1);
                    operand_bytes_vec = hex_string_to_bytes(addr_str);
                    if (operand_bytes_vec.size() != 2) { // JMP EXTENDED 2 byte adres alır
                        cerr << "Error (Line " << lineNumber << "): JMP address '"+actual_operand_value_str+"' must be 2 bytes for EXTENDED mode." << endl;
                        operand_bytes_vec.clear(); // Hatalıysa temizle
                        operand_bytes_vec.push_back(0xEE); operand_bytes_vec.push_back(0xEE); // Hata göstergesi
                    }
                } else {
                    cerr << "Error (Line " << lineNumber << "): Undefined symbol '" << actual_operand_value_str << "' for JMP" << endl;
                    operand_bytes_vec.push_back(0xEE); 
                    operand_bytes_vec.push_back(0xEE);
                }
            }
        } else {
            // Demo için diğer komutlar işlenmiyor, hata basılabilir veya XX ile geçilebilir.
            // cerr << "Warning (Line " << lineNumber << "): Addressing mode for '" << instruction_mnemonic << "' not implemented for demo." << endl;
            auto variants = instructionSet.get_instruction(instruction_mnemonic);
            if(!variants.empty()){
                 determined_mode = variants[0].addressing_mode; // İlk bulduğunu al
            } else { // Bu durum olmamalı, is_instruction kontrolü var.
                 cout << processedLineForRegex << " -> ERROR (Unknown instruction variant)" << endl;
                 return;
            }
        }

        optional<Instruction> ins_data_opt = instructionSet.get_instruction_wrt_address_mode(instruction_mnemonic, determined_mode);
        
        if (!ins_data_opt.has_value())
        {
            // Belirlenen modla komut bulunamadıysa, bu ciddi bir sorun.
            // instructions.txt veya adresleme modu belirleme mantığı hatalı olabilir.
            cerr << "FATAL Error (Line " << lineNumber << "): No instruction found for '" << instruction_mnemonic 
                 << "' with mode " << static_cast<int>(determined_mode) 
                 << ". Check instructions.txt and AddressingMode enum/logic." << endl;
            cout << processedLineForRegex << " -> ERROR (Opcode/Mode Mismatch)" << endl;
            // LC'yi artıralım ki sonraki satırlar kaymasın, ama bu satır hatalı.
            // Gerçek bir assembler burada durabilir veya hata sayısını artırabilir.
            // Demo için, LC'yi komutun olası byte sayısına göre artıralım (varsayılan 1 byte)
            auto any_variant = instructionSet.get_instruction(instruction_mnemonic);
            if (!any_variant.empty()) LC += any_variant[0].no_of_bytes; else LC +=1;
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
            // Fazla byte üretilmişse bu bir hatadır. Sadece beklenen kadarını basalım.
            // Bu durumun olmaması gerekir.
            // cerr << "Warning (Line " << lineNumber << "): Too many operand bytes generated for " << instruction_mnemonic << endl;
            // Sadece beklenen kadarını bas, fazlasını at.
            for (int i = 0; i < expected_operand_bytes; ++i) {
                 // Bu döngü zaten yukarıda vardı, burası gereksiz.
                 // Eğer operand_bytes_vec.size() > expected_operand_bytes ise, yukarıdaki döngü zaten doğru sayıda basar.
                 // Aslında, hex_string_to_bytes'ın doğru sayıda byte döndürmesi lazım.
            }
        }
        
        cout << endl;
        
        LC += instructionData.no_of_bytes;
    } else { // Regex eşleşmediyse
        if (!processedLineForRegex.empty()) { // Sadece boş olmayan ve eşleşmeyen satırlar için hata bas
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
