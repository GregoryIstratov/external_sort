#pragma once
#include <mutex>

#include "../config.hpp"
#include "../tools/util.hpp"
#include "../tools/perf_timer.hpp"
#include "task_management_unit.hpp"
#include "thread_management_unit.hpp"

template<typename T>
class sorting_unit
{
public:
        void run(task_management_unit<T>& tmu, thread_management_unit& thrmu)
        {
                perf_timer ltm;

                info() << "Starting sorting stage...";

                auto lock = thrmu.get_lock(std::defer_lock);

                ltm.start();

                auto task = _next_task(lock, tmu);
                while (!task.empty())
                {
                        task.execute();

                        tmu.save(lock, std::move(task));

                        task = _next_task(lock, tmu);
                }

                ltm.end();

                info2() << "Thread sorting stage is done for "
                        << ltm.elapsed<perf_timer::ms>() << " ms";
        }

private:
        auto _next_task(std::unique_lock<std::mutex>& lock,
                        task_management_unit<T>& tmu)
        {
                if (IS_ENABLED(CONFIG_PERF_MEASURE_GET_NEXT_SORT_TASK))
                {
                        decltype(tmu.next_sorting_task(lock)) task;

                        perf_timer("Getting next sorting task",
                        [&task, &lock, &tmu]()
                        {
                                task = tmu.next_sorting_task(lock);
                        });

                        return task;
                }
                else
                {
                        return tmu.next_sorting_task(lock);
                }
        }
};
