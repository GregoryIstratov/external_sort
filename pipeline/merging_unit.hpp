#pragma once
#include <mutex>

#include "memory_management_unit.hpp"
#include "thread_management_unit.hpp"
#include "task_management_unit.hpp"

template <typename T>
class merging_unit
{
public:
        void run(std::unique_lock<std::mutex>& lock,
                task_management_unit<T>& tmu,
                thread_management_unit& thrmu,
                memory_management_unit& mmu)
        {
                info() << "Starting merging stage...";

                for (;;)
                {
                        lock.lock();

                        size_t qsz = tmu.merge_queue_size();
                        if (qsz == 0)
                        {
                                debug() << "exiting with empty";
                                return;
                        }

                        if (thrmu.active_threads() > qsz)
                        {
                                debug() << "exiting with que size";
                                return;
                        }

                        if (tmu.sync_on_lvl(lock))
                                continue;

                        auto task = tmu.merge_queue_pop();

                        io_mem mem = mmu.get_memory();

                        debug() << "Got new merge task [tmem="
                                << size_format(mem.tmem)
                                << ", imem=" << size_format(mem.imem)
                                << ", omem=" << size_format(mem.omem) << "]";

                        lock.unlock();

                        task->execute(mem.imem, mem.omem);

                        tmu.save(std::move(task));
                }
        }
};
