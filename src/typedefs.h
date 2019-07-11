#ifndef __H_TYPEDEFS
#define __H_TYPEDEFS

typedef unsigned int uint;
typedef unsigned char byte;
typedef unsigned long ulong;

#define subsizeof(STRUCT, MEMBER_BEGIN, MEMBER_END) (offsetof(STRUCT, MEMBER_END) + sizeof(decltype(std::declval<STRUCT>().MEMBER_END)) - offsetof(STRUCT, MEMBER_BEGIN))
#define sizeofmember(STRUCT, MEMBER) sizeof((( STRUCT *)0)-> MEMBER)



#endif