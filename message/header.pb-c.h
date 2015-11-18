/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: header.proto */

#ifndef PROTOBUF_C_header_2eproto__INCLUDED
#define PROTOBUF_C_header_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1000000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1001001 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct _Header__Header Header__Header;


/* --- enums --- */

typedef enum _Header__Module {
  HEADER__MODULE__LCMD = 1,
  HEADER__MODULE__VMDRIVER = 2,
  HEADER__MODULE__LCPD = 3,
  HEADER__MODULE__LCMOND = 4,
  HEADER__MODULE__LCSNFD = 5,
  HEADER__MODULE__VMCLOUDADAPTER = 6,
  HEADER__MODULE__POSTMAN = 7,
  HEADER__MODULE__LCRMD = 8,
  HEADER__MODULE__TALKER = 9,
  HEADER__MODULE__THIRDADAPTER = 10,
  HEADER__MODULE__PMCLOUDADAPTER = 11,
  HEADER__MODULE__MNTNCT = 12,
  HEADER__MODULE__NOTIFICATIONCENTER = 13,
  HEADER__MODULE__STOREKEEPER = 14
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(HEADER__MODULE)
} Header__Module;
typedef enum _Header__Type {
  HEADER__TYPE__UNICAST = 1
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(HEADER__TYPE)
} Header__Type;

/* --- messages --- */

struct  _Header__Header
{
  ProtobufCMessage base;
  /*
   * all fields must be required and fixed 
   */
  /*
   * module of sender 
   */
  uint32_t sender;
  /*
   * message type 
   */
  uint32_t type;
  /*
   * data length after this header 
   */
  uint32_t length;
  /*
   * message sequence number 
   */
  uint32_t seq;
};
#define HEADER__HEADER__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&header__header__descriptor) \
    , 0, 0, 0, 0u }


/* Header__Header methods */
void   header__header__init
                     (Header__Header         *message);
size_t header__header__get_packed_size
                     (const Header__Header   *message);
size_t header__header__pack
                     (const Header__Header   *message,
                      uint8_t             *out);
size_t header__header__pack_to_buffer
                     (const Header__Header   *message,
                      ProtobufCBuffer     *buffer);
Header__Header *
       header__header__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   header__header__free_unpacked
                     (Header__Header *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*Header__Header_Closure)
                 (const Header__Header *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCEnumDescriptor    header__module__descriptor;
extern const ProtobufCEnumDescriptor    header__type__descriptor;
extern const ProtobufCMessageDescriptor header__header__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_header_2eproto__INCLUDED */
