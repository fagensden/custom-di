#define swab32(x) ((u32)(                                     \
                                   (((u32)(x) & (u32)0x000000ffUL) << 24) | \
                                   (((u32)(x) & (u32)0x0000ff00UL) <<  8) | \
                                   (((u32)(x) & (u32)0x00ff0000UL) >>  8) | \
                                   (((u32)(x) & (u32)0xff000000UL) >> 24)))
#define swab16(x) ((u16)(                                   \
                (((u16)(x) & (u16)0x00ffU) << 8) |          \
                (((u16)(x) & (u16)0xff00U) >> 8)))

extern void *memset8( void *dst, int w, size_t length );
extern void *memset16( void *dst, int w, size_t length );
extern void *memset32( void *dst, int w, size_t length ); 