#include <pthread.h>
#include <jni.h>
#include <memory.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <dobby.h>
#include <android/log.h>

#define LOG_TAG "PitlaneMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Указатель на оригинальный метод игры
float (*old_CalculateDesiredSpeed)(void* instance) = nullptr;

// Наш модифицированный метод, который режет скорость
float new_CalculateDesiredSpeed(void* instance) {
    if (instance != nullptr) {
        // Из SpeedController: mRacingVehicle находится по смещению 0xC в 32-бит
        void* racingVehicle = *(void**)((uintptr_t)instance + 0xC);
        
        if (racingVehicle != nullptr) {
            // Ищем указатель на PathController внутри машины (смещение 0x2C)
            void* pathController = *(void**)((uintptr_t)racingVehicle + 0x2C);
            
            if (pathController != nullptr) {
                // Пытаемся вызвать IsOnPitlanePath (RVA: 0x6C7E58)
                typedef bool (*fnIsOnPitlanePath)(void* pathCtrl);
                
                uintptr_t il2cpp_base = (uintptr_t)dlopen("libil2cpp.so", RTLD_NOLOAD);
                if (il2cpp_base) {
                    auto IsOnPitlanePath = (fnIsOnPitlanePath)(il2cpp_base + 0x6C7E58);
                    
                    // Если машина на пит-лейне — жестко возвращаем скорость 2.0
                    if (IsOnPitlanePath && IsOnPitlanePath(pathController)) {
                        return 2.0f; 
                    }
                }
            }
        }
    }
    // Если машина на трассе — возвращаем её нормальную скорость
    return old_CalculateDesiredSpeed ? old_CalculateDesiredSpeed(instance) : 0.0f;
}

// Поток, ожидающий загрузки игры
void* hack_thread(void*) {
    uintptr_t il2cpp_base = 0;
    while (!il2cpp_base) {
        il2cpp_base = (uintptr_t)dlopen("libil2cpp.so", RTLD_NOLOAD);
        usleep(100000);
    }
    
    // Подменяем оригинальный метод CalculateDesiredSpeed (RVA: 0x6D7924) через встроенный Dobby хукер
    DobbyHook((void*)(il2cpp_base + 0x6D7924), (void*)new_CalculateDesiredSpeed, (void**)&old_CalculateDesiredSpeed);
    
    return nullptr;
}

// Автоматический старт при загрузке нашей .so библиотеки
void __attribute__((constructor)) init() {
    pthread_t pt;
    pthread_create(&pt, nullptr, hack_thread, nullptr);
}
