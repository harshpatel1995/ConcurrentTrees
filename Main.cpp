
#include <cstdlib>
#include <thread>
#include <cassert>
#include <cmath>
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>
#include <vector>

#include "BST.h"
#include "ConcurrentBST.h"

enum FNS
{
    ADD,
    REMOVE,
    CONTAINS
};

double getUnitRandom()
{
    static std::mt19937* generator = nullptr;
    if (!generator)
        generator = new std::mt19937(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(*generator);
}

int main(int argc, char **argv)
{
    int num_max_threads = 32;
    const int num_iterations = 65536; // power of 2 (seems to stack overflow if this gets any higher)
    const int num_runs = 10;
    float insert_percent = 1.0f / 3.0f;
    float remove_percent = 1.0f / 3.0f;
    float contains_percent = 1.0f / 3.0f;
    const int number_range = 100;

    if (argc > 1)
    {
        // usage example: ./bst.exe 33 33 33
        insert_percent = (std::atoi(argv[1]) / 100.0f);
        remove_percent = (std::atoi(argv[2]) / 100.0f); // beware!
        contains_percent = (std::atoi(argv[3]) / 100.0f);
        assert(std::abs(1.f - (insert_percent + remove_percent + contains_percent)) <= 0.01);
    }

    // precompute various queries to random as to not pollute timing results.
    alignas(64) FNS precomputed_function_ratios[num_iterations]; // compiler aligns sequential memory to fill cache lines to avoid false sharing
    alignas(64) int precomputed_randoms[num_iterations];
    for (int i = 0; i < num_iterations; ++i)
    {
        auto r = getUnitRandom();
        if (r < insert_percent)
            precomputed_function_ratios[i] = FNS::ADD;
        else if (r > (insert_percent + remove_percent))
            precomputed_function_ratios[i] = FNS::CONTAINS;
        else precomputed_function_ratios[i] = FNS::REMOVE;

        precomputed_randoms[i] = (int)(getUnitRandom() * number_range);
    }

    ////////////////// Run single threaded avl tests
    float average_time = 0.f;
    for (int r = 0; r < num_runs; ++r)
    {
        AVLTree<int> avl;
        auto start_time = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_iterations; ++i)
        {
            auto r = precomputed_randoms[i];
            switch (precomputed_function_ratios[i])
            {
                case FNS::ADD:      avl.insert(r); break;
                case FNS::REMOVE:   avl.remove(r); break;
                case FNS::CONTAINS: avl.contains(r); break;
            }
        }

        auto curr_time = std::chrono::high_resolution_clock::now();
        auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - start_time).count();
        average_time += delta_time;
    }

    average_time /= num_runs;
    std::cout << average_time << " ";

    ////////////////// Run concurrent avl tests
    for (int t = 2; t <= num_max_threads; t *= 2)
    {
        average_time = 0.f;

        for (int r = 0; r < num_runs; ++r)
        {
            std::vector<std::thread> threads(t);
            ConcurrentAVLTree<int> c_avl;

            auto start_time = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < t; ++i)
            {
                threads.emplace_back(std::thread([](ConcurrentAVLTree<int> &c_avl, int stride, int *randoms, FNS *ratios) {

                    // iterate for a specified stride, using precomputed random numbers to
                    // determine which interface function to call: i.e. insert, remove, contains.
                    for (int j = 0; j < stride; ++j)
                    {
                        switch (*ratios)
                        {
                            case FNS::ADD:      c_avl.insert(*randoms); break;
                            case FNS::REMOVE:   c_avl.remove(*randoms); break;
                            case FNS::CONTAINS: c_avl.contains(*randoms); break;
                        }

                        ratios++; randoms++;
                    }
                }, std::ref(c_avl),
                    std::floor(num_iterations / t),
                    &precomputed_randoms[(num_iterations / t) * i],
                    &precomputed_function_ratios[(num_iterations / t) * i]
                    ));
            }

            for (auto &t : threads)
            {
                if (t.joinable())
                    t.join();
            }

            auto curr_time = std::chrono::high_resolution_clock::now();
            auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - start_time).count();

            average_time += delta_time;
        }

        average_time /= num_runs;
        std::cout << average_time << " ";
    }
}
