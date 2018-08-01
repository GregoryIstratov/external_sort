#pragma once

#include <mutex>
#include <atomic>
#include <future>
#include <unordered_map>
#include "chunk.hpp"
#include "util.hpp"
#include "task_tree.hpp"

enum
{
        COND_ID_FLAT,
        BAR_ID_SORT1,
};

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
                mem_.imem = static_cast<size_t>(avail_mem_ * io_ratio / threads_n);
                mem_.omem = static_cast<size_t>(avail_mem_ * (1.0f - io_ratio) / threads_n);
                mem_.tmem = static_cast<size_t>(avail_mem_ / threads_n_);
        }


        io_mem get_memory() const
        {
                return mem_;
        }

        void release_thread_memory()
        {
                threads_n_--;

                if(threads_n_ == 0)
                        return;

                mem_.imem = static_cast<size_t>(avail_mem_ * io_ratio_ / threads_n_);
                mem_.omem = static_cast<size_t>(avail_mem_ * (1.0f - io_ratio_) / threads_n_);
                mem_.tmem = static_cast<size_t>(avail_mem_ / threads_n_);
        }

private:

private:
        size_t avail_mem_;
        uint32_t threads_n_;
        float io_ratio_;
        io_mem mem_;
};


class thread_management_unit
{
public:
        explicit
        thread_management_unit(uint32_t threads_n)
                : threads_n_(threads_n), active_threads_(0)
        {}

        void spawn_and_join(std::function<void(uint32_t)>&& fun)
        {
                std::vector<std::thread> threads;
                std::vector<std::future<void>> futures;

                for (uint32_t i = 1; i < threads_n_; ++i)
                {
                        std::packaged_task<void()> task([this, i, &fun](){
                                active_threads_++;
                                fun(i);
                                active_threads_--;
                        });

                        futures.emplace_back(task.get_future());
                        threads.emplace_back(std::move(task));

                }

                std::packaged_task<void()> task([this, &fun](){
                        active_threads_++;
                        fun(0);
                        active_threads_--;
                });

                futures.emplace_back(task.get_future());

                task();

                for (auto& t : threads)
                        t.join();

                bool any_failed = false;
                for(auto& f : futures)
                {
                        try
                        {
                                f.get();
                        }
                        catch(const std::exception& e)
                        {
                                error() << e.what();
                                any_failed = true;
                        }
                }

                if(any_failed)
                        throw_exception("Something went wrong");
        }

        uint32_t active_threads() const { return active_threads_; }

        template<typename LockMod>
        std::unique_lock<std::mutex> get_lock(LockMod lock_mod)
        {
                return std::unique_lock<std::mutex>(mtx_, lock_mod);
        }

        std::unique_lock<std::mutex> get_lock()
        {
                return std::unique_lock<std::mutex>(mtx_);
        }

        void condition_wait(uint32_t id, std::unique_lock<std::mutex>& lock,
                            std::function<bool()>&& cond)
        {
                cv_map_[id].wait(lock, cond);
        }

        void condition_notify_all(uint32_t id)
        {
                cv_map_[id].notify_all();
        }

        void barrier_wait(uint32_t id)
        {
                auto found = bar_map_.find(id);

                if(found != bar_map_.end())
                {
                        found->second.wait();
                }
                else
                {
                        auto res = bar_map_.emplace(id, threads_n_);

                        if(!res.second)
                                throw_exception("Failed to insert to hash map");

                        res.first->second.wait();
                }
        }
private:
        uint32_t threads_n_;
        std::mutex mtx_;
        std::atomic_uint32_t active_threads_;

        std::unordered_map<uint32_t, std::condition_variable> cv_map_;
        std::unordered_map<uint32_t, barrier> bar_map_;
};

template<typename T>
class task_management_unit
{
public:
        explicit
        task_management_unit(raw_file_reader&& fr,
                             size_t max_chunk_size,
                             size_t n_way_merge)
        : fr_(std::move(fr)),
          max_chunk_size_(max_chunk_size),
          n_way_merge_(n_way_merge),
          active_tasks_(0)
        {
        }

        chunk_sort_task<T> next_sorting_task(std::unique_lock<std::mutex>& lock)
        {
                static std::atomic<int> id(0);

                std::vector<T> buff(max_chunk_size_ / sizeof(T));

                {
                        unique_guard<std::mutex> lk_(lock);

                        if (fr_.eof())
                        {
                                return chunk_sort_task<T>();
                        }

                        uint64_t read = fr_.read((char*)&buff[0], max_chunk_size_);

                        if (read != max_chunk_size_) {
                                if (read == 0) {

                                        fr_.close();
                                        return chunk_sort_task<T>();
                                }

                                if(fr_.eof())
                                        fr_.close();

                                size_t n = read / sizeof(T);
                                buff.erase(buff.begin() + n, buff.end());
                        }
                }


                return chunk_sort_task<T>(std::move(buff), chunk_id(0, id++));
        }

        void save(std::unique_lock<std::mutex>& lock, chunk_sort_task<T>&& task)
        {
                auto name = make_filename(task.id());

                //perf_timer("Saving sort task", [&name, &task]() {
                        file_write(name.c_str(), task.data(), task.size());
                //});

                task.release();

                unique_guard<std::mutex> lk_(lock);

                l0_ids_.push_back(task.id());
        }

        void save(std::unique_ptr<chunk_merge_task<T>> task)
        {
                task->release();
                active_tasks_--;

                info2() << task->debug_str();

                sync_cv_.notify_all();
        }

        void build_merge_queue()
        {
                std::call_once(queue_flag_, [this](){
                        debug() << "Building queue...";

                        task_tree<T> tt;
                        tt.build(l0_ids_, n_way_merge_);
                        queue_ = tt.make_queue();

                        result_id_ = queue_.back()->id();
                });
        }

        size_t merge_queue_size() const { return queue_.size(); }

        std::unique_ptr<chunk_merge_task<T>> merge_queue_pop()
        {
                auto item = std::move(queue_.front());
                queue_.pop_front();

                last_lvl_ = item->id().lvl > last_lvl_ ? item->id().lvl : last_lvl_;
                active_tasks_++;

                return std::move(item);
        }

        const std::unique_ptr<chunk_merge_task<T>>& merge_queue_front() const
        {
                return queue_.front();
        }

        /* returns true if sync has done and releases the lock
         * if false the lock won't be released */
        bool sync_on_lvl(std::unique_lock<std::mutex>& lock)
        {
                unique_guard<std::mutex> lk_(lock, false);

                if(queue_.front()->id().lvl > last_lvl_ && active_tasks_ > 0)
                {
                        debug() << "Sync threads on the new lvl [at="
                                << active_tasks_ << "]";

                        sync_cv_.wait(lock, [this]()
                        { return active_tasks_ == 0; });

                        debug() << "Sync threads wake up";

                        lock.unlock();

                        return true;
                }

                return false;
        }

        chunk_id result_id() const { return result_id_; }
private:
        std::condition_variable sync_cv_;
        raw_file_reader fr_;

        const size_t max_chunk_size_;
        const size_t n_way_merge_;

        std::list<chunk_id> l0_ids_;

        std::list<std::unique_ptr<chunk_merge_task<T>>> queue_;
        std::once_flag queue_flag_;
        chunk_id::lvl_t last_lvl_ = 1;
        chunk_id result_id_;

        std::atomic_uint32_t active_tasks_;
};

template<typename T>
class sorting_unit
{
public:
        void run(task_management_unit<T>& tmu, thread_management_unit& thrmu)
        {
                perf_timer ltm;

                debug() << "[sorting_unit] run";

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
        auto _next_task(std::unique_lock<std::mutex>& lock, task_management_unit<T>& tmu)
        {
                decltype(tmu.next_sorting_task(lock)) task;

                //perf_timer("Getting next sorting task", [&task, &lock, &tmu](){
                        task = tmu.next_sorting_task(lock);
                //});

                return task;
        }
};

template <typename T>
class merging_unit
{
public:
        merging_unit()
        {

        }

        ~merging_unit()
        {

        }

        void run(std::unique_lock<std::mutex>& lock,
                 task_management_unit<T>& tmu,
                 thread_management_unit& thrmu,
                 memory_management_unit& mmu)
        {
                debug() << "[merging_unit] run";

                for(;;)
                {
                        lock.lock();

                        size_t qsz = tmu.merge_queue_size();
                        if(qsz == 0)
                        {
                                debug() << "exiting with empty";
                                return;
                        }

                        if(thrmu.active_threads() > qsz)
                        {
                                debug() << "exiting with que size";
                                return;
                        }

                        if(tmu.sync_on_lvl(lock))
                                continue;

                        auto task = tmu.merge_queue_pop();

                        io_mem mem = mmu.get_memory();

                        debug() << "Got new merge task [tmem="
                                << size_format(mem.tmem)
                                << ", imem=" <<size_format(mem.imem)
                                << ", omem=" << size_format(mem.omem) << "]";

                        lock.unlock();

                        task->execute(mem.imem, mem.omem);

                        tmu.save(std::move(task));
                }
        }
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
                active_pipelines_++;
        }

        ~pipeline()
        {
                info2() << "worker [" << id_ << "] exit";

                if(!lock_.owns_lock())
                        lock_.lock();

                mmu().release_thread_memory();

                active_pipelines_--;

                thrmu().condition_notify_all(COND_ID_FLAT);
        }

        void run()
        {
                info2() << "worker [" << id_ << "] enter";

                sort_unit().run(tmu(), thrmu());

                if(IS_ENABLED(CONFIG_N_WAY_FLAT))
                {
                        if(!lock_.owns_lock())
                                lock_.lock();

                        if(id_ <= 0)
                        {
                                thrmu().condition_wait(COND_ID_FLAT, lock_, [this]()
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

                merge_unit().run(lock_, tmu(), thrmu(), mmu());
        }

        thread_management_unit& thrmu() {return thrmu_; }
        task_management_unit<T>& tmu() { return tmu_; };
        memory_management_unit& mmu() { return mmu_; }
        sorting_unit<T>& sort_unit() { return sort_unit_; }
        merging_unit<T>& merge_unit() { return merge_unit_; }


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

template<typename T>
class processor
{
public:
        processor(raw_file_reader&& input_file, size_t max_chunk_size,
                size_t n_way_merge, uint32_t threads_n,
                size_t mem_avail, float io_ratio,
                const std::string& output_filename)
                : max_chunk_size_(round_down(max_chunk_size, sizeof(T))),
                  n_way_merge_(n_way_merge),
                  threads_n_(threads_n),
                  output_filename_(output_filename),
                  thrmu_(threads_n_),
                  tmu_(std::move(input_file), max_chunk_size_, n_way_merge_),
                  mmu_(mem_avail, threads_n_, io_ratio)

        {
        }

        void run()
        {
                thrmu_.spawn_and_join([this](uint32_t id){
                        pipeline<T> pl(id, thrmu_, tmu_, mmu_);
                        pl.run();
                });


                if(std::rename(make_filename(tmu_.result_id()).c_str(),
                               output_filename_.c_str()) != 0)
                {
                        throw_exception("Cannot rename '" << make_filename(tmu_.result_id())
                                                          << "' to '" << output_filename_
                                                          << "': " << strerror(errno));
                }
        }

private:

private:
        const size_t max_chunk_size_;
        const size_t n_way_merge_;
        const uint32_t threads_n_;
        std::string output_filename_;

        thread_management_unit thrmu_;
        task_management_unit<T> tmu_;
        memory_management_unit mmu_;
};
