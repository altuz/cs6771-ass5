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
    unsigned int count_; // Initialized as locked.

public:
    semaphore(unsigned int c = 0) : count_(c) {};
    semaphore(semaphore &&other) : count_(other.count_) {};
    void notify() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        ++count_;
        condition_.notify_one();
    }

    void wait() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while(!count_) {
            // Handle spurious wake-ups.
            condition_.wait(lock);
        }
        --count_;
    }

    void wait2(unsigned int c) {
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

    void notify3(unsigned int c) {
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

    void fill(unsigned int c) {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        count_ = c;
        condition_.notify_all();
    }

    void wait_empty() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while (count_) {
            // std::cout << count_ << " waiting to be empty\n";
            condition_.wait(lock);
        }
    }

    void take_items(unsigned int c) {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while(!count_) {
            // Handle spurious wake-ups.
            // std::cout <<"FUCK THIS SHIT\n";
            condition_.wait(lock);
        }
        // std::cout << "attempting to take " << c << ", while count_ = " << count_ << "\n";
        count_ -= c;
        if(!count_) {
            // std::cout << "bucket is empty, notify all\n";
            // std::cout << "waking producer\n";
            condition_.notify_all();
        }
    }

    void wake_all() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        // std::cout << "WAKING ALL\n";
        count_ = 0;
        condition_.notify_all();
    }

    void try_take() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while(!count_) {
            // Handle spurious wake-ups.
            condition_.wait(lock);
        }
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

Bucket vector_pop(std::vector<Bucket> &queue) {
    auto res = std::move(queue.back());
    queue.pop_back();
    return res;
}

void BucketSort::sort(unsigned int numCores) {
    numCores = 8;
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
    semaphore bucket_status(0);
    semaphore thread_status(0);
    bool finished = false;

    //
    std::vector<std::thread> thread_list;

    for (unsigned int i = 0; i < 10; ++i) {
        sub_buckets.emplace_back(0);
    }
    // for (unsigned int i = 0; i < numCores - 1; ++i) {
    //     thread_list.emplace_back([&front_bucket, &pow_of_10, &curr_sf, &mutexes, &sub_buckets, &smutex, &sorted, &empty, &full, &bucket_queue, &finished, i, &numCores, &idx_count, &cmutex]
    //     () {
    //         while (true) {
    //             full.wait(i);
    //             if (finished)
    //                 break;
    //             std::vector<unsigned int> &numbers = front_bucket.numbers;
    //             while (true) {
    //                 unsigned int j = 0;
    //                 {
    //                     std::lock_guard<std::mutex> lock(cmutex);
    //                     j = idx_count++;
    //                 }
    //                 if (j >= numbers.size())
    //                     break;
    //                 if ((unsigned long) numbers[j] > pow_of_10) {
    //                     unsigned int nth_digit = getDigit(numbers[j], curr_sf + 1);
    //                     std::lock_guard<std::mutex> lock(mutexes[nth_digit]);
    //                     sub_buckets[nth_digit].numbers.push_back(numbers[j]);
    //                 }
    //                 else {
    //                     std::lock_guard<std::mutex> lock(smutex);
    //                     sorted.push_back(numbers[j]);
    //                 }
    //             }
    //             empty.notify2();
    //         }
    //     });
    // }
    std::vector<semaphore> semaphores;
    for (unsigned int i = 0; i < numCores-1; ++i) {
        semaphores.push_back(0);
    }
    for (unsigned int i = 0; i < numCores-1; ++i) {
        thread_list.emplace_back(
            [&bucket_status, &front_bucket, i, numCores, &sorted, &smutex, &mutexes, &pow_of_10, &curr_sf, &sub_buckets, &finished, &semaphores]{
                while (true) {
                    semaphores[i].wait();
                    if (finished)
                        break;
                    std::vector<unsigned int> &numbers = front_bucket.numbers;
                    unsigned int local_count = 0;
                    for (unsigned int j = i; j < numbers.size(); j += numCores) {
                        ++local_count;
                        // std::cout << "comparing " << numbers[j] << " with " << pow_of_10 << "\n";
                        if ((unsigned long long) numbers[j] > pow_of_10) {
                            unsigned int nth_digit = getDigit(numbers[j], curr_sf + 1);
                            std::lock_guard<std::mutex> lock(mutexes[nth_digit]);
                            sub_buckets[nth_digit].numbers.push_back(numbers[j]);
                        }
                        else {
                            std::lock_guard<std::mutex> lock(smutex);
                            sorted.push_back(numbers[j]);
                        }
                    }
                    // block
                    bucket_status.take_items(local_count);
                }
            }
        );
    }

    while (!bucket_queue.empty()) {
        // get current bucket
        front_bucket = std::move(vector_pop(bucket_queue));
        // get the next power of ten, to know which digit to inspect
        curr_sf = front_bucket.sig_fig;
        pow_of_10 = (unsigned long long) std::round(std::pow(10, curr_sf + 1));
        bucket_status.fill(front_bucket.numbers.size());
        for (unsigned int i = 0; i < semaphores.size() && i < front_bucket.numbers.size(); ++i) {
            semaphores[i].notify();
        }
        std::vector<unsigned int> &numbers = front_bucket.numbers;
        unsigned int local_count = 0;
        for (unsigned int j = numCores - 1; j < numbers.size(); j += numCores) {
            ++local_count;
            // std::cout << "comparing " << numbers[j] << " with " << pow_of_10 << "\n";
            if ((unsigned long long) numbers[j] > pow_of_10) {
                unsigned int nth_digit = getDigit(numbers[j], curr_sf + 1);
                std::lock_guard<std::mutex> lock(mutexes[nth_digit]);
                sub_buckets[nth_digit].numbers.push_back(numbers[j]);
            }
            else {
                std::lock_guard<std::mutex> lock(smutex);
                sorted.push_back(numbers[j]);
            }
        }
        // block
        if (local_count != 0) {
            bucket_status.take_items(local_count);
        }
        // for (auto &s : semaphores) {
        //     s.notify();
        // }
        bucket_status.wait_empty();
        // There are ten sub buckets, 1 -> 9 and None.
        // std::vector<unsigned int> &numbers = front_bucket.numbers;
        for (int j = 9; j >= 0; --j) {
            Bucket &sub_bucket = sub_buckets[j];
            if (sub_bucket.numbers.size() == 0) {
                continue;
            }
            sub_bucket.sig_fig = curr_sf + 1;
            bucket_queue.emplace_back(std::move(sub_bucket));
        }
    }
    finished = true;
    for (unsigned int i = 0; i < semaphores.size(); ++i) {
        semaphores[i].notify();
        thread_list[i].join();
    }
    this->numbersToSort = std::move(sorted);
}
