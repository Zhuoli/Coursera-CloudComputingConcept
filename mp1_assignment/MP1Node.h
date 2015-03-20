/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Header file of MP1Node class.
 **********************************/

#ifndef _MP1NODE_H_
#define _MP1NODE_H_

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "Member.h"
#include "EmulNet.h"
#include "Queue.h"

/**
 * Macros
 */
#define TREMOVE 20
#define TFAIL 5
#define GOSSIPYSIZE 5
#define TIMEOUT 10

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Message Types
 */
enum MsgTypes{
    JOINREQ,
    JOINREP,
    DUMMYLASTMSGTYPE
};

/**
 * STRUCT NAME: MessageHdr
 *
 * DESCRIPTION: Header and content of a message
 */
typedef struct MessageHdr {
	enum MsgTypes msgType;
}MessageHdr;

/**
 * CLASS NAME: MP1Node
 *
 * DESCRIPTION: Class implementing Membership protocol functionalities for failure detection
 */

class Message{
private:
	char* buf=NULL;
	size_t size=-1;
	MsgTypes messageType;
	Address *address=NULL;
	int id = -1;
	short port = -1;
	long heartbeat=-1;

public:
	Message();
	Message(char *,size_t size);
	//JOINREQ message: JOINREQ, Address, Heartbeat
	void SetJoiner(Address,long);
	//JOINREP message: JOINREP,Address,Heartbeat,MemberEntryList
	void setJoinep(Address,long,vector<MemberListEntry>);

	// return MessageHdr
	MsgTypes getMessageType();
	Address* getAddress();
	int getId();
	short getPort();
	long getHeartbeat();
	vector<MemberListEntry> getMemberListEntry();
	char* getBuf();
	size_t getSize();
};

class MP1Node {
private:
	EmulNet *emulNet;
	Log *log;
	Params *par;
	Member *memberNode;
	char NULLADDR[6];
	void handleJoinRequest(Message*);
	void handleGossipyRequest(char *, int);
	void propagateMemberList(MemberListEntry, Member *);

public:
	MP1Node(Member *, Params *, EmulNet *, Log *, Address *);
	Member * getMemberNode() {
		return memberNode;
	}
	int recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);
	void nodeStart(char *servaddrstr, short serverport);
	int initThisNode(Address *joinaddr);
	int introduceSelfToGroup(Address *joinAddress);
	int finishUpThisNode();
	void nodeLoop();
	void checkMessages();
	bool recvCallBack(void *env, char *data, int size);
	void nodeLoopOps();
	int isNullAddress(Address *addr);
	Address getJoinAddress();
	void initMemberListTable(Member *memberNode);
	void printAddress(Address *addr);
	virtual ~MP1Node();
};


#endif /* _MP1NODE_H_ */
