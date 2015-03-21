/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */


Message::Message(){
	this->messageType =DUMMYLASTMSGTYPE;
}
Message::Message(char* b,size_t size){
	this->buf=b;
	this->messageType = this->getMessageType();
	this->getAddress();
	this->getHeartbeat();
	this->getId();
	this->getPort();
	this->size = size;
	MessageHdr *msg= (MessageHdr*)buf;
    MemberListEntry* p = (MemberListEntry *)((char *)(msg+1) + sizeof(this->getAddress()->addr) + sizeof(long));
	size = size - (sizeof(MessageHdr) + sizeof(this->getAddress()->addr) + sizeof(long));
	size_t entrysize = 24;
	while(size>0){
		size -= entrysize;
		this->memberList.push_back(*p);
		p+=1;
	}

}

// create JOINREQ message: format of data is {MessageHdr,Address.char[6],long heartbeat}
void Message::SetJoiner(Address address,long heartbeat){
	size_t msize = sizeof(MessageHdr) + sizeof(address.addr)+sizeof(long);
	this->buf=(char*)malloc(msize * sizeof(char));
	MessageHdr *msg= (MessageHdr*)buf;
    msg->msgType = JOINREQ;
    //msg + sizeof(MessageHdr)
    memcpy((char *)(msg+1), &address.addr, 6);
    //msg + sizeof(MessageHdr) + sizeof(address.addr)
    memcpy((char *)(msg+1) + sizeof(address.addr), &heartbeat, sizeof(long));
    this->size = msize;
}

// create JOINREP message: format of data is {MessageHdr,Address.char[6],long heartbeat,[MemberListEntry|]}
void Message::setJoinep(Address address,long heartbeat,vector<MemberListEntry> memberList){
	size_t msize = sizeof(MessageHdr) + sizeof(address.addr)+sizeof(long);
    size_t s =0;
	if(memberList.size()!=0){
		MemberListEntry entry = memberList[0];
		msize +=sizeof(entry)*memberList.size();
	}
	this->buf=(char*)malloc(msize * sizeof(char));
	MessageHdr *msg= (MessageHdr*)buf;
    msg->msgType = JOINREP;
    //msg + sizeof(MessageHdr)
    memcpy((char *)(msg+1), &address.addr, sizeof(address.addr));
    //msg + sizeof(MessageHdr) + sizeof(address.addr)
    memcpy((char *)(msg+1) + sizeof(address.addr), &heartbeat, sizeof(long));
    MemberListEntry* p = (MemberListEntry *)((char *)(msg+1) + sizeof(address.addr) + sizeof(long));

	for(int i=0;i<memberList.size();i++){
		MemberListEntry entry = memberList[i];
		s = sizeof(entry);
	    memcpy(p, &entry, sizeof(entry));
	    p+=1;
	}
	cout<<s<<endl;
	this->size = msize;
}

char* Message::getBuf(){
	return this->buf;
}

size_t Message::getSize(){
	return this->size;
}
MsgTypes Message::getMessageType(){
	return ((MessageHdr*) this->buf)->msgType;
}

vector<MemberListEntry> Message::getMemberListEntry(){
	return this->memberList;
}
Address* Message::getAddress(){
	if(this->address == NULL){
		Address* addr=new Address();
		MessageHdr *msg= (MessageHdr*)this->buf;
		memcpy(&addr->addr,(char *)(msg+1),6);
		this->address= addr;
	}
	return this->address;
}

int Message::getId(){
	if(this->id==-1){
		this->id = *(int*)(&this->getAddress()->addr);
	}
	return id;
}
short Message::getPort(){
	if(this->port==-1){
		this->port = *(short*)(&this->getAddress()->addr[4]);
	}
	return port;
}
long Message::getHeartbeat(){
	if(this->heartbeat==-1){
		this->heartbeat = (long)*(buf+sizeof(MessageHdr) + 6);
	}
	return this->heartbeat;
}


/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 * joinaddr: the address of the coordinator
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
    	Message* message = new Message();
    	message->SetJoiner(memberNode->addr,memberNode->heartbeat);

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introfducer member
		// &memberNode->addr: the address of this noded
		// joinaddr: the address of the coordinator
        emulNet->ENsend(&memberNode->addr, joinaddr, message->getBuf(), message->getSize());

    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
	this->memberNode->inGroup = true;
	 Member *memberNode = (Member *) env;
	 Message* message=new Message(data,(size_t)size);
	#ifdef DEBUGLOG
	 	 cout<<"Yo, " << memberNode->addr.getAddress() << " received a new message: ";
    #endif
	 switch(message->getMessageType()){
	 	 case     JOINREQ :
	 		 handleJoinRequest(message);
	 		 break;
	 	 case 	   JOINREP :
	 		handleGossipyRequest(message);
	 		 break;
	 	 case      DUMMYLASTMSGTYPE :
	 		 cout << " DUMMYLASTMSGTYPE "<<endl;
	 		 break;
	 	 default:
	 		 cout << "UNrecognized message";
	 }
	 return true;
}
void updateMemberList(Member *memberNode, long currenttime,vector<MemberListEntry> newMemberList){
	#ifdef DEBUGLOG
//		cout <<endl;
//		cout <<"node: " << memberNode->addr.getAddress() << " memberList received from: " << srcAddress->getAddress() <<endl;
//		for(int i=0; i <newMemberList.size();i++){
//			cout<<"address: " << newMemberList[i].id<<" heartbeat: " << newMemberList[i].heartbeat <<endl;
//		}
	#endif
	for(int i=0;i<memberNode->memberList.size();i++){
		for(int k=0;k<newMemberList.size();k++){
			if(memberNode->memberList[i].id==newMemberList[k].id){
				if(newMemberList[k].heartbeat >= memberNode->memberList[i].heartbeat){
					memberNode->memberList[i].heartbeat = newMemberList[k].heartbeat;
					memberNode->memberList[i].timestamp = currenttime;
				}
				newMemberList.erase(newMemberList.begin()+k);
				break;
			}
		}
	}
	for(int i=0;i<newMemberList.size();i++){
		if(newMemberList[i].id!=0){
			newMemberList[i].timestamp=currenttime;
			memberNode->memberList.push_back(newMemberList[i]);
		}
	}
	#ifdef DEBUGLOG
//		cout <<" after update, node " << memberNode->addr.getAddress()<<" 's memberList:"<<endl;
//		for(int i=0; i <memberNode->memberList.size();i++){
//			cout<<"address: " << memberNode->memberList[i].id<<" heartbeat: " << memberNode->memberList[i].heartbeat <<endl;
//		}
	#endif
}

/**
 *  help function, handle join request
 */
void MP1Node::handleGossipyRequest(Message *message){

	updateMemberList(this->memberNode, this->par->getcurrtime(),message->getMemberListEntry());

	// debug
	#ifdef DEBUGLOG
	cout << "JOINREP from: " << message->getId()<<":"<<message->getPort() << " HeartBeat: "<< message->getHeartbeat()<<endl;
	#endif
}



/**
 *  help function, handle join request
 */
void MP1Node::handleJoinRequest(Message *message){
	cout << "JOINREQ Message from: " << message->getId()<<":"<<message->getPort() << " HeartBeat: "<< message->getHeartbeat()<< ", at timestamp: "<<this->par->getcurrtime() <<endl;
	MemberListEntry *entry = new MemberListEntry(message->getId(),message->getPort(),message->getHeartbeat(),this->par->getcurrtime());

	for(vector<MemberListEntry>::iterator item = memberNode->memberList.begin(); item !=memberNode->memberList.end();++item){
		if(item->id == entry->id){
			item->heartbeat = max(item->heartbeat,entry->heartbeat);
			item->port = entry->port;
			item->timestamp = entry->timestamp;
			return;
		}
	}
	memberNode->memberList.push_back(*entry);

}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
	this->memberNode->heartbeat+=1;
	// delete dead node
	int i=0;
	while(i<memberNode->memberList.size()){
		MemberListEntry memberListEntry = memberNode->memberList[i];
		if(this->memberNode->heartbeat - memberListEntry.heartbeat >= 2 * TIMEOUT){
			#ifdef DEBUGLOG
				cout<<"time out "<< memberNode->memberList[i].id<< "   " << this->memberNode->addr.getAddress()<<" gona erase memberlist entry: " << memberNode->memberList[i].id<<endl;
			#endif
			memberNode->memberList.erase(memberNode->memberList.begin()+i);
			continue;
		}
		i++;
	}
	//int start = (rand() % (int)(max(0, (int) memberNode->memberList.size()-GOSSIPYSIZE)));
	int start=0;
//	//Propagate
	for(int i=start;i<(int)memberNode->memberList.size();i++){
		MemberListEntry entry=memberNode->memberList[i];
		Member *member = this->memberNode;
		propagateMemberList(entry,member);
	}
    return;
}

Address* createAddress(int id, short port){
	Address* address = new Address();
	address->init();
	memcpy(&address->addr[0], &id,sizeof(int));
	memcpy(&address->addr[4], &port, sizeof(short));
	return address;
}

/**
 * propagate memberlist to this memberEntry
 */
void MP1Node::propagateMemberList(MemberListEntry memberEntry, Member *member){
//	cout<<"propagateMemberList: " << memberEntry.id <<endl;
	int memberID = *(int*)(&member->addr.addr);
	if(memberID == memberEntry.id){
		return;
	}
	int id = memberEntry.id;
	short port = memberEntry.port;
	Address* address = createAddress(id,port);
	size_t msgsize=0;

	Message *message = new Message();
	message->setJoinep(member->addr,memberEntry.heartbeat,member->memberList);

	Message *recMsg = new Message(message->getBuf(),message->getSize());
    // send JOINREP message to introducer member
	// &memberNode->addr: the address of this node
	// address: the address of the target
    emulNet->ENsend(&memberNode->addr, address, message->getBuf(), message->getSize());
}



/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
