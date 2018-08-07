#pragma once

#include "../tools/util.hpp"
#include "../config.hpp"
#include "../log.hpp"
#include "merging_unit.hpp"
#include "sorting_unit.hpp"

enum
{
        COND_ID_FLAT,
        BAR_ID_SORT1,
};

template<typename T>
class pipeline
{
public:
        pipeline(uint32_t id,
                 thread_management_unit& thrmu,
                 task_management_unit<T>& tmu,
                 memory_management_unit& mmu)
                : id_(id),
                  thrmu_(thrmu),
                  lock_(thrmu.get_lock(std::defer_lock)),
                  tmu_(tmu),
                  mmu_(mmu)
        {
                ++active_pipelines_;
        }

        ~pipeline()
        {
                info2() << "worker [" << id_ << "] exit";

                if(!lock_.owns_lock())
                        lock_.lock();

                mmu().release_thread_memory();

                --active_pipelines_;

                thrmu().condition_notify_all(COND_ID_FLAT);
        }

        void run()
        {
                info2() << "worker [" << id_ << "] enter";

                _run_sort();

                if(IS_ENABLED(CONFIG_N_WAY_FLAT))
                {
                        if(!lock_.owns_lock())
                                lock_.lock();

                        if(id_ <= 0)
                        {
                                thrmu().condition_wait(COND_ID_FLAT, lock_, []()
                                { return active_pipelines_ == 1; });

                                lock_.unlock();
                        }
                        else
                        {
                                return;
                        }
                }
                else
                {
                        thrmu().barrier_wait(BAR_ID_SORT1);
                }

                tmu().build_merge_queue();

                perf_timer("Merging stage is done for", [this]() 
                {
                        merge_unit().run(lock_, tmu(), thrmu(), mmu());
                });
        }

        thread_management_unit& thrmu() {return thrmu_; }
        task_management_unit<T>& tmu() { return tmu_; };
        memory_management_unit& mmu() { return mmu_; }
        sorting_unit<T>& sort_unit() { return sort_unit_; }
        merging_unit<T>& merge_unit() { return merge_unit_; }

private:
        void _run_sort()
        {
                if(IS_ENABLED(CONFIG_SKIP_SORT))
                {
                        // let only main thread pass on
                        if (id_ > 0)
                                return;

                        info2() << "Skipping sorting stage...";
                        info2() << "Looking for chunks in directory '"
                                << CONFIG_CHUNK_DIR << "'";

                        std::list<chunk_id> id_list;
                        iterate_dir(CONFIG_CHUNK_DIR,
                        [&id_list](const char* filename) -> void
                        {
                                info2() << "Found chunk '" << filename << "'";

                                id_list.emplace_back(filename);
                        });

                        // need at least 2 files to merge
                        if (id_list.size() < 2)
                                throw_exception("Nothing to merge");

                        tmu().set_id_list(std::move(id_list));
                }
                else
                {
                        sort_unit().run(tmu(), thrmu());
                }
        }

private:
        uint32_t id_;
        thread_management_unit& thrmu_;
        std::unique_lock<std::mutex> lock_;
        task_management_unit<T>& tmu_;
        memory_management_unit& mmu_;
        sorting_unit<T> sort_unit_;
        merging_unit<T> merge_unit_;

        static std::atomic_uint32_t active_pipelines_;

};

template<typename T>
std::atomic_uint32_t pipeline<T>::active_pipelines_(0);

