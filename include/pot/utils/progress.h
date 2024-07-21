#pragma once

#include <mutex>

class progress
{
public:
    progress(double min_val = 0.0, double max_val = 100.0) : min_value(min_val), max_value(max_val), current_value(min_val)
    {
    }

    void set_min(double min_val)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        min_value = min_val;
        if (current_value < min_value)
        {
            current_value = min_val;
        }
    }

    void set_max(double max_val)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        max_value = max_val;
        if (current_value > max_value)
        {
            current_value = max_value;
        }
    }

    void set_progress(double value)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (value < min_value)
        {
            current_value = min_value;
        }
        else if (value > max_value)
        {
            current_value = max_value;
        }
        else
        {
            current_value = value;
        }
    }

    double get_min() const
    {
        return min_value;
    }

    double get_max() const
    {
        return max_value;
    }

    double get_progress() const
    {
        return current_value;
    }

private:
    std::mutex m_mtx;
    double min_value;
    double max_value;
    double current_value;
};