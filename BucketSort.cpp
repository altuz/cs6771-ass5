#include "BucketSort.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <stack>
#include <thread>
#include <iostream>


bool aLessB(const unsigned int& x, const unsigned int& y, unsigned int pow) {
        if (x == y) return false; // if the two numbers are the same then one is not less than the other
        unsigned int a = x;
        unsigned int b = y;
        // work out the digit we are currently comparing on.
        if (pow == 0) {
            while (a / 10 > 0) {
                a = a / 10;
            }
            while (b / 10 > 0) {
                b = b / 10;
            }
        } else {
        	while (a / 10 >= (unsigned int) std::round(std::pow(10,pow))) {
                a = a / 10;
            }
            while (b / 10 >= (unsigned int) std::round(std::pow(10,pow))) {
                b = b / 10;
            }
        }
        if (a == b)
            return aLessB(x,y,pow + 1);  // recurse if this digit is the same
        else
            return a < b;

}


// TODO: replace this with a parallel version.

// void BucketSort::sort(unsigned int numCores) {
//     std::sort(numbersToSort.begin(),numbersToSort.end(), [](const unsigned int& x, const unsigned int& y){
//         return aLessB(x,y,0);
//     } );
// }
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
    // might change to stack
    std::stack<Bucket> bucket_queue;
    bucket_queue.emplace(this->numbersToSort);
    // one mutex for each sub bucket, an invariant no matter what bucket we are inspecting
    std::vector<unsigned int> sorted;
    std::mutex smutex;
    std::array<std::mutex, 10> mutexes;
    std::mutex cmutex;

    while (!bucket_queue.empty()) {
        // get top of bucket
        Bucket &front_bucket = bucket_queue.top();
        // early exit condition
        if (front_bucket.numbers.size() == 1) {
            sorted.push_back(front_bucket.numbers[0]);
            bucket_queue.pop();
            continue;
        }
        // get the next power of ten, to know which digit to inspect
        unsigned int curr_sf = front_bucket.sig_fig;
        unsigned long pow_of_10 = (unsigned long) std::round(std::pow(10, curr_sf + 1));
        // vector of threads and their respective sub buckets
        std::vector<std::thread> thread_list;
        std::vector<Bucket> sub_buckets;
        // There are ten sub buckets, 1 -> 9 and None.
        for (unsigned int i = 0; i < 10; ++i) {
            sub_buckets.emplace_back(curr_sf + 1);
        }
        std::vector<unsigned int> &numbers = front_bucket.numbers;
        for (unsigned int i = 1; i < numCores && i < numbers.size(); ++i) {
            thread_list.emplace_back(
                [i, &front_bucket, numCores, pow_of_10, &mutexes, &sub_buckets, &sorted, &smutex, &curr_sf, &numbers] {
                    unsigned int my_idx = i;
                    for (unsigned int j = my_idx; j < numbers.size(); j += numCores) {
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
                }
            );
        }

        for (unsigned int j = 0; j < numbers.size(); j += numCores) {
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

        // Join all threads
        for (unsigned int i = 0; i < thread_list.size(); ++i) {
            thread_list[i].join();
        }
        bucket_queue.pop();
        for (int i = 9; i >= 0; --i) {
            Bucket &sub_bucket = sub_buckets[i];
            if (sub_bucket.numbers.size() == 0) {
                continue;
            }
            bucket_queue.emplace(std::move(sub_bucket));
        }
    }
    this->numbersToSort = std::move(sorted);
}
