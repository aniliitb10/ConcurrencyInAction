#include <thread_pool.h>
#include <algorithm>


class ParallelQuickSort {

public:
    template <typename ForwardIt>
    static void sort(ForwardIt first, ForwardIt last, std::size_t thread_count = 2);

private:
    explicit ParallelQuickSort(size_t thread_count);

    template <typename ForwardIt>
    void sort_impl(ForwardIt first, ForwardIt last);

    // This could have been a local variable inside @sort method, but that would mean
    // passing an additional parameter by ref in the @sort_impl method
    ThreadPool<> _thread_pool;
};

template<typename ForwardIt>
void ParallelQuickSort::sort(ForwardIt first, ForwardIt last, std::size_t thread_count) {
    ParallelQuickSort parallel_quick_sort{thread_count};

    if (first == last) {
        return; // just a sanity check to avoid initializing thread pool unnecessarily
    }

    parallel_quick_sort.sort_impl(first, last);
    auto& queue = parallel_quick_sort._thread_pool.stop_early();

    if (!queue.is_empty()) {
        std::cerr << "There are " << queue.size() << " items left on the queue, executing on main thread!\n";
        do {
            (*(queue.try_pop().first))();
        }
        while (!queue.is_empty());
    }
}

ParallelQuickSort::ParallelQuickSort(size_t thread_count) : _thread_pool{
        thread_count, std::numeric_limits<std::size_t>::max(), 0ms} {}

template<typename ForwardIt>
void ParallelQuickSort::sort_impl(ForwardIt first, ForwardIt last) {
    if (first == last) {
        return;
    }

    auto pivot = *(std::next(first, std::distance(first, last) / 2));
    auto med1 = std::partition(first, last, [pivot](const auto& elem) { return elem < pivot; });
    // intentional to use '<' operator instead of '>=' as '>=' might not be defined for some classes
    auto med2 = std::partition(med1, last, [pivot](const auto& elem) { return !(pivot < elem); });
    sort_impl(first, med1);
    _thread_pool.add_task(&ParallelQuickSort::sort_impl<ForwardIt>, this, med2, last);
}
