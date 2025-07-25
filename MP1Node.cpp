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
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
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
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
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
//Assignment starts here
bool MP1Node::recvCallBack(void *env, char *data, int size) {
    MessageHdr *msg = (MessageHdr *)data;

    if (msg->msgType == JOINREQ) {
        Address joinerAddr;
        memcpy(&joinerAddr.addr, data + sizeof(MessageHdr), sizeof(joinerAddr.addr));
        long hb;
        memcpy(&hb, data + sizeof(MessageHdr) + sizeof(joinerAddr.addr) + 1, sizeof(long));

        // Add new member to membership list
        int id = *(int*)(&joinerAddr.addr);
        short port = *(short*)(&joinerAddr.addr[4]);

        memberNode->memberList.emplace_back(id, port, hb, par->getcurrtime());

        // Send JOINREP back with this node’s membership list
        size_t msgsize = sizeof(MessageHdr) + memberNode->memberList.size() * sizeof(MemberListEntry);
        MessageHdr *reply = (MessageHdr *)malloc(msgsize);
        reply->msgType = JOINREP;

        memcpy((char*)(reply + 1), memberNode->memberList.data(), memberNode->memberList.size() * sizeof(MemberListEntry));
        emulNet->ENsend(&memberNode->addr, &joinerAddr, (char*)msg, msgsize);

        log->logNodeAdd(&memberNode->addr, &joinerAddr);
        free(reply);
    }

    else if (msg->msgType == JOINREP) {
        // Update our membership list from JOINREP
        size_t entryCount = (size - sizeof(MessageHdr)) / sizeof(MemberListEntry);
        MemberListEntry *entries = (MemberListEntry *)(data + sizeof(MessageHdr));

        for (size_t i = 0; i < entryCount; ++i) {
            bool found = false;
            for (auto &entry : memberNode->memberList) {
                if (entry.id == entries[i].id && entry.port == entries[i].port) {
                    found = true;
                    if (entries[i].heartbeat > entry.heartbeat) {
                        entry.heartbeat = entries[i].heartbeat;
                        entry.timestamp = par->getcurrtime();
                    }
                    break;
                }
            }
            if (!found) {
                memberNode->inGroup = true;
                memberNode->memberList.emplace_back(entries[i].id, entries[i].port, entries[i].heartbeat, par->getcurrtime());
            }
        }
    }

    else if (msg->msgType == HEARTBEAT) {
        Address senderAddr;
        memcpy(&senderAddr.addr, data + sizeof(MessageHdr), sizeof(senderAddr.addr));
        long hb;
        memcpy(&hb, data + sizeof(MessageHdr) + sizeof(senderAddr.addr) + 1, sizeof(long));

        int id = *(int*)(&senderAddr.addr);
        short port = *(short*)(&senderAddr.addr[4]);

        bool found = false;
        for (auto &entry : memberNode->memberList) {
            if (entry.id == id && entry.port == port) {
                found = true;
                if (hb > entry.heartbeat) {
                    entry.heartbeat = hb;
                    entry.timestamp = par->getcurrtime();
                }
                break;
            }
        }

        if (!found) {
            memberNode->memberList.emplace_back(id, port, hb, par->getcurrtime());
        }
    }

    return true;
}
//Assignment ends here

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
//Assignment starts here
void MP1Node::nodeLoopOps() {
    memberNode->heartbeat++;
    long currTime = par->getcurrtime();

    // Update own entry
    int selfId = *(int*)(&memberNode->addr.addr);
    short selfPort = *(short*)(&memberNode->addr.addr[4]);

    for (auto &entry : memberNode->memberList) {
        if (entry.id == selfId && entry.port == selfPort) {
            entry.heartbeat = memberNode->heartbeat;
            entry.timestamp = currTime;
        }
    }

    // Send heartbeat to a few random nodes
    for (auto &entry : memberNode->memberList) {
        if (entry.id == selfId && entry.port == selfPort) continue;

        Address target;
        *(int*)(&target.addr[0]) = entry.id;
        *(short*)(&target.addr[4]) = entry.port;

        size_t msgsize = sizeof(MessageHdr) + sizeof(memberNode->addr.addr) + sizeof(long) + 1;
        MessageHdr *hbMsg = (MessageHdr *) malloc(msgsize);
        hbMsg->msgType = HEARTBEAT;

        memcpy((char *)(hbMsg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(hbMsg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

        emulNet->ENsend(&memberNode->addr, &target, (char*)hbMsg, msgsize);
        free(hbMsg);
    }

    // Failure detection
    std::vector<MemberListEntry> updatedList;
    for (auto &entry : memberNode->memberList) {
        if (entry.id == selfId && entry.port == selfPort) {
            updatedList.push_back(entry);
            continue;
        }
        if (currTime - entry.timestamp <= TFAIL) {
            updatedList.push_back(entry);
        } else {
            // You can log failure if needed
        }
    }

    memberNode->memberList = updatedList;
}
//Assignment ends here

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
