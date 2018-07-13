#pragma once

#include <cstdint>
#include <sstream>

#include "settings.hpp"

struct chunk_id
{
        using id_t = uint16_t;
        using lvl_t = uint16_t;

        chunk_id() = default;
        chunk_id(lvl_t _lvl, id_t _id)
                : lvl(_lvl), id(_id)
        {}

        union
        {
                struct
                {
                        lvl_t lvl;
                        id_t  id;
                };

                uint32_t _bits;
        };
};


inline
bool operator<(const chunk_id& a, const chunk_id& b)
{
        return a._bits < b._bits;
}

inline
bool operator==(const chunk_id& a, const chunk_id& b)
{
        return a._bits == b._bits;
}

inline
std::string make_filename(uint32_t lvl, uint32_t id)
{
        std::stringstream ss;
        ss << CONFIG_SCHUNK_FILENAME_PAT
           << lvl << "_" << id;

        return ss.str();
}

inline
std::string make_filename(chunk_id id)
{
        return make_filename(id.lvl, id.id);
}

inline
std::ostream& operator<<(std::ostream& os, chunk_id id)
{
        os << make_filename(id);
        return os;
}