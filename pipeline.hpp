#pragma once

#include <mutex>
#include <atomic>
#include <future>
#include "chunk.hpp"
#include "util.hpp"
#include "task_tree.hpp"

template<typename T>
class pipeline
{
public:
        pipeline(raw_file_reader&& input_file, size_t max_chunk_size,
                 size_t n_way_merge, uint32_t threads_n,
                 size_t mem_avail, float io_ratio,
                 const std::string& output_filename)
                : fr_(std::move(input_file)),
                  max_chunk_size_(round_down(max_chunk_size, sizeof(T))),
                  n_way_merge_(n_way_merge),
                  threads_n_(threads_n),
                  active_tasks_(0),
                  active_threads_(0),
                  mem_avail_(mem_avail),
                  bar_wrk_enter_(threads_n),
                  io_ratio_(io_ratio),
                  output_filename_(output_filename)
        {
                hmap_ = std::make_shared<chunk_header_map<T>>();
        }

        void run()
        {
                size_t worker_mem = mem_avail_ / threads_n_;

                std::vector<std::thread> threads;
                std::vector<std::future<void>> futures;
                for (uint32_t i = 1; i < threads_n_; ++i)
                {
                        std::packaged_task<void()> task([this, i, worker_mem](){
                                worker(i, worker_mem);
                        });

                        futures.emplace_back(task.get_future());

                        threads.emplace_back(std::move(task));

                        if(IS_ENABLED(CONFIG_N_WAY_FLAT))
                                seq_mtx_.enque(i);
                }

                seq_mtx_.enque(0);

                std::packaged_task<void()> task([this, worker_mem](){
                        worker(0, worker_mem);
                });

                futures.emplace_back(task.get_future());

                task();

                for (auto& t : threads)
                        t.join();


                bool any_failed = false;
                auto get_thr_res = [&any_failed](std::future<void>& f){
                        try
                        {
                                f.get();
                        }
                        catch(const std::exception& e)
                        {
                                error() << e.what();
                                any_failed = true;
                        }
                };


                for(auto& f : futures)
                        get_thr_res(f);

                if(any_failed)
                        throw_exception("Pipeline workers error");

                if(std::rename(make_filename(result_id_).c_str(),
                               output_filename_.c_str()) != 0)
                {
                        throw_exception("Cannot rename '" << make_filename(result_id_)
                                                 << "' to '" << output_filename_
                                                 << "': " << strerror(errno));
                }
        }
private:
        void worker(uint32_t id, size_t start_mem)
        {
                std::unique_lock<std::mutex> lock(mtx_, std::defer_lock);
                perf_timer tm_;
                size_t tmem = 0;

                worker_enter(id, start_mem, tmem);

                sorting_stage();

                if(IS_ENABLED(CONFIG_N_WAY_FLAT))
                {
                        seq_mtx_.lock(id, cmd_mod::skip_enque);

                        std::unique_lock<sequential_mutex> flat_lock(seq_mtx_,
                                                                     std::adopt_lock);

                        if(id > 0)
                        {
                                worker_exit(id, tmem);
                                return;;
                        }
                }

                build_merge_queue();

                merging_stage(id, tmem, lock);

                worker_exit(id, tmem);
        }

        void worker_enter(uint32_t id, size_t start_mem, size_t& tmem)
        {
                info2() << "worker enter";

                active_threads_++;

                tmem = start_mem;
                mem_avail_ -= tmem;

                bar_wrk_enter_.wait();
        }

        void worker_exit(uint32_t id, size_t tmem)
        {
                mem_avail_ += tmem;

                active_threads_--;

                sync_cv_.notify_one();

                info2() << "worker [" << id << "] exit";
        }

        void sorting_stage()
        {
                static std::once_flag tm_start_flag, tm_end_flag;
                static perf_timer tm;
                static barrier bar(threads_n_);

                perf_timer ltm;

                debug() << "worker sort";

                std::call_once(tm_start_flag, [](perf_timer& t){ t.start(); }, tm);
                ltm.start();

                auto task = split_next();
                while (!task.empty())
                {
                        task.execute();

                        save(std::move(task));

                        task = split_next();
                }

                ltm.end();

                // sync threads before time measurement
                bar.wait();

                std::call_once(tm_end_flag, [](perf_timer& t) {
                        t.end();
                        info() << "Sorting stage is done for "
                               << t.elapsed<perf_timer::ms>() << " ms";
                }, tm);

                info2() << "Thread sorting stage is done for "
                           << ltm.elapsed<perf_timer::ms>() << " ms";

        }

        void merging_stage(uint32_t id, size_t& tmem, std::unique_lock<std::mutex>& lock)
        {
                debug() << "worker merge";

                for(;;)
                {
                        lock.lock();

                        if(queue_.empty())
                        {
                                debug() << "exiting with empty";
                                return;
                        }

                        size_t qsz = queue_.size();
                        if(active_threads_ > qsz)
                        {
                                debug() << "exiting with que size";
                                return;
                        }

                        if(!sync_on_lvl(lock))
                                continue;

                        auto task = std::move(queue_.front());
                        queue_.pop_front();

                        last_lvl_ = task->id().lvl > last_lvl_ ?
                                    task->id().lvl : last_lvl_;

                        tmem += acquire_free_mem();

                        size_t imem = tmem * io_ratio_;
                        size_t omem = tmem * (1.0 - io_ratio_);

                        debug() << "Got new merge task [tmem="
                                 <<size_format(tmem)
                                 <<", imem=" <<size_format(imem)
                                 << ", omem=" << size_format(omem) << "]";

                        ++active_tasks_;

                        lock.unlock();

                        task->execute(imem, omem, hmap_);
                        task->release();

                        --active_tasks_;

                        info2() << "Worker [" << id << "]: " << task->debug_str();

                        sync_cv_.notify_all();
                }
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

        bool sync_on_lvl(std::unique_lock<std::mutex>& lock)
        {
                if(queue_.front()->id().lvl > last_lvl_ && active_tasks_ > 0)
                {
                        debug() << "Sync threads on the new lvl [at="
                                << active_tasks_ << "]";

                        sync_cv_.wait(lock, [this]()
                        { return active_tasks_ == 0; });

                        debug() << "Sync threads wake up";

                        lock.unlock();

                        return false;
                }

                return true;
        }

        size_t acquire_free_mem()
        {
                return mem_avail_.exchange(0);
        }

        chunk_sort_task<T> split_next()
        {
                static std::atomic<chunk_id::id_t> id(0);

                std::vector<T> buff(max_chunk_size_ / sizeof(T));

                {
                        std::lock_guard<std::mutex> lock(mtx_);

                        if (fr_.eof())
                                return chunk_sort_task<T>();

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

                return chunk_sort_task<T>(std::move(buff), chunk_id(0, id++), hmap_);
        }

        void save(chunk_sort_task<T>&& task)
        {
                auto name = make_filename(task.id());

                file_write(name.c_str(), task.data().data(), task.size());

                task.release();

                std::lock_guard<std::mutex> lock(mtx_);

                l0_ids_.push_back(task.id());
        }
private:
        raw_file_reader fr_;
        const size_t max_chunk_size_;
        const size_t n_way_merge_;
        const uint32_t threads_n_;

        std::list<chunk_id> l0_ids_;

        std::list<std::unique_ptr<chunk_merge_task<T>>> queue_;
        std::once_flag queue_flag_;
        chunk_id::lvl_t last_lvl_ = 1;
        chunk_id result_id_;

        std::atomic_uint32_t active_tasks_;
        std::atomic_uint32_t active_threads_;
        std::atomic<size_t>  mem_avail_;

        barrier bar_wrk_enter_;

        const float io_ratio_;

        std::mutex mtx_;
        std::condition_variable sync_cv_;
        sequential_mutex seq_mtx_;

        std::string output_filename_;

        chunk_header_map_sptr<T> hmap_;
};
