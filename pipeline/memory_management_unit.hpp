#pragma once
#include <cstdint>



struct io_mem
{
        size_t imem, omem, tmem;
};

class memory_management_unit
{
public:
        memory_management_unit(size_t avail_mem, uint32_t threads_n, float io_ratio)
                : avail_mem_(avail_mem),
                threads_n_(threads_n),
                io_ratio_(io_ratio)
        {
                _recalculate_memory();
        }


        const io_mem& get_memory() const
        {
                return mem_;
        }

        void release_thread_memory()
        {
                threads_n_--;

                if (threads_n_ == 0)
                        return;

                _recalculate_memory();
        }

private:
        void _recalculate_memory()
        {
                mem_.imem = static_cast<size_t>(avail_mem_ * io_ratio_
                                                / threads_n_);
                mem_.omem = static_cast<size_t>(avail_mem_ * (1.0f - io_ratio_)
                                                / threads_n_);
                mem_.tmem = static_cast<size_t>(avail_mem_ / threads_n_);
        }

private:
        size_t avail_mem_;
        uint32_t threads_n_;
        float io_ratio_;
        io_mem mem_{};
};
