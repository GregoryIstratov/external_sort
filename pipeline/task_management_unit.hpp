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
        explicit task_management_unit(std::unique_ptr<mapped_file>&& input_file,
                                      size_t max_chunk_size,
                                      size_t n_way_merge
        )        : input_file_(std::move(input_file)),
                  max_chunk_size_(max_chunk_size),
                  n_way_merge_(n_way_merge),
                  active_tasks_(0)
        {
        }

        chunk_sort_task<T>
        next_sorting_task(std::unique_lock<std::mutex>& lock)
        {
                static std::atomic<int> id(0);

                std::unique_ptr<mapped_file> chunk_file;
                chunk_id new_id(0, id++);

                {
                        unique_guard<std::mutex> lk_(lock);

                        if (!input_file_)
                                return chunk_sort_task<T>();

                        std::size_t remained = input_file_->size() - gpos_;

                        if (remained == 0)
                        {
                                input_file_.reset();
                                return chunk_sort_task<T>();
                        }

                        std::size_t chunk_size = std::min(remained, 
                                                          max_chunk_size_);

                        auto filename = new_id.to_full_filename();
                        chunk_file = input_file_->range(gpos_, chunk_size)
                                     ->map_to_new_file(filename.c_str());

                        gpos_ += chunk_size;
                      
                        if (chunk_size < max_chunk_size_) 
                        {
                                input_file_.reset();
                        }
                }

                return chunk_sort_task<T>(std::move(chunk_file), std::move(new_id));
        }

        void save(std::unique_lock<std::mutex>& lock, chunk_sort_task<T>&& task)
        {
                //perf_timer("Saving sort task", [&name, &task]() {
                task.release();
                //});

                unique_guard<std::mutex> lk(lock);

                l0_ids_.push_back(task.id());
        }

        void save(std::unique_ptr<chunk_merge_task<T>> task)
        {
                task->release();
                --active_tasks_;

                info2() << task->debug_str();

                sync_cv_.notify_all();
        }

        void set_id_list(std::list<chunk_id>&& id_list)
        {
                l0_ids_ = std::move(id_list);
        }

        void build_merge_queue()
        {
                build_merge_queue(l0_ids_);
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

        std::size_t gpos_ = 0;

        const size_t max_chunk_size_;
        const size_t n_way_merge_;

        std::list<chunk_id> l0_ids_;

        std::list<std::unique_ptr<chunk_merge_task<T>>> queue_;
        std::once_flag queue_flag_;
        chunk_id::lvl_t last_lvl_ = 1;
        chunk_id result_id_;

        std::atomic_uint32_t active_tasks_;
};
