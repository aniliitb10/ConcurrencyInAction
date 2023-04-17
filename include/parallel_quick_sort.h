#include <thread_pool.h>
#include <algorithm>


class ParallelQuickSort {

public:
    template <typename ForwardIt>
    static void sort(ForwardIt first, ForwardIt last, std::size_t thread_count = 2);

private:
    /**
     * A private constructor: so that only static @sort can call this and not any external user
     * @param thread_count
     */
    explicit ParallelQuickSort(size_t thread_count);

    /**
     * The method which actually implements the sorting
     * and sorts the elements from [first, last)
     */
    template <typename ForwardIt>
    void sort_impl(ForwardIt first, ForwardIt last);

    ThreadPool<> _thread_pool;
};

template<typename ForwardIt>
void ParallelQuickSort::sort(ForwardIt first, ForwardIt last, std::size_t thread_count) {

    if (first == last) {
        return; // just a sanity check to avoid initializing thread pool unnecessarily
    }

    ParallelQuickSort parallel_quick_sort{thread_count};
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
    auto med2 = std::partition(med1, last, [pivot](const auto& elem) { return !(pivot < elem); }); // only depend on '<'
    sort_impl(first, med1);
    _thread_pool.add_task(&ParallelQuickSort::sort_impl<ForwardIt>, this, med2, last);
}
