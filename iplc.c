#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#define NIP4_FMT "%u.%u.%u.%u"
#define NIP6_FMT "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x"

#define NIP4(addr) ((unsigned char*)&addr)[0], ((unsigned char*)&addr)[1], \
                   ((unsigned char*)&addr)[2], ((unsigned char*)&addr)[3]

#define NIP6(addr) ntohs((addr).s6_addr[0]), ntohs((addr).s6_addr[1]), \
                   ntohs((addr).s6_addr[2]), ntohs((addr).s6_addr[3]), \
                   ntohs((addr).s6_addr[4]), ntohs((addr).s6_addr[5]), \
                   ntohs((addr).s6_addr[6]), ntohs((addr).s6_addr[7])

/* column IDs */
enum {
  COL_LABEL,
  COL_IFADDR,
  COL_LOCALADDR,
  COL_BCASTADDR,
  COL_ANYCASTADDR,

  __NCOLUMNS
};

struct colinfo_t {
  const char *name;
  int whint;
};

struct colinfo_t infos[__NCOLUMNS] = {
  [COL_LABEL] = { "LABEL", 20 },
  [COL_IFADDR] = { "IFADDR", 20 },
  [COL_LOCALADDR] = { "LOCALADDR", 20 },
  [COL_BCASTADDR] = { "BCASTADDR", 20 },
  [COL_ANYCASTADDR] = { "ANYCASTADDR", 20 }
};

struct interface_t {
  char label[64];
  char ifaddr[64];
  char localaddr[64];
  char bcastaddr[64];
  char anycastaddr[64];
};

struct request_t {
  struct nlmsghdr nh;
  struct ifaddrmsg im;
};

int ncolumns;

static int create_request(struct request_t *req, int inetproto) {
  int sock;

  sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
  if (sock < 0) {
    perror("socket");
    return(-1);
  }

  /* Create a netlink message header of type RTM_GETADDR to signify what we're
   * requesting from the kernel interface table. NLM_F_REQUEST is required for
   * any message requesting data, and NLM_F_ROOT specifies that we want to get
   * the entire table back. This should eventually use NLM_F_MATCH to get a
   * specific interface. Note that NLM_F_MATCH isn't always available.
   */

  memset(req, 0, sizeof *req);
  req->nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
  req->nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
  req->nh.nlmsg_type = RTM_GETADDR;
  req->im.ifa_family = inetproto;

  return sock;
}

static struct interface_t *read_response(struct nlmsghdr *msg, int proto) {
  struct ifaddrmsg *rtmp;
  struct interface_t *iface;
  struct rtattr *rtatp;
  struct in_addr *inp;
  struct in6_addr *in6p;

  int rtattrlen;

  iface = calloc(1, sizeof *iface);

  rtmp = (struct ifaddrmsg *)NLMSG_DATA(msg);
  rtatp = (struct rtattr *)IFA_RTA(rtmp);

  rtattrlen = IFA_PAYLOAD(msg);
  while (RTA_OK(rtatp, rtattrlen)) {
    /* these attribute types can be found in rtnetlink(7) */
    switch (rtatp->rta_type) {
      case IFA_LABEL:
        snprintf(iface->label, 64, "%s", (const char*)RTA_DATA(rtatp));
        break;
      case IFA_ADDRESS:
        if (proto == AF_INET) {
          inp = (struct in_addr *)RTA_DATA(rtatp);
          snprintf(iface->ifaddr, 64, NIP4_FMT, NIP4(*inp));
        } else if (proto == AF_INET6) {
          in6p = (struct in6_addr *)RTA_DATA(rtatp);
          snprintf(iface->ifaddr, 64, NIP6_FMT, NIP6(*in6p));
        }
        break;
      case IFA_LOCAL:
        if (proto == AF_INET) {
          inp = (struct in_addr *)RTA_DATA(rtatp);
          snprintf(iface->localaddr, 64, NIP4_FMT, NIP4(*inp));
        } else if (proto == AF_INET6) {
          in6p = (struct in6_addr *)RTA_DATA(rtatp);
          snprintf(iface->localaddr, 64, NIP6_FMT, NIP6(*in6p));
        }
        break;
      case IFA_BROADCAST:
        if (proto == AF_INET) {
          inp = (struct in_addr *)RTA_DATA(rtatp);
          snprintf(iface->bcastaddr, 64, NIP4_FMT, NIP4(*inp));
        } else if (proto == AF_INET6) {
          in6p = (struct in6_addr *)RTA_DATA(rtatp);
          snprintf(iface->bcastaddr, 64, NIP6_FMT, NIP4(*in6p));
        }
        break;
      case IFA_ANYCAST:
        if (proto == AF_INET) {
          inp = (struct in_addr *)RTA_DATA(rtatp);
          snprintf(iface->anycastaddr, 64, NIP4_FMT, NIP4(*inp));
        } else if (proto == AF_INET6) {
          in6p = (struct in6_addr *)RTA_DATA(rtatp);
          snprintf(iface->anycastaddr, 64, NIP6_FMT, NIP4(*in6p));
        }
        break;
    }
    rtatp = RTA_NEXT(rtatp, rtattrlen);
  }
  return iface;

  return NULL;
}

void print_interface(struct interface_t *iface) {
  printf("%s: %s\n", iface->label, iface->ifaddr);
  return;
}

int main(void) {
  struct nlmsghdr *nlmh;
  struct request_t req;
  struct interface_t *iface;

  ssize_t msglen;
  int rtnsock;
  char buf[16384];


  /* netlink magic happens below here */
  rtnsock = create_request(&req, AF_INET);
  if (rtnsock < 0) {
    return 1;
  }

  msglen = send(rtnsock, &req, req.nh.nlmsg_len, 0);
  if (msglen < 0) {
    perror("send");
    return 1;
  }

  msglen = recv(rtnsock, buf, sizeof(buf), 0);
  if (msglen < 0) {
    perror("recv");
    return 1;
  } else if (msglen == 0) {
    /* this should never happen */
    fprintf(stderr, "unexpected EOF\n");
    return 1;
  }

  /* Typically the message is stored in buf, so we need to parse the message to
   * get the required data for our display. */
  nlmh = (struct nlmsghdr*)buf;
  while (msglen > (ssize_t)sizeof(*nlmh)) {
    int len = nlmh->nlmsg_len;
    int req_len = len - sizeof(*nlmh);
    if (req_len < 0 || len > msglen) {
      fprintf(stderr, "error\n");
      return 1;
    }

    if (!NLMSG_OK(nlmh, msglen)) {
      fprintf(stderr, "error: NLMSG truncated\n");
      return 1;
    }

    iface = read_response(nlmh, AF_INET);
    if (iface) {
      print_interface(iface);

      /* move onto the next response */
      msglen -= NLMSG_ALIGN(len);
      nlmh = (struct nlmsghdr*)((char*)nlmh + NLMSG_ALIGN(len));
      free(iface);
    }
  }

  return 0;
}

