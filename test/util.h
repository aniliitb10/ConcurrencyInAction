#pragma once

#include <iostream>
#include <iterator>
#include <random>
#include <type_traits>
#include <string>
#include <numeric>
#include <vector>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

template <typename Itr>
void print(Itr begin, Itr end, char sep = ' ', bool newline = true)
{
    if (begin == end) return;

    while (std::next(begin) != end) // to void putting sep even after the last element
    {
        std::cout << *begin << sep;
        std::advance(begin, 1);
    }
    std::cout << *begin;
    if (newline)
    {
        std::cout << '\n';
    }
}

// To determine types which are string like, e.g. char*, char[N] and std::string
// - basically, the types which can construct a std::string
template <typename T, typename = void>
struct string_like_type : std::false_type
{};

template <typename T>
struct string_like_type <T, std::enable_if_t<std::is_constructible_v<std::string, T>>> : std::true_type
{};

template <typename T>
constexpr bool is_string_like_type_v = string_like_type<T>::value;


// to determine if type T is an iterable
// - string like containers are exempted from this
template <typename T, typename = void>
struct is_iterable : std::false_type
{};

template <typename T>
struct is_iterable <T,
        std::void_t<decltype(std::cbegin(std::declval<T>())),
                decltype(std::cend(std::declval<T>())),
                std::enable_if_t<!is_string_like_type_v<T>> // this will return void but that's fine
        >
> : std::true_type
{};

// an helper to use is_iterable
template <typename T>
constexpr bool is_iterable_v = is_iterable<T>::value;

template <typename Container>
std::enable_if_t<is_iterable_v<Container>, void>
print(const Container& container, char sep = ' ', bool newline = true)
{
    print(container.cbegin(), container.cend(), sep, newline);
}

// good thing: it works even with std::vector<std::vector>
// bad thing: can't specify separator and if newline is needed
template <typename Container>
std::enable_if_t<is_iterable_v<Container>, std::ostream&>
operator<<(std::ostream& os, const Container& container)
{
    auto begin = std::cbegin(container);
    auto end = std::cend(container);

    if (begin == end) return os;

    while (std::next(begin) != end)
    {
        os << *begin;
        if constexpr (!is_iterable_v<decltype(*begin)>)
        {
            // Append white space only if iterator doesn't point to another container
            // - e.g. iterator of std::vector<std::vector<int>> points to another container
            // - and printing space after newline will make it look ugly
            os << ' ';
        }

        std::advance(begin, 1);
    }
    os << *begin << '\n';
    return os;
}

template <typename OutputIt>
void fill_random_int(OutputIt it, int from, int to, int count)
{
    std::mt19937 engine{ std::random_device{}() };
    std::uniform_int_distribution<int> dist{ from, to };
    while (count--)
    {
        it = dist(engine);
    }
}

template <typename OutputIt>
void fill_random_int(OutputIt it, int count)
{
    constexpr auto min = std::numeric_limits<int>::min();
    constexpr auto max = std::numeric_limits<int>::max();
    fill_random_int(it, min, max, count);
}

inline
std::vector<int> get_random_int_vec(int from, int to, int count)
{
    std::vector<int> ret{};
    fill_random_int(std::back_inserter(ret), from, to, count);
    return ret;
}

inline
std::vector<int> get_random_int_vec(std::size_t count)
{
    std::vector<int> ret{};
    fill_random_int(std::back_inserter(ret), count);
    return ret;
}

static int get_random_number(int from_, int to_)
{
    // intentionally making static
    static std::mt19937 engine{ std::random_device{}() };
    std::uniform_int_distribution<int> dist{ from_, to_ };
    return dist(engine);
}

// as gtest macros don't allow much template params, this can be super useful
// in avoiding first declaring the vector and then passing in macros
template <typename T=int>
std::vector<T> get_vector(std::initializer_list<T> list)
{
    return std::vector<T>(list);
}

// to get a slow dummy task for testing threadpool
inline
auto get_slow_task(std::chrono::milliseconds ms, int num = 0)
{
    return [ms, num] {
        std::this_thread::sleep_for(ms);
        return num;
    };
}