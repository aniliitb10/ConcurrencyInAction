#pragma once

#include <string>
#include <iostream>
#include <date/date.h>
#include <chrono>
#include <ostream>
#include <fmt/format.h>

namespace dt
{
    struct time_t
    {
        explicit time_t(const date::hh_mm_ss<std::chrono::milliseconds> &hh_mm_ss) :
                hours(hh_mm_ss.hours().count()),
                minutes(hh_mm_ss.minutes().count()),
                secs(hh_mm_ss.seconds().count()),
                milli_secs(hh_mm_ss.subseconds().count())
        {}

        friend std::ostream &operator<<(std::ostream &out, const time_t &);

        uint16_t hours;
        uint16_t minutes;
        uint16_t secs;
        uint16_t milli_secs;
    };

    std::ostream &operator<<(std::ostream &out, const time_t &time)
    {
        // :0>2 -> '0' is the filler, '>' means actual text should be right aligned, '2' -> is the length including filler
        out << fmt::format("{:0>2}:{:0>2}:{:0>2}:{:0>3}", time.hours, time.minutes, time.secs, time.milli_secs);
        return out;
    }

    class datetime
    {
    public:
        using date_t = date::year_month_day;

        explicit datetime(const std::chrono::time_point<std::chrono::system_clock> &time_point
        = std::chrono::system_clock::now()) :
                date(date::floor<date::days>(time_point)),
                time(date::hh_mm_ss{date::floor<std::chrono::milliseconds>(
                        time_point - date::floor<date::days>(time_point)
                )})
        {}

        static datetime datetime_IST()
        {
            // this is, precisely, only for IST
            using namespace std::chrono_literals;
            return datetime{std::chrono::system_clock::now() + (5h + 30min)};
        }

        static datetime now()
        {
            return datetime{};
        }

        // streams
        friend std::ostream &operator<<(std::ostream &out, const datetime &);

    private:
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> date_point;
        date_t date;
        time_t time;
    };

    std::ostream &operator<<(std::ostream &out, const datetime &datetime)
    {
        out << datetime.date << " " << datetime.time;
        return out;
    }
}