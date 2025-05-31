#include "emulator.hpp"
#include <iostream> 
#include <iomanip>  
#include <string>   // std::to_string için (opsiyonel, debug için)

// Global CPU durumu ve Bellek Tanımlamaları
CPUState cpu;
std::vector<uint8_t> memory(65536, 0x00); // 64KB bellek, başlangıçta 0 ile dolu

// --- initialize_emulator, reset_cpu_state, load_program_to_memory ---
// --- read_memory_byte, write_memory_byte, read_memory_word, write_memory_word ---
// --- update_N_flag, update_Z_flag, update_Z_flag_word ---
// Bu fonksiyonlar bir önceki mesajınızdaki gibi doğru görünüyor ve aynı kalabilir.
// Tekrar etmemek için buraya kopyalamıyorum, lütfen mevcut hallerini koruyun.

void initialize_emulator() {
    for (size_t i = 0; i < memory.size(); ++i) {
        memory[i] = 0x00; 
    }
    reset_cpu_state();
}

void reset_cpu_state() {
    cpu.pc = 0x0000; // load_program_to_memory bunu güncelleyecek
    cpu.sp = 0x01FF; 
    cpu.ix = 0x0000;
    cpu.accA = 0x00;
    cpu.accB = 0x00;
    cpu.ccr = 0xC0; 
    cpu.set_I_flag(true); 
    cpu.set_Z_flag(true); 
    // N, V, C, H bayrakları işlem sonrası güncellenir.
}

void load_program_to_memory(const std::vector<uint8_t>& program_bytes, uint16_t start_address) {
    if (program_bytes.empty()) {
        std::cout << "Yüklenecek program byte'ı yok." << std::endl;
        cpu.pc = start_address; // Yine de PC'yi ayarla
        return;
    }
    for (size_t i = 0; i < program_bytes.size(); ++i) {
        if (start_address + i < memory.size()) {
            memory[start_address + i] = program_bytes[i];
        } else {
            std::cerr << "Hata: Program belleğe sığmıyor! Adres: $" << std::hex << (start_address + i) << std::dec << std::endl;
            break;
        }
    }
    cpu.pc = start_address; 
    std::cout << "Program belleğe yüklendi. PC = $" << std::hex << std::setw(4) << std::setfill('0') << cpu.pc << std::dec << std::endl;
}

uint8_t read_memory_byte(uint16_t address) {
    if (address >= memory.size()) {
        std::cerr << "Hata: Geçersiz bellek okuma adresi: $" << std::hex << address << std::dec << std::endl;
        return 0xFF; // Hata durumunda bir değer döndür
    }
    return memory[address];
}

void write_memory_byte(uint16_t address, uint8_t value) {
    if (address >= memory.size()) {
        std::cerr << "Hata: Geçersiz bellek yazma adresi: $" << std::hex << address << std::dec << std::endl;
        return;
    }
    memory[address] = value;
}

uint16_t read_memory_word(uint16_t address) {
    if (address + 1 >= memory.size()) {
        std::cerr << "Hata: Geçersiz word okuma adresi (sınır aşımı): $" << std::hex << address << std::dec << std::endl;
        return 0xFFFF;
    }
    uint8_t high_byte = read_memory_byte(address);
    uint8_t low_byte = read_memory_byte(address + 1);
    return (static_cast<uint16_t>(high_byte) << 8) | low_byte;
}

void write_memory_word(uint16_t address, uint16_t value) {
    if (address + 1 >= memory.size()) {
        std::cerr << "Hata: Geçersiz word yazma adresi (sınır aşımı): $" << std::hex << address << std::dec << std::endl;
        return;
    }
    write_memory_byte(address, static_cast<uint8_t>(value >> 8));
    write_memory_byte(address + 1, static_cast<uint8_t>(value & 0xFF));
}

void update_N_flag(uint8_t result) {
    cpu.set_N_flag((result & 0x80) != 0);
}

void update_Z_flag(uint8_t result) {
    cpu.set_Z_flag(result == 0);
}

void update_Z_flag_word(uint16_t result) { 
    cpu.set_Z_flag(result == 0);
}

// Yardımcı fonksiyon: PC'den bir byte oku ve PC'yi artır
uint8_t fetch_byte_and_increment_pc() {
    if (cpu.pc >= memory.size()) {
        std::cerr << "Hata: Bellek sonundan okuma girişimi (fetch_byte)." << std::endl;
        // Burada bir exception fırlatmak veya bir hata durumu yönetmek daha iyi olabilir.
        // Şimdilik, programın çökmesini önlemek için 0xFF döndürelim ve PC'yi artırmayalım.
        // Ya da PC'yi artırıp 0xFF döndürebiliriz, bu da bir sonraki adımda program sonunu tetikler.
        // En iyisi, execute_single_step başında PC kontrolü yapmak.
        return 0xFF; // Hata veya geçersiz veri
    }
    uint8_t byte = read_memory_byte(cpu.pc);
    cpu.pc++;
    return byte;
}

// Yardımcı fonksiyon: PC'den bir word (2 byte) oku ve PC'yi artır
uint16_t fetch_word_and_increment_pc() {
    uint8_t high_byte = fetch_byte_and_increment_pc();
    uint8_t low_byte = fetch_byte_and_increment_pc();
    return (static_cast<uint16_t>(high_byte) << 8) | low_byte;
}


// Tek bir komut çalıştırır
void execute_single_step(InstructionSet& inst_set) { // inst_set parametresi şimdilik kullanılmıyor ama ileride lazım olabilir
    if (cpu.pc >= memory.size()) {
        std::cout << "Program sonu veya geçersiz PC ($" << std::hex << cpu.pc << std::dec << ")." << std::endl;
        return;
    }

    uint16_t initial_pc_for_debug = cpu.pc; // Hata ayıklama için başlangıç PC'si
    uint8_t opcode = fetch_byte_and_increment_pc(); // Opcode okundu, PC 1 arttı

    std::cout << "Executing Opcode: $" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(opcode)
            << " at PC: $" << std::setw(4) << initial_pc_for_debug << std::dec << std::endl;

    // Komutları opcode'larına göre işle
    switch (opcode) {
        case 0x01: // NOP (Implied)
            // Hiçbir şey yapma. PC zaten opcode için 1 arttı.
            std::cout << "  NOP executed." << std::endl;
            break;

        case 0x86: // LDAA Immediate
            {
                uint8_t value = fetch_byte_and_increment_pc(); // Operandı oku, PC 1 daha arttı
                cpu.accA = value;
                update_N_flag(cpu.accA);
                update_Z_flag(cpu.accA);
                cpu.set_V_flag(false); // LDA V'yi temizler
                std::cout << "  LDAA #$" << std::hex << static_cast<int>(value) << " executed. A = $" << static_cast<int>(cpu.accA) << std::dec << std::endl;
            }
            break;

        case 0x97: // STAA Direct
            {
                uint8_t direct_address_low_byte = fetch_byte_and_increment_pc(); // Adresin düşük byte'ını oku, PC 1 daha arttı
                uint16_t effective_address = 0x0000 | direct_address_low_byte; // Yüksek byte $00 varsayılır
                
                write_memory_byte(effective_address, cpu.accA); // <<--- BELLEĞE YAZMA İŞLEMİ
                
                update_N_flag(cpu.accA); // STAA, saklanan değere göre N ve Z'yi etkiler
                update_Z_flag(cpu.accA);
                cpu.set_V_flag(false);   // STAA V'yi temizler
                std::cout << "  STAA $" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(direct_address_low_byte) 
                        << " executed. Memory[$" << std::setw(4) << effective_address << "] = $" << static_cast<int>(cpu.accA) << std::dec << std::endl;
            }
            break;

        case 0xB7: // STAA Extended
            {
                uint16_t effective_address = fetch_word_and_increment_pc(); // Adresi (2 byte) oku, PC 2 daha arttı
                
                write_memory_byte(effective_address, cpu.accA); // <<--- BELLEĞE YAZMA İŞLEMİ
                
                update_N_flag(cpu.accA);
                update_Z_flag(cpu.accA);
                cpu.set_V_flag(false);
                std::cout << "  STAA $" << std::hex << std::setw(4) << std::setfill('0') << effective_address 
                        << " executed. Memory[$" << effective_address << "] = $" << static_cast<int>(cpu.accA) << std::dec << std::endl;
            }
            break;

        case 0x7E: // JMP Extended
            {
                uint16_t jump_address = fetch_word_and_increment_pc(); // Dallanılacak adresi (2 byte) oku, PC 2 daha arttı
                cpu.pc = jump_address; // PC'yi yeni adrese ayarla
                std::cout << "  JMP $" << std::hex << std::setw(4) << std::setfill('0') << jump_address << " executed. New PC = $" << cpu.pc << std::dec << std::endl;
            }
            break;
        
        // --- Diğer tüm opcodelar için benzer case blokları eklenecek ---
        // Örnek: LDAA Extended (Opcode B6)
        case 0xB6: // LDAA Extended
            {
                uint16_t effective_address = fetch_word_and_increment_pc();
                cpu.accA = read_memory_byte(effective_address);
                update_N_flag(cpu.accA);
                update_Z_flag(cpu.accA);
                cpu.set_V_flag(false);
                std::cout << "  LDAA $" << std::hex << std::setw(4) << effective_address << " executed. A = $" << static_cast<int>(cpu.accA) << std::dec << std::endl;
            }
            break;

        default:
            std::cerr << "Hata: Bilinmeyen veya henüz implemente edilmemiş Opcode: $" << std::hex << static_cast<int>(opcode) 
                    << " at PC: $" << std::setw(4) << (initial_pc_for_debug) << std::dec << std::endl;
            // Hata durumunda PC'yi bir sonraki byte'a ilerletmek yerine,
            // belki de simülasyonu durdurmak veya bir hata bayrağı set etmek daha iyi olabilir.
            // Şimdilik, PC zaten bir sonraki olası komuta ilerlemiş durumda (opcode okunduğu için).
            break;
    }
}
