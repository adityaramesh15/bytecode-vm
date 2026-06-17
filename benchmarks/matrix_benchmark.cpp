#include <vector>
#include <iostream>
#include <chrono>
#include <numeric>

constexpr size_t ROWS = 4000;
constexpr size_t COLS = 4000;

// To defeat compiler optimization optimization passes that might optimize out
// our accumulation loops, we force the compiler to keep our sum alive.
void escape_optimization(long long sum) {
    [[maybe_unused]] volatile long long dummy = sum;
}


void run_row_major_benchmark(const std::vector<int>& matrix) {
    std::cout << "[*] Commencing Row-Major Traversal...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    long long sum = 0;

    for (size_t i = 0; i < ROWS; ++i) {
        for (size_t j = 0; j < COLS; ++j) {
            sum += matrix[i * COLS + j];
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    
    escape_optimization(sum);
    std::cout << "    -> Row-Major Completed in: " << duration.count() << " ms\n";
}

void run_column_major_benchmark(const std::vector<int>& matrix) {
    std::cout << "[*] Commencing Column-Major Traversal...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    long long sum = 0;

    for (size_t j = 0; j < COLS; ++j) {
        for (size_t i = 0; i < ROWS; ++i) {
            sum += matrix[i * COLS + j];
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    
    escape_optimization(sum);
    std::cout << "    -> Column-Major Completed in: " << duration.count() << " ms\n";
}


int main() {
    std::cout << "[+] Allocating " << (ROWS * COLS * sizeof(int)) / (1024 * 1024) 
              << " MB flat matrix block contiguously in memory...\n";
              
    std::vector<int> matrix(ROWS * COLS);
    std::iota(matrix.begin(), matrix.end(), 1); // Fill with dummy incremental data

    // Run tests
    run_row_major_benchmark(matrix);
    run_column_major_benchmark(matrix);

    return 0;
}