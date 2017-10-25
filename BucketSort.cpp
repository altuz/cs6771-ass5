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

    void wait(unsigned int i) {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while(!count_) {
            // Handle spurious wake-ups. 
            condition_.wait(lock);
        }
        --count_;
    }

    void wait2(unsigned long c) {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while(count_) {
            condition_.wait(lock);
        }
        count_ = c;
    }
    
    void notify2() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        --count_;
        if(!count_) {
            condition_.notify_one();
        }
    }
    
    void notify3(unsigned long c) {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        count_ = c;
        condition_.notify_all();
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
    numCores = numCores;
    // have queue of buckets
    // all threads will work on one bucket at a time
    std::vector<Bucket> bucket_queue;
    unsigned int curr_sf = 0;
    unsigned long long pow_of_10 = 0;
    bucket_queue.emplace_back(this->numbersToSort);
    Bucket front_bucket;
    // 10 sub buckets, 1 mutex each
    std::vector<Bucket> sub_buckets;
    std::array<std::mutex, 10> mutexes;
    // final sorted list, 1 mutex
    std::vector<unsigned int> sorted;
    std::mutex smutex;
    // semaphores
    semaphore empty(numCores-1);
    semaphore full(0l);
    // 
    bool finished = false;
    std::vector<std::thread> thread_list;
    unsigned int idx_count = 0;
    std::mutex cmutex;
    for (unsigned int i = 0; i < numCores - 1; ++i) {
        thread_list.emplace_back([&front_bucket, &pow_of_10, &curr_sf, &mutexes, &sub_buckets, &smutex, &sorted, &empty, &full, &bucket_queue, &finished, i, &numCores, &idx_count, &cmutex]
        () {
            while (true) {
                full.wait(i);
                if (finished)
                    break;
                std::vector<unsigned int> &numbers = front_bucket.numbers;
                while (true) {
                    unsigned int j = 0;
                    {
                        std::lock_guard<std::mutex> lock(cmutex);
                        j = idx_count++;
                    }
                    if (j >= numbers.size())
                        break;
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
                empty.notify2();  
            }
        });
    }
    
    
    while (!bucket_queue.empty()) {
        // get current bucket
        front_bucket = std::move(bucket_queue.back());
        bucket_queue.pop_back();
        // get the next power of ten, to know which digit to inspect
        curr_sf = front_bucket.sig_fig;
        pow_of_10 = (unsigned long long) std::round(std::pow(10, curr_sf + 1));
        // There are ten sub buckets, 1 -> 9 and None.
        sub_buckets.clear();
        for (unsigned int j = 0; j < 10; ++j) {
            sub_buckets.emplace_back(curr_sf + 1);
        }
        full.notify3(numCores-1);
        // down empty
        std::vector<unsigned int> &numbers = front_bucket.numbers;
        while (true) {
            unsigned int j = 0;
            {
                std::lock_guard<std::mutex> lock(cmutex);
                j = idx_count++;
            }
            if (j >= numbers.size())
                break;
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
        // up full
        empty.wait2(numCores-1);
        {
            std::lock_guard<std::mutex> lock(cmutex);
            idx_count = 0;
        }
        for (int j = 9; j >= 0; --j) {
            Bucket &sub_bucket = sub_buckets[j];
            if (sub_bucket.numbers.size() == 0) {
                continue;
            }
            bucket_queue.emplace_back(std::move(sub_bucket));
        }
    }
    finished = true;
    full.notify3(numCores-1);
    for (auto &t : thread_list) {
        t.join();
    }
    this->numbersToSort = std::move(sorted);
}
