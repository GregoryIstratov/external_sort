#pragma once
#include <ostream>
#include "../config.hpp"
#include "../tools/format.hpp"
#include "../tools/exception.hpp"

struct chunk_id
{
        using id_t = uint32_t;
        using lvl_t = uint32_t;

        chunk_id() = default;

        chunk_id(lvl_t _lvl, id_t _id)
                : lvl(_lvl), id(_id)
        {}

        explicit
                chunk_id(const char* name)
        {
                const char* name_end = name + std::strlen(name);

                const char* sep_pos = std::strchr(name, CONFIG_CHUNK_NAME_SEP);
                if (sep_pos == nullptr)
                        throw_exception("Failed to find separator '"
                                << CONFIG_CHUNK_NAME_SEP
                                << "' in '" << name << "'");


                // each byte can be represented as a 2 digit number in hex
                constexpr size_t hex_digits = sizeof(lvl_t) * 2;
                char lvl_s[hex_digits + 1];
                const char* id_s = sep_pos + 1;

                std::strncpy(lvl_s, name, sep_pos - name);
                lvl_s[sep_pos - name] = '\0';
                const char* lvl_s_end = &lvl_s[sep_pos - name];

                char* end;
                lvl = (lvl_t)std::strtoull(lvl_s, &end, 16);
                if (errno != 0 || end != lvl_s_end)
                        throw_exception("Cannot convert '" << lvl_s << "' to an integer");

                id = (id_t)std::strtoull(id_s, &end, 16);
                if (errno != 0 || end != name_end)
                        throw_exception("Cannot convert '" << id_s << "' to an integer");
        }


        chunk_id(const chunk_id&) noexcept = default;
        chunk_id& operator=(const chunk_id&) noexcept = default;

        chunk_id(chunk_id&& o) noexcept
                : _bits(o._bits)
        {
                o._bits = (uint64_t)-1;
        }

        chunk_id& operator=(chunk_id&& o) noexcept
        {
                if (&o == this)
                        return *this;

                _bits = o._bits;
                o._bits = (uint64_t)-1;

                return *this;
        }

        std::string to_filename() const
        {
                return format::to_hex_string(lvl)
                        + CONFIG_CHUNK_NAME_SEP
                        + format::to_hex_string(id);
        }

        std::string to_full_filename() const
        {
                return std::string(CONFIG_CHUNK_DIR) + "/" + to_filename();
        }

        union
        {
                struct
                {
                        lvl_t lvl;
                        id_t  id;
                };

                uint64_t _bits = (uint64_t)-1;
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
std::ostream& operator<<(std::ostream& os, const chunk_id& id)
{
        os << id.to_filename();
        return os;
}
