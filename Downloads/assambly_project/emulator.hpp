#ifndef EMULATOR_HPP
#define EMULATOR_HPP

#include <vector>
#include <cstdint> // uint8_t, uint16_t için
#include <string>
#include "main.hpp" // InstructionSet ve AddressingMode gibi tanımlar için

// CPU Yazmaçları ve Durum Bayrakları
struct CPUState {
    uint16_t pc; // Program Counter
    uint16_t sp; // Stack Pointer
    uint16_t ix; // Index Register
    uint8_t accA; // Accumulator A
    uint8_t accB; // Accumulator B
    
    // Condition Code Register (CCR) Bayrakları
    // H (Bit 5), I (Bit 4), N (Bit 3), Z (Bit 2), V (Bit 1), C (Bit 0)
    // CCR'nin 7. ve 6. bitleri her zaman 1'dir.
    uint8_t ccr; // Durum Kodu Yazmacı (8-bit)

    // CCR bayraklarına erişim için yardımcı fonksiyonlar
    bool get_H_flag() const { return (ccr >> 5) & 1; }
    bool get_I_flag() const { return (ccr >> 4) & 1; }
    bool get_N_flag() const { return (ccr >> 3) & 1; }
    bool get_Z_flag() const { return (ccr >> 2) & 1; }
    bool get_V_flag() const { return (ccr >> 1) & 1; }
    bool get_C_flag() const { return ccr & 1; }

    void set_H_flag(bool val) { if (val) ccr |= (1 << 5); else ccr &= ~(1 << 5); }
    void set_I_flag(bool val) { if (val) ccr |= (1 << 4); else ccr &= ~(1 << 4); }
    void set_N_flag(bool val) { if (val) ccr |= (1 << 3); else ccr &= ~(1 << 3); }
    void set_Z_flag(bool val) { if (val) ccr |= (1 << 2); else ccr &= ~(1 << 2); }
    void set_V_flag(bool val) { if (val) ccr |= (1 << 1); else ccr &= ~(1 << 1); }
    void set_C_flag(bool val) { if (val) ccr |= 1; else ccr &= ~1; }

    CPUState() : pc(0), sp(0), ix(0), accA(0), accB(0), ccr(0xC0) { // CCR'nin 7. ve 6. bitleri her zaman 1 (0xC0)
        set_I_flag(true); // Kesmeler başlangıçta maskeli
        set_Z_flag(true); // Genellikle başlangıçta Zero flag set edilir (sonuç 0 gibi)
    } 
};

// Global CPU durumu ve Bellek (emulator.cpp içinde tanımlanacak)
extern CPUState cpu;
extern std::vector<uint8_t> memory; // 64KB (65536 byte)

// Emülatör Fonksiyon Bildirimleri
void initialize_emulator(); // Emülatörü ve CPU'yu başlangıç durumuna getirir
void reset_cpu_state();     // Sadece CPU yazmaçlarını resetler
void load_program_to_memory(const std::vector<uint8_t>& program_bytes, uint16_t start_address);
uint8_t read_memory_byte(uint16_t address);
void write_memory_byte(uint16_t address, uint8_t value);
uint16_t read_memory_word(uint16_t address);
void write_memory_word(uint16_t address, uint16_t value);
void execute_single_step(InstructionSet& inst_set); // Tek bir komut çalıştırır

// Bayrakları güncellemek için yardımcı fonksiyonlar (emulator.cpp'de tanımlanacak)
void update_N_flag(uint8_t result);
void update_Z_flag(uint8_t result);
void update_Z_flag_word(uint16_t result); // 16-bit işlemler için (CPX gibi)
// V, C, H bayrakları için daha karmaşık güncellemeler gerekecek, komutlara özel olacak.

#endif // EMULATOR_HPP