#pragma once
#include <atomic>
#include <mutex>
#include <list>
#include "../chunk/chunk_id.hpp"
#include "../log.hpp"
#include "../task.hpp"
#include "../tools/unique_guard.hpp"
#include "../task_tree.hpp"

template<typename T>
class task_management_unit
{
public:
        explicit task_management_unit(mapped_file_uptr&& input_file,
                                      mapped_file_uptr&& output_file, 
                                      size_t max_chunk_size,
                                      size_t n_way_merge
        )        : input_file_(std::move(input_file)),
                   input_size_(input_file_->size()),
                   output_file_(std::move(output_file)),
                   gpos_(0),
                   max_chunk_size_(max_chunk_size),
                   n_way_merge_(n_way_merge),
                   active_tasks_(0)
        {
        }

        chunk_sort_task<T>
        next_sorting_task(std::unique_lock<std::mutex>&)
        {
                static std::atomic<int> id(0);

                //unique_guard<std::mutex> lk(lock);

                chunk_id new_id(0, id++);

                std::size_t remained = input_size_ - gpos_.load(std::memory_order_acquire);
                if (remained == 0)
                {
                        return chunk_sort_task<T>();
                }

                std::size_t chunk_size = std::min(remained, max_chunk_size_);

                auto offset = gpos_.fetch_add(chunk_size, std::memory_order_acq_rel);
                auto chunk_range = input_file_->range(offset, chunk_size);

                return chunk_sort_task<T>(std::move(chunk_range), std::move(new_id));
        }

        void save(std::unique_lock<std::mutex>& lock, chunk_sort_task<T>&& task)
        {
                unique_guard<std::mutex> lk(lock);

                chunk_istream<T> istream(task.acquire_mapped_mem(), task.id());
                istreams_.push_back(std::move(istream));
        }

        void save(std::unique_ptr<chunk_merge_task<T>> task)
        {
                task->release();
                --active_tasks_;

                info2() << task->debug_str();

                sync_cv_.notify_all();
        }

        void build_merge_queue()
        {
                std::call_once(queue_flag_, [this]() {
                        chunk_ostream<T> ostream(std::move(output_file_));
                        auto p = new chunk_merge_task<T>(std::move(istreams_), 
                                                         std::move(ostream));
                        std::unique_ptr<chunk_merge_task<T>> task(p);

                        queue_.push_back(std::move(task));
                });
        }

        void build_merge_queue(std::list<chunk_id>& id_list)
        {
                std::call_once(queue_flag_, [this, &id_list]() {
                        debug() << "Building queue...";

                        task_tree<T> tt;
                        tt.build(id_list, n_way_merge_);
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
                ++active_tasks_;

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

                if (queue_.front()->id().lvl > last_lvl_ && active_tasks_ > 0)
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
        std::unique_ptr<mapped_file> input_file_;
        const std::size_t input_size_;
        mapped_file_uptr output_file_;

        std::atomic_uint64_t gpos_;

        const size_t max_chunk_size_;
        const size_t n_way_merge_;

        std::vector<chunk_istream<T>> istreams_;

        std::list<std::unique_ptr<chunk_merge_task<T>>> queue_;
        std::once_flag queue_flag_;
        chunk_id::lvl_t last_lvl_ = 1;
        chunk_id result_id_;

        std::atomic_uint32_t active_tasks_;
};
