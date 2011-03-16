// common.cpp

#include "common.h"
#include "sys.h"
#include "socket.h"

///////////////////////////////////////////////
// classs GunID

// ---------------------------
/// GunIDが同じかどうか
bool	GnuID::isSame(GnuID &gid)
{
	for (int i = 0; i < 16 ; ++i)
		if (gid.id[i] != id[i])
			return false;
	return true;
}

// ---------------------------
/// GunIDが既にセットされてるかどうか
bool	GnuID::isSet()
{
	for (int i = 0; i < 16 ; ++i)
		if (id[i] != 0)
			return true;
	return false;
}

// ---------------------------
/// GunIDをクリアする
void	GnuID::clear()
{
	for(int i = 0; i < 16; ++i)
		id[i] = 0;
	storeTime = 0;
}

// ---------------------------
void GnuID::encode(Host *h, const char *salt1, const char *salt2, unsigned char salt3)
{
	int s1 = 0, s2 = 0;
	for(int i = 0; i < 16; ++i)
	{
		unsigned char ipb = id[i];

		// encode with IP address 
		if (h)
			ipb ^= ((unsigned char *)&h->ip)[i&3];

		// add a bit of salt 
		if (salt1)
		{
			if (salt1[s1])
				ipb ^= salt1[s1++];
			else
				s1=0;
		}

		// and some more
		if (salt2)
		{
			if (salt2[s2])
				ipb ^= salt2[s2++];
			else
				s2=0;
		}

		// plus some pepper
		ipb ^= salt3;

		id[i] = ipb;
	}

}

// ---------------------------
void GnuID::toStr(char *str)
{

	str[0] = 0;
	for(int i = 0; i < 16; ++i)
	{
		char tmp[8];
		unsigned char ipb = id[i];

		sprintf(tmp,"%02X",ipb);
		strcat(str,tmp);
	}

}

// ---------------------------
void GnuID::fromStr(const char *str)
{
	clear();

	if (strlen(str) < 32)
		return;

	char buf[8];

	buf[2] = 0;

	for (int i = 0; i < 16; ++i)
	{
		buf[0] = str[i*2];
		buf[1] = str[i*2+1];
		id[i] = (unsigned char)strtoul(buf,NULL,16);
	}

}

// ---------------------------
void GnuID::generate(unsigned char flags)
{
	clear();

	for (int i = 0; i < 16; ++i)
		id[i] = sys->rnd();

	id[0] = flags;
}

// ---------------------------
unsigned char GnuID::getFlags()
{
	return id[0];
}



//////////////////////////////////////////////////
// class GunIDList

// ---------------------------
GnuIDList::GnuIDList(int max)
	: ids(new GnuID[max])
{
	maxID = max;
	for(int i = 0; i < maxID; ++i)
		ids[i].clear();
}

// ---------------------------
GnuIDList::~GnuIDList()
{
	delete [] ids;
}

// ---------------------------
bool GnuIDList::contains(GnuID &id)
{
	for(int i = 0; i < maxID; ++i)
		if (ids[i].isSame(id))
			return true;
	return false;
}

// ---------------------------
int GnuIDList::numUsed()
{
	int cnt = 0;
	for(int i = 0; i < maxID; ++i)
		if (ids[i].storeTime)
			cnt++;
	return cnt;
}

// ---------------------------
unsigned int GnuIDList::getOldest()
{
	unsigned int t=(unsigned int)-1;
	for(int i = 0; i < maxID; ++i)
		if (ids[i].storeTime)
			if (ids[i].storeTime < t)
				t = ids[i].storeTime;
	return t;
}

// ---------------------------
void GnuIDList::add(GnuID &id)
{
	unsigned int minTime = (unsigned int) -1;
	int minIndex = 0;

	// find same or oldest
	for(int i = 0; i < maxID; ++i)
	{
		if (ids[i].isSame(id))
		{
			ids[i].storeTime = sys->getTime();
			return;
		}
		if (ids[i].storeTime <= minTime)
		{
			minTime = ids[i].storeTime;
			minIndex = i;
		}
	}

	ids[minIndex] = id;
	ids[minIndex].storeTime = sys->getTime();
}

// ---------------------------
void GnuIDList::clear()
{
	for(int i = 0; i < maxID; ++i)
		ids[i].clear();
}



/////////////////////////////////////////////////////
// class Host

// ---------------------------
/// コンストラクタ
Host::Host(unsigned int i, unsigned short p)
{
	ip = i;
	port = p;
	value = 0;
}

// ---------------------------
/// Host初期化
void	Host::init()
{
	ip = 0;
	port = 0;
	value = 0;
}

// -----------------------------------
bool Host::isMemberOf(Host &h)
{
	if (h.ip==0)
		return false;

    if( h.ip0() != 255 && ip0() != h.ip0() )
        return false;
    if( h.ip1() != 255 && ip1() != h.ip1() )
        return false;
    if( h.ip2() != 255 && ip2() != h.ip2() )
        return false;
    if( h.ip3() != 255 && ip3() != h.ip3() )
        return false;

/* removed for endieness compatibility
	for(int i=0; i<4; i++)
		if (h.ipByte[i] != 255)
			if (ipByte[i] != h.ipByte[i])
				return false;
*/
	return true;
}


// ---------------------------
/// グローバルIPがどうか
bool	Host::globalIP()
{
	// local host
	if ((ip3() == 127) && (ip2() == 0) && (ip1() == 0) && (ip0() == 1))
		return false;

	// class A
	if (ip3() == 10)
		return false;

	// class B
	if ((ip3() == 172) && (ip2() >= 16) && (ip2() <= 31))
		return false;

	// class C
	if ((ip3() == 192) && (ip2() == 168))
		return false;

	return true;
}

// ---------------------------
/// ループバッグIP(127.0.0.1)かどうか
bool	Host::loopbackIP()
{
//		return ((ipByte[3] == 127) && (ipByte[2] == 0) && (ipByte[1] == 0) && (ipByte[0] == 1));
	return ((ip3() == 127) && (ip2() == 0) && (ip1() == 0) && (ip0() == 1));
}

// ---------------------------
bool	Host::isSameType(Host &h)
{
	return ( (globalIP() && h.globalIP()) ||
			    (!globalIP() && !h.globalIP()) ); 
}

// ---------------------------
void	Host::IPtoStr(char *str)
{
	sprintf(str,"%d.%d.%d.%d", ip3(), ip2(), ip1(), ip0());
}

// ---------------------------
void	Host::toStr(char *str) const
{
	sprintf(str,"%d.%d.%d.%d:%d", ip3(), ip2(), ip1(), ip0(), port);
}


// ------------------------------------------
bool Host::isLocalhost()
{
	return loopbackIP() || (ip == ClientSocket::getIP(NULL)); 
}

// ------------------------------------------
void Host::fromStrName(const char *str, int p)
{
	if (!strlen(str))
	{
		port = 0;
		ip = 0;
		return;
	}

	char name[128];
	strncpy(name,str,sizeof(name)-1);
	name[127] = '\0';
	port = p;
	char *pp = strstr(name,":");
	if (pp)
	{
		port = atoi(pp+1);
		pp[0] = 0;
	}

	ip = ClientSocket::getIP(name);
}
// ------------------------------------------
void Host::fromStrIP(const char *str, int p)
{
	unsigned int ipb[4];
	unsigned int ipp;


	if (strstr(str,":"))
	{
		if (sscanf(str,"%03d.%03d.%03d.%03d:%d",&ipb[0],&ipb[1],&ipb[2],&ipb[3],&ipp) == 5)
		{
			ip = ((ipb[0]&0xff) << 24) | ((ipb[1]&0xff) << 16) | ((ipb[2]&0xff) << 8) | ((ipb[3]&0xff));
			port = ipp;
		}else
		{
			ip = 0;
			port = 0;
		}
	}else{
		port = p;
		if (sscanf(str,"%03d.%03d.%03d.%03d",&ipb[0],&ipb[1],&ipb[2],&ipb[3]) == 4)
			ip = ((ipb[0]&0xff) << 24) | ((ipb[1]&0xff) << 16) | ((ipb[2]&0xff) << 8) | ((ipb[3]&0xff));
		else
			ip = 0;
	}
}











