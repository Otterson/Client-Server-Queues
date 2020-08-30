#include "common.h"
#include "MQreqchannel.h"
#include <mqueue.h>

using namespace std;

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR/DESTRUCTOR FOR CLASS   R e q u e s t C h a n n e l  */
/*--------------------------------------------------------------------------*/

MQRequestChannel::MQRequestChannel(const string _name, const Side _side, int _bs) : RequestChannel(_name, _side){
	pipe1 = "/mq_" + my_name + "1";
	pipe2 = "/mq_" + my_name + "2";

	buffersize = _bs;
	if (_side == SERVER_SIDE){
		wfd = open_pipe(pipe1, O_WRONLY);
		rfd = open_pipe(pipe2, O_RDONLY);
	}
	else{
		rfd = open_pipe(pipe1, O_RDONLY);
		wfd = open_pipe(pipe2, O_WRONLY);
	}

}

MQRequestChannel::~MQRequestChannel(){
	mq_close(wfd);
	mq_close(rfd);

	mq_unlink(pipe1.c_str());
	mq_unlink(pipe2.c_str());
}

int MQRequestChannel::open_pipe(string _pipe_name, int mode){
	struct mq_attr attr{0, 1, buffersize, 0};
	int fd = mq_open(_pipe_name.c_str(), O_RDWR | O_CREAT, 0600, &attr);
	if (fd < 0){
		EXITONERROR(_pipe_name);
	}
	return fd;
}

int MQRequestChannel::cread(void* msgbuf, int bufcapacity){
	return mq_receive(rfd, (char *) msgbuf, buffersize, 0);
}

int MQRequestChannel::cwrite(void* msgbuf, int len){
	return mq_send(wfd, (char *) msgbuf, len, 0);
}
