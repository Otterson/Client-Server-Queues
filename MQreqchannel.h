
#ifndef _MQreqchannel_H_
#define _MQreqchannel_H_

#include "common.h"
#include "Reqchannel.h"

class MQRequestChannel: public RequestChannel{

private:
	/*  The current implementation uses named pipes. */
	int wfd;
	int rfd;

	string pipe1, pipe2;
	int open_pipe(string _pipe_name, int mode);
	int buffersize;
public:
	MQRequestChannel(const string _name, const Side _side, int _buffersize);

	~MQRequestChannel();

	int cread(void* msgbuf, int bufcapacity);

	int cwrite(void *msgbuf , int msglen);

};

#endif
