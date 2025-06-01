#include "emulator.hpp"
#include <iostream> 
#include <iomanip>  
#include <string>   

// Global CPU durumu ve Bellek Tanımlamaları
CPUState cpu;
std::vector<uint8_t> memory(65536, 0x00); // 64KB bellek, başlangıçta 0 ile dolu

// --- Diğer Fonksiyonlarınız (initialize_emulator, reset_cpu_state, vb.) ---
// Bu fonksiyonların doğru olduğunu varsayıyoruz ve değiştirmiyoruz.
// Lütfen bu fonksiyonların tam ve doğru hallerinin dosyanızda olduğundan emin olun.

void initialize_emulator() {
    for (size_t i = 0; i < memory.size(); ++i) {
        memory[i] = 0x00; 
    }
    reset_cpu_state();
}

void reset_cpu_state() {
    cpu.pc = 0x0000; 
    cpu.sp = 0x01FF; 
    cpu.ix = 0x0000;
    cpu.accA = 0x00;
    cpu.accB = 0x00;
    cpu.ccr = 0xC0; // Bit 7 ve 6 her zaman 1 (0xC0)
    cpu.set_I_flag(true); 
    cpu.set_Z_flag(true); // Genellikle başlangıçta Zero flag set edilir
}

void load_program_to_memory(const std::vector<uint8_t>& program_bytes, uint16_t start_address) {
    if (program_bytes.empty()) {
        std::cout << "Yüklenecek program byte'ı yok." << std::endl;
        cpu.pc = start_address;
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
        return 0xFF; 
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

uint8_t fetch_byte_and_increment_pc() {
    if (cpu.pc >= memory.size()) {
        std::cerr << "Hata: Bellek sonundan okuma girişimi (fetch_byte)." << std::endl;
        return 0xFF; 
    }
    uint8_t byte = read_memory_byte(cpu.pc);
    cpu.pc++;
    return byte;
}

uint16_t fetch_word_and_increment_pc() {
    uint8_t high_byte = fetch_byte_and_increment_pc();
    uint8_t low_byte = fetch_byte_and_increment_pc();
    return (static_cast<uint16_t>(high_byte) << 8) | low_byte;
}

// Tek bir komut çalıştırır
void execute_single_step(InstructionSet& inst_set) { // inst_set parametresi şimdilik kullanılmıyor
    if (cpu.pc >= memory.size()) {
        std::cout << "Program sonu veya geçersiz PC ($" << std::hex << cpu.pc << std::dec << ")." << std::endl;
        return;
    }

    uint16_t initial_pc_for_debug = cpu.pc; 
    uint8_t opcode = fetch_byte_and_increment_pc(); 

    std::cout << "Executing Opcode: $" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(opcode)
              << " at PC: $" << std::setw(4) << initial_pc_for_debug << std::dec << std::endl;

    switch (opcode) {
        case 0x01: // NOP (Implied)
            std::cout << "  NOP executed." << std::endl;
            break;

        case 0x86: // LDAA Immediate
            {
                uint8_t value = fetch_byte_and_increment_pc(); 
                cpu.accA = value;
                update_N_flag(cpu.accA);
                update_Z_flag(cpu.accA);
                cpu.set_V_flag(false); 
                std::cout << "  LDAA #$" << std::hex << static_cast<int>(value) << " executed. A = $" << static_cast<int>(cpu.accA) << std::dec << std::endl;
            }
            break;

        case 0x97: // STAA Direct
            {
                uint8_t direct_address_low_byte = fetch_byte_and_increment_pc(); 
                uint16_t effective_address = 0x0000 | direct_address_low_byte; 
                
                write_memory_byte(effective_address, cpu.accA); 
                
                update_N_flag(cpu.accA); 
                update_Z_flag(cpu.accA);
                cpu.set_V_flag(false);   
                std::cout << "  STAA $" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(direct_address_low_byte) 
                          << " executed. Memory[$" << std::setw(4) << effective_address << "] = $" << static_cast<int>(cpu.accA) << std::dec << std::endl;
            }
            break;

        case 0xB7: // STAA Extended
            {
                uint16_t effective_address = fetch_word_and_increment_pc(); 
                
                write_memory_byte(effective_address, cpu.accA); 
                
                update_N_flag(cpu.accA);
                update_Z_flag(cpu.accA);
                cpu.set_V_flag(false);
                std::cout << "  STAA $" << std::hex << std::setw(4) << std::setfill('0') << effective_address 
                          << " executed. Memory[$" << effective_address << "] = $" << static_cast<int>(cpu.accA) << std::dec << std::endl;
            }
            break;

        case 0x7E: // JMP Extended
            {
                uint16_t jump_address = fetch_word_and_increment_pc(); 
                cpu.pc = jump_address; 
                std::cout << "  JMP $" << std::hex << std::setw(4) << std::setfill('0') << jump_address << " executed. New PC = $" << cpu.pc << std::dec << std::endl;
            }
            break;
        
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

        // ***** YENİ EKLENEN INCA KOMUTU *****
        case 0x4C: // INCA (Increment Accumulator A - Implied)
            {
                uint8_t original_A_value = cpu.accA; // V bayrağı için orijinal değeri sakla
                
                cpu.accA++; // Akümülatör A'yı bir artır
                
                // Bayrakları Güncelle:
                update_N_flag(cpu.accA); // N: Sonucun 7. biti 1 ise N=1
                update_Z_flag(cpu.accA); // Z: Sonuç 0 ise Z=1
                
                // V (Overflow): Sadece $7F'den $80'e geçişte V=1 olur.
                if (original_A_value == 0x7F && cpu.accA == 0x80) {
                    cpu.set_V_flag(true);
                } else {
                    cpu.set_V_flag(false);
                }
                // C (Carry) Bayrağı: INCA komutu C bayrağını etkilemez.
                // H (Half Carry) Bayrağı: M6800'de INCA H bayrağını etkilemez.

                std::cout << "  INCA executed. A = $" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.accA) 
                          << " (V=" << cpu.get_V_flag() << ")" << std::dec << std::endl;
            }
            break;
        
        // emulator.cpp içinde, execute_single_step fonksiyonundaki switch bloğu

        // ... (diğer case'leriniz) ...

        case 0x3F: // SWI (Software Interrupt - Implied)
            {
                std::cout << "  SWI executed. Program halted by software interrupt." << std::endl;
                // Simülasyonu durdurmak için bir bayrak set edilebilir (CPUState içinde bool halted;)
                // cpu.halted = true; 
                // Python tarafı bu bayrağı kontrol ederek "Adım At" butonunu pasif hale getirebilir.
                // Gerçek SWI, tüm yazmaçları yığına kaydeder ve $FFFA/$FFFB'ye dallanır.
                // Demo için, programın durduğunu belirtmek ve belki PC'yi sabit bir değere
                // (örn. mevcut PC) ayarlamak yeterli olabilir ki daha fazla ilerlemesin.
                // Veya Python tarafı, bu opcode'dan sonra "Adım At" butonunu pasifleştirebilir.
                // Şimdilik, sadece mesaj basalım. PC zaten bir sonraki adrese ilerlemişti.
                // Daha iyi bir çözüm, PC'yi bu komutta tutmak veya özel bir "halted" durumuna geçmek.
                // Örneğin, PC'yi bir azaltarak JMP kendi üzerine gibi davranmasını sağlayabiliriz:
                // cpu.pc--; // SWI tek byte'lık olduğu için, PC zaten bir sonraki adreste. 
                            // Sonsuz döngüye sokmak için: cpu.pc = initial_pc_for_debug; 
                            // Veya daha iyisi, Python tarafında SWI sonrası step'i engellemek.
            }
            break;

        default:
            std::cerr << "Hata: Bilinmeyen veya henüz implemente edilmemiş Opcode: $" << std::hex << static_cast<int>(opcode) 
                      << " at PC: $" << std::setw(4) << (initial_pc_for_debug) << std::dec << std::endl;
            break;
    }
}
