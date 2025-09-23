#pragma once
#include <stdlib.h>

#include <array>
#include <cstdint>
#include <tuple>
#include <type_traits>
namespace hsm
{

    using kind_t = std::uint64_t;

    namespace kind
    {
        constexpr size_t length = 64;
        constexpr size_t id_length = 8;
        constexpr size_t depth_max = length / id_length;
        constexpr size_t id_mask = (1ULL << id_length) - 1;

        constexpr kind_t id(kind_t kind)
        {
            return static_cast<kind_t>((kind)&id_mask);
        }

        constexpr auto bases(kind_t kind)
        {
            auto bases = std::array<kind_t, depth_max>{};
            for (size_t i = 1; i < depth_max; i++)
            {
                bases[i - 1] = (kind >> (id_length * i)) & id_mask;
            }
            return bases;
        }

    }; // namespace

    template <typename... TBases>
    constexpr kind_t make_kind(kind_t id = 0, TBases... bases)
    {
        static_assert(
            (std::is_convertible_v<TBases, kind_t> && ...),
            "bases must be convertible to kind_t");

        std::array<kind_t, kind::depth_max * kind::depth_max> kind_ids{};
        auto bases_ids = std::array<kind_t, sizeof...(bases)>{bases...};
        std::size_t index = 0;
        kind_t kind_id = (id + 1) & kind::id_mask;
        for (size_t i = 0; i < sizeof...(bases); i++)
        {
            auto base = bases_ids[i];
            for (size_t j = 0; j < kind::depth_max; j++)
            {
                kind_t base_id = (base >> (kind::id_length * j)) & kind::id_mask;
                if (base_id == 0)
                {
                    break;
                }
                bool exists = false;
                for (size_t k = 0; k < index; k++)
                {
                    if (kind_ids[k] == base_id)
                    {
                        exists = true;
                        break;
                    }
                }
                if (!exists)
                {
                    kind_ids[index] = base_id;
                    index++;
                    kind_id |= base_id << (kind::id_length * index);
                }
            }
        }

        return kind_id;
    }

    template <>
    constexpr kind_t make_kind(kind_t id)
    {
        return (id + 1) & kind::id_mask;
    }

    template <typename Tkind, typename TBase, typename... TBases>
    constexpr bool is_kind(Tkind kind, TBase base, TBases... bases)
    {
        return is_kind(kind, base) || is_kind(kind, bases...);
    }

    template <typename Tkind, typename TBase>
    constexpr bool is_kind(Tkind kind, TBase base)
    {
        kind_t base_id = kind::id(static_cast<kind_t>(base));
        for (size_t i = 0; i < kind::depth_max; i++)
        {
            kind_t current_id =
                kind::id(static_cast<kind_t>(kind) >> (kind::id_length * i));
            if (current_id == base_id)
            {
                return true;
            }
            else if (current_id == 0)
            {
                break;
            }
        }
        return false;
    }

    constexpr kind_t base(kind_t kind)
    {
        return kind >> kind::id_length;
    }
}; // namespace sf