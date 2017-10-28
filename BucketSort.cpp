#include "BucketSort.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <stack>
#include <thread>
#include <iostream>
#include <random>

// Given an integer, get the nth digit from the left
unsigned int get_digit(unsigned int value, unsigned int left_pos) {
    // calculate the number of digits in value
    // make a copy of value,
    // keep dividing it by 10 until it becomes 0, and count the number of times you divide by 10
    auto digit_length = 1;
    auto v = value;
    while (v /= 10)
        ++digit_length;
    // using the length of value, and the position from left,
    // calculate the position from right
    auto pos_right = digit_length - (left_pos - 1);
    // divide value by 10 until the right most digit is the digit we want to get
    // that value % 10 = the digit
    while (--pos_right)
        value /= 10;
    value %= 10;
    return value > 0 ? value : -value;
}
// Get top of vector.
// WHY DOES POP NOT RETURN TOP AS WELL???
Bucket vector_pop(std::vector<Bucket> &queue) {
    auto res = std::move(queue.back());
    queue.pop_back();
    return res;
}
// Parallel bucket sort!!
void BucketSort::sort(unsigned int numCores) {
    Bucket initial_bucket(this->numbersToSort);
    // categorize initial bucket using current thread.
    std::vector<Bucket> buckets(std::move(initial_bucket.categorize()));
    std::vector<std::thread> thread_list;
    std::vector<std::vector<unsigned int>> sorted_buckets(10);
    // spawn thread and have each thread sort each bucket, no race condition
    for (auto i = 0u; i < numCores - 1; ++i) {
        thread_list.emplace_back(
            [&buckets, &sorted_buckets, i, numCores] {
                // no race condition here, since each thread will take unique indices
                for (auto j = i; j < buckets.size() - 1; j += numCores) {
                    std::vector<unsigned int> sorted_bucket(std::move(buckets[j].sort()));
                    sorted_buckets[j] = std::move(sorted_bucket);
                }
            }
        );
    }
    // give some work to current thread
    for (auto j = numCores - 1; j < buckets.size() - 1; j += numCores) {
        std::vector<unsigned int> sorted_bucket(std::move(buckets[j].sort()));
        sorted_buckets[j] = std::move(sorted_bucket);
    }
    // join thread
    for (auto &t : thread_list) {
        t.join();
    }
    // reassemble results
    // buckets[10] won't contain anything but....
    //      -- I lied, it will contain 0s
    std::vector<unsigned int> sorted = std::move(buckets[10].numbers);
    
    for (auto vectors : sorted_buckets) {
        sorted.insert(sorted.end(), vectors.begin(), vectors.end());
    }
    
    this->numbersToSort = std::move(sorted);
}

// Bucket sort current bucket, iteratively
// Similar logic to categorize, but keeps doing it until there is nothing left to categorize
std::vector<unsigned int> Bucket::sort() {
    std::vector<unsigned int> sorted;
    // initialize queue
    std::vector<Bucket> bucket_queue;
    bucket_queue.emplace_back(this->numbers, this->sig_fig);
    // initialize sub bucket
    std::vector<Bucket> sub_buckets;
    for (auto i = 0u; i < 11; ++i) {
        sub_buckets.emplace_back(0);
    }
    // run until no more bucket in queue
    while (!bucket_queue.empty()) {
        auto front_bucket = vector_pop(bucket_queue);
        // Early exit condition
        // There are more to this, but we risk putting more overhead
        // Basically if all numbers in queue are the same, we could do this.
        if (front_bucket.numbers.size() == 1) {
            sorted.push_back(front_bucket.numbers[0]);
            continue;
        }
        // get sig_fig and 10^sig_fig
        auto curr_sf = front_bucket.sig_fig;
        auto pow_of_10 = (unsigned long long) std::round(std::pow(10, curr_sf));
        // basically what we did in categorize
        for (unsigned int i : front_bucket.numbers) {
            if ((unsigned long long) i > pow_of_10) {
                auto nth_digit = get_digit(i, curr_sf + 1);
                sub_buckets[nth_digit].numbers.push_back(i);
            }
            else {
                sub_buckets[10].numbers.push_back(i);
            }
        }
        // for each of those sub buckets except the 11th, place it back to queue.
        // 11th can be put straight to sorted
        for (int i = sub_buckets.size() - 2; i >= 0; --i) {
            auto &sub_bucket = sub_buckets[i];
            if (sub_bucket.numbers.size() == 0) {
                continue;
            }
            sub_bucket.sig_fig = curr_sf + 1;
            bucket_queue.emplace_back(std::move(sub_bucket));
        }
        // 11th bucket is sorted!
        if (sub_buckets[10].numbers.size() != 0) {
            sorted.insert(sorted.end(), sub_buckets[10].numbers.begin(), sub_buckets[10].numbers.end());
            sub_buckets[10].numbers.clear();
        }
    }
    return sorted;
}

// categorize the bucket into 11 buckets.
// depending on the digit on the sig_fig^th digit.
//      sub buckets include 0-9,
//      the 11th sub bucket includes numbers < 10^(curr_sf + 1)
std::vector<Bucket> Bucket::categorize() {
    std::vector<Bucket> sub_buckets(11);
    auto curr_sf = this->sig_fig;
    auto pow_of_10 = (unsigned long long) std::round(std::pow(10, curr_sf));
    // categorize numbers to buckets
    for (auto i : this->numbers) {
        if ((unsigned long long) i > pow_of_10) {
            auto nth_digit = get_digit(i, curr_sf + 1);
            sub_buckets[nth_digit].numbers.push_back(i);
        }
        else {
            sub_buckets[10].numbers.push_back(i);
        }
    }
    // increment significant figure for new buckets
    for (auto &b : sub_buckets) {
        b.sig_fig = curr_sf + 1;
    }
    return sub_buckets;
}
