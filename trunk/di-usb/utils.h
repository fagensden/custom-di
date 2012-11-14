
#ifndef __UTILS_H__
#define __UTILS_H__

#define swab32(x) ((u32)(                                     \
                                   (((u32)(x) & (u32)0x000000ffUL) << 24) | \
                                   (((u32)(x) & (u32)0x0000ff00UL) <<  8) | \
                                   (((u32)(x) & (u32)0x00ff0000UL) >>  8) | \
                                   (((u32)(x) & (u32)0xff000000UL) >> 24)))
#define swab16(x) ((u16)(                                   \
                (((u16)(x) & (u16)0x00ffU) << 8) |          \
                (((u16)(x) & (u16)0xff00U) >> 8)))

static inline u32 read32(u32 addr)
{
	u32 data;
	__asm__ volatile ("ldr\t%0, [%1]" : "=l" (data) : "l" (addr));
	return data;
}

static inline void write32(u32 addr, u32 data)
{
	__asm__ volatile ("str\t%0, [%1]" : : "l" (data), "l" (addr));
}

static inline u32 set32(u32 addr, u32 set)
{
	u32 data;
	__asm__ volatile (
		"ldr\t%0, [%1]\n"
		"\torr\t%0, %2\n"
		"\tstr\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set)
	);
	return data;
}

static inline u32 clear32(u32 addr, u32 clear)
{
	u32 data;
	__asm__ volatile (
		"ldr\t%0, [%1]\n"
		"\tbic\t%0, %2\n"
		"\tstr\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (clear)
	);
	return data;
}

void *memset32(void *, int, size_t);

#endif

