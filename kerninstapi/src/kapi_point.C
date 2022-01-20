#include "kapi.h"


// Default constructor to create arrays of points
kapi_point::kapi_point() : address(0), pt_type(kapi_pre_instruction)
{
}

kapi_point::kapi_point(kptr_t addr, kapi_point_location tp) : 
    address(addr), pt_type(tp)
{
}

kptr_t kapi_point::getAddress() const
{
    return address;
}

kapi_point_location kapi_point::getType() const
{
    return pt_type;
}
