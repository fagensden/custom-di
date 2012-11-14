#include "string.h"
#include "syscalls.h"
#include "global.h"

void udelay(int us)
{
	u8 heap[0x10];
	struct ipcmessage *Message;
	int mqueue = -1;
	int TimeID = -1;

	mqueue = MessageQueueCreate(heap, 1);
	if(mqueue < 0)
		goto out;
	TimeID = TimerCreate(us, 0, mqueue, 0xbabababa);
	if(TimeID < 0)
		goto out;
	MessageQueueReceive(mqueue, &Message, 0);
	
out:
	if(TimeID > 0)
		TimerDestroy(TimeID);
	if(mqueue > 0)
		MessageQueueDestroy(mqueue);
}