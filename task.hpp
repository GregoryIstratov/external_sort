#pragma once

#include <algorithm>
#include <utility>
#include "tools/file.hpp"
#include "tools/perf_timer.hpp"
#include "tools/format.hpp"
#include "chunk/chunk_id.hpp"
#include "chunk/chunk_istream.hpp"
#include "chunk/chunk_ostream.hpp"
#include "extra/sort.hpp"
#include "tools/mapped_file.hpp"

template<typename T>
class chunk_sort_task
{
public:
        chunk_sort_task() = default;

        explicit
        chunk_sort_task(std::unique_ptr<mapped_file>&& file, chunk_id id)
                : file_(std::move(file)), id_(std::move(id))
        {}

        chunk_sort_task(chunk_sort_task&&) noexcept = default;
        chunk_sort_task& operator=(chunk_sort_task&&) noexcept = default;

        void execute()
        {
                perf_timer tm;
                tm.start();

                auto range = file_->range();
                range->lock();

                T* data = reinterpret_cast<T*>(range->data());
                std::size_t size = range->size() / sizeof(T);

                sort(data, size);

                range->unlock();

                tm.end();

                info2() << "sorted " << id_
                        << " (" << size_format(range->size())
                        << "/" << num_format(size) << ") for "
                        << tm.elapsed<perf_timer::ms>() << " ms";

                if (IS_ENABLED(CONFIG_PRINT_CHUNK_DATA))
                {
                        std::stringstream ss;
                        ss << "[";

                        for (size_t i = 0; i < size; ++i)
                                ss << data[i] << " ";

                        ss << "]";
                        info2() << id_ << " data: " << ss.rdbuf();
                }
        }

        //Saves mapped data to file and release resources
        void release()
        {
                file_.reset();
        }

        bool empty() const { return !file_; }

        chunk_id id() const { return id_; }

private:
        void sort(T* data, std::size_t size)
        {
                switch(CONFIG_SORT_ALGO)
                {
                        case CONFIG_SORT_STD:
                                std_sort(data, size);
                                break;

                        case CONFIG_SORT_HEAP:
                                heap_sort(data, size);
                                break;

                        case CONFIG_SORT_RADIX:
                                radix_sort(data, size);
                                break;

                        default:
                                THROW_EXCEPTION << "Unknown option";
                }
        }

        void heap_sort(T* data, std::size_t size)
        {
                std::make_heap(data, data + size);
                std::sort_heap(data, data + size);
        }

        void std_sort(T* data, std::size_t size)
        {
                std::sort(data, data + size);
        }

        void radix_sort(T* data, std::size_t size)
        {
                sort::integer_sort(data, data + size);
        }

private:
        std::unique_ptr<mapped_file> file_;
        chunk_id id_;
};

template<typename T>
class chunk_merge_task
{
public:
        chunk_merge_task() = default;

        chunk_merge_task(std::vector<chunk_istream<T>>&& input,
                         chunk_ostream<T>&& output,
                         chunk_id output_id)
                : input_(std::move(input)),
                  output_(std::move(output)),
                  output_id_(std::move(output_id))
        {
        }

        chunk_merge_task(chunk_merge_task&&) = default;
        chunk_merge_task& operator=(chunk_merge_task&&) = default;

        void execute(size_t in_buff_size, size_t out_buff_size)
        {
                perf_timer tm;
                tm.start();

                if(IS_ENABLED(CONFIG_REMOVE_TMP_FILES))
                        make_remove_queue();

                size_t ick_mem = round_down(in_buff_size/input_.size(), sizeof(T));
                size_t ock_mem = round_down(out_buff_size, sizeof(T));

                if(!ick_mem || !ock_mem)
                        THROW_EXCEPTION << "No memory for buffers [ick_mem="
                                                              << ick_mem
                                                              << " ock_mem="
                                                              << ock_mem
                                                              << "]";

                uint64_t output_size = 0;
                for (auto& is : input_)
                {
                        is.open(ick_mem);
                        output_size += is.size();
                }

                output_.open(ock_mem, output_size);

                if(CONFIG_INFO_LEVEL >= 2)
                {
                        ss_ << "Merged { ";
                        for(const auto& k : input_)
                                ss_ << k.id() << " ("
                                    << size_format(k.buff_size())
                                    << "/" << size_format(k.size())
                                    << "/" << num_format(k.count()) << ") ";
                }

                if (input_.size() == 2)
                        two_way_merge();
                else
                        pq_merge();

                output_.close();

                if(IS_ENABLED(CONFIG_REMOVE_TMP_FILES))
                        remove_tmp_files();

                if(CONFIG_INFO_LEVEL >= 2)
                {
                        tm.end();

                        ss_ << " } -> { " << id() << " ("
                            << size_format(output_.buff_size()) << ")"
                            << " } for " << tm.elapsed<perf_timer::ms>() << " ms";
                }

        }

        std::string debug_str() const { return ss_.str(); }

        chunk_id id() const { return output_id_; }

        void set_output_filename(const std::string& value)
        {
                output_.filename(value);
        }

        void release()
        {
                input_ = decltype(input_)();
                output_ = decltype(output_)();
        }

private:

        void make_remove_queue()
        {
                for(auto& is : input_)
                        remove_que_.push_back(is.id().to_full_filename());

        }

        void remove_tmp_files()
        {
                // closes all files
                input_ = decltype(input_)();

                for(const auto& filename : remove_que_)
                {
                        try
                        {
                                delete_file(filename.c_str());
                        }
                        catch(const std::exception& e)
                        {
                                error() << "Failed to remove tmp file '"
                                        << filename << "': " << e.what();
                        }
                }
        }

        void copy_to_output(chunk_istream<T>& is)
        {
                is.copy_to(output_);
        }


        // cache friendly heap items
        struct heap_item
        {
                T value;                
                chunk_istream<T>* is;


                friend bool operator!=(const heap_item& a, const heap_item& b)
                {
                        return a.value != b.value;
                }

                friend bool operator<(const heap_item& a, const heap_item& b)
                {
                        return a.value > b.value;
                }

                friend bool operator>(const heap_item& a, const heap_item& b)
                {
                        return a.value < b.value;
                }
        };

        void pq_merge()
        {
                pq_merge_std();
        }
        
        void pq_merge_std()
        {
                std::vector<heap_item> heap(input_.size());

                for (uint32_t i = 0; i < input_.size(); ++i)
                {
                        heap[i].value = input_[i].value();
                        heap[i].is = &input_[i];
                }

                std::make_heap(heap.begin(), heap.end());
                while (!heap.empty())
                {
                        std::pop_heap(heap.begin(), heap.end());

                        T v = heap.back().value;
                        output_.put(v);

                        if (heap.back().is->next())
                        {
                                heap.back().value = heap.back().is->value();
                                std::push_heap(heap.begin(), heap.end());
                        }
                        else
                                heap.pop_back();
                        

                        if (heap.size() == 1)
                        {
                                output_.put(heap.back().value);

                                if(heap.back().is->next())
                                        copy_to_output(*heap.back().is);

                                heap.pop_back();
                        }
                }
        }

        void two_way_merge()
        {
                for (;;)
                {
                        auto a = input_[0].value();
                        auto b = input_[1].value();

                        if (a < b)
                        {
                                output_.put(a);
                                if (!input_[0].next()) {
                                        copy_to_output(input_[1]);
                                        return;
                                }

                        }
                        else if (b <= a)
                        {
                                output_.put(b);
                                if (!input_[1].next()) {
                                        copy_to_output(input_[0]);
                                        return;
                                }

                        }
                }
        }
private:
        std::vector<chunk_istream<T>> input_;
        chunk_ostream<T> output_;
        chunk_id output_id_;

        std::stringstream ss_;

        std::vector<std::string> remove_que_;
};
