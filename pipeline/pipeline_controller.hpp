#pragma once
#include <cerrno>
#include "../tools/exception.hpp"
#include "pipeline.hpp"

template<typename T>
class pipeline_controller
{
public:
        pipeline_controller(raw_file_reader&& input_file, size_t max_chunk_size,
                size_t n_way_merge, uint32_t threads_n,
                size_t mem_avail, float io_ratio,
                std::string output_filename)
                : max_chunk_size_(round_down(max_chunk_size, sizeof(T))),
                n_way_merge_(n_way_merge),
                threads_n_(threads_n),
                output_filename_(std::move(output_filename)),
                thrmu_(threads_n_),
                tmu_(std::move(input_file), max_chunk_size_, n_way_merge_),
                mmu_(mem_avail, threads_n_, io_ratio)

        {
        }

        void run()
        {
                thrmu_.spawn_and_join([this](uint32_t id) {
                        pipeline<T> pl(id, thrmu_, tmu_, mmu_);
                        pl.run();
                });


                if (std::rename(tmu_.result_id().to_full_filename().c_str(),
                        output_filename_.c_str()) != 0)
                {
                        THROW_EXCEPTION("Cannot rename '"
                                        << tmu_.result_id().to_full_filename()
                                        << "' to '" << output_filename_
                                        << "': " << strerror(errno));
                }
        }

private:
        const size_t max_chunk_size_;
        const size_t n_way_merge_;
        const uint32_t threads_n_;
        std::string output_filename_;

        thread_management_unit thrmu_;
        task_management_unit<T> tmu_;
        memory_management_unit mmu_;
};
