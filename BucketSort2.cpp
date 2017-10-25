#include "BucketSort.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <stack>
#include <thread>
#include <iostream>
#include <condition_variable>

class semaphore
{
private:
    std::mutex mutex_;
    std::condition_variable condition_;
    unsigned long count_; // Initialized as locked.

public:
    semaphore(unsigned long c) : count_(c) {};
    void notify() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        ++count_;
        condition_.notify_one();
    }

    void wait() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while(!count_) {
            // Handle spurious wake-ups. 
            //std::cout << "waiting\n";
            condition_.wait(lock);
        }
        --count_;
    }

    bool try_wait() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        if(count_) {
            --count_;
            return true;
        }
        return false;
    }
};

unsigned int getDigit(unsigned int value, unsigned int positionFromLeft)
{
    int posFromRight = 1;
    {
        unsigned int v = value;
        while (v /= 10)
            ++posFromRight;
    }
    posFromRight -= positionFromLeft - 1;
    while (--posFromRight)
        value /= 10;
    value %= 10;
    return value > 0 ? value : -value;
}

void BucketSort::sort(unsigned int numCores) {
    numCores = 1;
    // have queue of buckets
    // all threads will work on one bucket at a time
    std::vector<Bucket> bucket_queue;
    unsigned int curr_sf;
    unsigned long pow_of_10;
    bucket_queue.emplace_back(this->numbersToSort);
    Bucket front_bucket;
    // 10 sub buckets, 1 mutex each
    std::vector<Bucket> sub_buckets;
    std::array<std::mutex, 10> mutexes;
    // final sorted list, 1 mutex
    std::vector<unsigned int> sorted;
    std::mutex smutex;
    // semaphores
    semaphore empty(numCores);
    semaphore full(0l);
    while (!bucket_queue.empty()) {
        // get current bucket
        front_bucket = std::move(bucket_queue.back());
        bucket_queue.pop_back();
        // get the next power of ten, to know which digit to inspect
        curr_sf = front_bucket.sig_fig;
        pow_of_10 = (unsigned long) std::round(std::pow(10, curr_sf + 1));
        // There are ten sub buckets, 1 -> 9 and None.
        sub_buckets.clear();
        for (unsigned int i = 0; i < 10; ++i) {
            sub_buckets.emplace_back(curr_sf + 1);
        }
        std::vector<unsigned int> &numbers = front_bucket.numbers;
        for (unsigned int j = 0; j < numbers.size(); ++j) {
            // std::cout << "j = " << j << "\n";
            if ((unsigned long) numbers[j] > pow_of_10) {
                unsigned int nth_digit = getDigit(numbers[j], curr_sf + 1);
                std::lock_guard<std::mutex> lock(mutexes[nth_digit]);
                sub_buckets[nth_digit].numbers.push_back(numbers[j]);
            }
            else {
                std::lock_guard<std::mutex> lock(smutex);
                sorted.push_back(numbers[j]);
            }
        }
        empty.notify();
        empty.wait();
        
        for (int i = 9; i >= 0; --i) {
            Bucket &sub_bucket = sub_buckets[i];
            if (sub_bucket.numbers.size() == 0) {
                continue;
            }
            bucket_queue.emplace_back(std::move(sub_bucket));
        }
    }
    for (auto i : sorted) {
        std::cout << i << "\n";
    }
    this->numbersToSort = std::move(sorted);
}
