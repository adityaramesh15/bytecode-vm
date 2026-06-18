#include <new>
#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <functional>

constexpr uint64_t ITERATIONS = 100000000; 
constexpr size_t NUM_THREADS = 4;

/*
    8 bytes per value, so when I create UnalignedCounter values[4]
    there'll be 32 bytes of data packed together w/o spacing between
    modifying co-located vars will cause cache thrashing due to false sharing
*/
struct UnalignedCounter {
    uint64_t value{0};
};

struct alignas(std::hardware_destructive_interference_size) AlignedCounter {
    uint64_t value{0};
};

void increment_worker(uint64_t& counter, uint64_t iterations) {
    for (uint64_t i = 0; i < iterations; ++i) {
        counter++;
    }
}

void run_unaligned_benchmark() {
    UnalignedCounter unaligned_values[NUM_THREADS];
    std::vector<std::jthread> workers;
    workers.reserve(NUM_THREADS);

    std::cout << "[*] Commencing Unaligned (False Sharing) Multi-Threaded Test...\n";
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back(increment_worker, std::ref(unaligned_values[i].value), ITERATIONS);
    }
    workers.clear();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    uint64_t escape_sum = 0;
    for (size_t i = 0; i < NUM_THREADS; ++i) {
        escape_sum += unaligned_values[i].value;
    }
    [[maybe_unused]] volatile uint64_t dummy = escape_sum;
    std::cout << "    -> Unaligned Completed in: " << duration.count() << " ms\n\n";
}


void run_aligned_benchmark() {
    AlignedCounter aligned_values[NUM_THREADS];
    std::vector<std::jthread> workers;
    workers.reserve(NUM_THREADS);

    std::cout << "[*] Commencing Cache-Aligned (Isolated Line) Multi-Threaded Test...\n";
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back(increment_worker, std::ref(aligned_values[i].value), ITERATIONS);
    }

    workers.clear(); 

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    uint64_t escape_sum = 0;
    for (size_t i = 0; i < NUM_THREADS; ++i) {
        escape_sum += aligned_values[i].value;
    }
    
    [[maybe_unused]] volatile uint64_t dummy = escape_sum;
    std::cout << "    -> Aligned Completed in: " << duration.count() << " ms\n\n";
}

int main() {
    std::cout << "[i] Size of Unaligned Element Array: " << sizeof(UnalignedCounter) * NUM_THREADS << " bytes\n";
    std::cout << "[i] Size of Aligned Element Array:   " << sizeof(AlignedCounter) * NUM_THREADS << " bytes\n\n";

    run_unaligned_benchmark();
    run_aligned_benchmark();

    return 0;
}