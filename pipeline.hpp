#pragma once

#include <mutex>
#include <atomic>
#include "file_io.hpp"
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
                  max_chunk_size_(max_chunk_size),
                  n_way_merge_(n_way_merge),
                  threads_n_(threads_n),
                  active_tasks_(0),
                  active_threads_(0),
                  mem_avail_(mem_avail),
                  bar0_(threads_n),
                  bar1_(threads_n),
                  io_ratio_(io_ratio),
                  output_filename_(output_filename)
        {

        }

        void run()
        {
                size_t worker_mem = mem_avail_ / threads_n_;

                std::vector<std::thread> threads;
                for (uint32_t i = 1; i < threads_n_; ++i)
                {
                        threads.emplace_back(&pipeline::worker, this, i, worker_mem);
                }

                worker(0, worker_mem);

                for (auto& t : threads)
                        t.join();
        }
private:
        void worker(uint32_t id, size_t start_mem)
        {
                std::unique_lock<std::mutex> lock(mtx_, std::defer_lock);
                perf_timer tm_;

                info2() << "worker enter";

                active_threads_++;

                size_t tmem = start_mem;
                mem_avail_ -= tmem;

                bar0_.wait();

                sorting_stage();

                /* sync before building queue */
                bar1_.wait();

                build_merge_queue();

                merging_stage(id, tmem, lock);

                mem_avail_ += tmem;

                active_threads_--;

                sync_cv_.notify_one();

                info2() << "worker [" << id << "] exit";
        }

        void sorting_stage()
        {
                debug() << "worker sort";

                auto task = split_next();
                while (!task.empty())
                {
                        task.execute();

                        save(std::move(task));

                        task = split_next();
                }
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

                        debug() << "Getting new merge task [tmem="
                                 << size_format(tmem) << ", ior="
                                 <<io_ratio_<<", qsz="<<qsz<<"]";

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
                                 << ", omem=" << size_format(omem);

                        ++active_tasks_;

                        lock.unlock();

                        task->execute(imem, omem);
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

                        queue_.back()->set_output_filename(output_filename_);
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

                return chunk_sort_task<T>(std::move(buff), chunk_id(0, id++));
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
        size_t max_chunk_size_;
        size_t n_way_merge_;
        uint32_t threads_n_;

        std::list<chunk_id> l0_ids_;

        std::list<std::unique_ptr<chunk_merge_task<T>>> queue_;
        std::once_flag queue_flag_;
        chunk_id::lvl_t last_lvl_ = 1;

        std::atomic_uint32_t active_tasks_;
        std::atomic_uint32_t active_threads_;
        std::atomic<size_t>  mem_avail_;


        barrier bar0_, bar1_;

        const float io_ratio_;

        std::mutex mtx_;
        std::condition_variable sync_cv_;

        std::string output_filename_;
};
