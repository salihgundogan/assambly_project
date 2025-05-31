#include "emulator.hpp"       // CPUState, initialize_emulator, execute_single_step vb.
#include "main.hpp"           // InstructionSet ve belki assembler fonksiyonları için
#include "set_initializer.hpp" // set_initializer için

// InstructionSet için global bir örnek (veya initialize_engine içinde oluşturulup yönetilebilir)
InstructionSet global_instruction_set;
bool engine_initialized = false;

// DLL'den dışa aktarılacak fonksiyonları extern "C" ile sarmala
extern "C" {

    // Motoru ve komut setini başlat
    __declspec(dllexport) void initialize_engine_dll() { // Fonksiyon adını değiştirdim çakışmasın diye
        if (!engine_initialized) {
            set_initializer(global_instruction_set); // Komut setini yükle
            initialize_emulator();                   // Emülatörü başlat
            engine_initialized = true;
            std::cout << "Engine DLL initialized." << std::endl;
        }
    }

    // CPU durumunu resetle
    __declspec(dllexport) void reset_cpu_dll() {
        if (!engine_initialized) initialize_engine_dll();
        reset_cpu_state();
    }

    // Verilen byte dizisini ve başlangıç adresini alarak programı belleğe yükle
    __declspec(dllexport) void load_program_dll(const uint8_t* program_bytes, int length, uint16_t start_address) {
        if (!engine_initialized) initialize_engine_dll();
        if (program_bytes == nullptr || length <= 0) return;
        std::vector<uint8_t> code_vector(program_bytes, program_bytes + length);
        load_program_to_memory(code_vector, start_address);
    }

    // Tek bir CPU adımı çalıştır
    __declspec(dllexport) void step_cpu_dll() {
        if (!engine_initialized) initialize_engine_dll();
        execute_single_step(global_instruction_set);
    }

    // Mevcut CPU durumunu (yazmaçlar, bayraklar) döndür
    __declspec(dllexport) CPUState get_cpu_state_dll() {
        // initialize_engine_dll(); // Her çağrıda gerekmeyebilir, CPUState zaten global cpu'yu kopyalar.
        return cpu; // Global cpu nesnesini döndür
    }

    // Bellekten belirli bir adresteki byte'ı oku
    __declspec(dllexport) uint8_t read_memory_dll(uint16_t address) {
        // initialize_engine_dll();
        return read_memory_byte(address);
    }

    // Belleğe belirli bir adrese byte yaz
    __declspec(dllexport) void write_memory_dll(uint16_t address, uint8_t value) {
        // initialize_engine_dll();
        write_memory_byte(address, value);
    }

    // TODO: Assembler fonksiyonu için bir sarmalayıcı
    // Bu fonksiyon assembly string'ini alıp, makine kodu byte dizisini ve ORG adresini döndürmeli.
    // __declspec(dllexport) bool assemble_string_dll(const char* assembly_string, uint8_t** out_bytes, int* out_length, uint16_t* out_org_address) {
    //     // Mevcut main.cpp'deki parse mantığınızı buraya taşıyıp, sonucu out parametreleriyle döndürün.
    //     // Bu kısım mevcut assembler yapınızın büyük ölçüde yeniden düzenlenmesini gerektirir.
    //     return false; // Şimdilik implemente edilmedi
    // }
}