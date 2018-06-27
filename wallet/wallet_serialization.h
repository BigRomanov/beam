#pragma once

#include "core/ecc_native.h"
#include "wallet/common.h"
#include "wallet/receiver.h"
#include "wallet/sender.h" 

#include "utility/serialize.h"

namespace yas::detail
{
    template<std::size_t F>
    struct serializer<type_prop::not_a_fundamental
                    , ser_method::use_internal_serializer
                    , F
                    , beam::wallet::Sender>
    {
        template<typename Archive>
        static Archive& save(Archive& ar, const beam::wallet::Sender& sender)
        {
            const_cast<beam::wallet::Sender&>(sender).serialize(ar, 0);
            return ar;
        }

        template<typename Archive>
        static Archive& load(Archive& ar, beam::wallet::Sender& sender)
        {
            sender.serialize(ar, 0);
            return ar;
        }
    };

    template<std::size_t F>
    struct serializer<type_prop::not_a_fundamental
        , ser_method::use_internal_serializer
        , F
        , beam::wallet::Receiver>
    {
        template<typename Archive>
        static Archive& save(Archive& ar, const beam::wallet::Receiver& receiver)
        {
            const_cast<beam::wallet::Receiver&>(receiver).serialize(ar, 0);
            return ar;
        }

        template<typename Archive>
        static Archive& load(Archive& ar, beam::wallet::Receiver& receiver)
        {
            receiver.serialize(ar, 0);
            return ar;
        }
    };

    template<std::size_t F, int NumberOfRegions>
    struct serializer<type_prop::not_a_fundamental
        , ser_method::use_internal_serializer
        , F
        , boost::msm::back::NoHistoryImpl<NumberOfRegions>>
    {
        using Type = boost::msm::back::NoHistoryImpl<NumberOfRegions>;
        template<typename Archive>
        static Archive& save(Archive& ar,const Type& d)
        {
            const_cast<Type&>(d).serialize(ar, 0);
            return ar;
        }

        template<typename Archive>
        static Archive& load(Archive& ar, Type& d)
        {
            d.serialize(ar, 0);
            return ar;
        }
    };

    template<std::size_t F>
    struct serializer<type_prop::not_a_fundamental
        , ser_method::use_internal_serializer
        , F
        , beam::wallet::Sender::FSMDefinition>
    {
        using Type = beam::wallet::Sender::FSMDefinition;
        template<typename Archive>
        static Archive& save(Archive& ar, const Type& d)
        {
            const_cast<Type&>(d).serialize(ar, 0);
            return ar;
        }

        template<typename Archive>
        static Archive& load(Archive& ar, Type& d)
        {
            d.serialize(ar, 0);
            return ar;
        }
    };

    template<std::size_t F>
    struct serializer<type_prop::not_a_fundamental
        , ser_method::use_internal_serializer
        , F
        , beam::wallet::Receiver::FSMDefinition>
    {
        using Type = beam::wallet::Receiver::FSMDefinition;
        template<typename Archive>
        static Archive& save(Archive& ar, const Type& d)
        {
            const_cast<Type&>(d).serialize(ar, 0);
            return ar;
        }

        template<typename Archive>
        static Archive& load(Archive& ar, Type& d)
        {
            d.serialize(ar, 0);
            return ar;
        }
    };

    template<std::size_t F>
    struct serializer<type_prop::not_a_fundamental
        , ser_method::use_internal_serializer
        , F
        , ECC::Scalar::Native>
    {
        using Type = ECC::Scalar::Native;
        template<typename Archive>
        static Archive& save(Archive& ar, const Type& v)
        {
            ECC::Scalar s;
            v.Export(s);
            ar & s;
            return ar;
        }

        template<typename Archive>
        static Archive& load(Archive& ar, Type& v)
        {
            ECC::Scalar s;
            ar & s;
            v.Import(s);
            return ar;
        }
    };

    template<std::size_t F>
    struct serializer<type_prop::not_a_fundamental
        , ser_method::use_internal_serializer
        , F
        , ECC::Point::Native>
    {
        using Type = ECC::Point::Native;
        template<typename Archive>
        static Archive& save(Archive& ar, const Type& v)
        {
            ECC::Point s;
            v.Export(s);
            ar & s;
            return ar;
        }

        template<typename Archive>
        static Archive& load(Archive& ar, Type& v)
        {
            ECC::Point s;
            ar & s;
            v.Import(s);
            return ar;
        }
    };
    template<std::size_t F>
    struct serializer<type_prop::not_a_fundamental
        , ser_method::use_internal_serializer
        , F
        , beam::Coin>
    {
        using Type = beam::Coin;
        template<typename Archive>
        static Archive& save(Archive& ar, const Type& v)
        {
            ar  & v.m_id
                & v.m_height
                & v.m_maturity
                & v.m_key_type
                & v.m_amount
                & v.m_status;
            return ar;
        }

        template<typename Archive>
        static Archive& load(Archive& ar, Type& v)
        {
            // TODO: store id only
            ar & v.m_id
               & v.m_height
               & v.m_maturity
               & v.m_key_type
               & v.m_amount
               & v.m_status;
            return ar;
        }
    };
}