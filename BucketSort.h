#ifndef BUCKET_SORT_H

#define BUCKET_SORT_H


#include <vector>
#include <mutex>
#include <iostream>
struct BucketSort {
    // vector of numbers
    std::vector<unsigned int> numbersToSort;
    void sort(unsigned int numCores);
};

struct Bucket {
    std::vector<unsigned int> numbers;
    // significant figure of this bucket
    unsigned int sig_fig;
    Bucket(std::vector<unsigned int> nums, unsigned int sf = 0) : numbers(nums), sig_fig(sf) {};
    Bucket(unsigned int sf) : numbers(), sig_fig(sf) {
        //std::cout << "NEVER HERE\n";
    }
    Bucket(Bucket &&other) : numbers(std::move(other.numbers)), sig_fig(other.sig_fig)  {
        //std::cout << "MOVING\n";
        // other.numbers.clear();
        // other.sig_fig = 0;
    }

    ~Bucket() = default;
};
#endif /* BUCKET_SORT_H */
