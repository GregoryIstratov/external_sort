#pragma once

#include <algorithm>
#include <map>
#include "chunk.hpp"
#include "util.hpp"

template<typename T>
class chunk_sort_task
{
public:
        chunk_sort_task() = default;

        explicit
        chunk_sort_task(std::vector<T>&& data, chunk_id id)
                : data_(std::move(data)), id_(id)
        {}

        chunk_sort_task(chunk_sort_task&&) noexcept = default;
        chunk_sort_task& operator=(chunk_sort_task&&) noexcept = default;

        void execute()
        {
                perf_timer tm_;

                tm_.start();

                sort();

                tm_.end();

                info2() << "sorted " << make_filename(id_)
                        << " (" << size_format(size())
                        << "/" << num_format(count()) << ") for "
                        << tm_.elapsed<perf_timer::ms>() << " ms";
        }

        void release()
        {
                data_ = std::vector<T>();
        }

        bool empty() { return data_.empty(); }

        chunk_id id() const { return id_; }
        const auto* data() const { return &data_[0]; }
        size_t size() const { return data_.size() * sizeof(T); }
        size_t count() const { return data_.size(); }

private:
        void sort()
        {
                switch(CONFIG_SORT_ALGO)
                {
                        case CONFIG_SORT_STD:
                                std_sort();
                                break;

                        case CONFIG_SORT_HEAP:
                                heap_sort();
                                break;

                        default:
                                return;
                }
        }

        void heap_sort()
        {
                std::make_heap(data_.begin(), data_.end());
                std::sort_heap(data_.begin(), data_.end());
        }

        void std_sort()
        {
                std::sort(data_.begin(), data_.end());
        }

private:
        std::vector<T> data_;
        chunk_id id_;
};

template<typename T>
class chunk_merge_task
{
public:
        chunk_merge_task() = default;

        chunk_merge_task(std::vector<chunk_istream<T>>&& _input,
                         chunk_ostream<T>&& _output,
                         chunk_id _output_id)
                : input(std::move(_input)),
                  output(std::move(_output)),
                  output_id(_output_id)
        {
        }

        chunk_merge_task(chunk_merge_task&&) = default;
        chunk_merge_task& operator=(chunk_merge_task&&) = default;

        void execute(size_t in_buff_size, size_t out_buff_size)
        {
                perf_timer tm_;
                tm_.start();

                if(IS_ENABLED(CONFIG_REMOVE_TMP_FILES))
                        make_remove_queue();

                size_t ick_mem = round_down(in_buff_size/input.size(), sizeof(T));
                size_t ock_mem = round_down(out_buff_size, sizeof(T));

                if(!ick_mem || !ock_mem)
                        throw_exception("No memory for buffers [ick_mem="
                                                              << ick_mem
                                                              << " ock_mem="
                                                              << ock_mem
                                                              << "]");

                for(auto& is : input)
                        is.open(ick_mem);

                output.open(ock_mem);

                if(CONFIG_INFO_LEVEL >= 2)
                {
                        ss_ << "Merged { ";
                        for(const auto& k : input)
                                ss_ << k.filename() << " ("
                                    << size_format(k.buff_size())
                                    << "/" << size_format(k.size())
                                    << "/" << num_format(k.count()) << ") ";
                }

                if (input.size() == 2)
                        two_way_merge();
                else
                        pq_merge();

                output.close();

                if(IS_ENABLED(CONFIG_REMOVE_TMP_FILES))
                        remove_tmp_files();

                if(CONFIG_INFO_LEVEL >= 2)
                {
                        tm_.end();

                        ss_ << " } -> { " << make_filename(id()) << " ("
                            << size_format(output.buff_size()) << ")"
                            << " } for " << tm_.elapsed<perf_timer::ms>() << " ms";
                }

        }

        std::string debug_str() const { return ss_.str(); }

        chunk_id id() const { return output_id; }

        void set_output_filename(const std::string& value)
        {
                output.filename(value);
        }

        void release()
        {
                input = decltype(input)();
                output = decltype(output)();
        }

private:

        void make_remove_queue()
        {
                for(auto& is : input)
                        remove_que_.push_back(is.filename());

        }

        void remove_tmp_files()
        {
                // closes all files
                input = decltype(input)();

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
                is.copy(output);
        }


        // cache friendly heap items
        struct heap_item
        {
                T value;                
                chunk_istream<T>* is;

                friend static bool operator<(const heap_item& a, const heap_item& b)
                {
                        return a.value > b.value;
                }
        };
        
        void pq_merge()
        {
                std::vector<heap_item> heap(input.size());

                for (uint32_t i = 0; i < input.size(); ++i)
                {
                        heap[i].value = input[i].value();
                        heap[i].is = &input[i];
                }

                std::make_heap(heap.begin(), heap.end());
                while (!heap.empty())
                {
                        std::pop_heap(heap.begin(), heap.end());

                        T v = heap.back().value;
                        output.put(v);

                        if (heap.back().is->next())
                                heap.back().value = heap.back().is->value();
                        else
                                heap.pop_back();

                        std::push_heap(heap.begin(), heap.end());

                        if (heap.size() == 1)
                        {
                                copy_to_output(*heap.back().is);
                                heap.pop_back();
                        }
                }
        }

        void two_way_merge()
        {
                for (;;)
                {
                        auto a = input[0].value();
                        auto b = input[1].value();

                        if (a < b)
                        {
                                output.put(a);
                                if (!input[0].next()) {
                                        copy_to_output(input[1]);
                                        return;
                                }

                        }
                        else if (b < a)
                        {
                                output.put(b);
                                if (!input[1].next()) {
                                        copy_to_output(input[0]);
                                        return;
                                }

                        }
                        else
                        {
                                output.put(a);
                                if (!input[0].next()) {
                                        copy_to_output(input[1]);
                                        return;
                                }
                        }
                }
        }
private:
        std::vector<chunk_istream<T>> input;
        chunk_ostream<T> output;
        chunk_id output_id;

        std::stringstream ss_;

        std::vector<std::string> remove_que_;
};
